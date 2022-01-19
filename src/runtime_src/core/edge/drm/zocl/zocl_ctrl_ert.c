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
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include "zocl_drv.h"
#include "xrt_cu.h"
#include "xgq_cmd_ert.h"
#include "xgq_impl.h"
#include "zocl_util.h"
#include "zocl_xgq.h"
#include "zocl_cu_xgq.h"
#include "zocl_ert_intc.h"

/* ERT XGQ driver name. */
#define ZERT_NAME "zocl_ctrl_ert"

#define ZERT2PDEV(zert)			((zert)->zce_pdev)
#define ZERT2DEV(zert)			(&ZERT2PDEV(zert)->dev)
#define zert_err(zert, fmt, args...)	zocl_err(ZERT2DEV(zert), fmt"\n", ##args)
#define zert_info(zert, fmt, args...)	zocl_info(ZERT2DEV(zert), fmt"\n", ##args)
#define zert_dbg(zert, fmt, args...)	zocl_dbg(ZERT2DEV(zert), fmt"\n", ##args)

/* Legacy ERT resources. */
#define ZERT_HW_RES     		0
#define ZERT_CQ_RES     		1

#define ZERT_CU_DMA_ENABLE		0x18
/*
 * The CU_STATUS is for CU to notify PS about completion of a command.
 */
#define ZERT_CU_STATUS_REG		0x40
/*
 * The CQ_STATUS is for host to notify PS about arriving of a new command.
 */
#define ZERT_CQ_STATUS_REG		0x54
/*
 * This STATUS REGISTER is for communicating completed CQ slot indices
 * MicroBlaze write, host reads.  MB(W) / HOST(COR). In total, there are
 * four of them contiguously.
 */
#define ZERT_CMD_STATUS_REG		0x0
/*
 * Enable global interrupts from MB to HOST on command completion.
 * When enabled writing to STATUS_REGISTER causes an interrupt in HOST.
 * MB(W)
 */
#define ZERT_HOST_INT_ENABLE		0x100

#define ZERT_XGQ_SQ_REG			0X0
#define ZERT_XGQ_SQ_INT_CONF_REG	0XC
#define ZERT_XGQ_CQ_REG			0X100

#define ERT_CQ_IRQ			0
#define ERT_CU_IRQ			1

/*
 * CQ format version 1.0:
 * First word on CQ is version number, followed by ctrl XGQ, which may go up to 1.5k.
 */
#define ZERT_CQ_FMT_VER			0x10000
#define CTRL_XGQ_SLOT_SIZE		512
#define MAX_CTRL_XGQ_SIZE		(1024 + 512)
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

/* Max 128 PL kernels and 128 PS kernels. */
#define ZERT_MAX_NUM_CU			256

/* For now, hard-coded 32 CU XGQs so we only use one interrupt line. */
#define ZERT_MAX_NUM_CU_XGQ		32
/*
 * Num of slots for each CU XGQ. Adding more slots will just result in
 * more cmd sitting on HW which will not help with performance, but only
 * waste HW resources. Using less slots may impact performance.
 */
#define ZERT_CU_XGQ_MAX_SLOTS		128
#define ZERT_CU_XGQ_MIN_SLOTS		4

/* Config for each CU sub-dev. */
#define ZERT_INVALID_XGQ_ID	((u32)-1)
struct zocl_ctrl_ert_cu {
	u32			zcec_xgq_idx;
	struct platform_device	*zcec_pdev;
};

/* Config for each CU XGQ sub-dev. */
struct zocl_ctrl_ert_cu_xgq {
	u32			zcecx_irq;
	struct platform_device	*zcecx_intc_pdev;

	// for XGQ IP access
	resource_size_t		zcecx_xgq_reg;
	// for triggering intr to host, if the write to tail pointer does not
	resource_size_t		zcecx_cq_int_reg;

	resource_size_t		zcecx_ring;
	resource_size_t		zcecx_slot_size;
	resource_size_t		zcecx_ring_size;
	bool			zcecx_echo_mode;
	struct platform_device	*zcecx_pdev;
};

/* Config for INTC sub-dev. */
struct zocl_ctrl_ert_intc {
	size_t			zcei_num_irqs;
	u32			*zcei_irqs;
	resource_size_t		zcei_status_reg;
	struct platform_device	*zcei_pdev;
};

struct zocl_ctrl_ert {
	struct platform_device	*zce_pdev;

	struct zocl_ert_cq __iomem *zce_cq;
	size_t			zce_cq_size;
	u64			zce_cq_start;
	u64			zce_cu_xgq_ring_start;
	size_t			zce_cu_xgq_ring_size;

	void			*zce_ctrl_xgq_hdl;

	size_t			zce_num_cu_xgqs;
	struct zocl_ctrl_ert_cu_xgq *zce_cu_xgqs;

