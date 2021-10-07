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
#include "zocl_xgq_plat.h"
#include "xgq_impl.h"
#include "zocl_xgq_ert.h"

/* CU XGQ driver name. */
#define ZXGQ_NAME "zocl_xgq_cu"

#define ZXGQ2PDEV(zxgq)			((zxgq)->zxc_pdev)
#define ZXGQ2DEV(zxgq)			(&ZXGQ2PDEV(zxgq)->dev)
#define zxgq_err(zxgq, fmt, args...)	zocl_err(ZXGQ2DEV(zxgq), fmt"\n", ##args)
#define zxgq_info(zxgq, fmt, args...)	zocl_info(ZXGQ2DEV(zxgq), fmt"\n", ##args)
#define zxgq_dbg(zxgq, fmt, args...)	zocl_dbg(ZXGQ2DEV(zxgq), fmt"\n", ##args)

/* Slot size for all CU XGQs is the same. */
#define CU_XGQ_SLOT_SZ		2048

struct zocl_xgq_cu {
	struct platform_device	*zxc_pdev;
	void __iomem		*zxc_sq_prod;
	void __iomem		*zxc_cq_prod;
	void __iomem		*zxc_cq_prod_int;
	void __iomem		*zxc_ring;
	void __iomem		*zxc_cq_int;
	u32			zxc_irq;
	struct xgq		zxc_xgq;
};

static inline void reg_write(void __iomem  *addr, u32 val)
{
	iowrite32(val, addr);
}

static inline u32 reg_read(void __iomem *addr)
{
	return ioread32(addr);
}

static void __iomem *zxgq_map_res(struct zocl_xgq_cu *zxgq, unsigned int id, size_t *szp)
{
	struct resource *res;
	void __iomem *map;

	res = platform_get_resource(ZXGQ2PDEV(zxgq), IORESOURCE_MEM, id);
	if (!res) {
		zxgq_err(zxgq, "failed to find CU XGQ resource (%d)", id);
		return NULL;
	}
	zxgq_info(zxgq, "XGQ CU resource (%d) range: [0x%llx, 0x%llx]", id, res->start, res->end);

	map = devm_ioremap(ZXGQ2DEV(zxgq), res->start, res->end - res->start + 1);
	if (IS_ERR(map)) {
		zxgq_err(zxgq, "Failed to map CU XGQ resource (%d): %ld", id, PTR_ERR(map));
		return NULL;
	}

	if (szp)
		*szp = res->end - res->start + 1;
	return map;
}

static int zxgq_probe(struct platform_device *pdev)
{
	int rc;
	size_t rsz;
	struct resource *res;
	struct zocl_xgq_cu *zxgq = devm_kzalloc(&pdev->dev, sizeof(*zxgq), GFP_KERNEL);

	if (!zxgq)
		return -ENOMEM;
	zxgq->zxc_pdev = pdev;

	res = platform_get_resource(ZXGQ2PDEV(zxgq), IORESOURCE_IRQ, 0);
	if (!res) {
		zxgq_err(zxgq, "failed to find CU XGQ IRQ"); 
		return -EINVAL;
	}
	zxgq->zxc_irq = res->start;
	zxgq_info(zxgq, "CU XGQ IRQ: %d", zxgq->zxc_irq); 

	zxgq->zxc_sq_prod = zxgq_map_res(zxgq, ZCX_RES_SQ_PROD, NULL);
	if (!zxgq->zxc_sq_prod)
		return -EINVAL;
	zxgq->zxc_cq_prod = zxgq_map_res(zxgq, ZCX_RES_CQ_PROD, NULL);
	if (!zxgq->zxc_sq_prod)
		return -EINVAL;
	zxgq->zxc_cq_prod_int = zxgq_map_res(zxgq, ZCX_RES_CQ_PROD_INT, NULL);
	if (!zxgq->zxc_cq_prod_int)
		return -EINVAL;
	zxgq->zxc_ring = zxgq_map_res(zxgq, ZCX_RES_RING, &rsz);
	if (!zxgq->zxc_ring)
		return -EINVAL;

	/* Init CU XGQ */
	rc = xgq_alloc(&zxgq->zxc_xgq, XGQ_SERVER, 0, (u64)(uintptr_t)zxgq->zxc_ring, &rsz,
		       CU_XGQ_SLOT_SZ, (u64)(uintptr_t)zxgq->zxc_sq_prod,
		       (u64)(uintptr_t)zxgq->zxc_cq_prod);
	if (rc) {
		zxgq_err(zxgq, "failed to alloc CU XGQ: %d", rc);
		return rc;
	}

	platform_set_drvdata(pdev, zxgq);
	return 0;
}

static int zxgq_remove(struct platform_device *pdev)
{
	struct zocl_xgq_cu *zxgq = platform_get_drvdata(pdev);

	zxgq_info(zxgq, "Removing %s", ZXGQ_NAME);
	return 0;
}

static const struct platform_device_id zocl_xgq_cu_id_match[] = {
	{ CU_XGQ_DEV_NAME, 0 },
	{ /* end of table */ },
};

struct platform_driver zocl_xgq_cu_driver = {
	.driver = {
		.name = ZXGQ_NAME,
	},
	.probe  = zxgq_probe,
	.remove = zxgq_remove,
	.id_table = zocl_xgq_cu_id_match,
};
