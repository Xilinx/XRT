/*
 * FPGA Manager bindings for XRT driver
 *
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Sonal Santan
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
 * FPGA Mgr integration is support limited to Ubuntu for now. RHEL/CentOS kernels
 * do not support FPGA Mgr
 */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
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
 * 1. Refactor icap_download_bitstream_axlf() to read in the full xclbin into kernel
 *    memory instead of copying in section by section
 * 2. Call icap_download_bitstream_axlf() from FPGA Manager's write complete hook,
 *    xocl_pr_write_complete() when we have the full binary
 */

struct xfpga_klass {
	struct xocl_dev *xdev;
	struct axlf *blob;
	size_t count;
};

#if defined(FPGA_MGR_SUPPORT)
static int xocl_pr_write_init(struct fpga_manager *mgr,
			      struct fpga_image_info *info, const char *buf, size_t count)
{
	struct xfpga_klass *obj = mgr->priv;
	const struct axlf *bin = (const struct axlf *)buf;
	if (count < sizeof(struct axlf))
	    return -EINVAL;

	vfree(obj->blob);
	obj->blob = vmalloc(bin->m_header.m_length);
	if (!obj->blob)
	    return -ENOMEM;

	memcpy(obj->blob, buf, count);
	xocl_info(&mgr->dev, "Begin download of xclbin %pUb of length %lld B", &obj->blob->m_header.uuid,
		  obj->blob->m_header.m_length);
	obj->count = count;
	return 0;
}

static int xocl_pr_write(struct fpga_manager *mgr,
			 const char *buf, size_t count)
{
	struct xfpga_klass *obj = mgr->priv;
	char *curr = (char *)obj->blob;
	curr += obj->count;
	memcpy(curr, buf, count);
	obj->count += count;
	xocl_info(&mgr->dev, "Next block of %zu B of xclbin %pUb", count, &obj->blob->m_header.uuid);
	return 0;
}


static int xocl_pr_write_complete(struct fpga_manager *mgr,
				  struct fpga_image_info *info)
{
	struct xfpga_klass *obj = mgr->priv;
	xocl_info(&mgr->dev, "Finish download of xclbin %pUb of size %zu B", &obj->blob->m_header.uuid, obj->count);
	/* Send the xclbin blob to actual download framework in icap */
	vfree(obj->blob);
	obj->blob = NULL;
	obj->count = 0;
	return 0;
}

static enum fpga_mgr_states xocl_pr_state(struct fpga_manager *mgr)
{
	return FPGA_MGR_STATE_UNKNOWN;
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
	{ XOCL_FMGR, 0 },
	{ },
};

static int fmgr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct xfpga_klass *obj = kzalloc(sizeof(struct xfpga_klass), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, obj);
	obj->xdev = xocl_get_xdev(pdev);
#if defined(FPGA_MGR_SUPPORT)
	ret = fpga_mgr_register(&pdev->dev,
				"Xilinx PCIe FPGA Manager", &xocl_pr_ops, obj);
#endif
	return ret;
}

static int fmgr_remove(struct platform_device *pdev)
{
	struct xfpga_klass *obj = dev_get_drvdata(&pdev->dev);
	vfree(obj->blob);
	kfree(obj);
#if defined(FPGA_MGR_SUPPORT)
	fpga_mgr_unregister(&pdev->dev);
#endif
	return 0;
}

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
