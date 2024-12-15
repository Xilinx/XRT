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

#define WORD_BITS			32
/* ERT INTC driver name. */
#define ZINTC_NAME			"zocl_csr_intc"
/* The CSR IP provided 128 bit status. */
#define ZINTC_MAX_VECTORS		128
/* Every 32 status bits requires one irq */
#define ZINTC_MAX_IRQS			(ZINTC_MAX_VECTORS / WORD_BITS)

#define ZINTC2PDEV(zintc)		((zintc)->zei_pdev)
#define ZINTC2DEV(zintc)		(&ZINTC2PDEV(zintc)->dev)
#define zintc_err(zintc, fmt, args...)	zocl_err(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_info(zintc, fmt, args...)	zocl_info(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_dbg(zintc, fmt, args...)	zocl_dbg(ZINTC2DEV(zintc), fmt"\n", ##args)

struct zocl_csr_intc {
	struct platform_device		*zei_pdev;
	int				zei_num_irqs;
	u32				zei_irqs[ZINTC_MAX_IRQS];
	struct zocl_ert_intc_status_reg	*zei_status;

	spinlock_t			zei_lock;
	struct zocl_ert_intc_handler	zei_handler[ZINTC_MAX_VECTORS];
};

static inline void reg_write(void __iomem  *addr, u32 val)
{
	iowrite32(val, addr);
}

static inline u32 reg_read(void __iomem *addr)
{
	return ioread32(addr);
}

static irqreturn_t zintc_isr(int irq, void *arg)
{
	unsigned long irqflags;
	const u32 lastbit = 0x1;
	int word_idx, i;
	u32 intr;
	struct zocl_csr_intc *zintc = (struct zocl_csr_intc *)arg;
	struct zocl_ert_intc_handler *h;

	spin_lock_irqsave(&zintc->zei_lock, irqflags);
	/* Find out status word based on irq. */
	for (word_idx = 0; word_idx < zintc->zei_num_irqs; word_idx++) {
		if (zintc->zei_irqs[word_idx] == irq)
			break;
	}
	BUG_ON(word_idx >= zintc->zei_num_irqs);

	intr = reg_read(&zintc->zei_status->zeisr_status[i]);
	for (i = word_idx * WORD_BITS; i < (word_idx + 1) * WORD_BITS && intr; i++, intr >>= 1) {
		if ((intr & lastbit) == 0) 
			continue;
		h = &zintc->zei_handler[i];
		if (h->zeih_cb && h->zeih_enabled)
			h->zeih_cb(i, h->zeih_arg);
		else
			zintc_err(zintc, "Spurious interrupt received on %d", i); 
	}
	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
	return IRQ_HANDLED;
}

static int zintc_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct zocl_csr_intc *zintc = NULL;
	int num_irqs = platform_irq_count(pdev);

	if (num_irqs <= 0 || num_irqs > ZINTC_MAX_IRQS) {
		zocl_err(&pdev->dev, "invalid num of IRQ: %d\n", num_irqs); 
		return -EINVAL;
	}

	zintc = devm_kzalloc(&pdev->dev, sizeof(*zintc), GFP_KERNEL);
	if (!zintc)
		return -ENOMEM;
	zintc->zei_pdev = pdev;
	zintc->zei_num_irqs = num_irqs;
	spin_lock_init(&zintc->zei_lock);
	platform_set_drvdata(pdev, zintc);

	zintc->zei_status = zlib_map_res_by_name(pdev, ZEI_RES_STATUS, NULL, NULL);
	if (!zintc->zei_status) {
		zintc_err(zintc, "failed to find INTC Status registers"); 
		return -EINVAL;
	}
	/* Disable interrupt till we are ready to handle it. */
	reg_write(&zintc->zei_status->zeisr_enable, 0);

	for (i = 0; i < num_irqs; i++) {
		struct resource *res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		u32 irq = res->start;

		zintc->zei_irqs[i] = irq;
		ret = devm_request_irq(&pdev->dev, irq, zintc_isr, 0, ZINTC_NAME, zintc);
		if (ret)
			zintc_err(zintc, "failed to add isr for IRQ: %d: %d", irq, ret); 
		else
			zintc_info(zintc, "managing IRQ %d", irq); 
	}

	for (i = 0; i < ZINTC_MAX_VECTORS; i++) {
		struct zocl_ert_intc_handler *h = &zintc->zei_handler[i];

		h->zeih_pdev = pdev;
		h->zeih_irq = zintc->zei_irqs[i / WORD_BITS];
	}

	/* Ready to turn on interrupts! */
	reg_write(&zintc->zei_status->zeisr_enable, 1);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void zintc_remove(struct platform_device *pdev) 
#else
static int zintc_remove(struct platform_device *pdev) 
#endif
{
	struct zocl_csr_intc *zintc = platform_get_drvdata(pdev);

	zintc_info(zintc, "Removing %s", ZINTC_NAME);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	return 0;
#endif
}

/*
 * Interfaces exposed to other subdev drivers.
 */

static int zocl_csr_intc_add(struct platform_device *pdev, u32 id, irq_handler_t cb, void *arg)
{
	unsigned long irqflags;
	struct zocl_csr_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h;

	if (id >= ZINTC_MAX_VECTORS)
		return -EINVAL;

	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	h = &zintc->zei_handler[id];
	if (h->zeih_irq == 0)
		zintc_err(zintc, "vector %d has no matching irq", id);

	if (h->zeih_cb)
		return -EINVAL;
	h->zeih_cb = cb;
	h->zeih_arg = arg;
	h->zeih_enabled = true;

	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);

	return 0;
}

static void zocl_csr_intc_remove(struct platform_device *pdev, u32 id)
{
	unsigned long irqflags;
	struct zocl_csr_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h;

	BUG_ON(id >= ZINTC_MAX_VECTORS);
	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	h = &zintc->zei_handler[id];
	h->zeih_cb = NULL;
	h->zeih_arg = NULL;
	h->zeih_enabled = false;

	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
}

static struct zocl_ert_intc_drv_data zocl_csr_intc_drvdata = {
	.add = zocl_csr_intc_add,
	.remove = zocl_csr_intc_remove,
};

static const struct platform_device_id zocl_csr_intc_id_match[] = {
	{ ERT_CSR_INTC_DEV_NAME, (kernel_ulong_t)&zocl_csr_intc_drvdata },
	{ /* end of table */ },
};

struct platform_driver zocl_csr_intc_driver = {
	.driver = {
		.name = ZINTC_NAME,
	},
	.probe  = zintc_probe,
	.remove = zintc_remove,
	.id_table = zocl_csr_intc_id_match,
};
