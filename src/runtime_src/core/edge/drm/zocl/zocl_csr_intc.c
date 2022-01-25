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
#include "zocl_util.h"
#include "zocl_ert_intc.h"

/* ERT INTC driver name. */
#define ZINTC_NAME			"zocl_csr_intc"
/* The CSR IP provided 128 bit status. We only support the first 32 bit here since, in practice,
 * it's unlikely to create a design with more than 32 units. */
#define ZINTC_MAX_VECTORS		32

#define ZINTC2PDEV(zintc)		((zintc)->zei_pdev)
#define ZINTC2DEV(zintc)		(&ZINTC2PDEV(zintc)->dev)
#define zintc_err(zintc, fmt, args...)	zocl_err(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_info(zintc, fmt, args...)	zocl_info(ZINTC2DEV(zintc), fmt"\n", ##args)
#define zintc_dbg(zintc, fmt, args...)	zocl_dbg(ZINTC2DEV(zintc), fmt"\n", ##args)

struct zocl_csr_intc {
	struct platform_device		*zei_pdev;
	u32				zei_irq;
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

static void __iomem *zintc_map_res(struct zocl_csr_intc *zintc, const char *name, size_t *szp)
{
	struct resource *res;
	void __iomem *map;

	res = platform_get_resource_byname(ZINTC2PDEV(zintc), IORESOURCE_MEM, name);
	if (!res) {
		zintc_err(zintc, "res not found: %s", name);
		return NULL;
	}
	zintc_info(zintc, "%s range: %pR", name, res);

	map = devm_ioremap_resource(ZINTC2DEV(zintc), res);
	if (IS_ERR(map)) {
		zintc_err(zintc, "Failed to map res: %s: %ld", name, PTR_ERR(map));
		return NULL;
	}

	if (szp)
		*szp = res->end - res->start + 1;
	return map;
}

static irqreturn_t zintc_isr(int irq, void *arg)
{
	unsigned long irqflags;
	const u32 lastbit = 0x1;
	int i;
	u32 intr;
	struct zocl_csr_intc *zintc = (struct zocl_csr_intc *)arg;
	struct zocl_ert_intc_handler *h;

	spin_lock_irqsave(&zintc->zei_lock, irqflags);
	/* Only fetch first 32 status bits. */
	intr = reg_read(&zintc->zei_status->zeisr_status[0]);
	for (i = 0; i < ZINTC_MAX_VECTORS && intr; i++, intr >>= 1) {
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
	struct resource *res;
	struct zocl_csr_intc *zintc = devm_kzalloc(&pdev->dev, sizeof(*zintc), GFP_KERNEL);

	if (!zintc)
		return -ENOMEM;
	zintc->zei_pdev = pdev;

	res = platform_get_resource_byname(ZINTC2PDEV(zintc), IORESOURCE_IRQ, ZEI_RES_IRQ);
	if (!res) {
		zintc_err(zintc, "failed to find IRQ"); 
		return -EINVAL;
	}
	/* TODO: We expect only one irq for now, needs to support 4. */
	zintc->zei_irq = res->start;
	zintc_info(zintc, "managing IRQ: %d", zintc->zei_irq); 

	zintc->zei_status = zintc_map_res(zintc, ZEI_RES_STATUS, NULL);
	if (!zintc->zei_status) {
		zintc_err(zintc, "failed to find INTC Status registers"); 
		return -EINVAL;
	}
	/* Disable interrupt till we are ready to handle it. */
	reg_write(&zintc->zei_status->zeisr_enable, 0);

	spin_lock_init(&zintc->zei_lock);
	platform_set_drvdata(pdev, zintc);

	for (i = 0; i < ZINTC_MAX_VECTORS; i++) {
		struct zocl_ert_intc_handler *h = &zintc->zei_handler[i];

		h->zeih_pdev = pdev;
		h->zeih_irq = zintc->zei_irq;
	}

	/* Ready to turn on interrupts! */
	ret = devm_request_irq(ZINTC2DEV(zintc), zintc->zei_irq, zintc_isr, 0, ZINTC_NAME, zintc);
	if (ret)
		zintc_err(zintc, "failed to add isr for IRQ: %d: %d", zintc->zei_irq, ret); 
	else
		reg_write(&zintc->zei_status->zeisr_enable, 1);
	return 0;
}

static int zintc_remove(struct platform_device *pdev)
{
	struct zocl_csr_intc *zintc = platform_get_drvdata(pdev);

	zintc_info(zintc, "Removing %s", ZINTC_NAME);
	return 0;
}

/*
 * Interfaces exposed to other subdev drivers.
 */

static void zocl_csr_intc_add(struct platform_device *pdev, u32 id, irq_handler_t cb, void *arg)
{
	unsigned long irqflags;
	struct zocl_csr_intc *zintc = platform_get_drvdata(pdev);
	struct zocl_ert_intc_handler *h;

	BUG_ON(id >= ZINTC_MAX_VECTORS);

	spin_lock_irqsave(&zintc->zei_lock, irqflags);

	h = &zintc->zei_handler[id];
	BUG_ON(h->zeih_cb);
	h->zeih_cb = cb;
	h->zeih_arg = arg;
	h->zeih_enabled = true;

	spin_unlock_irqrestore(&zintc->zei_lock, irqflags);
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
