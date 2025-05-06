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
#include "../lib/libxdma_api.h"

/*
 * Interrupt controls
 */
#define MAX_INTR_NUM            32
#define MAX_USER_INTR           16

struct msix_xdma_irq {
	struct eventfd_ctx	*event_ctx;
	bool			in_use;
	bool			enabled;
	irq_handler_t		handler;
	void			*arg;
};

struct xocl_msix_xdma {
	struct platform_device	*pdev;
	void __iomem		*base;
	int			msix_user_start_vector;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	/* msi-x vector/entry table */
	struct msix_entry	msix_irq_entries[MAX_INTR_NUM];
#endif

	int			max_user_intr;
	struct msix_xdma_irq	*user_msix_table;
	spinlock_t		user_msix_table_lock;

	void			*dev_handle;
};

static int user_intr_config(struct platform_device *pdev, u32 intr, bool en)
{
	struct xocl_msix_xdma *msix_xdma;
	const unsigned int mask = 1 << intr;
	unsigned long flags;
	int ret;

	msix_xdma= platform_get_drvdata(pdev);

	if (intr >= msix_xdma->max_user_intr) {
		xocl_err(&pdev->dev, "Invalid intr %d, max %d",
			intr, msix_xdma->max_user_intr);
		return -EINVAL;
	}

	spin_lock_irqsave(&msix_xdma->user_msix_table_lock, flags);
	if (msix_xdma->user_msix_table[intr].enabled == en) {
		ret = 0;
		goto end;
	}

	ret = en ? xdma_user_isr_enable(msix_xdma->dev_handle, mask) :
		xdma_user_isr_disable(msix_xdma->dev_handle, mask);
	if (!ret)
		msix_xdma->user_msix_table[intr].enabled = en;
end:
	spin_unlock_irqrestore(&msix_xdma->user_msix_table_lock, flags);

	return ret;
}

static irqreturn_t msix_xdma_isr(int irq, void *arg)
{
	struct msix_xdma_irq *irq_entry = arg;
	int ret = IRQ_HANDLED;

	if (irq_entry->handler)
		ret = irq_entry->handler(irq, irq_entry->arg);

	if (!IS_ERR_OR_NULL(irq_entry->event_ctx)) {
#if KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE || defined(RHEL_9_5_GE)
		eventfd_signal(irq_entry->event_ctx);
#else
		eventfd_signal(irq_entry->event_ctx, 1);
#endif
	}

	return ret;
}

static int user_intr_unreg(struct platform_device *pdev, u32 intr)
{
	struct xocl_msix_xdma *msix_xdma;
	const unsigned int mask = 1 << intr;
	unsigned long flags;
	int ret;

	msix_xdma= platform_get_drvdata(pdev);

	if (intr >= msix_xdma->max_user_intr) {
		xocl_err(&pdev->dev, "intr %d greater than max", intr);
		return -EINVAL;
	}

	spin_lock_irqsave(&msix_xdma->user_msix_table_lock, flags);
	if (!msix_xdma->user_msix_table[intr].in_use) {
		xocl_err(&pdev->dev, "intr %d is not in use", intr);
		ret = -EINVAL;
		goto failed;
	}
	msix_xdma->user_msix_table[intr].handler = NULL;
	msix_xdma->user_msix_table[intr].arg = NULL;

	ret = xdma_user_isr_register(msix_xdma->dev_handle, mask, NULL, NULL);
	if (ret) {
		xocl_err(&pdev->dev, "xdma unregister isr failed");
		goto failed;
	}

	msix_xdma->user_msix_table[intr].in_use = false;

failed:
	spin_unlock_irqrestore(&msix_xdma->user_msix_table_lock, flags);
	return ret;
}

static int user_intr_register(struct platform_device *pdev, u32 intr,
	irq_handler_t handler, void *arg, int event_fd)
{
	struct xocl_msix_xdma *msix_xdma;
	struct eventfd_ctx *trigger = ERR_PTR(-EINVAL);
	const unsigned int mask = 1 << intr;
	unsigned long flags;
	int ret;

	msix_xdma= platform_get_drvdata(pdev);

	if (intr >= msix_xdma->max_user_intr) {
		xocl_err(&pdev->dev, "Invalid intr %d, max %d",
			intr, msix_xdma->max_user_intr);
		return -EINVAL;
	}

	if (event_fd >= 0) {
		trigger = eventfd_ctx_fdget(event_fd);
		if (IS_ERR(trigger)) {
			xocl_err(&pdev->dev, "get event ctx failed");
			return -EFAULT;
		}
	}

	spin_lock_irqsave(&msix_xdma->user_msix_table_lock, flags);
	if (msix_xdma->user_msix_table[intr].in_use) {
		xocl_err(&pdev->dev, "IRQ %d is in use", intr);
		ret = -EPERM;
		goto failed;
	}
	msix_xdma->user_msix_table[intr].event_ctx = trigger;
	msix_xdma->user_msix_table[intr].handler = handler;
	msix_xdma->user_msix_table[intr].arg = arg;

	ret = xdma_user_isr_register(msix_xdma->dev_handle, mask, msix_xdma_isr,
			&msix_xdma->user_msix_table[intr]);
	if (ret) {
		xocl_err(&pdev->dev, "IRQ register failed");
		msix_xdma->user_msix_table[intr].handler = NULL;
		msix_xdma->user_msix_table[intr].arg = NULL;
		msix_xdma->user_msix_table[intr].event_ctx = NULL;
		goto failed;
	}

	msix_xdma->user_msix_table[intr].in_use = true;

	spin_unlock_irqrestore(&msix_xdma->user_msix_table_lock, flags);


	return 0;

failed:
	spin_unlock_irqrestore(&msix_xdma->user_msix_table_lock, flags);
	if (!IS_ERR(trigger))
		eventfd_ctx_put(trigger);

	return ret;
}


