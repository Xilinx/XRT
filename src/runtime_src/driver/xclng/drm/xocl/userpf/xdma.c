/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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
#include "../xocl_drv.h"
#include "../lib/libxdma_api.h"
#include "common.h"
#include "xocl_drm.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
#include <linux/memremap.h>
#endif

struct xocl_xdma_dev {
	struct xocl_dev		ocl_dev;
};

static const struct pci_device_id pciidlist[] = {
	XOCL_USER_XDMA_PCI_IDS,
	{ 0, }
};

static int user_intr_config(xdev_handle_t xdev_hdl, u32 intr, bool en)
{
	struct xocl_dev *xdev = (struct xocl_dev *)xdev_hdl;
	const unsigned int mask = 1 << intr;

        return en ? xdma_user_isr_enable(xdev->dma_handle, mask) :
		xdma_user_isr_disable(xdev->dma_handle, mask);
}

static int user_intr_register(xdev_handle_t xdev_hdl, u32 intr,
	irq_handler_t handler, void *arg)
{
	struct xocl_dev *xdev = (struct xocl_dev *)xdev_hdl;
	const unsigned int mask = 1 << intr;
	int	ret;

	ret = xdma_user_isr_register(xdev->dma_handle, mask, handler, arg);
	return (ret);
}

static int user_dev_online(xdev_handle_t xdev_hdl)
{
	struct xocl_dev *xdev = (struct xocl_dev *)xdev_hdl;

	if (xdev->offline) {
		xdma_device_online(xdev->core.pdev, xdev->dma_handle);
		xdev->offline = false;
	}
	xocl_info(&xdev->core.pdev->dev, "Device online");

	return 0;
}

static int user_dev_offline(xdev_handle_t xdev_hdl)
{
	struct xocl_dev *xdev = (struct xocl_dev *)xdev_hdl;

	if (!xdev->offline) {
		xdma_device_offline(xdev->core.pdev, xdev->dma_handle);
		xdev->offline = true;
	}
	xocl_info(&xdev->core.pdev->dev, "Device offline");
	return 0;
}

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

static int xocl_p2p_mem_reserve(struct pci_dev *pdev, xdev_handle_t xdev_hdl)
{
	resource_size_t p2p_bar_addr;
	resource_size_t p2p_bar_len;
	struct resource res;
	uint32_t p2p_bar_idx;
	struct xocl_dev *xdev = (struct xocl_dev *)xdev_hdl;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
	int32_t ret;
#endif

	p2p_bar_len = xdev->bypass_bar_len;
	p2p_bar_idx = xdev->bypass_bar_idx;

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
		return ret;

	ret = devm_add_action_or_reset(&(pdev->dev), xocl_dev_percpu_exit,
							&xdev->ref);
	if (ret)
		return ret;
#endif


/* Ubuntu 16.04 kernel_ver 4.4.0.116*/
#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	xdev->bypass_bar_addr= devm_memremap_pages(&(pdev->dev), &res);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
	xdev->bypass_bar_addr= devm_memremap_pages(&(pdev->dev), &res
						   , &xdev->ref, NULL);

	ret = devm_add_action_or_reset(&(pdev->dev), xocl_dev_percpu_kill,
							&xdev->ref);
	if (ret)
		return ret;
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) || RHEL_P2P_SUPPORT
	if(!xdev->bypass_bar_addr)
		return -ENOMEM;;
#endif

	return 0;
}

struct xocl_pci_funcs xdma_pci_ops = {
	.intr_config = user_intr_config,
	.intr_register = user_intr_register,
	.dev_online = user_dev_online,
	.dev_offline = user_dev_offline,
};

