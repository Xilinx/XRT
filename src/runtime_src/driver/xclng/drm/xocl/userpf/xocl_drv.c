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

#include <linux/pci.h>
#include <linux/aer.h>
#include <linux/version.h>
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

static int userpf_intr_config(xdev_handle_t xdev_hdl, u32 intr, bool en)
{
	return xocl_dma_intr_config(xdev_hdl, intr, en);
}

static int userpf_intr_register(xdev_handle_t xdev_hdl, u32 intr,
		irq_handler_t handler, void *arg)
{
	return handler ?
		xocl_dma_intr_register(xdev_hdl, intr, handler, arg, -1) :
		xocl_dma_intr_unreg(xdev_hdl, intr);
}

struct xocl_pci_funcs userpf_pci_ops = {
	.intr_config = userpf_intr_config,
	.intr_register = userpf_intr_register,
};

void xocl_reset_notify(struct pci_dev *pdev, bool prepare)
{
        struct xocl_dev *xdev = pci_get_drvdata(pdev);

        xocl_info(&pdev->dev, "PCI reset NOTIFY, prepare %d", prepare);

        if (prepare) {
		xocl_mailbox_reset(xdev, false);
		xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_DMA);
        } else {
		reset_notify_client_ctx(xdev);
		xocl_subdev_create_by_id(xdev, XOCL_SUBDEV_DMA);
		xocl_mailbox_reset(xdev, true);
		xocl_exec_reset(xdev);
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

int64_t xocl_hot_reset(struct xocl_dev *xdev, bool force)
{
	bool skip = false;
	int64_t ret = 0, mbret = 0;
	struct mailbox_req mbreq = { MAILBOX_REQ_HOT_RESET, };
	size_t resplen = sizeof (ret);

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
	mbret = xocl_peer_request(xdev, &mbreq, sizeof(struct mailbox_req), &ret, &resplen, NULL, NULL);
	if (mbret)
		ret = mbret;
	xocl_reset_notify(xdev->core.pdev, false);

	mutex_lock(&xdev->ctx_list_lock);
	xdev->offline = false;
	mutex_unlock(&xdev->ctx_list_lock);

	return ret;
}


int xocl_reclock(struct xocl_dev *xdev, void *data)
{
	int err = 0;
	int64_t msg = -ENODEV;
	struct mailbox_req *req = NULL;
	size_t resplen = sizeof (msg);
	size_t reqlen = sizeof(struct mailbox_req)+sizeof(struct drm_xocl_reclock_info);
	req = (struct mailbox_req *)kzalloc(reqlen, GFP_KERNEL);
	req->req = MAILBOX_REQ_RECLOCK;
	req->data_total_len = sizeof(struct drm_xocl_reclock_info);
	memcpy(req->data, data, sizeof(struct drm_xocl_reclock_info));

	err = xocl_peer_request(xdev, req, reqlen,
		&msg, &resplen, NULL, NULL);

	if(msg != 0){
		err = -ENODEV;
	}

	kfree(req);
  return err;
}

static void xocl_mailbox_srv(void *arg, void *data, size_t len,
	u64 msgid, int err)
{
	struct xocl_dev *xdev = (struct xocl_dev *)arg;
	struct mailbox_req *req = (struct mailbox_req *)data;

	if (err != 0)
		return;

	userpf_info(xdev, "received request (%d) from peer\n", req->req);

	switch (req->req) {
	case MAILBOX_REQ_FIREWALL:
		(void) xocl_hot_reset(xdev, true);
		break;
	default:
		userpf_err(xdev, "dropped bad request (%d)\n", req->req);
		break;
	}
}

void get_pcie_link_info(struct xocl_dev *xdev,
	unsigned short *link_width, unsigned short *link_speed, bool is_cap)
{
	u16 stat;
	long result;
	int pos = is_cap ? PCI_EXP_LNKCAP : PCI_EXP_LNKSTA;

	result = pcie_capability_read_word(xdev->core.pdev, pos, &stat);
	if (result) {
		*link_width = *link_speed = 0;
		xocl_info(&xdev->core.pdev->dev, "Read pcie capability failed");
		return;
	}
	*link_width = (stat & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
	*link_speed = stat & PCI_EXP_LNKSTA_CLS;
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

	if (xdev->p2p_bar_addr) {
		devres_release_group(&pdev->dev, xdev->p2p_res_grp);
		xdev->p2p_bar_addr = NULL;
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
			xdev->p2p_bar_idx, xdev->p2p_bar_len);

	if(xdev->p2p_bar_idx < 0 ||
		xdev->p2p_bar_len <= (1<<XOCL_PA_SECTION_SHIFT)) {
		/* only p2p_bar_len > SECTION (256MB) */
		xocl_info(&pdev->dev, "Did not find p2p BAR");
		return 0;
	}

	p2p_bar_len = xdev->p2p_bar_len;
	p2p_bar_idx = xdev->p2p_bar_idx;

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0) || RHEL_P2P_SUPPORT_76
	xdev->pgmap.ref = &xdev->ref;
	memcpy(&xdev->pgmap.res, &res, sizeof(struct resource));
	xdev->pgmap.altmap_valid = false;
#endif

/* Ubuntu 16.04 kernel_ver 4.4.0.116*/
#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	xdev->p2p_bar_addr= devm_memremap_pages(&(pdev->dev), &res);

#elif KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) || RHEL_P2P_SUPPORT_74
	xdev->p2p_bar_addr = devm_memremap_pages(&(pdev->dev), &res
						   , &xdev->ref, NULL);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0) || RHEL_P2P_SUPPORT_76
	xdev->p2p_bar_addr = devm_memremap_pages(&(pdev->dev), &xdev->pgmap);

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || RHEL_P2P_SUPPORT
	if(!xdev->p2p_bar_addr) {
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
		xocl_info(&dev->dev, "rebar cap does not exist");
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
		xocl_info(&dev->dev, "rebar cap does not exist");
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
			&& resource_size(root_res) > req_size)
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

static int identify_bar(struct xocl_dev *xdev)
{
	struct pci_dev *pdev = xdev->core.pdev;
	resource_size_t bar_len;
	int		i;

	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		bar_len = pci_resource_len(pdev, i);
		if (bar_len >= (1 << XOCL_PA_SECTION_SHIFT)) {
			xdev->p2p_bar_idx = i;
			xdev->p2p_bar_len = bar_len;
			pci_request_selected_regions(pdev, 1 << i,
				XOCL_MODULE_NAME);
		} else if (bar_len >= 32 * 1024 * 1024) {
			xdev->core.bar_addr = ioremap_nocache(
				pci_resource_start(pdev, i), bar_len);
			if (!xdev->core.bar_addr)
				return -EIO;
			xdev->core.bar_idx = i;
			xdev->core.bar_size = bar_len;
		}
	}

	return 0;
}

