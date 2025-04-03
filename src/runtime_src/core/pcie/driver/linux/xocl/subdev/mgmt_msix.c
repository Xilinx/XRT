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

#include <linux/version.h>
#include <linux/eventfd.h>
#include "../xocl_drv.h"

/*
 * Interrupt controls
 */
#define XCLMGMT_MAX_INTR_NUM            32
#define XCLMGMT_MAX_USER_INTR           16
#define XCLMGMT_INTR_CTRL_BASE          (0x2000UL)
#define XCLMGMT_INTR_USER_ENABLE        (XCLMGMT_INTR_CTRL_BASE + 0x08)
#define XCLMGMT_INTR_USER_DISABLE       (XCLMGMT_INTR_CTRL_BASE + 0x0C)
#define XCLMGMT_INTR_USER_VECTOR        (XCLMGMT_INTR_CTRL_BASE + 0x80)
#define XCLMGMT_MAILBOX_INTR            11


struct mgmt_msix_irq {
	bool			in_use;
	bool			enabled;
	irq_handler_t		handler;
	void			*arg;
};
struct xocl_mgmt_msix {
	struct platform_device	*pdev;
	void __iomem		*base;
	int			msix_user_start_vector;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	/* msi-x vector/entry table */
	struct msix_entry	msix_irq_entries[XCLMGMT_MAX_INTR_NUM];
#endif

	int			max_user_intr;
	struct mgmt_msix_irq		*user_msix_table;
	spinlock_t		user_msix_table_lock;

	struct xocl_msix_privdata *privdata;
};

static int user_intr_config(struct platform_device *pdev, u32 intr, bool en)
{
	struct xocl_mgmt_msix *mgmt_msix;
	unsigned long flags;

	mgmt_msix = platform_get_drvdata(pdev);

	if (intr >= mgmt_msix->max_user_intr) {
		xocl_err(&pdev->dev, "Invalid intr %d, max %d",
			intr, mgmt_msix->max_user_intr);
		return -EINVAL;
	}

	xocl_info(&pdev->dev, "configure intr at 0x%lx",
			(unsigned long)mgmt_msix->base);
	spin_lock_irqsave(&mgmt_msix->user_msix_table_lock, flags);
	if (mgmt_msix->user_msix_table[intr].enabled == en)
		goto end;

	if (!mgmt_msix->privdata) {
		XOCL_WRITE_REG32(1 << intr, mgmt_msix->base +
			(en ? XCLMGMT_INTR_USER_ENABLE :
			 XCLMGMT_INTR_USER_DISABLE));
	}

	mgmt_msix->user_msix_table[intr].enabled = en;
end:
	spin_unlock_irqrestore(&mgmt_msix->user_msix_table_lock, flags);

	return 0;
}

