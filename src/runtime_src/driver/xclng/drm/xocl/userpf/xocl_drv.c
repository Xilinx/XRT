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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
#include <linux/memremap.h>
#endif

#ifndef PCI_EXT_CAP_ID_REBAR
#define PCI_EXT_CAP_ID_REBAR 0x15
#endif

#ifndef PCI_REBAR_CTRL
#define PCI_REBAR_CTRL          8       /* control register */
#endif

#ifndef PCI_REBAR_CTRL_BAR_SIZE
#define  PCI_REBAR_CTRL_BAR_SIZE        0x00001F00  /* BAR size */
#endif

#ifndef PCI_REBAR_CTRL_BAR_SHIFT
#define  PCI_REBAR_CTRL_BAR_SHIFT       8           /* shift for BAR size */
#endif

#define REBAR_FIRST_CAP		4

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
static void xocl_dev_percpu_release(struct percpu_ref *ref)
{
        struct xocl_dev *xdev = container_of(ref, struct xocl_dev, ref);

        complete(&xdev->cmp);
}

static void xocl_dev_percpu_exit(void *data)
{
        struct percpu_ref *ref = data;
        struct xocl_dev *xdev = container_of(ref, struct xocl_dev, ref);

        wait_for_completion(&xdev->cmp);
        percpu_ref_exit(ref);
}


static void xocl_dev_percpu_kill(void *data)
{
        struct percpu_ref *ref = data;

        percpu_ref_kill(ref);
}

#endif

void xocl_p2p_mem_release(struct xocl_dev *xdev, bool recov_bar_sz)
{
	struct pci_dev *pdev = xdev->core.pdev;
	int p2p_bar = -1;

	if (xdev->bypass_bar_addr) {
		devres_release_group(&pdev->dev, xdev->p2p_res_grp);
		xdev->bypass_bar_addr = NULL;
		xdev->p2p_res_grp = NULL;
	}
	if (xdev->p2p_res_grp) {
		devres_remove_group(&pdev->dev, xdev->p2p_res_grp);
		xdev->p2p_res_grp = NULL;
	}

	if (recov_bar_sz) {
		p2p_bar = xocl_get_p2p_bar(xdev, NULL);
	        if (p2p_bar < 0) {
			return;
		}

		xocl_pci_resize_resource(pdev, p2p_bar,
				(XOCL_PA_SECTION_SHIFT - 20));

		xocl_info(&pdev->dev, "Resize p2p bar %d to %d M ", p2p_bar,
			(1 << XOCL_PA_SECTION_SHIFT));
	}
}

int xocl_p2p_mem_reserve(struct xocl_dev *xdev)
{
        resource_size_t p2p_bar_addr;
	resource_size_t p2p_bar_len;
	struct resource res;
	uint32_t p2p_bar_idx;
	struct pci_dev *pdev = xdev->core.pdev;
	int32_t ret;

	xocl_info(&pdev->dev, "reserve p2p mem, bar %d, len %lld",
			xdev->bypass_bar_idx, xdev->bypass_bar_len);

	if(xdev->bypass_bar_idx < 0 ||
		xdev->bypass_bar_len <= (1<<XOCL_PA_SECTION_SHIFT)) {
		/* only bypass_bar_len > SECTION (256MB) */
		xocl_info(&pdev->dev, "Did not find bypass BAR");
		return 0;
	}

	p2p_bar_len = xdev->bypass_bar_len;
	p2p_bar_idx = xdev->bypass_bar_idx;

	xdev->p2p_res_grp = devres_open_group(&pdev->dev, NULL, GFP_KERNEL);
	if (!xdev->p2p_res_grp) {
		xocl_err(&pdev->dev, "open p2p resource group failed");
		ret = -ENOMEM;
		goto failed;
	}

	p2p_bar_addr = pci_resource_start(pdev, p2p_bar_idx);

	res.start = p2p_bar_addr;
	res.end   = p2p_bar_addr+p2p_bar_len-1;
	res.name  = NULL;
	res.flags = IORESOURCE_MEM;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT

	init_completion(&xdev->cmp);

	ret = percpu_ref_init(&xdev->ref, xocl_dev_percpu_release, 0,
		GFP_KERNEL);
	if (ret)
		goto failed;

	ret = devm_add_action_or_reset(&(pdev->dev), xocl_dev_percpu_exit,
		&xdev->ref);
	if (ret)
		goto failed;
#endif

/* Ubuntu 16.04 kernel_ver 4.4.0.116*/
#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	xdev->bypass_bar_addr= devm_memremap_pages(&(pdev->dev), &res);

#elif KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) || RHEL_P2P_SUPPORT_74
	xdev->bypass_bar_addr = devm_memremap_pages(&(pdev->dev), &res
						   , &xdev->ref, NULL);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0) || RHEL_P2P_SUPPORT_76
	xdev->bypass_bar_addr = devm_memremap_pages(&(pdev->dev), &xdev->pgmap);