static void unmap_bar(struct xocl_dev *xdev)
{
	if (xdev->core.bar_addr) {
		iounmap(xdev->core.bar_addr);
		xdev->core.bar_addr = NULL;
	}

	if (xdev->p2p_bar_len)
		pci_release_selected_regions(xdev->core.pdev,
				1 << xdev->p2p_bar_idx);
}

/* pci driver callbacks */
int xocl_userpf_probe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	struct xocl_dev			*xdev;
	struct xocl_board_private	*dev_info;
	int				ret;

	xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
	if (!xdev) {
		xocl_err(&pdev->dev, "failed to alloc xocl_dev");
		return -ENOMEM;
	}

	/* this is used for all subdevs, bind it to device earlier */
	pci_set_drvdata(pdev, xdev);
	dev_info = (struct xocl_board_private *)ent->driver_data;

	xdev->core.pci_ops = &userpf_pci_ops;
	xdev->core.pdev = pdev;
	xocl_fill_dsa_priv(xdev, dev_info);

	ret = identify_bar(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to identify bar");
		goto failed_to_bar;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to enable device.");
		goto failed_to_enable;
	}

	ret = xocl_alloc_dev_minor(xdev);
	if (ret)
		goto failed_alloc_minor;

	ret = xocl_subdev_create_all(xdev, dev_info->subdev_info,
			dev_info->subdev_num);
	if (ret) {
		xocl_err(&pdev->dev, "failed to register subdevs");
		goto failed_create_subdev;
	}

	ret = xocl_p2p_mem_reserve(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to reserve p2p memory region");
	}

	ret = xocl_init_sysfs(&pdev->dev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init sysfs");
		goto failed_init_sysfs;
	}

	mutex_init(&xdev->ctx_list_lock);
	xdev->needs_reset=false;
	atomic64_set(&xdev->total_execs, 0);
	atomic_set(&xdev->outstanding_execs, 0);
	INIT_LIST_HEAD(&xdev->ctx_list);

        /* Launch the mailbox server. */
        (void) xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);

	return 0;