static int user_intr_unreg(struct platform_device *pdev, u32 intr)
{
	struct xocl_mgmt_msix *mgmt_msix;
	struct xocl_dev_core *core;
	unsigned long flags;
	u32 vec;
	int ret = 0;

	mgmt_msix = platform_get_drvdata(pdev);

	if (intr >= mgmt_msix->max_user_intr)
		return -EINVAL;

	spin_lock_irqsave(&mgmt_msix->user_msix_table_lock, flags);
	if (!mgmt_msix->user_msix_table[intr].in_use) {
		ret = -EINVAL;
		goto failed;
	}
	core = xocl_get_xdev(pdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	vec = pci_irq_vector(core->pdev, mgmt_msix->msix_user_start_vector + intr);
#else
	vec = mgmt_msix->msix_irq_entries[mgmt_msix->msix_user_start_vector+intr].vector;
#endif

	free_irq(vec, mgmt_msix->user_msix_table[intr].arg);

	mgmt_msix->user_msix_table[intr].handler = NULL;
	mgmt_msix->user_msix_table[intr].arg = NULL;
	mgmt_msix->user_msix_table[intr].in_use = false;
	xocl_info(&pdev->dev, "intr %d unreg success, start vec %d",
		intr, mgmt_msix->msix_user_start_vector);
failed:
	spin_unlock_irqrestore(&mgmt_msix->user_msix_table_lock, flags);

	return ret;
}

static int user_intr_register(struct platform_device *pdev, u32 intr,
	irq_handler_t handler, void *arg, int event_fd)
{
	struct xocl_mgmt_msix *mgmt_msix;
	struct xocl_dev_core *core;
	unsigned long flags;
	u32 vec;
	int ret;

	mgmt_msix = platform_get_drvdata(pdev);

	if (intr >= mgmt_msix->max_user_intr)
		return -EINVAL;

	spin_lock_irqsave(&mgmt_msix->user_msix_table_lock, flags);
	if (mgmt_msix->user_msix_table[intr].in_use) {
		xocl_err(&pdev->dev, "IRQ %d is in use", intr);
		ret = -EPERM;
		goto failed;
	}

	core = xocl_get_xdev(pdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	vec = pci_irq_vector(core->pdev, mgmt_msix->msix_user_start_vector + intr);
#else
	vec = mgmt_msix->msix_irq_entries[mgmt_msix->msix_user_start_vector+intr].vector;
#endif

	ret = request_irq(vec, handler, 0, XCLMGMT_MODULE_NAME, arg);
	if (ret) {
		xocl_err(&pdev->dev, "request IRQ failed %x", ret);
		goto failed;
	}

	mgmt_msix->user_msix_table[intr].handler = handler;
	mgmt_msix->user_msix_table[intr].arg = arg;
	mgmt_msix->user_msix_table[intr].in_use = true;

	xocl_info(&pdev->dev, "intr %d register success, start vec %d",
		       intr, mgmt_msix->msix_user_start_vector);

failed:
	spin_unlock_irqrestore(&mgmt_msix->user_msix_table_lock, flags);

	return ret;
}

static struct xocl_dma_funcs mgmt_msix_ops = {
	.user_intr_register = user_intr_register,
	.user_intr_config = user_intr_config,
	.user_intr_unreg = user_intr_unreg,
};

static int identify_intr_bar(struct xocl_mgmt_msix *mgmt_msix)
{
	struct pci_dev *pdev = XOCL_PL_TO_PCI_DEV(mgmt_msix->pdev);
	int	i;
	resource_size_t bar_len;

	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		bar_len = pci_resource_len(pdev, i);
		if (bar_len < 1024 * 1024 && bar_len > 0) {
			mgmt_msix->base = ioremap_nocache(
				pci_resource_start(pdev, i), bar_len);
			return i;
		}
	}

	return -1;
}

static int mgmt_msix_probe(struct platform_device *pdev)
{
	struct xocl_mgmt_msix *mgmt_msix= NULL;
	int	i, ret = 0, bar;
	u32 total;
	xdev_handle_t		xdev;
	struct resource *res;

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	mgmt_msix = devm_kzalloc(&pdev->dev, sizeof(*mgmt_msix), GFP_KERNEL);
	if (!mgmt_msix) {
		xocl_err(&pdev->dev, "alloc mgmt_msix dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	mgmt_msix->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_info(&pdev->dev,
			"legacy platform, identify intr bar by size");
		bar = identify_intr_bar(mgmt_msix);
		if (bar < 0) {
			xocl_err(&pdev->dev, "Can not find intr bar");
			ret = -ENXIO;
			goto failed;
		}

	} else
		mgmt_msix->base = ioremap_nocache(res->start,
				res->end - res->start + 1);
	if (!mgmt_msix->base) {
		ret = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

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
	mgmt_msix->privdata = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (mgmt_msix->privdata) {
		mgmt_msix->msix_user_start_vector = mgmt_msix->privdata->start;
		total = mgmt_msix->privdata->total;
	} else {
		mgmt_msix->msix_user_start_vector =
			XOCL_READ_REG32(mgmt_msix->base + XCLMGMT_INTR_USER_VECTOR) & 0xf;
		total = mgmt_msix->msix_user_start_vector + XCLMGMT_MAX_USER_INTR;
	}

	if (total > XCLMGMT_MAX_INTR_NUM) {
		xocl_err(&pdev->dev, "Invalid number of interrupts %d", total);
		goto failed;
	}

	if (total > pci_msix_vec_count(XDEV(xdev)->pdev)) {
		xocl_info(&pdev->dev, "Actual number of msix less then expected total %d", total);
		total = pci_msix_vec_count(XDEV(xdev)->pdev);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	i = 0;
	ret = pci_alloc_irq_vectors(XDEV(xdev)->pdev, total, total,
			PCI_IRQ_MSIX);
	if (ret != total) {
		xocl_err(&pdev->dev, "init msix failed ret %d", ret);
		ret = -ENOENT;
		goto failed;
	}
#else
	for (i = 0; i < total; i++)
		mgmt_msix->msix_irq_entries[i].entry = i;

	ret = pci_enable_msix(XDEV(xdev)->pdev, mgmt_msix->msix_irq_entries, total);
	if (ret) {
		xocl_err(&pdev->dev, "init msix failed ret %d", ret);
		ret = -ENOENT;
		goto failed;
	}


#endif
	mgmt_msix->max_user_intr = total;

	mgmt_msix->user_msix_table = devm_kzalloc(&pdev->dev,
			mgmt_msix->max_user_intr *
			sizeof(struct mgmt_msix_irq), GFP_KERNEL);
	if (!mgmt_msix->user_msix_table) {
		xocl_err(&pdev->dev, "alloc user_msix_table failed");
		ret = -ENOMEM;
		goto failed;
	}
	spin_lock_init(&mgmt_msix->user_msix_table_lock);

	platform_set_drvdata(pdev, mgmt_msix);

	return 0;

failed:
	pci_disable_msix(XDEV(xdev)->pdev);

	if (mgmt_msix) {
		if (mgmt_msix->user_msix_table) {
			devm_kfree(&pdev->dev, mgmt_msix->user_msix_table);
		}
		devm_kfree(&pdev->dev, mgmt_msix);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int __mgmt_msix_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev;
	struct xocl_mgmt_msix *mgmt_msix;
	struct mgmt_msix_irq *irq_entry;
	int i;

	mgmt_msix = platform_get_drvdata(pdev);
	if (!mgmt_msix) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	for (i = 0; i < mgmt_msix->max_user_intr; i++) {
		irq_entry = &mgmt_msix->user_msix_table[i];
		if (irq_entry->in_use && irq_entry->enabled) {
			xocl_err(&pdev->dev,
				"ERROR: Interrupt %d is still on", i);
		}
	}

	pci_disable_msix(XDEV(xdev)->pdev);

	devm_kfree(&pdev->dev, mgmt_msix->user_msix_table);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, mgmt_msix);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void mgmt_msix_remove(struct platform_device *pdev)
{
	__mgmt_msix_remove(pdev);
}
#else
#define mgmt_msix_remove __mgmt_msix_remove
#endif

struct xocl_drv_private mgmt_msix_priv = {
	.ops = &mgmt_msix_ops,
};

static struct platform_device_id mgmt_msix_id_table[] = {
	{ XOCL_DEVNAME(XOCL_DMA_MSIX), (kernel_ulong_t)&mgmt_msix_priv },
	{ },
};

static struct platform_driver	mgmt_msix_driver = {
	.probe		= mgmt_msix_probe,
	.remove		= mgmt_msix_remove,
	.driver		= {
		.name = "mgmt_msix",
	},
	.id_table	= mgmt_msix_id_table,
};

int __init xocl_init_mgmt_msix(void)
{
	return platform_driver_register(&mgmt_msix_driver);
}

void xocl_fini_mgmt_msix(void)
{
	return platform_driver_unregister(&mgmt_msix_driver);
}
