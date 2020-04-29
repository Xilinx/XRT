// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#ifndef _XRT_CU_H
#define _XRT_CU_H

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include "kds_command.h"

#define MAX_CUS 128

#define xcu_info(xcu, fmt, args...)			\
	dev_info(xcu->dev, " %llx %s: "fmt, (u64)xcu->dev, __func__, ##args)
#define xcu_err(xcu, fmt, args...)			\
	dev_err(xcu->dev, " %llx %s: "fmt, (u64)xcu->dev, __func__, ##args)
#define xcu_dbg(xcu, fmt, args...)			\
	dev_dbg(xcu->dev, " %llx %s: "fmt, (u64)xcu->dev, __func__, ##args)

/* HLS CU macros */
#define CU_AP_START	(0x1 << 0)
#define CU_AP_DONE	(0x1 << 1)
#define CU_AP_IDLE	(0x1 << 2)
#define CU_AP_READY	(0x1 << 3)
#define CU_AP_CONTINUE	(0x1 << 4)
#define CU_AP_RESET	(0x1 << 5)

/* PLRAM CU macros */

enum xcu_model {
	XCU_HLS,
	XCU_ACC,
	XCU_PLRAM,
};

enum xcu_config_type {
	CONSECUTIVE_T,
	PAIRS_T,
};

struct xcu_status {
	u32	num_done;
	u32	num_ready;
};

struct xcu_funcs {
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
	void (*put_credit)(void *core, u32 count);

	/**
	 * @configure:
	 *
	 * Congifure CU arguments.
	 *
	 * There are two types of configuration format.
	 *
	 * 1. CONSECUTIVE: Which is a blind copy from data to CU.
	 * 2. PAIRS: The data contains {offset, val} pairs.
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
	void (*check)(void *core, struct xcu_status *status);

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

struct xrt_cu_info {
	u32	model;
	int	cu_idx;
	int	inst_idx;
	u64	addr;
	u32	protocol;
	u32	intr_id;
	u32	num_res;
	bool	intr_enable;
};

struct xrt_cu {
	struct device		 *dev;
	struct xrt_cu_info	  info;
	struct resource		**res;
	struct list_head	  sq;
	u32			  num_sq;
	struct list_head	  rq;
	struct list_head	  pq;
	spinlock_t		  pq_lock;
	struct semaphore	  sem;
	u32			  num_pq;
	void                     *core;
	u32			  stop;
	u32			  done_cnt;
	u32			  ready_cnt;
	/**
	 * @funcs:
	 *
	 * Compute unit functions.
	 */
	struct xcu_funcs          *funcs;
	/* TODO: Maybe rethink if we should use two threads,
	 * one for submit, one for complete
	 */
	struct task_struct	  *thread;
};

void xrt_cu_reset(struct xrt_cu *xcu);
int  xrt_cu_reset_done(struct xrt_cu *xcu);
void xrt_cu_enable_intr(struct xrt_cu *xcu, u32 intr_type);
void xrt_cu_disable_intr(struct xrt_cu *xcu, u32 intr_type);
u32  xrt_cu_clear_intr(struct xrt_cu *xcu);

static inline void xrt_cu_config(struct xrt_cu *xcu, u32 *data, size_t sz, int type)
{
	xcu->funcs->configure(xcu->core, data, sz, type);
}

static inline void xrt_cu_start(struct xrt_cu *xcu)
{
	xcu->funcs->start(xcu->core);
}

static inline void xrt_cu_check(struct xrt_cu *xcu)
{
	struct xcu_status status;

	xcu->funcs->check(xcu->core, &status);
	/* XRT CU assume command finished in order
	 */
	xcu->done_cnt += status.num_done;
	xcu->ready_cnt += status.num_ready;
}

static inline int xrt_cu_get_credit(struct xrt_cu *xcu)
{
	return xcu->funcs->get_credit(xcu->core);
}

static inline void xrt_cu_put_credit(struct xrt_cu *xcu, u32 count)
{
	xcu->funcs->put_credit(xcu->core, count);
}

/* 1. Move commands from pending command queue to running queue
 * 2. If CU is ready, submit command(Configure hardware)
 * 3. Check if submitted command is completed or not
 */
int xrt_cu_thread(void *data);
void xrt_cu_submit(struct xrt_cu *xcu, struct kds_command *xcmd);

int  xrt_cu_init(struct xrt_cu *xcu);
void xrt_cu_fini(struct xrt_cu *xcu);

/* CU Implementations */
struct xrt_cu_hls {
	void __iomem		*vaddr;
	int			 max_credits;
	int			 credits;
};

int xrt_cu_hls_init(struct xrt_cu *xcu);
void xrt_cu_hls_fini(struct xrt_cu *xcu);

struct xrt_cu_plram {
	void __iomem		*vaddr;
	void __iomem		*plram;
	int			 max_credits;
	int			 credits;
};

int xrt_cu_plram_init(struct xrt_cu *xcu);
void xrt_cu_plram_fini(struct xrt_cu *xcu);

#endif /* _XRT_CU_H */
