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

/* ERT XGQ driver name. */
#define ZERT_NAME "zocl_xgq_ert"

/* XGQ ERT device resources. */
#define ZERT_HW_RES     0
#define ZERT_CQ_RES     1

#define ZERT2PDEV(zert)			((zert)->zxe_pdev)
#define ZERT2DEV(zert)			(&ZERT2PDEV(zert)->dev)
#define zert_err(zert, fmt, args...)	zocl_err(ZERT2DEV(zert), fmt"\n", ##args)
#define zert_info(zert, fmt, args...)	zocl_info(ZERT2DEV(zert), fmt"\n", ##args)
#define zert_dbg(zert, fmt, args...)	zocl_dbg(ZERT2DEV(zert), fmt"\n", ##args)

#define ZERT_CU_DMA_ENABLE	0x18
/**
 * The CQ_STATUS_ENABLE (MB(W)/HW(R)) enables interrupts from HOST to
 * MB to indicate the presense of a new command in cmd queue.  The
 * slot index is written to the CQ_STATUS_REGISTER (HOST(W)/MB(R))
 */
#define ZERT_CQ_INT_ENABLE	0x54
#define ZERT_CQ_STATUS		0x58
/**
 * Enable global interrupts from MB to HOST on command completion.
 * When enabled writing to STATUS_REGISTER causes an interrupt in HOST.
 * MB(W)
 */
#define ZERT_HOST_INT_ENABLE	0x100

/**
 * This STATUS REGISTER is for communicating completed CQ slot indices
 * MicroBlaze write, host reads.  MB(W) / HOST(COR). In total, there are
 * four of them contiguously.
 */
#define ZERT_STATUS_REG		0x0

/*
 * CQ format version 1.0:
 * Ctrl XGQ always starts right after version of cmd queue and go up to 1.5k.
 */
#define ZERT_CQ_FMT_VER		(0x10000)
#define CTRL_XGQ_SLOT_SIZE	(512)
#define MAX_CTRL_XGQ_SIZE	(1024 + 512)
struct zocl_ert_cq_header {
	u32			zcx_ver;
	u32			zcx_ctrl_ring[0];
};

struct zocl_ert_cq {
	union {
		struct zocl_ert_cq_header	zec_header;
		char				zec_buf[MAX_CTRL_XGQ_SIZE];
	};
};

/* For now, hard-coded 4 CU XGQs. */
#define ZERT_NUM_CU_XGQ		4
#define MAX_CU_XGQ_SIZE		(32 * 1024)


struct zocl_xgq_ert_cu {
	u32			zxec_irq;
	u32			zxec_sq_reg;
	u32			zxec_cq_reg;
	u32			zxec_cq_int_reg;
	u32			zxec_ring;
	size_t			zxec_ring_size;
	struct platform_device	*zxec_pdev;
};

struct zocl_xgq_ert {
	struct platform_device	*zxe_pdev;
	struct zocl_ert_cq __iomem *zxe_cq;
	u32			zxe_irq;
	struct xgq		zxe_ctrl_xgq;
	struct zocl_xgq_ert_cu	zxe_cu_xgqs[ZERT_NUM_CU_XGQ];
};

static inline void reg_write(void __iomem *base, u64 off, u32 val)
{
	iowrite32(val, base + off);
}

static inline u32 reg_read(void __iomem *base, u64 off)
{
	return ioread32(base + off);
}

static void __iomem *zert_map_res(struct zocl_xgq_ert *zert, unsigned int id,
	u64 *startp, size_t *szp)
{
	struct resource *res;
	void __iomem *map;

	res = platform_get_resource(ZERT2PDEV(zert), IORESOURCE_MEM, id);
	if (!res) {
		zert_err(zert, "failed to find ERT resource (%d)", id);
		return NULL;
	}
	zert_info(zert, "ERT resource (%d) range: [0x%llx, 0x%llx]", id, res->start, res->end);

	map = devm_ioremap(ZERT2DEV(zert), res->start, res->end - res->start + 1);
	if (IS_ERR(map)) {
		zert_err(zert, "Failed to map ERT resource (%d): %ld", id, PTR_ERR(map));
		return NULL;
	}

	if (startp)
		*startp = res->start;
	if (szp)
		*szp = res->end - res->start + 1;
	return map;
}

