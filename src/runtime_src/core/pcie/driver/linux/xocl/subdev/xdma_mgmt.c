/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
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

/* XDMA version Memory Mapped DMA */

#include <linux/version.h>
#include <linux/eventfd.h>
#include "mgmt-reg.h"
#include "../xocl_drv.h"

struct xdma_irq {
	bool			in_use;
	bool			enabled;
	irq_handler_t		handler;
	void			*arg;
};
struct xocl_xdma {
	struct platform_device	*pdev;
	void __iomem		*base;
	int			msix_user_start_vector;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	/* msi-x vector/entry table */
	struct msix_entry	msix_irq_entries[XCLMGMT_MAX_INTR_NUM];
#endif

	int			max_user_intr;
	struct xdma_irq		*user_msix_table;
	struct mutex		user_msix_table_lock;
};

static int user_intr_config(struct platform_device *pdev, u32 intr, bool en)
{
	struct xocl_xdma *xdma;

	xdma= platform_get_drvdata(pdev);

	if (intr >= xdma->max_user_intr) {
		xocl_err(&pdev->dev, "Invalid intr %d, max %d",
			intr, xdma->max_user_intr);
		return -EINVAL;
	}

	xocl_info(&pdev->dev, "configure intr at 0x%lx",
			(unsigned long)xdma->base);
	mutex_lock(&xdma->user_msix_table_lock);
	if (xdma->user_msix_table[intr].enabled == en)
		goto end;

	XOCL_WRITE_REG32(1 << intr, xdma->base +
			(en ? XCLMGMT_INTR_USER_ENABLE :
			 XCLMGMT_INTR_USER_DISABLE));

	xdma->user_msix_table[intr].enabled = en;
end:
	mutex_unlock(&xdma->user_msix_table_lock);

	return 0;
}