int xocl_user_xdma_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	struct xocl_xdma_dev		*xd;
	struct xocl_dev			*ocl_dev;
	struct xocl_board_private	*dev_info;
	u32 channel = 0;
	int ret;

	xd = devm_kzalloc(&pdev->dev, sizeof (*xd), GFP_KERNEL);
	if (!xd) {
		xocl_err(&pdev->dev, "failed to alloc xocl_dev");
		return -ENOMEM;
	}
	/* this is used for all subdevs, bind it to device earlier */
	pci_set_drvdata(pdev, xd);
	dev_info = (struct xocl_board_private *)ent->driver_data;

	ocl_dev = (struct xocl_dev *)xd;

	ocl_dev->core.pdev = pdev;
	xocl_fill_dsa_priv(ocl_dev, dev_info);

	ocl_dev->dma_handle = xdma_device_open(XOCL_XDMA_PCI, pdev,
		&ocl_dev->max_user_intr, &channel,
		&channel);
	if (ocl_dev->dma_handle == NULL) {
		xocl_err(&pdev->dev, "XDMA Device Open failed");
		ret = -ENOENT;
		goto failed;
	}

	xocl_info(&pdev->dev, "XDMA open succeed: intr: %d channel %d",
		ocl_dev->max_user_intr, channel);

	ocl_dev->start_user_intr = 0;
	ocl_dev->user_msix_table = devm_kzalloc(&pdev->dev,
		sizeof (struct eventfd_ctx *) * ocl_dev->max_user_intr,
		GFP_KERNEL);
	if (!ocl_dev->user_msix_table) {
		xocl_err(&pdev->dev, "Failed to alloc mem for user intr table");
		ret = -ENOMEM;
		goto failed;
	}

	ret = xdma_get_userio(ocl_dev->dma_handle, &ocl_dev->base_addr,
		&ocl_dev->bar_len, &ocl_dev->core.bar_idx);
	if (ret) {
		xocl_err(&pdev->dev, "Get user bar info failed");
		goto failed;
	}

	ret = xdma_get_bypassio(ocl_dev->dma_handle,
		&ocl_dev->bypass_bar_len, &ocl_dev->bypass_bar_idx);
	if (ret) {
		xocl_err(&pdev->dev, "Get bypass bar info failed");
		goto failed;
	}

	ocl_dev->core.pci_ops = &xdma_pci_ops;

	ret = xocl_subdev_create_all(ocl_dev, dev_info->subdev_info,
		dev_info->subdev_num);
	if (ret) {
		xocl_err(&pdev->dev, "failed to register subdevs");
		goto failed_reg_subdevs;
	}

	ret = xocl_set_max_channel(ocl_dev, channel);
	if (ret)
		goto failed_set_channel;

	ret = xocl_drm_init(ocl_dev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init drm mm");
		goto failed_drm_init;
	}

	if(ocl_dev->bypass_bar_idx>=0){
		/* only bypass_bar_len >= SECTION (256MB) */
		if (ocl_dev->bypass_bar_len >= (1<<PA_SECTION_SHIFT)){
			xocl_info(&pdev->dev, "Found bypass BAR");
			ret = xocl_p2p_mem_reserve(pdev, ocl_dev);
			if (ret) {
				xocl_err(&pdev->dev, "failed to reserve p2p memory region");
				goto failed_drm_init;
			}
	  }
  }

	ret = xocl_init_sysfs(&pdev->dev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init sysfs");
		goto failed_sysfs_init;
	}

	mutex_init(&ocl_dev->user_msix_table_lock);

	(void) xocl_icap_unlock_bitstream(xd, NULL, 0);

	return 0;

failed_sysfs_init:
	xocl_drm_fini(&xd->ocl_dev);
failed_drm_init:
failed_set_channel:
	xocl_subdev_destroy_all(ocl_dev);
failed_reg_subdevs:
	xdma_device_close(pdev, ocl_dev->dma_handle);
failed:
	if (ocl_dev->user_msix_table)
		devm_kfree(&pdev->dev, ocl_dev->user_msix_table);
	devm_kfree(&pdev->dev, xd);
	pci_set_drvdata(pdev, NULL);
	return ret;
}

void xocl_user_xdma_remove(struct pci_dev *pdev)
{
	struct xocl_xdma_dev	*xd;

	xd = pci_get_drvdata(pdev);
	if (!xd) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return;
	}

	xocl_subdev_destroy_all(&xd->ocl_dev);

	xocl_fini_sysfs(&pdev->dev);
	xocl_drm_fini(&xd->ocl_dev);
	xdma_device_close(pdev, xd->ocl_dev.dma_handle);
	if (xd->ocl_dev.user_msix_table)
		devm_kfree(&pdev->dev, xd->ocl_dev.user_msix_table);
	mutex_destroy(&xd->ocl_dev.user_msix_table_lock);

	devm_kfree(&pdev->dev, xd);
	pci_set_drvdata(pdev, NULL);
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
	.reset_prepare  = user_pci_reset_prepare,
	.reset_done     = user_pci_reset_done,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	.reset_notify	= xocl_reset_notify,
#endif
};

static struct pci_driver user_xdma_driver = {
	.name = XOCL_XDMA_PCI,
	.id_table = pciidlist,
	.probe = xocl_user_xdma_probe,
	.remove = xocl_user_xdma_remove,
	.err_handler = &xocl_err_handler,
};

int __init xocl_init_drv_user_xdma(void)
{
	int ret;

	ret = pci_register_driver(&user_xdma_driver);
	if (ret) {
		goto failed;
	}

	return 0;

failed:
	return ret;
}

void xocl_fini_drv_user_xdma(void)
{
	pci_unregister_driver(&user_xdma_driver);
}