	size_t			zce_num_cus;
	struct zocl_ctrl_ert_cu *zce_cus;
	resource_size_t		zce_max_cu_size;

	struct zocl_ctrl_ert_intc zce_xgq_intc;
	struct zocl_ctrl_ert_intc zce_cu_intc;

	bool			zce_config_completed;
	bool			zce_echo_mode;
};

static void zert_cmd_handler(struct platform_device *pdev, struct xgq_cmd_sq_hdr *cmd);

static inline void reg_write(void __iomem *base, u64 off, u32 val)
{
	iowrite32(val, base + off);
}

static inline u32 reg_read(void __iomem *base, u64 off)
{
	return ioread32(base + off);
}

static void __iomem *zert_map_res(struct zocl_ctrl_ert *zert,
				  struct resource *res, u64 *startp, size_t *szp)
{
	void __iomem *map = devm_ioremap(ZERT2DEV(zert), res->start, res->end - res->start + 1);

	if (IS_ERR(map)) {
		zert_err(zert, "Failed to map CTRL ERT resource: %ld", PTR_ERR(map));
		return NULL;
	}

	if (startp)
		*startp = res->start;
	if (szp)
		*szp = res->end - res->start + 1;
	return map;
}

static void __iomem *zert_map_res_by_id(struct zocl_ctrl_ert *zert, int id,
					u64 *startp, size_t *szp)
{
	struct resource *res = platform_get_resource(ZERT2PDEV(zert), IORESOURCE_MEM, id);

	if (!res) {
		zert_err(zert, "failed to find CTRL ERT resource ID (%d)", id);
		return NULL;
	}
	zert_info(zert, "Found CTRL ERT resource (%d): %pR", id, res);

	return zert_map_res(zert, res, startp, szp);
}

static void __iomem *zert_map_res_by_name(struct zocl_ctrl_ert *zert, const char *name,
					  u64 *startp, size_t *szp)
{
	int ret = -EINVAL;
	struct resource res = {};
	struct device_node *np = NULL;

	np = of_parse_phandle(ZERT2PDEV(zert)->dev.of_node, name, 0);
	if (np)
		ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		zert_err(zert, "failed to find CTRL ERT resource (%s): %d", name, ret);
		return NULL;
	}
	zert_info(zert, "Found CTRL ERT resource (%s): %pR", name, &res);

	return zert_map_res(zert, &res, startp, szp);
}

static void fill_irq_res(struct resource *res, u32 irq, char *name)
{
	res->start = irq;
	res->end = irq;
	res->flags = IORESOURCE_IRQ;
	res->name = name;
}

static void fill_iomem_res(struct resource *res, resource_size_t start,
			   resource_size_t size, char *name)
{
	res->start = start;
	res->end = start + size - 1;
	res->flags = IORESOURCE_MEM;
	res->name = name;
}

static void fill_reg_res(struct resource *res, resource_size_t start, char *name)
{
	fill_iomem_res(res, start, sizeof(u32), name);
}

static void zert_destroy_subdev(struct platform_device *pdev)
{
	platform_device_del(pdev);
	platform_device_put(pdev);
}

static int zert_create_subdev(struct zocl_ctrl_ert *zert, char *devname,
			      struct resource *res, size_t nres,
			      void *info, size_t info_size,
			      struct platform_device **pdevp)
{
	struct platform_device *pldev;
	int ret;

	pldev = platform_device_alloc(devname, PLATFORM_DEVID_AUTO);
	if (!pldev) {
		zert_err(zert, "Failed to alloc %s device", devname);
		return -ENOMEM;
	}

	ret = platform_device_add_resources(pldev, res, nres);
	if (ret) {
		zert_err(zert, "Failed to add resource for %s device", devname);
		goto err;
	}

	if (info) {
		ret = platform_device_add_data(pldev, info, info_size);
		if (ret) {
			zert_err(zert, "Failed to add data for %s device", devname);
			goto err;
		}
	}

	pldev->dev.parent = ZERT2DEV(zert);

	ret = platform_device_add(pldev);
	if (ret) {
		zert_err(zert, "Failed to create %s device", devname);
		goto err;
	}

	ret = device_attach(&pldev->dev);
	if (ret != 1) {
		ret = -EINVAL;
		zert_err(zert, "Failed to attach driver to %s device", devname);
		goto err1;
	}

	*pdevp = pldev;

	return 0;

err1:
	platform_device_del(pldev);
err:
	platform_device_put(pldev);
	return ret;
}

static void cu_conf2info(struct xgq_cmd_config_cu *conf, struct xrt_cu_info *info)
{
	char *kname_p = conf->name;

	memset(info, 0, sizeof(*info));
	info->num_res = 1;
	info->addr = conf->haddr;
	info->addr <<= 32;
	info->addr |= conf->laddr;
	info->size = conf->map_size;
	info->intr_enable = 0;
	info->intr_id = 0;
	info->protocol = conf->ip_ctrl;
	if (info->protocol == CTRL_FA)
		info->model = XCU_FA;
	else
		info->model = XCU_HLS;
	info->inst_idx = conf->cu_idx;
	strcpy(info->kname, strsep(&kname_p, ":"));
	strcpy(info->iname, strsep(&kname_p, ":"));
}

