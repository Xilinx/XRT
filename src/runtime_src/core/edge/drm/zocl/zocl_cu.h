/** * Compute unit structures.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Min Ma <min.ma@xilinx.com>
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

#ifndef _ZOCL_CU_H_
#define _ZOCL_CU_H_

#include <linux/types.h>

#include "zocl_util.h"

#define U32_MASK 0xFFFFFFFF

#define ZOCL_KDS_MASK		(~0xFF)
#define ZOCL_CU_FREE_RUNNING	(U32_MASK & ZOCL_KDS_MASK)

#define CU_INTR_DONE  0x1
#define CU_INTR_READY 0x2

#define CU_VERSION_MASK         0x00000F00
#define CU_MAX_CAP_MASK         0x0000F000
#define CU_READY_CNT_MASK       0x00FF0000
#define CU_DONE_CNT_MASK        0xFF000000

#define CU_AP_START	(0x1 << 0)
#define CU_AP_DONE	(0x1 << 1)
#define CU_AP_IDLE	(0x1 << 2)
#define CU_AP_READY	(0x1 << 3)
#define CU_AP_CONTINUE	(0x1 << 4)
#define CU_AP_RESET	(0x1 << 5)

struct zocl_cu;

/* Supported CU models */
enum zcu_model {
	MODEL_HLS,
	MODEL_ACC,
};

enum zcu_configure_type {
	PAIRS,
	CONSECUTIVE,
};

struct zcu_tasks_info {
	u32	num_tasks_done;
	u32	num_tasks_ready;
};

struct zcu_core {
	phys_addr_t		 paddr;
	u32 __iomem		*vaddr;
	u32			 max_credits;
	u32			 credits;
	u32			 intr_type;
	u32			 pending_intr;
	u32			 running;
};

struct zcu_funcs {
	/**
	 * @get_credit:
	 *
	 * Try to get one credit from the CU. A credit is required before
	 * submit a task to the CU. Otherwise, it would lead to unknown CU
	 * behaviour.
	 * Return: the number of remaining credit.
	 */
	int (*get_credit)(void *core);

	/**
	 * @refund_credit:
	 *
	 * refund credit to the CU.
	 */
	void (*refund_credit)(void *core, u32 count);

	/**
	 * @configure:
	 *
	 * Congifure CU registers. It supports two types of configuration
	 * format.
	 *
	 * 1. CONSECUTIVE: Which is a blind copy from data to CU.
	 * 2. PAIRS: The data contains {addr, val} pairs.
	 */
	void (*configure)(void *core, u32 *data, size_t sz, int type);

	/**
	 * @start:
	 *
	 * Start a CU.
	 */
	void (*start)(void *core);

	/**
	 * @check:
	 *
	 * Check CU status and the pending task status.
	 */
	void (*check)(void *core, struct zcu_tasks_info *tasks);

	/**
	 * @reset:
	 *
	 * Reset CU.
	 */
	void (*reset)(void *core);

	/**
	 * @reset_done:
	 *
	 * Check if CU is properly reset
	 */
	int (*reset_done)(void *core);

	/**
	 * @enable_intr:
	 *
	 * Enable interrupt. Support DONE and READY interrupt.
	 */
	void (*enable_intr)(void *core, u32 intr_type);

	/**
	 * @disable_intr:
	 *
	 * Disable interrupt.
	 */
	void (*disable_intr)(void *core, u32 intr_type);

	/**
	 * @clear_intr:
	 *
	 * Clear interrupt.
	 */
	u32 (*clear_intr)(void *core);
};

struct zocl_cu {
	enum zcu_model	          model;
	struct list_head	  running_queue;
	void                     *core;
	u32			  usage;
	u32			  done_cnt;
	u32			  ready_cnt;
	u32                       run_timeout;
	u32                       reset_timeout;
	u32			  irq;
	/**
	 * @funcs:
	 *
	 * Compute unit functions. Use these functions for operating the CU.
	 */
	struct zcu_funcs          *funcs;
};

int zocl_cu_init(struct zocl_cu *cu, enum zcu_model m, phys_addr_t paddr);
int zocl_cu_fini(struct zocl_cu *cu);

int  zocl_cu_get_credit(struct zocl_cu *cu);
void zocl_cu_refund_credit(struct zocl_cu *cu, u32 count);
void zocl_cu_configure(struct zocl_cu *cu, u32 *data, size_t sz, int type);
void zocl_cu_start(struct zocl_cu *cu);
void zocl_cu_check(struct zocl_cu *cu);
void zocl_cu_reset(struct zocl_cu *cu);
int  zocl_cu_reset_done(struct zocl_cu *cu);
void zocl_cu_enable_intr(struct zocl_cu *cu, u32 intr_type);
void zocl_cu_disable_intr(struct zocl_cu *cu, u32 intr_type);
u32  zocl_cu_clear_intr(struct zocl_cu *cu);

phys_addr_t zocl_cu_get_paddr(struct zocl_cu *cu);
void zocl_cu_status_print(struct zocl_cu *cu);

#endif /* _ZOCL_CU_H_ */
