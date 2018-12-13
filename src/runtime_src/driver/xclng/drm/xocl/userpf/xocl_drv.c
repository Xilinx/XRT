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

void xocl_reset_notify(struct pci_dev *pdev, bool prepare)
{
        struct xocl_dev *xdev = pci_get_drvdata(pdev);

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

static void kill_all_clients(struct xocl_dev *xdev)
{
	struct list_head *ptr;
	struct client_ctx *entry;
	int ret;
	int total_wait_secs = 10; // sec
	int wait_interval = 100; // millisec
	int retry = total_wait_secs * 1000 / wait_interval;

	mutex_lock(&xdev->ctx_list_lock);

	list_for_each(ptr, &xdev->ctx_list) {
		entry = list_entry(ptr, struct client_ctx, link);
		ret = kill_pid(entry->pid, SIGBUS, 1);
		if (ret) {
			userpf_err(xdev, "killing pid: %d failed. err: %d",
				pid_nr(entry->pid), ret);
		}
	}

	mutex_unlock(&xdev->ctx_list_lock);

	while (!list_empty(&xdev->ctx_list) && retry--)
		msleep(wait_interval);

	if (!list_empty(&xdev->ctx_list))
		userpf_err(xdev, "failed to kill all clients");
}

int xocl_hot_reset(struct xocl_dev *xdev, bool force)
{
	bool skip = false;
	int ret = 0;
	struct mailbox_req mbreq = { MAILBOX_REQ_HOT_RESET, };
	size_t resplen = sizeof (ret);
	int mbret;

	mutex_lock(&xdev->ctx_list_lock);
	if (xdev->offline) {
		skip = true;
	} else if (!force && !list_is_singular(&xdev->ctx_list)) {
		/* We should have one context for ourselves. */
		BUG_ON(list_empty(&xdev->ctx_list));
		userpf_err(xdev, "device is in use, can't reset");
		ret = -EBUSY;
	} else {
		xdev->offline = true;
	}
	mutex_unlock(&xdev->ctx_list_lock);
	if (ret < 0 || skip)
		return ret;

	userpf_info(xdev, "resetting device...");

	if (force)
		kill_all_clients(xdev);

	xocl_reset_notify(xdev->core.pdev, true);
	mbret = xocl_peer_request(xdev, &mbreq, &ret, &resplen, NULL, NULL);
	if (mbret)
		ret = mbret;
	xocl_reset_notify(xdev->core.pdev, false);

	mutex_lock(&xdev->ctx_list_lock);
	xdev->offline = false;
	mutex_unlock(&xdev->ctx_list_lock);

	return ret;
}

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
