/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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

/* ERT INTC driver name. */
#define ZINTC_NAME			"zocl_xgq_intc"

#define ZINTC2PDEV(zintc)		((zintc)->zei_pdev)
#define ZINTC2DEV(zintc)		(&ZINTC2PDEV(zintc)->dev)
#define zintc_err(zintc, fmt, args...)	zocl_err(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_info(zintc, fmt, args...)	zocl_info(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_dbg(zintc, fmt, args...)	zocl_dbg(ZINTC2DEV(zintc), fmt"\n", ##args)

struct zocl_xgq_intc {
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
	struct zocl_xgq_intc *zintc = platform_get_drvdata(h->zeih_pdev);

	spin_lock_irqsave(&zintc->zei_lock, irqflags);
	if (h->zeih_cb && h->zeih_enabled)
		h->zeih_cb(irq, h->zeih_arg);
	else
		zintc_err(zintc, "Spurious interrupt received on %d", irq); 
	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
	return IRQ_HANDLED;
}

static int zintc_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct zocl_xgq_intc *zintc;
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
	spin_lock_init(&zintc->zei_lock);
	platform_set_drvdata(pdev, zintc);

	/* Ready to turn on interrupts! */
	for (i = 0; i < num_irqs; i++) {
		struct zocl_ert_intc_handler *h = &zintc->zei_handler[i];
		struct resource *res = platform_get_resource(ZINTC2PDEV(zintc), IORESOURCE_IRQ, i);
		u32 irq = res->start;

		h->zeih_pdev = pdev;
		h->zeih_irq = irq;
		BUG_ON(irq < 0);
		ret = devm_request_irq(ZINTC2DEV(zintc), irq, zintc_isr, 0, ZINTC_NAME, h);
		if (ret)
			zintc_err(zintc, "failed to add isr for IRQ: %d: %d", irq, ret); 
		else
			zintc_info(zintc, "managing IRQ %d", irq); 
	}

	return 0;
}

static int zintc_remove(struct platform_device *pdev)
{
	struct zocl_xgq_intc *zintc = platform_get_drvdata(pdev);

	zintc_info(zintc, "Removing %s", ZINTC_NAME);
	return 0;
}

/*
 * Interfaces exposed to other subdev drivers.
 */

static struct zocl_ert_intc_handler *find_handler(struct zocl_xgq_intc *zintc, u32 irq)
{
	int i;
	struct zocl_ert_intc_handler *h;

	for (i = 0; i < zintc->zei_num_irqs; i++) {
		h = &zintc->zei_handler[i];
		if (h->zeih_irq == irq)
			return h;
	}
	return NULL;
}

static void zocl_xgq_intc_add(struct platform_device *pdev, u32 irq, irq_handler_t cb, void *arg)
{
	unsigned long irqflags;
	struct zocl_xgq_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h = find_handler(zintc, irq);

	BUG_ON(!h);
	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	BUG_ON(h->zeih_cb);
	h->zeih_cb = cb;
	h->zeih_arg = arg;
	h->zeih_enabled = true;

	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
}

static void zocl_xgq_intc_remove(struct platform_device *pdev, u32 irq)
{
	unsigned long irqflags;
	struct zocl_xgq_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h = find_handler(zintc, irq);

	BUG_ON(!h);
	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	h->zeih_cb = NULL;
	h->zeih_arg = NULL;
	h->zeih_enabled = false;

	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
}

static struct zocl_ert_intc_drv_data zocl_xgq_intc_drvdata = {
	.add = zocl_xgq_intc_add,
	.remove = zocl_xgq_intc_remove,
};

static const struct platform_device_id zocl_xgq_intc_id_match[] = {
	{ ERT_XGQ_INTC_DEV_NAME, (kernel_ulong_t)&zocl_xgq_intc_drvdata },
	{ /* end of table */ },
};

struct platform_driver zocl_xgq_intc_driver = {
	.driver = {
		.name = ZINTC_NAME,
	},
	.probe  = zintc_probe,
	.remove = zintc_remove,
	.id_table = zocl_xgq_intc_id_match,
};
