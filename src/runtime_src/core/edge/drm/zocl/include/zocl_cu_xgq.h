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

#ifndef _ZOCL_CU_XGQ_H_
#define _ZOCL_CU_XGQ_H_

#include <linux/platform_device.h>

#define CU_XGQ_DEV_NAME		"ZOCL_CU_XGQ"

/*
 * Resources for one cu xgq device.
 */
/* XGQ IP reg. Use in-mem version, if not provided. */
#define	ZCX_RES_XGQ_IP		"ZOCL_CU_XGQ_IP"
/* Reg to trigger interrupt to peer. Use prod/con pointer regs, if not provided. */
#define	ZCX_RES_CQ_PROD_INT	"ZOCL_CU_XGQ_CQ_PRODUCER_INT"
/* Shared ring buffer. */
#define	ZCX_RES_RING		"ZOCL_CU_XGQ_RING"
/* IRQ for incoming interrupt. */
#define	ZCX_RES_IRQ		"ZOCL_CU_XGQ_IRQ"

/* Platform data structure. */
struct zocl_cu_xgq_info {
	size_t zcxi_slot_size;
	bool zcxi_echo_mode;
	struct platform_device *zcxi_intc_pdev;
};

int zcu_xgq_assign_cu(struct platform_device *pdev, u32 cu_idx, u32 cu_domain);
int zcu_xgq_unassign_cu(struct platform_device *pdev, u32 cu_idx, u32 cu_domain);

#endif /* _ZOCL_CU_XGQ_H_ */