#endif 

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || RHEL_P2P_SUPPORT
	if(!xdev->bypass_bar_addr) {
		ret = -ENOMEM;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
		percpu_ref_kill(&xdev->ref);
#endif
		devres_close_group(&pdev->dev, xdev->p2p_res_grp);
		goto failed;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
	ret = devm_add_action_or_reset(&(pdev->dev), xocl_dev_percpu_kill,
		&xdev->ref);
	if (ret) {
		percpu_ref_kill(&xdev->ref);
		devres_close_group(&pdev->dev, xdev->p2p_res_grp);
		goto failed;
	}
#endif
	devres_close_group(&pdev->dev, xdev->p2p_res_grp);

	return 0;

failed:
	xocl_p2p_mem_release(xdev, false);

	return ret;
}

static inline u64 xocl_pci_rebar_size_to_bytes(int size)
{
	        return 1ULL << (size + 20);
}

int xocl_get_p2p_bar(struct xocl_dev *xdev, u64 *bar_size)
{
	struct pci_dev *dev = xdev->core.pdev;
	int i, pos;
	u32 cap, ctrl, size;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		xocl_err(&dev->dev, "did not find rebar cap");
		return -ENOTSUPP;
	}

	pos += REBAR_FIRST_CAP;
	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		pci_read_config_dword(dev, pos, &cap);
		pci_read_config_dword(dev, pos + 4, &ctrl);
		size = (ctrl & PCI_REBAR_CTRL_BAR_SIZE) >>
			PCI_REBAR_CTRL_BAR_SHIFT;
		if (xocl_pci_rebar_size_to_bytes(size) >=
			(1 << XOCL_PA_SECTION_SHIFT) &&
			cap >= 0x1000) {
			if (bar_size)
				*bar_size = xocl_pci_rebar_size_to_bytes(size);
			return i;
		}
		pos += 8;
	}

	if (bar_size)
		*bar_size = 0;

	return -1;
}

static int xocl_reassign_resources(struct pci_dev *dev, int resno)
{
	pci_assign_unassigned_bus_resources(dev->bus);

	return 0;
}

int xocl_pci_resize_resource(struct pci_dev *dev, int resno, int size)
{
	struct resource *res = dev->resource + resno;
	struct pci_dev *root;
	struct resource *root_res;
	u64 bar_size, req_size;
	unsigned long flags;
	u16 cmd;
	int pos, ret = 0;
	u32 ctrl, i;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		xocl_err(&dev->dev, "did not find rebar cap");
		return -ENOTSUPP;
	}

	pos += resno * PCI_REBAR_CTRL;
	pci_read_config_dword(dev, pos + PCI_REBAR_CTRL, &ctrl);

	bar_size = xocl_pci_rebar_size_to_bytes(
			(ctrl & PCI_REBAR_CTRL_BAR_SIZE) >>
			PCI_REBAR_CTRL_BAR_SHIFT);
	req_size = xocl_pci_rebar_size_to_bytes(size);

	xocl_info(&dev->dev, "req_size %lld, bar size %lld\n",
			req_size, bar_size);
	if (req_size == bar_size) {
		xocl_info(&dev->dev, "same size, return success");
		return -EALREADY;
	}

	xocl_get_root_dev(dev, root);

	for (i = 0; i < PCI_BRIDGE_RESOURCE_NUM; i++) {
		root_res = root->subordinate->resource[i];
		root_res = (root_res) ? root_res->parent : NULL;
		if (root_res && (root_res->flags & IORESOURCE_MEM)
			&& resource_size(root_res) >= req_size)
			break;
	}

	if (i == PCI_BRIDGE_RESOURCE_NUM) {
		xocl_err(&dev->dev, "Not enough IO Mem space, "
			"Please check BIOS settings. ");
		return -ENOSPC;
	}
	pci_release_selected_regions(dev, (1 << resno));
	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	pci_write_config_word(dev, PCI_COMMAND,
		cmd & ~PCI_COMMAND_MEMORY);

	flags = res->flags;
	if (res->parent)
		release_resource(res);

	ctrl &= ~PCI_REBAR_CTRL_BAR_SIZE;
	ctrl |= size << PCI_REBAR_CTRL_BAR_SHIFT;
	pci_write_config_dword(dev, pos + PCI_REBAR_CTRL, ctrl);


	res->start = 0;
	res->end = req_size - 1;

	xocl_info(&dev->dev, "new size %lld", resource_size(res));
	xocl_reassign_resources(dev, resno);
	res->flags = flags;

	pci_write_config_word(dev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);
	pci_request_selected_regions(dev, (1 << resno),
		XOCL_MODULE_NAME);

	return ret;
}

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