failed_init_sysfs:
	xocl_p2p_mem_release(xdev, false);
	xocl_subdev_destroy_all(xdev);

failed_create_subdev:
	xocl_free_dev_minor(xdev);

failed_alloc_minor:
	pci_disable_device(pdev);
failed_to_enable:
	unmap_bar(xdev);
failed_to_bar:
	devm_kfree(&pdev->dev, xdev);
	pci_set_drvdata(pdev, NULL);

	return ret;
}

void xocl_userpf_remove(struct pci_dev *pdev)
{
	struct xocl_dev		*xdev;

	xdev = pci_get_drvdata(pdev);
	if (!xdev) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return;
	}

	xocl_p2p_mem_release(xdev, false);
	xocl_subdev_destroy_all(xdev);

	xocl_fini_sysfs(&pdev->dev);
	xocl_free_dev_minor(xdev);

	pci_disable_device(pdev);

	unmap_bar(xdev);

	mutex_destroy(&xdev->ctx_list_lock);

	pci_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, xdev);
}

static pci_ers_result_t user_pci_error_detected(struct pci_dev *pdev,
		pci_channel_state_t state)
{
	switch (state) {
	case pci_channel_io_normal:
		xocl_info(&pdev->dev, "PCI normal state error\n");
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		xocl_info(&pdev->dev, "PCI frozen state error\n");
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		xocl_info(&pdev->dev, "PCI failure state error\n");
		return PCI_ERS_RESULT_DISCONNECT;
	default:
		xocl_info(&pdev->dev, "PCI unknown state (%d) error\n", state);
		break;
	}

	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t user_pci_slot_reset(struct pci_dev *pdev)
{
	xocl_info(&pdev->dev, "PCI reset slot");
	pci_restore_state(pdev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void user_pci_error_resume(struct pci_dev *pdev)
{
	xocl_info(&pdev->dev, "PCI error resume");
	pci_cleanup_aer_uncorrect_error_status(pdev);
}

static const struct pci_error_handlers xocl_err_handler = {
	.error_detected	= user_pci_error_detected,
	.slot_reset	= user_pci_slot_reset,
	.resume		= user_pci_error_resume,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
	.reset_prepare	= user_pci_reset_prepare,
	.reset_done	= user_pci_reset_done,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	.reset_notify   = xocl_reset_notify,
#endif
};

static struct pci_driver userpf_driver = {
	.name = XOCL_MODULE_NAME,
	.id_table = pciidlist,
	.probe = xocl_userpf_probe,
	.remove = xocl_userpf_remove,
	.err_handler = &xocl_err_handler,
};

/* INIT */
static int (*xocl_drv_reg_funcs[])(void) __initdata = {
	xocl_init_feature_rom,
	xocl_init_xdma,
	xocl_init_qdma,
	xocl_init_mb_scheduler,
	xocl_init_mailbox,
	xocl_init_xmc,
	xocl_init_icap,
	xocl_init_xvc,
};

static void (*xocl_drv_unreg_funcs[])(void) = {
	xocl_fini_feature_rom,
	xocl_fini_xdma,
	xocl_fini_qdma,
	xocl_fini_mb_scheduler,
	xocl_fini_mailbox,
	xocl_fini_xmc,
	xocl_fini_icap,
	xocl_fini_xvc,
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

	ret = pci_register_driver(&userpf_driver);
	if (ret)
		goto failed;

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

	pci_unregister_driver(&userpf_driver);

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