static int zert_create_cu(struct zocl_ctrl_ert *zert, struct xgq_cmd_config_cu *conf)
{
	int ret;
	struct xrt_cu_info info;
	u32 cuidx = conf->cu_idx;

	if (cuidx >= zert->zce_num_cus) {
		zert_err(zert, "CU index (%d) is out of range", cuidx);
		return -EINVAL;
	}

	BUG_ON(zert->zce_cus[cuidx].zcec_pdev);
	cu_conf2info(conf, &info);
	ret = subdev_create_cu(ZERT2DEV(zert), &info, &zert->zce_cus[cuidx].zcec_pdev);
	if (ret) {
		zert_err(zert, "Failed to create CU.%d device", cuidx);
		return ret;
	}

	if (conf->payload_size > zert->zce_max_cu_size)
		zert->zce_max_cu_size = conf->payload_size;

	return 0;
}

static void zert_init_cus(struct zocl_ctrl_ert *zert)
{
	u32 i;
	struct zocl_ctrl_ert_cu *cu = &zert->zce_cus[0];

	for (i = 0; i < zert->zce_num_cus; i++, cu++) {
		cu->zcec_pdev = NULL;
		cu->zcec_xgq_idx = ZERT_INVALID_XGQ_ID;
	}
}

static int zert_validate_cus(struct zocl_ctrl_ert *zert)
{
	u32 i;
	struct zocl_ctrl_ert_cu *cu = &zert->zce_cus[0];

	for (i = 0; i < zert->zce_num_cus; i++, cu++) {
		if ((cu->zcec_pdev == NULL) && (i < zert->zce_num_cus)) {
			zert_err(zert, "Some CUs are not configured properly.");
			return -EINVAL;
		}
		if (cu->zcec_pdev && (i >= zert->zce_num_cus)) {
			zert_err(zert, "CU index out of range");
			return -EINVAL;
		}
	}

	return 0;
}

static void zert_unassign_cu_xgqs(struct zocl_ctrl_ert *zert)
{
	int ret;
	u32 i;
	u32 idx = 0;
	struct zocl_ctrl_ert_cu *cu = &zert->zce_cus[0];

	for (i = 0; i < zert->zce_num_cus; i++, cu++) {
		idx = cu->zcec_xgq_idx;
		if (idx != ZERT_INVALID_XGQ_ID) {
			ret = zcu_xgq_unassign_cu(zert->zce_cu_xgqs[idx].zcecx_pdev, i);
			if (ret)
				zert_err(zert, "Failed to unassign CU %d to XGQ %d", i, idx);
		}
		cu->zcec_xgq_idx = ZERT_INVALID_XGQ_ID;
	}
}

static void zert_destroy_cus(struct zocl_ctrl_ert *zert)
{
	u32 i;
	struct zocl_ctrl_ert_cu *cu = &zert->zce_cus[0];

	zert_unassign_cu_xgqs(zert);

	for (i = 0; i < zert->zce_num_cus; i++, cu++) {
		if (cu->zcec_pdev) {
			zert_destroy_subdev(cu->zcec_pdev);
			cu->zcec_pdev = NULL;
			BUG_ON(cu->zcec_xgq_idx != ZERT_INVALID_XGQ_ID);
		}
	}
	zert->zce_num_cus = 0;
	zert->zce_max_cu_size = 0;
	kfree(zert->zce_cus);
	zert->zce_cus = NULL;
}

static int zert_create_cu_xgq(struct zocl_ctrl_ert *zert, struct zocl_ctrl_ert_cu_xgq *info)
{
	int ret, i;
	struct resource res[10] = {};
	struct zocl_cu_xgq_info zci = {};

	i = 0;
	fill_irq_res(&res[i++], info->zcecx_irq, ZCX_RES_IRQ);
	// Using XGQ IP
	if (info->zcecx_xgq_reg) {
		void __iomem *xgq;

		/* HACK:
		 * 1. enable CQ intr to host, should be removed when intr is always on.
		 * 2. reset CQ to 0, should be replaced by resetting XGQ IP.
		 */
		xgq = devm_ioremap(ZERT2DEV(zert), info->zcecx_xgq_reg, PAGE_SIZE);
		reg_write(xgq, ZERT_XGQ_SQ_INT_CONF_REG, 0x1);
		reg_write(xgq, ZERT_XGQ_CQ_REG, 0);
		devm_iounmap(ZERT2DEV(zert), xgq);

		fill_reg_res(&res[i++], info->zcecx_xgq_reg + ZERT_XGQ_SQ_REG, ZCX_RES_SQ_PROD);
		fill_reg_res(&res[i++], info->zcecx_xgq_reg + ZERT_XGQ_CQ_REG, ZCX_RES_CQ_PROD);
	}
	// Legacy CQ status
	if (info->zcecx_cq_int_reg)
		fill_reg_res(&res[i++], info->zcecx_cq_int_reg, ZCX_RES_CQ_PROD_INT);
	fill_iomem_res(&res[i++], info->zcecx_ring, info->zcecx_ring_size, ZCX_RES_RING);

	zci.zcxi_slot_size = info->zcecx_slot_size;
	zci.zcxi_echo_mode = info->zcecx_echo_mode;
	zci.zcxi_intc_pdev = info->zcecx_intc_pdev;

	ret = zert_create_subdev(zert, CU_XGQ_DEV_NAME, res, i, &zci, sizeof(zci),
				 &info->zcecx_pdev);
	if (ret)
		zert_err(zert, "Failed to create %s.%d device", CU_XGQ_DEV_NAME, info->zcecx_irq);

	return ret;
}

