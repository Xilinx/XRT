/*
 * FPGA Manager bindings for XRT driver
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors: Sonal Santan
 *          Jan Stephan <j.stephan@hzdr.de>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * FPGA Mgr integration is support limited to Ubuntu for now. RHEL/CentOS 7.X
 * kernels do not support FPGA Mgr yet.
 */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0) && defined(ENABLE_FPGA_MGR_SUPPORT)
#define FPGA_MGR_SUPPORT
#include <linux/fpga/fpga-mgr.h>
#endif

#include "../xocl_drv.h"
#include "xclbin.h"

/*
 * Container to capture and cache full xclbin as it is passed in blocks by FPGA
 * Manager. xocl needs access to full xclbin to walk through xclbin sections. FPGA
 * Manager's .write() backend sends incremental blocks without any knowledge of
 * xclbin format forcing us to collect the blocks and stitch them together here.
 * TODO:
 * 1. Add a variant of API, icap_download_bitstream_axlf() which works off kernel buffer
 * 2. Call this new API from FPGA Manager's write complete hook, xocl_pr_write_complete()
 */

struct xfpga_klass {
	struct xocl_dev *xdev;
	struct axlf *blob;
	char name[64];
	size_t count;
#if defined(FPGA_MGR_SUPPORT)
	enum fpga_mgr_states state;
#endif
};

#if defined(FPGA_MGR_SUPPORT)
static int xocl_pr_write_init(struct fpga_manager *mgr,
			      struct fpga_image_info *info, const char *buf, size_t count)
{
	struct xfpga_klass *obj = mgr->priv;
	const struct axlf *bin = (const struct axlf *)buf;
	if (count < sizeof(struct axlf)) {
	 	obj->state = FPGA_MGR_STATE_WRITE_INIT_ERR;
		return -EINVAL;
	}

	if (count > bin->m_header.m_length) {
	 	obj->state = FPGA_MGR_STATE_WRITE_INIT_ERR;
		return -EINVAL;
	}

	/* Free up the previous blob */
	vfree(obj->blob);
	obj->blob = vmalloc(bin->m_header.m_length);
	if (!obj->blob) {
		obj->state = FPGA_MGR_STATE_WRITE_INIT_ERR;
		return -ENOMEM;
	}

	memcpy(obj->blob, buf, count);
	xocl_info(&mgr->dev, "Begin download of xclbin %pUb of length %lld B", &obj->blob->m_header.uuid,
		  obj->blob->m_header.m_length);
	obj->count = count;
	obj->state = FPGA_MGR_STATE_WRITE_INIT;
	return 0;
}

static int xocl_pr_write(struct fpga_manager *mgr,
			 const char *buf, size_t count)
{
	struct xfpga_klass *obj = mgr->priv;
	char *curr = (char *)obj->blob;

	if ((obj->state != FPGA_MGR_STATE_WRITE_INIT) && (obj->state != FPGA_MGR_STATE_WRITE)) {
		obj->state = FPGA_MGR_STATE_WRITE_ERR;
		return -EINVAL;
	}

	curr += obj->count;
	obj->count += count;
	/* Check if the xclbin buffer is not longer than advertised in the header */
	if (obj->blob->m_header.m_length < obj->count) {
		obj->state = FPGA_MGR_STATE_WRITE_ERR;
		return -EINVAL;
	}
	memcpy(curr, buf, count);
	xocl_info(&mgr->dev, "Next block of %zu B of xclbin %pUb", count, &obj->blob->m_header.uuid);
	obj->state = FPGA_MGR_STATE_WRITE;
	return 0;
}


static int xocl_pr_write_complete(struct fpga_manager *mgr,
				  struct fpga_image_info *info)
{
	int result;
	struct xfpga_klass *obj = mgr->priv;
	if (obj->state != FPGA_MGR_STATE_WRITE) {
		obj->state = FPGA_MGR_STATE_WRITE_COMPLETE_ERR;
		return -EINVAL;
	}

