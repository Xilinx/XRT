/*
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@xilinx.com
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

#include <linux/module.h>
#include <linux/pci.h>
#include "../xocl_drv.h"
#include "common.h"
#include "version.h"

static const struct pci_device_id pciidlist[] = {
	XOCL_USER_XDMA_PCI_IDS,
	XOCL_USER_QDMA_PCI_IDS,
	{ 0, }
};

struct class *xrt_class = NULL;

MODULE_DEVICE_TABLE(pci, pciidlist);

#define	EBUF_LEN	256
void xocl_reset_notify(struct pci_dev *pdev, bool prepare)
{
        struct xocl_dev *xdev = pci_get_drvdata(pdev);

	if (!pdev->driver ||
		strcmp(pdev->driver->driver.mod_name, XOCL_MODULE_NAME)) {
		xocl_err(&pdev->dev, "XOCL driver is not bound");
		return;
	}
        xocl_info(&pdev->dev, "PCI reset NOTIFY, prepare %d", prepare);

        if (prepare) {
		xocl_mailbox_reset(xdev, false);
		xocl_user_dev_offline(xdev);
        } else {
		reset_notify_client_ctx(xdev);
		xocl_user_dev_online(xdev);
		xocl_mailbox_reset(xdev, true);
        }
}
EXPORT_SYMBOL_GPL(xocl_reset_notify);

/* hack for mgmt driver to request scheduler reset */
int xocl_reset_scheduler(struct pci_dev *pdev)
{
	return xocl_exec_reset(pci_get_drvdata(pdev));
}
EXPORT_SYMBOL_GPL(xocl_reset_scheduler);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
void user_pci_reset_prepare(struct pci_dev *pdev)
{
        xocl_reset_notify(pdev, true);
}

void user_pci_reset_done(struct pci_dev *pdev)
{
        xocl_reset_notify(pdev, false);
}
#endif

void xocl_dump_sgtable(struct device *dev, struct sg_table *sgt)
{
	int i;
	struct page *pg;
	struct scatterlist *sg = sgt->sgl;
	unsigned long long pgaddr;
	int nents = sgt->orig_nents;

        for (i = 0; i < nents; i++, sg = sg_next(sg)) {
	        if (!sg)
       		     break;
                pg = sg_page(sg);
                if (!pg)
                        continue;
                pgaddr = page_to_phys(pg);
                xocl_err(dev, "%i, 0x%llx, offset %d, len %d\n",
			i, pgaddr, sg->offset, sg->length);
        }
}

/* INIT */
static int (*xocl_drv_reg_funcs[])(void) __initdata = {
	xocl_init_feature_rom,
	xocl_init_mm_xdma,
	xocl_init_mm_qdma,
	xocl_init_str_qdma,
	xocl_init_mb_scheduler,
	xocl_init_mailbox,
	xocl_init_icap,
	xocl_init_xvc,
	xocl_init_drv_user_xdma,
	xocl_init_drv_user_qdma,
};

static void (*xocl_drv_unreg_funcs[])(void) = {
	xocl_fini_feature_rom,
	xocl_fini_mm_xdma,
	xocl_fini_mm_qdma,
	xocl_fini_str_qdma,
	xocl_fini_mb_scheduler,
	xocl_fini_mailbox,
	xocl_fini_icap,
	xocl_fini_xvc,
	xocl_fini_drv_user_xdma,
	xocl_fini_drv_user_qdma,
};

static int __init xocl_init(void)
{
	int		ret, i;

	xrt_class = class_create(THIS_MODULE, "xrt_user");
	if (IS_ERR(xrt_class)) {
		ret = PTR_ERR(xrt_class);
		goto err_class_create;
	}

	for (i = 0; i < ARRAY_SIZE(xocl_drv_reg_funcs); ++i) {
		ret = xocl_drv_reg_funcs[i]();
		if (ret) {
			goto failed;
		}
	}

	return 0;

failed:
	for (i--; i >= 0; i--) {
		xocl_drv_unreg_funcs[i]();
	}
	class_destroy(xrt_class);

err_class_create:
	return ret;
}

static void __exit xocl_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(xocl_drv_unreg_funcs) - 1; i >= 0; i--) {
		xocl_drv_unreg_funcs[i]();
	}

	class_destroy(xrt_class);
}

module_init(xocl_init);
module_exit(xocl_exit);

MODULE_VERSION(XRT_DRIVER_VERSION);

MODULE_DESCRIPTION(XOCL_DRIVER_DESC);
MODULE_AUTHOR("Lizhi Hou <lizhi.hou@xilinx.com>");
MODULE_LICENSE("GPL v2");