static void zert_assign_cu_xgqs(struct zocl_ctrl_ert *zert)
{
	int ret;
	u32 i;
	u32 idx;
	struct platform_device *xgqpdev = NULL;
	u32 xgqidx = 0;
	struct zocl_ctrl_ert_cu *cu = &zert->zce_cus[0];

	for (i = 0; i < zert->zce_num_cu_xgqs; i++) {
		if (zert->zce_cu_xgqs[i].zcecx_pdev)
			break;
	}
	if (i == zert->zce_num_cu_xgqs) {
		zert_err(zert, "No XGQ is available");
		return;
	}

	for (i = 0; i < zert->zce_num_cus; i++, cu++) {
		if (cu->zcec_pdev) {
			/* Find next enabled XGQ, we are guaranteed to have one. */
			while (xgqpdev == NULL) {
				idx = xgqidx++ % zert->zce_num_cu_xgqs;
				xgqpdev = zert->zce_cu_xgqs[idx].zcecx_pdev;
			}

			BUG_ON(cu->zcec_xgq_idx != ZERT_INVALID_XGQ_ID);
			ret = zcu_xgq_assign_cu(xgqpdev, i);
			if (ret)
				zert_err(zert, "Failed to assign CU %d to XGQ %d", i, idx);
			else
				cu->zcec_xgq_idx = idx;
		}
	}
}

static int zert_create_cu_xgqs(struct zocl_ctrl_ert *zert)
{
	int rc, i;
	const u32 alignment = sizeof(u32);
	struct zocl_ctrl_ert_cu_xgq *xcu;
	size_t slot_sz = zert->zce_max_cu_size;
	size_t slot_num = ZERT_CU_XGQ_MAX_SLOTS;
	size_t xgq_ring_size;
	size_t nxgqs;

	BUG_ON(zert->zce_cu_xgq_ring_start % alignment);
	BUG_ON(zert->zce_cu_xgq_ring_size % alignment);

	/* No need to create CU XGQ if there is no CU configured. */
	if (zert->zce_num_cus == 0) {
		zert_info(zert, "No CU is configured, skip creating XGQs");
		return 0;
	}

	/* Find out the appropriate number of slots. */
	xgq_ring_size = xgq_ring_len(slot_num, slot_sz);
	while (slot_num && xgq_ring_size > zert->zce_cu_xgq_ring_size) {
		slot_num >>= 1;
		xgq_ring_size = xgq_ring_len(slot_num, slot_sz);
	}
	if (slot_num < ZERT_CU_XGQ_MIN_SLOTS) {
		zert_err(zert, "XGQ slot size is too big: %ld", slot_sz);
		return -E2BIG;
	}

	/* Find out the appropriate number of XGQs to enable. */
	nxgqs = zert->zce_cu_xgq_ring_size / xgq_ring_size;
	if (nxgqs > zert->zce_num_cus)
		nxgqs = zert->zce_num_cus;
	if (nxgqs > zert->zce_num_cu_xgqs)
		nxgqs = zert->zce_num_cu_xgqs;

	zert_info(zert, "Creating %ld XGQs (slot size 0x%lx) for %ld CUs",
		  nxgqs, slot_sz, zert->zce_num_cus);

	/* Enable first nxgqs number of CU XGQs. */
	for (i = 0; i < nxgqs; i++) {
		xcu = &zert->zce_cu_xgqs[i];
		xcu->zcecx_ring = zert->zce_cu_xgq_ring_start + xgq_ring_size * i;
		xcu->zcecx_ring_size = xgq_ring_size;
		xcu->zcecx_slot_size = slot_sz; /* All CU XGQs use the same slot size. */
		xcu->zcecx_echo_mode = zert->zce_echo_mode;

		/* intc for receiving interrupt from host. */
		xcu->zcecx_intc_pdev = zert->zce_xgq_intc.zcei_pdev;

		rc = zert_create_cu_xgq(zert, xcu);
		if (rc) {
			zert_err(zert, "failed to alloc CU XGQ %d: %d", i, rc);
			break;
		}
	}

	zert_assign_cu_xgqs(zert);
	return rc;
}