	/* Check if we got the complete xclbin */
	if (obj->blob->m_header.m_length != obj->count) {
		obj->state = FPGA_MGR_STATE_WRITE_COMPLETE_ERR;
		return -EINVAL;
	}
	/* Send the xclbin blob to actual download framework in icap */
	result = xocl_icap_download_axlf(obj->xdev, obj->blob, false);
	obj->state = result ? FPGA_MGR_STATE_WRITE_COMPLETE_ERR : FPGA_MGR_STATE_WRITE_COMPLETE;
	xocl_info(&mgr->dev, "Finish download of xclbin %pUb of size %zu B", &obj->blob->m_header.uuid, obj->count);
	vfree(obj->blob);
	obj->blob = NULL;
	obj->count = 0;
	return result;
}

static enum fpga_mgr_states xocl_pr_state(struct fpga_manager *mgr)
{
	struct xfpga_klass *obj = mgr->priv;

	return obj->state;
}

static const struct fpga_manager_ops xocl_pr_ops = {
	.initial_header_size = sizeof(struct axlf),
	.write_init = xocl_pr_write_init,
	.write = xocl_pr_write,
	.write_complete = xocl_pr_write_complete,
	.state = xocl_pr_state,
};
#endif

struct platform_device_id fmgr_id_table[] = {
	{ XOCL_DEVNAME(XOCL_FMGR), 0 },
	{ },
};

static int fmgr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct xfpga_klass *obj = kzalloc(sizeof(struct xfpga_klass), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	obj->xdev = xocl_get_xdev(pdev);
	snprintf(obj->name, sizeof(obj->name), "Xilinx PCIe FPGA Manager");

	/* TODO: Remove old fpga_mgr_register call as soon as Linux < 4.18 is no
	 * longer supported.
	 */
#if defined(FPGA_MGR_SUPPORT)
	obj->state = FPGA_MGR_STATE_UNKNOWN;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	{
	struct fpga_manager *mgr = fpga_mgr_create(&pdev->dev,
						   obj->name,
						   &xocl_pr_ops,
						   obj);
	if (!mgr)
		return -ENOMEM;

	/* Historically this was internally called by fpga_mgr_register (in the form
	 * of drv_set_drvdata) but is expected to be called here since Linux 4.18.
	 */
	platform_set_drvdata(pdev, mgr);

	ret = fpga_mgr_register(mgr);
	if (ret)
		fpga_mgr_free(mgr);
	}
#else
	ret = fpga_mgr_register(&pdev->dev, obj->name, &xocl_pr_ops, obj);
#endif
#else
	platform_set_drvdata(pdev, obj);
#endif

	return ret;
}

static int __fmgr_remove(struct platform_device *pdev)
{
#if defined(FPGA_MGR_SUPPORT)
	struct fpga_manager *mgr = platform_get_drvdata(pdev);
	struct xfpga_klass *obj = mgr->priv;

	obj->state = FPGA_MGR_STATE_UNKNOWN;
	/* TODO: Remove old fpga_mgr_unregister as soon as Linux < 4.18 is no
	 * longer supported.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
	fpga_mgr_unregister(mgr);
#else
	fpga_mgr_unregister(&pdev->dev);
#endif // LINUX_VERSION_CODE
#else
	struct xfpga_klass *obj = platform_get_drvdata(pdev);
#endif

	platform_set_drvdata(pdev, NULL);
	vfree(obj->blob);
	kfree(obj);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void fmgr_remove(struct platform_device *pdev)
{
	__fmgr_remove(pdev);
}
#else
#define fmgr_remove __fmgr_remove
#endif

static struct platform_driver	fmgr_driver = {
	.probe		= fmgr_probe,
	.remove		= fmgr_remove,
	.driver		= {
		.name = "xocl_fmgr",
	},
	.id_table = fmgr_id_table,
};

int __init xocl_init_fmgr(void)
{
	return platform_driver_register(&fmgr_driver);
}

void xocl_fini_fmgr(void)
{
	platform_driver_unregister(&fmgr_driver);
}