static int zert_create_cu_xgq(struct zocl_xgq_ert *zert, struct zocl_xgq_ert_cu *info)
{
	struct platform_device *pldev;
	int ret;
	struct resource *cr;
	struct resource res[ZCX_NUM_RES] = {};
	u32 inst = info->zxec_irq;

	pldev = platform_device_alloc(CU_XGQ_DEV_NAME, PLATFORM_DEVID_AUTO);
	if (!pldev) {
		zert_err(zert, "Failed to alloc %s device", CU_XGQ_DEV_NAME);
		return -ENOMEM;
	}

	cr = &res[ZCX_RES_IRQ];
	cr->start = info->zxec_irq;
	cr->end = info->zxec_irq;
	cr->flags = IORESOURCE_IRQ;

	cr = &res[ZCX_RES_SQ_PROD];
	cr->start = info->zxec_sq_reg;
	cr->end = cr->start + sizeof(u32) - 1;
	cr->flags = IORESOURCE_MEM;

	cr = &res[ZCX_RES_CQ_PROD];
	cr->start = info->zxec_cq_reg;
	cr->end = cr->start + sizeof(u32) - 1;
	cr->flags = IORESOURCE_MEM;

	cr = &res[ZCX_RES_CQ_PROD_INT];
	cr->start = info->zxec_cq_int_reg;
	cr->end = cr->start + sizeof(u32) - 1;
	cr->flags = IORESOURCE_MEM;

	cr = &res[ZCX_RES_RING];
	cr->start = info->zxec_ring;
	cr->end = cr->start + info->zxec_ring_size - 1;
	cr->flags = IORESOURCE_MEM;

	ret = platform_device_add_resources(pldev, res, ZCX_NUM_RES);
	if (ret) {
		zert_err(zert, "Failed to add resource for %s[%d] device", CU_XGQ_DEV_NAME, inst);
		goto err;
	}

	pldev->dev.parent = ZERT2DEV(zert);

	ret = platform_device_add(pldev);
	if (ret) {
		zert_err(zert, "Failed to create %s[%d] device", CU_XGQ_DEV_NAME, inst);
		goto err;
	}

	ret = device_attach(&pldev->dev);
	if (ret != 1) {
		zert_err(zert, "Failed to attach driver to %s[%d] device", CU_XGQ_DEV_NAME, inst);
		goto err1;
	}

	info->zxec_pdev = pldev;

	return 0;

err1:
	platform_device_del(pldev);
err:
	platform_device_put(pldev);
	return ret;
}

static void zert_create_cu_xgqs(struct zocl_xgq_ert *zert, u64 ring_start, size_t ring_size,
			       u64 reg_start)
{
	int rc, i;
	const u32 alignment = sizeof(u32);
	struct zocl_xgq_ert_cu *xcu;
	size_t xgq_ring_size = (ring_size / ZERT_NUM_CU_XGQ) & ~(alignment - 1);

	BUG_ON(ring_start % alignment);

	if (xgq_ring_size > MAX_CU_XGQ_SIZE)
		xgq_ring_size = MAX_CU_XGQ_SIZE;

	for (i = 0; i < ZERT_NUM_CU_XGQ; i++) {
		xcu = &zert->zxe_cu_xgqs[i];
		/*
		 * Each CU XGQ begins with SQ producer pointer and CQ producer pointer
		 * followed by ring.
		 */
		xcu->zxec_sq_reg = ring_start + xgq_ring_size * i;
		xcu->zxec_cq_reg = xcu->zxec_sq_reg + sizeof(u32);
		xcu->zxec_ring = xcu->zxec_cq_reg + sizeof(u32);
		xcu->zxec_ring_size = xcu->zxec_sq_reg + xgq_ring_size - xcu->zxec_ring;

		/* irq for receiving interrupt from host. */
		xcu->zxec_irq = i;
		/* reg for triggering interrupt to host. */
		xcu->zxec_cq_int_reg = reg_start + ZERT_STATUS_REG + i * sizeof(u32);

		rc = zert_create_cu_xgq(zert, xcu);
		if (rc)
			zert_err(zert, "failed to alloc CU XGQ %d: %d", i, rc);
	}
}