static void zert_destroy_cu_xgqs(struct zocl_ctrl_ert *zert)
{
	int i;
	struct zocl_ctrl_ert_cu_xgq *xcu;

	if (zert->zce_num_cu_xgqs == 0)
		return;

	for (i = 0; i < zert->zce_num_cu_xgqs; i++) {
		xcu = &zert->zce_cu_xgqs[i];
		if (xcu->zcecx_pdev)
			zert_destroy_subdev(xcu->zcecx_pdev);
		zert->zce_cu_xgqs[i].zcecx_pdev = NULL;
	}
}

static int zert_create_intc(struct zocl_ctrl_ert *zert,
			    struct zocl_ctrl_ert_intc *info, char *dev_name)
{
	int ret = 0, i = 0;
	struct resource res[10] = {};

	for (i = 0; i < info->zcei_num_irqs; i++)
		fill_irq_res(&res[i], info->zcei_irqs[i], ZEI_RES_IRQ);

	fill_iomem_res(&res[i++], info->zcei_status_reg, sizeof(struct zocl_ert_intc_status_reg),
		       ZEI_RES_STATUS);

	ret = zert_create_subdev(zert, dev_name, res, i, NULL, 0, &info->zcei_pdev);
	if (ret)
		zert_err(zert, "Failed to add resource for %s device", dev_name);

	return ret;
}

static void zert_destroy_intc(struct zocl_ctrl_ert_intc *info)
{
	if (info->zcei_pdev) {
		zert_destroy_subdev(info->zcei_pdev);
		info->zcei_pdev = NULL;
	}
	if (info->zcei_irqs) {
		kfree(info->zcei_irqs);
		info->zcei_irqs = NULL;
	}
}

static int zert_versal_init(struct zocl_ctrl_ert *zert)
{
	int i;
	int ret;
	const char *cq_res_name = "xlnx,xgq_buffer";
	const char *xgq_res_name = "xlnx,xgq_device";
	struct device_node *np = NULL;
	struct zocl_ctrl_ert_cu_xgq *cuxgq = NULL;
	struct resource res = {};

	/* Obtain shared ring buffer. */
	zert->zce_cq = zert_map_res_by_name(zert, cq_res_name, &zert->zce_cq_start,
					    &zert->zce_cq_size);
	if (!zert->zce_cq) {
		zert_err(zert, "failed to find ERT command queue");
		return -EINVAL;
	}

	/* Obtain all CU XGQs. */
	ret = of_count_phandle_with_args(ZERT2DEV(zert)->of_node, xgq_res_name, NULL);
	if (ret <= 0) {
		zert_err(zert, "failed to find CU XGQs");
		return -EINVAL;
	}
	zert->zce_num_cu_xgqs = ret;
	zert->zce_cu_xgqs = kzalloc(sizeof(struct zocl_ctrl_ert_cu_xgq) * ret, GFP_KERNEL);
	if (!zert->zce_cu_xgqs)
		return -ENOMEM;

	for (i = 0; i < zert->zce_num_cu_xgqs; i++) {
		np = of_parse_phandle(ZERT2DEV(zert)->of_node, xgq_res_name, i);
		if (!np) {
			zert_err(zert, "failed to find node for CU XGQ %d", i);
			continue;
		}
		ret = of_address_to_resource(np, 0, &res);
		if (ret) {
			zert_err(zert, "failed to find res for CU XGQ %d: %d", i, ret);
			continue;
		}
		zert_err(zert, "Found CU XGQ @ %pR", &res);

		cuxgq = &zert->zce_cu_xgqs[i];
		cuxgq->zcecx_irq = of_irq_get(np, 0);
		cuxgq->zcecx_xgq_reg = res.start;
		/* Write to tail pointer will trigger interrupt. */
		cuxgq->zcecx_cq_int_reg = 0;
	}

	/* Bring up XGQ INTC. */
	zert->zce_xgq_intc.zcei_num_irqs = zert->zce_num_cu_xgqs;
	zert->zce_xgq_intc.zcei_irqs = kzalloc(sizeof(u32) * zert->zce_xgq_intc.zcei_num_irqs,
					       GFP_KERNEL);
	if (zert->zce_xgq_intc.zcei_irqs) {
		for (i = 0; i < zert->zce_num_cu_xgqs; i++)
			zert->zce_xgq_intc.zcei_irqs[i] = zert->zce_cu_xgqs[i].zcecx_irq;
		zert_create_intc(zert, &zert->zce_xgq_intc, ERT_XGQ_INTC_DEV_NAME);
	} else {
		zert_err(zert, "Failed to alloc irq array for intc device");
	}

	/* TODO: Bringup INTC sub-dev to handle interrupts for all CUs. */

	return 0;
}