static int user_intr_unreg(struct platform_device *pdev, u32 intr)
{
	struct xocl_xdma *xdma;
	struct xocl_dev_core *core;
	u32 vec;
	int ret;

	xdma= platform_get_drvdata(pdev);

	if (intr >= xdma->max_user_intr)
		return -EINVAL;

	mutex_lock(&xdma->user_msix_table_lock);
	if (!xdma->user_msix_table[intr].in_use) {
		ret = -EINVAL;
		goto failed;
	}
	core = xocl_get_xdev(pdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	vec = pci_irq_vector(core->pdev, xdma->msix_user_start_vector + intr);
#else
	vec = xdma->msix_irq_entries[xdma->msix_user_start_vector+intr].vector;
#endif

	free_irq(vec, xdma->user_msix_table[intr].arg);

	xdma->user_msix_table[intr].handler = NULL;
	xdma->user_msix_table[intr].arg = NULL;
	xdma->user_msix_table[intr].in_use = false;
	xocl_info(&pdev->dev, "intr %d unreg success, start vec %d",
		intr, xdma->msix_user_start_vector);
failed:
	mutex_unlock(&xdma->user_msix_table_lock);

	return ret;
}

static int user_intr_register(struct platform_device *pdev, u32 intr,
	irq_handler_t handler, void *arg, int event_fd)
{
	struct xocl_xdma *xdma;
	struct xocl_dev_core *core;
	u32 vec;
	int ret;

	xdma= platform_get_drvdata(pdev);

	if (intr >= xdma->max_user_intr)
		return -EINVAL;

	mutex_lock(&xdma->user_msix_table_lock);
	if (xdma->user_msix_table[intr].in_use) {
		xocl_err(&pdev->dev, "IRQ %d is in use", intr);
		ret = -EPERM;
		goto failed;
	}

	core = xocl_get_xdev(pdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	vec = pci_irq_vector(core->pdev, xdma->msix_user_start_vector + intr);
#else
	vec = xdma->msix_irq_entries[xdma->msix_user_start_vector+intr].vector;
#endif

	ret = request_irq(vec, handler, 0, XCLMGMT_MODULE_NAME, arg);
	if (ret) {
		xocl_err(&pdev->dev, "request IRQ failed %x", ret);
		goto failed;
	}

	xdma->user_msix_table[intr].handler = handler;
	xdma->user_msix_table[intr].arg = arg;
	xdma->user_msix_table[intr].in_use = true;

	xocl_info(&pdev->dev, "intr %d register success, start vec %d",
		       intr, xdma->msix_user_start_vector);

failed:
	mutex_unlock(&xdma->user_msix_table_lock);

	return ret;
}

static struct xocl_dma_funcs xdma_ops = {
	.user_intr_register = user_intr_register,
	.user_intr_config = user_intr_config,
	.user_intr_unreg = user_intr_unreg,
};

static int xdma_mgmt_probe(struct platform_device *pdev)
{
	struct xocl_xdma	*xdma = NULL;
	int	i, ret = 0, total = 0;
	xdev_handle_t		xdev;
	struct resource *res;

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	xdma = devm_kzalloc(&pdev->dev, sizeof(*xdma), GFP_KERNEL);
	if (!xdma) {
		xocl_err(&pdev->dev, "alloc xdma dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	xdma->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!xdma->base) {
		ret = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	xdma->pdev = pdev;

	/*
	 * Get start vector (index into msi-x table) of msi-x usr intr
	 * on this device.
	 *
	 * The device has XCLMGMT_MAX_USER_INTR number of usr intrs,
	 * the last half of them belongs to mgmt pf, and the first
	 * half to user pf. All vectors are hard-wired.
	 *
	 * The device also has some number of DMA intrs whose vectors
	 * come before usr ones.
	 *
	 * This means that mgmt pf needs to allocate msi-x table big
	 * enough to cover its own usr vectors. So, only the last
	 * chunk of the table will ever be used for mgmt pf.
	 */
	xdma->msix_user_start_vector =
		XOCL_READ_REG32(xdma->base + XCLMGMT_INTR_USER_VECTOR) & 0xf;
	total = xdma->msix_user_start_vector + XCLMGMT_MAX_USER_INTR;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	i = 0;
	ret = pci_alloc_irq_vectors(XDEV(xdev)->pdev, total, total,
			PCI_IRQ_MSIX);
	if (ret != total) {
		ret = -ENOENT;
		goto failed;
	}
#else
	for (i = 0; i < total; i++)
		xdma->msix_irq_entries[i].entry = i;

	ret = pci_enable_msix(XDEV(xdev)->pdev, xdma->msix_irq_entries, total);
#endif
	xdma->max_user_intr = total;

	xdma->user_msix_table = devm_kzalloc(&pdev->dev,
			xdma->max_user_intr *
			sizeof(struct xdma_irq), GFP_KERNEL);
	if (!xdma->user_msix_table) {
		xocl_err(&pdev->dev, "alloc user_msix_table failed");
		ret = -ENOMEM;
		goto failed;
	}
	mutex_init(&xdma->user_msix_table_lock);

	xocl_subdev_register(pdev, XOCL_SUBDEV_DMA, &xdma_ops);
	platform_set_drvdata(pdev, xdma);

	return 0;

failed:
	pci_disable_msix(XDEV(xdev)->pdev);

	if (xdma) {
		if (xdma->user_msix_table) {
			devm_kfree(&pdev->dev, xdma->user_msix_table);
		}
		devm_kfree(&pdev->dev, xdma);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int xdma_mgmt_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev;
	struct xocl_xdma *xdma;
	struct xdma_irq *irq_entry;
	int i;

	xdma= platform_get_drvdata(pdev);
	if (!xdma) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	for (i = 0; i < xdma->max_user_intr; i++) {
		irq_entry = &xdma->user_msix_table[i];
		if (irq_entry->in_use && irq_entry->enabled) {
			xocl_err(&pdev->dev,
				"ERROR: Interrupt %d is still on", i);
		}
	}

	pci_disable_msix(XDEV(xdev)->pdev);

	mutex_destroy(&xdma->user_msix_table_lock);

	devm_kfree(&pdev->dev, xdma->user_msix_table);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, xdma);

	return 0;
}

static struct platform_device_id xdma_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XDMA), 0 },
	{ },
};

static struct platform_driver	xdma_driver = {
	.probe		= xdma_mgmt_probe,
	.remove		= xdma_mgmt_remove,
	.driver		= {
		.name = "xclmgmt_xdma",
	},
	.id_table	= xdma_id_table,
};

int __init xocl_init_xdma_mgmt(void)
{
	return platform_driver_register(&xdma_driver);
}

void xocl_fini_xdma_mgmt(void)
{
	return platform_driver_unregister(&xdma_driver);
}