static int zert_probe(struct platform_device *pdev)
{
	int rc;
	void __iomem *regs;
	u64 reg_start, cq_start;
	size_t ctrl_xgq_size, cq_size;
	struct zocl_xgq_ert *zert = devm_kzalloc(&pdev->dev, sizeof(*zert), GFP_KERNEL);

	if (!zert)
		return -ENOMEM;
	zert->zxe_pdev = pdev;

	/* Obtain CSR and CQ status registers. */
	regs = zert_map_res(zert, ZERT_HW_RES, &reg_start, NULL);
	if (!regs) {
		zert_err(zert, "failed to find ERT registers");
		return -EINVAL;
	}
	/* Obtain shared ring buffer. */
	zert->zxe_cq = zert_map_res(zert, ZERT_CQ_RES, &cq_start, &cq_size);
	if (!zert->zxe_cq) {
		zert_err(zert, "failed to find ERT command queue");
		return -EINVAL;
	}
	/* Remap CQ to just what we need. The rest will be passed onto CU XGQ drivers. */
	devm_iounmap(ZERT2DEV(zert), zert->zxe_cq);
	zert->zxe_cq = devm_ioremap(ZERT2DEV(zert), cq_start, sizeof(struct zocl_ert_cq));

	/* Disable CUDMA, always. */
	reg_write(regs, ZERT_CU_DMA_ENABLE, 0);
	/* Enable cmd queue intr, always. */
	reg_write(regs, ZERT_CQ_INT_ENABLE, 1);
	zert->zxe_irq = platform_get_irq(pdev, 0);
	/* Enable host intr, always. */
	reg_write(regs, ZERT_HOST_INT_ENABLE, 1);
	/* Done with registers. */
	devm_iounmap(ZERT2DEV(zert), regs);

	/* Init cmd queue */
	memset_io(zert->zxe_cq, 0, sizeof(struct zocl_ert_cq));
	/* Advertise CQ version */
	iowrite32(ZERT_CQ_FMT_VER, &zert->zxe_cq->zec_header.zcx_ver);

	/* Init CTRL XGQ */
	ctrl_xgq_size = sizeof(struct zocl_ert_cq) - sizeof(struct zocl_ert_cq_header);
	rc = xgq_alloc(&zert->zxe_ctrl_xgq, XGQ_SERVER | XGQ_IN_MEM_PROD, 0,
		       (u64)(uintptr_t)zert->zxe_cq->zec_header.zcx_ctrl_ring,
		       &ctrl_xgq_size, CTRL_XGQ_SLOT_SIZE, 0, 0);
	if (rc) {
		zert_err(zert, "failed to alloc CTRL XGQ: %d", rc);
		return rc;
	}

	/* Create CU XGQ subdevs. */
	zert_create_cu_xgqs(zert, cq_start + sizeof(struct zocl_ert_cq),
			    cq_size - sizeof(struct zocl_ert_cq), reg_start);

	platform_set_drvdata(pdev, zert);
	return 0;
}

static int zert_remove(struct platform_device *pdev)
{
	int i;
	struct zocl_xgq_ert *zert = platform_get_drvdata(pdev);

	zert_info(zert, "Removing %s", ZERT_NAME);

	for (i = 0; i < ZERT_NUM_CU_XGQ; i++) {
		struct zocl_xgq_ert_cu *xcu = &zert->zxe_cu_xgqs[i];

		if (xcu->zxec_pdev) {
			platform_device_del(xcu->zxec_pdev);
			platform_device_put(xcu->zxec_pdev);
		}
	}

	return 0;
}

static const struct of_device_id zocl_xgq_ert_of_match[] = {
	{ .compatible = "xlnx,embedded_sched",
	},
	{ .compatible = "xlnx,embedded_sched_versal",
	},
	{ /* end of table */ },
};

struct platform_driver zocl_xgq_ert_driver = {
	.driver = {
		.name = ZERT_NAME,
		.of_match_table = zocl_xgq_ert_of_match,
	},
	.probe  = zert_probe,
	.remove = zert_remove,
};