static int zert_mpsoc_init(struct zocl_ctrl_ert *zert)
{
	void __iomem *regs;
	struct zocl_ctrl_ert_intc *intc;
	u64 reg_start;
	/* We support max 32 XGQs since we have only one interrupt line from host. */
	const int max_xgq = 32;
	size_t i;

	/* Obtain CSR and CQ status registers. */
	regs = zert_map_res_by_id(zert, ZERT_HW_RES, &reg_start, NULL);
	if (!regs) {
		zert_err(zert, "failed to find ERT registers");
		return -EINVAL;
	}
	/* Obtain shared ring buffer. */
	zert->zce_cq = zert_map_res_by_id(zert, ZERT_CQ_RES,
					  &zert->zce_cq_start, &zert->zce_cq_size);
	if (!zert->zce_cq) {
		zert_err(zert, "failed to find ERT command queue");
		return -EINVAL;
	}

	/* Disable CUDMA, always. */
	reg_write(regs, ZERT_CU_DMA_ENABLE, 0);
	/* Enable host intr, always. */
	reg_write(regs, ZERT_HOST_INT_ENABLE, 1);
	/* Done with registers. */
	devm_iounmap(ZERT2DEV(zert), regs);

	/* Obtain all XGQs. */
	zert->zce_num_cu_xgqs = max_xgq;
	zert->zce_cu_xgqs = kzalloc(sizeof(struct zocl_ctrl_ert_cu_xgq) * zert->zce_num_cu_xgqs,
				    GFP_KERNEL);
	if (!zert->zce_cu_xgqs)
		return -ENOMEM;
	for (i = 0; i < zert->zce_num_cu_xgqs; i++) {
		struct zocl_ctrl_ert_cu_xgq *cuxgq = &zert->zce_cu_xgqs[i];
		cuxgq->zcecx_irq = i;
		cuxgq->zcecx_xgq_reg = 0;
		cuxgq->zcecx_cq_int_reg = reg_start + ZERT_CMD_STATUS_REG;
	}

	/* Bringup INTC sub-dev to handle interrupts for all CU XGQs. */
	intc = &zert->zce_xgq_intc;
	intc->zcei_num_irqs = 1;
	intc->zcei_irqs = kzalloc(sizeof(u32) * intc->zcei_num_irqs, GFP_KERNEL);
	if (!intc->zcei_irqs) {
		zert_err(zert, "Failed to alloc irq array for intc device");
	} else {
		intc->zcei_irqs[0] = platform_get_irq(ZERT2PDEV(zert), ERT_CQ_IRQ);
		intc->zcei_status_reg = reg_start + ZERT_CQ_STATUS_REG;
		zert_create_intc(zert, intc, ERT_CQ_INTC_DEV_NAME);
	}

	return 0;
}

static const struct zocl_ctrl_ert_drvdata {
	int (*zced_dev_init)(struct zocl_ctrl_ert *zert);
}
zocl_ctrl_ert_drvdata_mpsoc = { zert_mpsoc_init },
zocl_ctrl_ert_drvdata_versal = { zert_versal_init };

static const struct of_device_id zocl_ctrl_ert_of_match[] = {
	{ .compatible = "xlnx,embedded_sched", .data = &zocl_ctrl_ert_drvdata_mpsoc },
	{ .compatible = "xlnx,embedded_sched_versal", .data = &zocl_ctrl_ert_drvdata_versal },
	{ /* end of table */ },
};

static int zert_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *id = NULL;
	struct zocl_ctrl_ert_drvdata *data = NULL;
	struct zocl_ctrl_ert *zert = devm_kzalloc(&pdev->dev, sizeof(*zert), GFP_KERNEL);
	struct zocl_xgq_init_args xgq_arg = {};

	if (!zert)
		return -ENOMEM;
	zert->zce_pdev = pdev;
	platform_set_drvdata(pdev, zert);

	id = of_match_node(zocl_ctrl_ert_of_match, pdev->dev.of_node);
	data = (struct zocl_ctrl_ert_drvdata *)id->data;
	ret = data->zced_dev_init(zert);
	if (ret)
		return ret;

	/* Init CQ. */
	zert->zce_cu_xgq_ring_start = zert->zce_cq_start + sizeof(struct zocl_ert_cq);
	zert->zce_cu_xgq_ring_size = zert->zce_cq_size - sizeof(struct zocl_ert_cq);
	/* Init cmd queue */
	memset_io(zert->zce_cq, 0, zert->zce_cq_size);
	/* Remap CQ to just what we need. The rest will be passed onto CU XGQ drivers. */
	devm_iounmap(ZERT2DEV(zert), zert->zce_cq);
	zert->zce_cq = devm_ioremap(ZERT2DEV(zert), zert->zce_cq_start, sizeof(struct zocl_ert_cq));
	/* Advertise CQ version */
	iowrite32(ZERT_CQ_FMT_VER, &zert->zce_cq->zec_header.zcx_ver);

	/* Bringup CTRL XGQ last */
	xgq_arg.zxia_pdev = ZERT2PDEV(zert);
	xgq_arg.zxia_ring = zert->zce_cq->zec_header.zcx_ctrl_ring;
	xgq_arg.zxia_ring_size = sizeof(struct zocl_ert_cq) - sizeof(struct zocl_ert_cq_header);
	xgq_arg.zxia_ring_slot_size = CTRL_XGQ_SLOT_SIZE;
	xgq_arg.zxia_cmd_handler = zert_cmd_handler;
	zert->zce_ctrl_xgq_hdl = zxgq_init(&xgq_arg);
	if (!zert->zce_ctrl_xgq_hdl)
		zert_err(zert, "failed to initialize CTRL XGQ");

	return 0;
}

