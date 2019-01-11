/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@Xilinx.com
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
#include "../xocl_drv.h"
#include "../lib/libqdma/libqdma_export.h"
#include "../lib/libqdma/libqdma_config.h"
#include "common.h"
#include "xocl_drm.h"

#define	QDMA_MM_ENGINE_MAX		1 /* 2 with Everest */

struct xocl_qdma_dev {
	struct xocl_dev		ocl_dev;
	struct qdma_dev_conf	dev_conf;
};

static const struct pci_device_id pciidlist[] = {
	XOCL_USER_QDMA_PCI_IDS,
	{ 0, }
};

static int user_intr_config(xdev_handle_t xdev_hdl, u32 intr, bool en)
{
	return 0;
}

static int user_intr_register(xdev_handle_t xdev_hdl, u32 intr,
	irq_handler_t handler, void *arg)
{
	return 0;
}

static int user_dev_online(xdev_handle_t xdev_hdl)
{
	struct xocl_qdma_dev    *qd;
	struct xocl_dev			*ocl_dev;
	struct pci_dev *pdev;
	int ret;

	pdev = XDEV(xdev_hdl)->pdev;
        qd = pci_get_drvdata(pdev);
	ocl_dev = (struct xocl_dev *)qd;

	ret = qdma_device_open(XOCL_QDMA_PCI, &qd->dev_conf,
		(unsigned long *)(&qd->ocl_dev.dma_handle));
	if (ret < 0) {
		xocl_err(&pdev->dev, "QDMA Device Open failed");
	}

	if (MM_DMA_DEV(ocl_dev)) {
		/* use 2 channels (queue pairs) */
		ret = xocl_set_max_channel(ocl_dev, 2);
		if (ret)
			xocl_err(&pdev->dev, "Set channel failed");
	}


	return ret;
} 

static int user_dev_offline(xdev_handle_t xdev_hdl)
{
	struct xocl_qdma_dev    *qd;
	struct pci_dev *pdev;

	pdev = XDEV(xdev_hdl)->pdev;
        qd = pci_get_drvdata(pdev);

	qdma_device_close(pdev, (unsigned long)qd->ocl_dev.dma_handle);
	return 0;
} 

struct xocl_pci_funcs qdma_pci_ops = {
	.intr_config = user_intr_config,
	.intr_register = user_intr_register,
	.dev_online = user_dev_online,
	.dev_offline = user_dev_offline,
};

static int xocl_user_qdma_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	struct xocl_qdma_dev		*qd;
	struct xocl_dev			*ocl_dev;
	struct qdma_dev_conf		*conf;
	struct xocl_board_private	*dev_info;
	int ret = 0;

	qd = devm_kzalloc(&pdev->dev, sizeof (*qd), GFP_KERNEL);
	if (!qd) {
		xocl_err(&pdev->dev, "failed to alloc xocl_dev");
		return -ENOMEM;
	}
	/* this is used for all subdevs, bind it to device earlier */
	pci_set_drvdata(pdev, qd);

	dev_info = (struct xocl_board_private *)ent->driver_data;

	ocl_dev = (struct xocl_dev *)qd;

	ocl_dev->core.pdev = pdev;
	xocl_fill_dsa_priv(ocl_dev, dev_info);

	ret = xocl_alloc_dev_minor(ocl_dev);
	if (ret)
		goto failed;

	conf = &qd->dev_conf;
	memset(conf, 0, sizeof(*conf));
	conf->pdev = pdev;
	conf->intr_rngsz = QDMA_INTR_COAL_RING_SIZE;
	conf->master_pf = 1;
	conf->qsets_max = 2048;

	ret = qdma_device_open(XOCL_QDMA_PCI, conf,
		(unsigned long *)(&ocl_dev->dma_handle));
	if (ret < 0) {
		xocl_err(&pdev->dev, "QDMA Device Open failed");
		goto failed_open_dev;
	}

	xocl_info(&pdev->dev, "QDMA open succeed: intr: %d",
		ocl_dev->max_user_intr);

	/* map user bar */
	ocl_dev->core.bar_idx = XOCL_QDMA_USER_BAR;
	ocl_dev->bar_len = pci_resource_len(pdev,
		ocl_dev->core.bar_idx);
	ocl_dev->base_addr = pci_iomap(pdev, ocl_dev->core.bar_idx,
		ocl_dev->bar_len);
	if (!ocl_dev->base_addr) {
		xocl_err(&pdev->dev, "Map user bar info failed");
		goto failed_map_io;
	}
	ocl_dev->core.pci_ops = &qdma_pci_ops;

	ret = xocl_subdev_create_all(ocl_dev, dev_info->subdev_info,
		dev_info->subdev_num);
	if (ret) {
		xocl_err(&pdev->dev, "failed to register subdevs");
		goto failed_reg_subdevs;
	}

	if (MM_DMA_DEV(ocl_dev)) {
		/* use 2 channels (queue pairs) */
		ret = xocl_set_max_channel(ocl_dev, 2);
		if (ret)
			goto failed_set_channel;

	}

	ret = xocl_drm_init(ocl_dev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init drm mm");
		goto failed_drm_init;
	}

	ret = xocl_init_sysfs(&pdev->dev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init sysfs");
		goto failed_sysfs_init;
	}

	mutex_init(&ocl_dev->user_msix_table_lock);

	(void) xocl_icap_unlock_bitstream(qd, NULL, 0);

	return 0;

