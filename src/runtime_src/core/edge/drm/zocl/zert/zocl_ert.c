/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */
#include "ert.h"
#include "zocl_ert.h"
#include "zocl_util.h"
#include "zocl_mailbox.h"

#define ert_err(pdev, fmt, args...)  \
	zocl_err(&pdev->dev, fmt"\n", ##args)
#define ert_info(pdev, fmt, args...)  \
	zocl_info(&pdev->dev, fmt"\n", ##args)
#define ert_dbg(pdev, fmt, args...)  \
	zocl_dbg(&pdev->dev, fmt"\n", ##args)

struct ert_packet *
get_packet(struct ert_packet *packet, u32 idx, u32 size)
{
	char *bytes = (char *)packet;

	return (struct ert_packet *)(bytes + idx * size);
}

static void
ert_mpsoc_init(struct zocl_ert_dev *ert)
{
	return;
}

static void
ert_mpsoc_fini(struct zocl_ert_dev *ert)
{
	return;
}

static void
ert_mpsoc_config(struct zocl_ert_dev *ert, struct ert_configure_cmd *cfg)
{
	char *ert_hw = ert->hw_ioremap;

	/* Set slot size(4K) */
	iowrite32(cfg->slot_size / 4, ert_hw + ERT_CQ_SLOT_SIZE_REG);

	/* CU offset in shift value */
	iowrite32(cfg->cu_shift, ert_hw + ERT_CU_OFFSET_REG);

	/* Number of command slots */
	iowrite32(CQ_SIZE / cfg->slot_size, ert_hw + ERT_CQ_NUM_OF_SLOTS_REG);

	/* CU physical address */
	/* TODO: Think about how to make the address mapping correct */
	iowrite32(0x81800000/4, ert_hw + ERT_CU_BASE_ADDR_REG);

	/* Command queue physical address */
	iowrite32(0x80190000/4, ert_hw + ERT_CQ_BASE_ADDR_REG);

	/* Number of CUs */
	iowrite32(cfg->num_cus, ert_hw + ERT_NUM_OF_CU_REG);

	/* Enable/Disable CU_DMA module */
	iowrite32(cfg->cu_dma, ert_hw + ERT_CU_DMA_ENABLE);

	if (cfg->cq_int)
		iowrite32(1, ert_hw + ERT_CQ_STATUS_ENABLE);
	else
		iowrite32(0, ert_hw + ERT_CQ_STATUS_ENABLE);

	/* Enable device to host interrupts */
	iowrite32(1, ert_hw + ERT_HOST_INT_ENABLE);
}

static u32 cq_status[4];

static struct ert_packet *
ert_mpsoc_next(struct zocl_ert_dev *ert, struct ert_packet *pkg, int *idx_ret)
{
	char *ert_hw = ert->hw_ioremap;
	int slot_idx, mask_idx, slot_tmp;
	int slot_sz;
	int i, found;
	u64 slot_info;

	slot_sz = ioread32(ert_hw + ERT_CQ_SLOT_SIZE_REG) * 4;
	/* Calculate current slot index */
	if (pkg == NULL) {
		slot_idx = -1;
		/* ERT CQ STATUS register is W/COR
		 * Store all of the status
		 * registers when getting the first command.
		 */
		cq_status[0] = ioread32(ert_hw + ERT_CQ_STATUS_REG0);
		cq_status[1] = ioread32(ert_hw + ERT_CQ_STATUS_REG1);
		cq_status[2] = ioread32(ert_hw + ERT_CQ_STATUS_REG2);
		cq_status[3] = ioread32(ert_hw + ERT_CQ_STATUS_REG3);
	} else {
		/* ERT mode is only for 64 bits system */
		slot_info = ((u64)pkg - (u64)ert->cq_ioremap);
		do_div(slot_info, slot_sz);
		slot_idx = slot_info;
	}

	/* Get mask index and local slot index for the next commmand */
	slot_idx += 1;
	mask_idx = slot_idx >> 5;
	slot_tmp = slot_idx % 32;

	found = 0;
	for (i = mask_idx; i < 4; i++) {
		while (cq_status[i] && (slot_tmp < 32)) {
			if (cq_status[i] & (1 << slot_tmp)) {
				found = 1;
				cq_status[i] ^= (1 << slot_tmp);
				break;
			}
			slot_tmp++;
			slot_idx++;
		}
		if (found)
			break;

		slot_tmp = 0;
	}

	if (found) {
		*idx_ret = slot_idx;
		return get_packet(ert->cq_ioremap, slot_idx, slot_sz);
	} else {
		*idx_ret = -1;
		return NULL;
	}
}

static void ert_mpsoc_notify_host(struct zocl_ert_dev *ert, int slot_idx)
{
	u32 mask_idx = slot_idx >> 5;
	u32 csr_offset = ERT_STATUS_REG + (mask_idx<<2);
	u32 pos = slot_idx % 32;

	iowrite32(1 << pos, ert->hw_ioremap + csr_offset);
}

/* ert versal ops */
static void
ert_versal_init(struct zocl_ert_dev *ert)
{
	return;
}

static void
ert_versal_fini(struct zocl_ert_dev *ert)
{
	return;
}

static void
ert_versal_config(struct zocl_ert_dev *ert, struct ert_configure_cmd *cfg)
{
	return;
}

static struct ert_packet *
ert_versal_next(struct zocl_ert_dev *ert, struct ert_packet *pkg, int *idx_ret)
{
	return NULL;
}

static void
ert_versal_notify_host(struct zocl_ert_dev *ert, int slot_idx)
{
	struct mailbox mbx;
	u32 status = (u32)-1;
	mbx.mbx_regs = (struct mailbox_reg *)ert->hw_ioremap;

	while (1) {
		status = zocl_mailbox_status(&mbx);
		if (status == (u32)-1) {
			ert_err(ert->pdev, "mailbox error: 0x%x", status);
			break;
		}

		if ((status & MBX_STATUS_FULL) == 0) {
			zocl_mailbox_set(&mbx, slot_idx);
			break;
		}
	}
}

static void
update_cmd(struct zocl_ert_dev *ert, int idx, void *data, int sz)
{
	struct ert_packet *pkg;
	char *ert_hw = ert->hw_ioremap;
	int slot_sz;

	slot_sz = ioread32(ert_hw + ERT_CQ_SLOT_SIZE_REG) * 4;

	pkg = ert->cq_ioremap + idx * slot_sz;
	memcpy_toio(pkg->data, data, sz);
}

static struct zocl_ert_ops mpsoc_ops = {
	.init         = ert_mpsoc_init,
	.fini         = ert_mpsoc_fini,
	.config       = ert_mpsoc_config,
	.get_next_cmd = ert_mpsoc_next,
	.notify_host  = ert_mpsoc_notify_host,
	.update_cmd   = update_cmd,
};

static struct zocl_ert_ops versal_ops = {
	.init         = ert_versal_init,
	.fini         = ert_versal_fini,
	.config       = ert_versal_config,
	.get_next_cmd = ert_versal_next,
	.notify_host  = ert_versal_notify_host,
	.update_cmd   = update_cmd,
};

static const struct zocl_ert_info mpsoc_ert_info = {
	.ops   = &mpsoc_ops,
};

static const struct zocl_ert_info versal_ert_info = {
	.ops   = &versal_ops,
};

static const struct of_device_id zocl_ert_of_match[] = {
	{ .compatible = "xlnx,embedded_sched",
	  .data = &mpsoc_ert_info,
	},
	{ .compatible = "xlnx,embedded_sched_versal",
	  .data = &versal_ert_info,
	},
	{ /* end of table */ },
};

MODULE_DEVICE_TABLE(of, zocl_ert_of_match);

static int zocl_ert_probe(struct platform_device *pdev)
{
	struct zocl_ert_dev *ert;
	const struct of_device_id *id;
	struct resource *res;
	const struct zocl_ert_info *info;
	void __iomem *map;

	id = of_match_node(zocl_ert_of_match, pdev->dev.of_node);
	ert_info(pdev, "Probing for %s", id->compatible);

	ert = devm_kzalloc(&pdev->dev, sizeof(*ert), GFP_KERNEL);
	ert->pdev = pdev;

	info = of_device_get_match_data(&pdev->dev);
	ert->ops = info->ops;

	res = platform_get_resource(pdev, IORESOURCE_MEM, ZOCL_ERT_HW_RES);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		ert_err(pdev, "Failed to map ERT HW registers: %0lx",
				PTR_ERR(map));
		return PTR_ERR(map);
	}
	ert->hw_ioremap = map;
	ert_info(pdev, "IP(embedded_scheduler_hw) IO start %lx, end %lx",
	      (unsigned long)res->start, (unsigned long)res->end);

	res = platform_get_resource(pdev, IORESOURCE_MEM, ZOCL_ERT_CQ_RES);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		ert_err(pdev, "Failed to map Command Queue: %0lx",
				PTR_ERR(map));
		return PTR_ERR(map);
	}
	ert->cq_ioremap = map;
	ert_info(pdev, "Command Queue IO start %lx, end %lx",
		(unsigned long)res->start, (unsigned long)res->end);

	/* Initial CQ */
	memset_io(ert->cq_ioremap, 0, res->end - res->start + 1);

	ert->irq[ERT_CQ_IRQ] = platform_get_irq(pdev, ERT_CQ_IRQ);
	ert->irq[ERT_CU_IRQ] = platform_get_irq(pdev, ERT_CU_IRQ);
	ert_info(pdev, "CQ irq %d, CU irq %d", ert->irq[ERT_CQ_IRQ],
			ert->irq[ERT_CU_IRQ]);

	if (info->ops == NULL) {
		ert_err(pdev, "zocl ert probe failed due to ops has not been set");
		return -ENODEV;
	}
	platform_set_drvdata(pdev, ert);
	return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void zocl_ert_remove(struct platform_device *pdev) {
	ert_dbg(pdev, "Release resource");
}
#else
static int zocl_ert_remove(struct platform_device *pdev) {
	ert_dbg(pdev, "Release resource");
	return 0;
}
#endif
struct platform_driver zocl_ert_driver = {
	.driver = {
		.name = ZOCL_ERT_NAME,
		.of_match_table = zocl_ert_of_match,
	},
	.probe  = zocl_ert_probe,
	.remove = zocl_ert_remove,
};
