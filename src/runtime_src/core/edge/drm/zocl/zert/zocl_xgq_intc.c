/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Author(s):
 *        Max Zhen <maxz@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include "zocl_lib.h"
#include "zocl_ert_intc.h"

#define ZINTC_DRV_NAME			"zocl_irq_intc"

#define ZINTC2PDEV(zintc)		((zintc)->zei_pdev)
#define ZINTC2DEV(zintc)		(&ZINTC2PDEV(zintc)->dev)
#define zintc_err(zintc, fmt, args...)	zocl_err(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_info(zintc, fmt, args...)	zocl_info(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_dbg(zintc, fmt, args...)	zocl_dbg(ZINTC2DEV(zintc), fmt"\n", ##args)

struct zocl_irq_intc {
	struct platform_device		*zei_pdev;

	spinlock_t			zei_lock;

	int				zei_num_irqs;
	/* variable length based on num of irqs, always the last member */
	struct zocl_ert_intc_handler	zei_handler[0];
};

static irqreturn_t zintc_isr(int irq, void *arg)
{
	unsigned long irqflags;
	struct zocl_ert_intc_handler *h = (struct zocl_ert_intc_handler *)arg;
	struct zocl_irq_intc *zintc = platform_get_drvdata(h->zeih_pdev);

	spin_lock_irqsave(&h->zeih_lock, irqflags);
	if (h->zeih_cb && h->zeih_enabled)
		h->zeih_cb(irq, h->zeih_arg);
	else
		zintc_err(zintc, "Spurious interrupt received on %d", irq);
	spin_unlock_irqrestore(&h->zeih_lock, irqflags);
	return IRQ_HANDLED;
}

static int zintc_probe(struct platform_device *pdev)
{
	int i;
	struct zocl_irq_intc *zintc;
	int num_irqs = platform_irq_count(pdev);

	if (num_irqs <= 0) {
		zocl_err(&pdev->dev, "failed to find IRQs, num of IRQ: %d\n", num_irqs);
		return -EINVAL;
	}

	zintc = devm_kzalloc(&pdev->dev,
			     sizeof(*zintc) + sizeof(struct zocl_ert_intc_handler) * num_irqs,
			     GFP_KERNEL);
	if (!zintc)
		return -ENOMEM;
	zintc->zei_pdev = pdev;
	zintc->zei_num_irqs = num_irqs;
	platform_set_drvdata(pdev, zintc);

	/* Ready to turn on interrupts! */
	for (i = 0; i < num_irqs; i++) {
		/*
		 * The irq resources ordering is important.
		 * Later on, use the resource index to add a handler.
		 */
		struct zocl_ert_intc_handler *h = &zintc->zei_handler[i];
		struct resource *res = platform_get_resource(ZINTC2PDEV(zintc), IORESOURCE_IRQ, i);
		u32 irq = res->start;

		h->zeih_pdev = pdev;
		h->zeih_irq = irq;
		BUG_ON(irq < 0);
		spin_lock_init(&h->zeih_lock);
	}

	return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void zintc_remove(struct platform_device *pdev)
#else
static int zintc_remove(struct platform_device *pdev)
#endif
{
	struct zocl_irq_intc *zintc = platform_get_drvdata(pdev);

	zintc_info(zintc, "Removing %s", ZINTC_DRV_NAME);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	return 0;
#endif
}

/*
 * Interfaces exposed to other subdev drivers.
 */

static int zocl_irq_intc_add(struct platform_device *pdev, u32 id, irq_handler_t cb, void *arg)
{
	unsigned long irqflags;
	struct zocl_irq_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h;
	int ret = 0;

	if (id >= zintc->zei_num_irqs)
		return -EINVAL;

	h = &zintc->zei_handler[id];
	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	if (h->zeih_cb) {
		ret = -EINVAL;
		goto unlock_and_out;
	}

	ret = request_irq(h->zeih_irq, zintc_isr, 0, ZINTC_DRV_NAME, h);
	if (ret) {
		zintc_err(zintc, "failed to add isr for IRQ: %d: %d", h->zeih_irq, ret);
		goto unlock_and_out;
	}

	zintc_info(zintc, "managing IRQ %d", h->zeih_irq);
	h->zeih_cb = cb;
	h->zeih_arg = arg;
	h->zeih_enabled = true;

unlock_and_out:
	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);

	return ret;
}

static void zocl_irq_intc_remove(struct platform_device *pdev, u32 id)
{
	unsigned long irqflags;
	struct zocl_irq_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h;

	BUG_ON(id >= zintc->zei_num_irqs);
	h = &zintc->zei_handler[id];
	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	h->zeih_cb = NULL;
	h->zeih_arg = NULL;
	h->zeih_enabled = false;
	free_irq(h->zeih_irq, h);

	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
}

static void zocl_irq_intc_config(struct platform_device *pdev, u32 id, bool enabled)
{
	unsigned long irqflags;
	struct zocl_irq_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h;

	BUG_ON(id >= zintc->zei_num_irqs);
	h = &zintc->zei_handler[id];
	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	if (enabled)
		enable_irq(h->zeih_irq);
	else
		disable_irq(h->zeih_irq);

	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
}

static struct zocl_ert_intc_drv_data zocl_irq_intc_drvdata = {
	.add = zocl_irq_intc_add,
	.remove = zocl_irq_intc_remove,
	.config = zocl_irq_intc_config
};

static const struct platform_device_id zocl_irq_intc_id_match[] = {
	{ ERT_XGQ_INTC_DEV_NAME, (kernel_ulong_t)&zocl_irq_intc_drvdata },
	{ ERT_CU_INTC_DEV_NAME, (kernel_ulong_t)&zocl_irq_intc_drvdata },
	{ /* end of table */ },
};

struct platform_driver zocl_irq_intc_driver = {
	.driver = {
		.name = ZINTC_DRV_NAME,
	},
	.probe  = zintc_probe,
	.remove = zintc_remove,
	.id_table = zocl_irq_intc_id_match,
};