static int zert_remove(struct platform_device *pdev)
{
	struct zocl_ctrl_ert *zert = platform_get_drvdata(pdev);

	zert_info(zert, "Removing %s", ZERT_NAME);

	if (zert->zce_ctrl_xgq_hdl)
		zxgq_fini(zert->zce_ctrl_xgq_hdl);

	zert_destroy_cus(zert);
#if 0
	zert_destroy_intc(&zert->zce_cu_intc);
#endif
	zert_destroy_cu_xgqs(zert);
	kfree(zert->zce_cu_xgqs);
	zert->zce_cu_xgqs = NULL;
	zert->zce_num_cu_xgqs = 0;

	zert_destroy_intc(&zert->zce_xgq_intc);

	return 0;
}

struct platform_driver zocl_ctrl_ert_driver = {
	.driver = {
		.name = ZERT_NAME,
		.of_match_table = zocl_ctrl_ert_of_match,
	},
	.probe  = zert_probe,
	.remove = zert_remove,
};


/*
 * Control commands are handled below.
 */

#define ZERT_CMD_HANDLER_VER_MAJOR	1
#define ZERT_CMD_HANDLER_VER_MINOR	0

typedef void (*cmd_handler)(struct zocl_ctrl_ert *zert, struct xgq_cmd_sq_hdr *cmd,
			    struct xgq_com_queue_entry *resp);

static void init_resp(struct xgq_com_queue_entry *resp, u16 cid, u32 rcode)
{
	memset(resp, 0, sizeof(*resp));
	resp->hdr.cid = cid;
	resp->hdr.cstate = XGQ_CMD_STATE_COMPLETED;
	resp->rcode = rcode;
}

static void zert_cmd_identify(struct zocl_ctrl_ert *zert, struct xgq_cmd_sq_hdr *cmd,
			      struct xgq_com_queue_entry *resp)
{
	struct xgq_cmd_resp_identify *r = (struct xgq_cmd_resp_identify *)resp;

	init_resp(resp, cmd->cid, 0);

	r->major = ZERT_CMD_HANDLER_VER_MAJOR;
	r->minor = ZERT_CMD_HANDLER_VER_MINOR;
}

static void zert_cmd_cfg_start(struct zocl_ctrl_ert *zert, struct xgq_cmd_sq_hdr *cmd,
			       struct xgq_com_queue_entry *resp)
{
	struct xgq_cmd_config_start *c = (struct xgq_cmd_config_start *)cmd;
	struct xgq_cmd_resp_config_start *r = (struct xgq_cmd_resp_config_start *)resp;
	struct zocl_ctrl_ert_cu *cus = NULL;

	if (ZERT_MAX_NUM_CU < c->num_cus) {
		zert_err(zert, "Configuring too many CUs: %d", c->num_cus);
		init_resp(resp, cmd->cid, -EINVAL);
		return;
	}
	cus = kzalloc(sizeof(struct zocl_ctrl_ert_cu) * c->num_cus, GFP_KERNEL);
	if (!cus) {
		init_resp(resp, cmd->cid, -ENOMEM);
		return;
	}

	zert_destroy_cus(zert);
	zert_destroy_cu_xgqs(zert);
	kds_reset(&zocl_get_zdev()->kds);
	zert->zce_config_completed = false;

	zert->zce_cus = cus;
	zert->zce_num_cus = c->num_cus;
	zert->zce_echo_mode = c->echo;
	zert_init_cus(zert);

	init_resp(resp, cmd->cid, 0);
	r->i2h = true;
	r->i2e = true;
	r->cui = false;
	r->ob = false;
}

static void zert_cmd_cfg_end(struct zocl_ctrl_ert *zert, struct xgq_cmd_sq_hdr *cmd,
			     struct xgq_com_queue_entry *resp)
{
	int rc;

	if (zert->zce_config_completed) {
		zert_err(zert, "ERT is not in config mode");
		init_resp(resp, cmd->cid, -EINVAL);
		return;
	}
	zert->zce_config_completed = true;