static struct xocl_msix_funcs msix_xdma_ops = {
	.user_intr_register = user_intr_register,
	.user_intr_config = user_intr_config,
	.user_intr_unreg = user_intr_unreg,
};

static int msix_xdma_probe(struct platform_device *pdev)
{
	struct xocl_msix_xdma *msix_xdma= NULL;
	int	ret = 0;
	xdev_handle_t		xdev;

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	msix_xdma = devm_kzalloc(&pdev->dev, sizeof(*msix_xdma), GFP_KERNEL);
	if (!msix_xdma) {
		xocl_err(&pdev->dev, "alloc dev data failed");
		return -ENOMEM;
	}

	msix_xdma->pdev = pdev;

	msix_xdma->dev_handle = xdma_device_open(XOCL_MSIX_XDMA,
		XDEV(xdev)->pdev, &msix_xdma->max_user_intr, NULL, NULL, true);
	if (!msix_xdma->dev_handle) {
		xocl_err(&pdev->dev, "failed open xdma device");
		ret = -EIO;
		goto failed;
	}

	xocl_info(&pdev->dev, "max user intr %d", msix_xdma->max_user_intr);
	msix_xdma->user_msix_table = devm_kzalloc(&pdev->dev,
		msix_xdma->max_user_intr *
		sizeof(struct msix_xdma_irq), GFP_KERNEL);
	if (!msix_xdma->user_msix_table) {
		ret = -ENOMEM;
		goto failed;
	}

	platform_set_drvdata(pdev, msix_xdma);
	spin_lock_init(&msix_xdma->user_msix_table_lock);

	return 0;

failed:

	if (msix_xdma->dev_handle)
		xdma_device_close(XDEV(xdev)->pdev, msix_xdma->dev_handle);

	if (msix_xdma->user_msix_table)
		devm_kfree(&pdev->dev, msix_xdma->user_msix_table);

	devm_kfree(&pdev->dev, msix_xdma);
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int __msix_xdma_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev;
	struct xocl_msix_xdma *msix_xdma;
	struct msix_xdma_irq *irq_entry;
	int i;

	msix_xdma = platform_get_drvdata(pdev);
	if (!msix_xdma) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	if (msix_xdma->dev_handle)
		xdma_device_close(XDEV(xdev)->pdev, msix_xdma->dev_handle);

	for (i = 0; i < msix_xdma->max_user_intr; i++) {
		irq_entry = &msix_xdma->user_msix_table[i];
		if (irq_entry->in_use && irq_entry->enabled) {
			xocl_err(&pdev->dev,
				"ERROR: Interrupt %d is still on", i);
		}
		if(!IS_ERR_OR_NULL(irq_entry->event_ctx))
			eventfd_ctx_put(irq_entry->event_ctx);
	}

	if (msix_xdma->user_msix_table)
		devm_kfree(&pdev->dev, msix_xdma->user_msix_table);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, msix_xdma);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void msix_xdma_remove(struct platform_device *pdev)
{
	__msix_xdma_remove(pdev);
}
#else
#define msix_xdma_remove __msix_xdma_remove
#endif

struct xocl_drv_private msix_xdma_priv = {
	.ops = &msix_xdma_ops,
};

static struct platform_device_id msix_xdma_id_table[] = {
	{ XOCL_DEVNAME(XOCL_MSIX_XDMA), (kernel_ulong_t)&msix_xdma_priv },
	{ },
};

static struct platform_driver	msix_xdma_driver = {
	.probe		= msix_xdma_probe,
	.remove		= msix_xdma_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_MSIX_XDMA),
	},
	.id_table	= msix_xdma_id_table,
};

int __init xocl_init_msix_xdma(void)
{
	return platform_driver_register(&msix_xdma_driver);
}

void xocl_fini_msix_xdma(void)
{
	return platform_driver_unregister(&msix_xdma_driver);
}