failed_sysfs_init:
	xocl_drm_fini(&qd->ocl_dev);

failed_drm_init:
failed_set_channel:
	xocl_subdev_destroy_all(ocl_dev);
failed_reg_subdevs:
	pci_iounmap(pdev, ocl_dev->base_addr);
failed_map_io:
	qdma_device_close(pdev, (unsigned long)ocl_dev->dma_handle);
failed_open_dev:
	xocl_free_dev_minor(ocl_dev);
failed:
	if (ocl_dev->user_msix_table)
		devm_kfree(&pdev->dev, ocl_dev->user_msix_table);
	devm_kfree(&pdev->dev, qd);
	pci_set_drvdata(pdev, NULL);
	return ret;
}

void xocl_user_qdma_remove(struct pci_dev *pdev)
{
	struct xocl_qdma_dev	*qd;

	qd = pci_get_drvdata(pdev);
	if (!qd) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return;
	}

	xocl_p2p_mem_release(&qd->ocl_dev, false);
	xocl_subdev_destroy_all(&qd->ocl_dev);

	xocl_fini_sysfs(&pdev->dev);
	xocl_drm_fini(&qd->ocl_dev);
	qdma_device_close(pdev, (unsigned long)qd->ocl_dev.dma_handle);
	if (qd->ocl_dev.base_addr)
		pci_iounmap(pdev, qd->ocl_dev.base_addr);
	if (qd->ocl_dev.user_msix_table)
		devm_kfree(&pdev->dev, qd->ocl_dev.user_msix_table);
	mutex_destroy(&qd->ocl_dev.user_msix_table_lock);

	xocl_free_dev_minor(&qd->ocl_dev);

	devm_kfree(&pdev->dev, qd); 
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
		xocl_info(&pdev->dev, "PCI unknown state %d error\n", state);
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

static struct pci_driver user_qdma_driver = {
	.name = XOCL_QDMA_PCI,
	.id_table = pciidlist,
	.probe = xocl_user_qdma_probe,
	.remove = xocl_user_qdma_remove,
	.err_handler = &xocl_err_handler,
};

int __init xocl_init_drv_user_qdma(void)
{
	int ret;

	ret = libqdma_init(0);
	if (ret) {
		goto failed;
	}
	ret = pci_register_driver(&user_qdma_driver);
	if (ret) {
		libqdma_exit();
		goto failed;
	}

	return 0;

failed:
	return ret;
}

void xocl_fini_drv_user_qdma(void)
{
	pci_unregister_driver(&user_qdma_driver);
	libqdma_exit();
}