	zocl_get_zdev()->kds.cu_intr_cap = 1;
	zocl_get_zdev()->kds.cu_intr = 0;
	kds_cfg_update(&zocl_get_zdev()->kds);

	rc = zert_validate_cus(zert);
	if (!rc)
		rc = zert_create_cu_xgqs(zert);

	init_resp(resp, cmd->cid, rc);
}

static void zert_cmd_default_handler(struct zocl_ctrl_ert *zert, struct xgq_cmd_sq_hdr *cmd,
				     struct xgq_com_queue_entry *resp)
{
	zert_err(zert, "Unknown cmd: %d", cmd->opcode);
	init_resp(resp, cmd->cid, -ENOTTY);
}

static void zert_cmd_cfg_cu(struct zocl_ctrl_ert *zert, struct xgq_cmd_sq_hdr *cmd,
			    struct xgq_com_queue_entry *resp)
{
	int rc;
	struct xgq_cmd_config_cu *c = (struct xgq_cmd_config_cu *)cmd;

	rc = zert_create_cu(zert, c);
	init_resp(resp, cmd->cid, rc);
}

static void zert_cmd_query_cu(struct zocl_ctrl_ert *zert, struct xgq_cmd_sq_hdr *cmd,
			      struct xgq_com_queue_entry *resp)
{
	struct zocl_ctrl_ert_cu *cu;
	struct xgq_cmd_query_cu *c = (struct xgq_cmd_query_cu *)cmd;
	struct xgq_cmd_resp_query_cu *r = (struct xgq_cmd_resp_query_cu *)resp;

	if (zert->zce_num_cus <= c->cu_idx) {
		zert_err(zert, "CU index (%d) out of range", c->cu_idx);
		init_resp(resp, cmd->cid, -EINVAL);
		return;
	}

	cu = &zert->zce_cus[c->cu_idx];
	if (!cu->zcec_pdev) {
		zert_err(zert, "CU index (%d) not exists", c->cu_idx);
		init_resp(resp, cmd->cid, -ENOENT);
		return;
	}

	init_resp(resp, cmd->cid, 0);

	switch (c->type) {
	case XGQ_CMD_QUERY_CU_CONFIG:
		r->xgq_id = cu->zcec_xgq_idx;
		r->type = XGQ_CMD_RESP_QUERY_XGQ;
		r->offset = zert->zce_cu_xgqs[r->xgq_id].zcecx_ring - zert->zce_cq_start;
		break;
	case XGQ_CMD_QUERY_CU_STATUS:
		r->status = zocl_cu_get_status(cu->zcec_pdev);
		break;
	default:
		zert_err(zert, "Unknown query cu type: %d", c->type);
		init_resp(resp, cmd->cid, -EINVAL);
		break;
	}
}

struct zert_ops {
	u32 op;
	char *name;
	cmd_handler handler;
} zert_op_table[] = {
	{ XGQ_CMD_OP_CFG_START, "XGQ_CMD_OP_CFG_START", zert_cmd_cfg_start },
	{ XGQ_CMD_OP_CFG_END, "XGQ_CMD_OP_CFG_END", zert_cmd_cfg_end },
	{ XGQ_CMD_OP_CFG_CU, "XGQ_CMD_OP_CFG_CU", zert_cmd_cfg_cu },
	{ XGQ_CMD_OP_QUERY_CU, "XGQ_CMD_OP_QUERY_CU", zert_cmd_query_cu },
	{ XGQ_CMD_OP_IDENTIFY, "XGQ_CMD_OP_IDENTIFY", zert_cmd_identify }
};

static inline const struct zert_ops *opcode2op(u32 op)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(zert_op_table); i++) {
		if (zert_op_table[i].op == op)
			return &zert_op_table[i];
	}
	return NULL;
}

static inline const char *opcode2name(u32 opcode)
{
	const struct zert_ops *op = opcode2op(opcode);

	return op ? op->name : "UNKNOWN_CMD";
}

static inline cmd_handler opcode2handler(u32 opcode)
{
	const struct zert_ops *op = opcode2op(opcode);

	return op ? op->handler : NULL;
}

/* All control command is run-to-complete, no async process is supported. */
static void zert_cmd_handler(struct platform_device *pdev, struct xgq_cmd_sq_hdr *cmd)
{
	struct zocl_ctrl_ert *zert = platform_get_drvdata(pdev);
	u32 op = cmd->opcode;
	cmd_handler func = opcode2handler(op);
	struct xgq_com_queue_entry r = {};

	zert_info(zert, "%s received", opcode2name(op));
	if (func)
		func(zert, cmd, &r);
	else
		zert_cmd_default_handler(zert, cmd, &r);
	zxgq_send_response(zert->zce_ctrl_xgq_hdl, &r);
	kfree(cmd);
}

