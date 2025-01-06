/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _XRT_CU_H
#define _XRT_CU_H

#include <linux/version.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/circ_buf.h>
#include "kds_command.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define ioremap_nocache         ioremap
#endif

/* Avoid the CU soft lockup warning when CU thread keep busy.
 * Small value leads to lower performance on APU.
 */
#define MAX_CU_LOOP 300

/* If poll count reach this threashold, switch to interrupt mode */
#if defined(CONFIG_ARM64)
#define CU_DEFAULT_POLL_THRESHOLD 30 /* About 60 us on APU */
#else
#define CU_DEFAULT_POLL_THRESHOLD 300 /* About 75 us on host */
#endif

/* The normal CU in ip_layout would assign a interrupt
 * ID in range 0 to 127. Use 128 for m2m cu could ensure
 * m2m CU is at the end of the CU, which is compatible with
 * legacy implementation.
 */
#define M2M_CU_ID 128

#define xcu_info(xcu, fmt, args...)			\
	dev_info(xcu->dev, " %llx %s: "fmt, (u64)xcu->dev, __func__, ##args)
#define xcu_warn(xcu, fmt, args...)			\
	dev_warn(xcu->dev, " %llx %s: "fmt, (u64)xcu->dev, __func__, ##args)
#define xcu_err(xcu, fmt, args...)			\
	dev_err(xcu->dev, " %llx %s: "fmt, (u64)xcu->dev, __func__, ##args)
#define xcu_dbg(xcu, fmt, args...)			\
	dev_dbg(xcu->dev, " %llx %s: "fmt, (u64)xcu->dev, __func__, ##args)

/* XRT CU timer macros */
/* A low frequence timer per CU to check if CU/command timeout */
#define CU_TICKS_PER_SEC	2
#define CU_TIMER		(HZ / CU_TICKS_PER_SEC) /* in jiffies */
#define CU_EXEC_DEFAULT_TTL	(5UL * CU_TICKS_PER_SEC)
/* A customed frequency timer per CU to collect data */
#define CU_STATS_TICKS_PER_SEC  20
#define CU_STATS_TIMER          (HZ / CU_STATS_TICKS_PER_SEC) /* in jiffies */

/* HLS CU macros */
#define CU_AP_START	(0x1 << 0)
#define CU_AP_DONE	(0x1 << 1)
#define CU_AP_IDLE	(0x1 << 2)
#define CU_AP_READY	(0x1 << 3)
#define CU_AP_CONTINUE	(0x1 << 4)
#define CU_AP_RESET	(0x1 << 5)
#define CU_AP_SW_RESET	(0x1 << 8)
/* Special macro(s) which not defined by HLS CU */
#define CU_AP_CRASHED	(0xFFFFFFFF)

#define CU_INTR_DONE  0x1
#define CU_INTR_READY 0x2

enum xcu_model {
	XCU_HLS,
	XCU_ACC,
	XCU_FA,
	XCU_XGQ,
	XCU_AUTO,
};

enum xcu_config_type {
	CONSECUTIVE_T,
	PAIRS_T,
	XGQ_T,
};

enum xcu_process_result {
	XCU_IDLE = 0,
	XCU_BUSY,
};

/* Let's use HLS style status bits in new_status
 * Bit 0: start (running)
 * Bit 1: done
 * Bit 2: idle
 */
struct xcu_status {
	u32	num_done;
	u32	num_ready;
	u32	new_status;
	u32	rcode;
};

typedef void *xcu_core_t;
struct xcu_funcs {
	/**
	 * @alloc_credit:
	 *
	 * Try to alloc one credit on the CU. A credit is required before
	 * submit a task to the CU. Otherwise, it would lead to unknown CU
	 * behaviour.
	 * Return: the number of remaining credit.
	 */
	int (*alloc_credit)(void *core);

	/**
	 * @free_credit:
	 *
	 * free credits.
	 */
	void (*free_credit)(void *core, u32 count);

	/**
	 * @peek_credit:
	 *
	 * Check how many credits the CU could provide with side effect.
	 */
	int (*peek_credit)(void *core);

	/**
	 * @configure:
	 *
	 * Congifure CU arguments.
	 *
	 * There are two types of configuration format.
	 *
	 * 1. CONSECUTIVE: Which is a blind copy from data to CU.
	 * 2. PAIRS: The data contains {offset, val} pairs.
	 * 3. XGQ: The data contains a XGQ command.
	 */
	int (*configure)(void *core, u32 *data, size_t sz, int type);

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
	void (*check)(void *core, struct xcu_status *status, bool force);

	/**
	 * @submit_config:
	 *
	 * This is like configure CU. But it takes xcmd and own it.
	 *
	 */
	int (*submit_config)(void *core, struct kds_command *xcmd);

	/**
	 * @get_complete:
	 *
	 * Get next completed xcmd.
	 *
	 */
	struct kds_command *(*get_complete)(void *core);

	/**
	 * @abort:
	 *
	 */
	int (*abort)(void *core, void *cond, bool (*match)(struct kds_command *xcmd, void *cond));

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
	bool (*reset_done)(void *core);

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

enum arg_dir {
	DIR_NONE = 0,
	DIR_INPUT,
	DIR_OUTPUT
};

struct xrt_cu_arg {
	char	name[64];
	u32	offset;
	u32	size;
	u32	dir;
};

enum CU_PROTOCOL {
	CTRL_HS = 0,
	CTRL_CHAIN = 1,
	CTRL_NONE = 2,
	CTRL_ME = 3,
	CTRL_ACC = 4,
	CTRL_FA = 5
};

struct xrt_cu_info {
	u32			 model;
	u32			 slot_idx;
	/* CU Index respected to a slot */
	int			 cu_idx;
	/* CU Index respected to a subdevice instance index */
	int			 inst_idx;
	u64			 addr;
	size_t			 size;
	u32			 protocol;
	u32			 intr_id;
	u32			 is_m2m;
	u32			 num_res;
	bool			 intr_enable;
	bool			 sw_reset;
	struct xrt_cu_arg	*args;
	u32			 num_args;
	char			 iname[64];
	char			 kname[64];
	void			*xgq;
	int			 cu_domain;
	unsigned char		 uuid[16];
};

#define CU_STATE_GOOD  0x1
#define CU_STATE_BAD   0x2

/* The Buf size and xrt_cu_log size must be power of 2 */
#define CIRC_BUF_SIZE 2 * 4096
struct xrt_cu_log {
	u64		stage:32;
	uintptr_t	cmd_id:32;
	u64		ts;
};
#define CU_LOG_STAGE_RQ		1
#define CU_LOG_STAGE_ISR	2
#define CU_LOG_STAGE_SQ		3
#define CU_LOG_STAGE_CQ		4

struct xrt_cu_range {
	struct mutex		  xcr_lock;
	u32			  xcr_start;
	u32			  xcr_end;
};

/* For cu profiling statistic */
struct xrt_cu_stats {
	spinlock_t		   xcs_lock;
	struct timer_list          stats_timer;
	u32                        stats_tick;

	u32                        stats_enabled;
	u32                        last_ts_status;
	/* length of sq*/
	u32                        max_sq_length;
	u32                        sq_total;
	u32                        sq_count;
	u32                        idle;
	/* last timestamp used for calculation*/
	u64                        last_timestamp;
	u64                        last_read_idle_start;
	u64                        last_idle_total;
	/* cmds count */
	u64                        usage_prev;
	u64                        usage_curr;
	u64                        incre_ecmds;
	/* for idle time calculation*/
	u64                        idle_total;
	u64                        idle_start;
	u64                        idle_end;

};

/* Supported event type */
struct xrt_cu {
	struct device		 *dev;
	struct xrt_cu_info	  info;
	struct resource		**res;
	struct list_head	  cu;
	/* Range of Read-only registers */
	struct xrt_cu_range	  read_regs;
	/* pending queue */
	struct list_head	  pq;
	spinlock_t		  pq_lock;
	u32			  num_pq;
	/* high priority queue */
	struct list_head	  hpq;
	spinlock_t		  hpq_lock;
	u32			  num_hpq;
	struct completion	  comp;
	/*
	 * Pending Q is used in thread that is submitting CU cmds.
	 * Other Qs are used in thread that is completing them.
	 * In order to prevent false sharing, they need to be in different
	 * cache lines.
	 */
	/* run queue */
	struct list_head	  rq ____cacheline_aligned_in_smp;
	u32			  num_rq;
	/* submitted queue */
	u32			  num_sq;
	/* completed queue */
	struct list_head	  cq;
	u32			  num_cq;
	struct semaphore	  sem;
	struct semaphore	  sem_cu;
	void			 *core;
	u32			  stop;
	bool			  bad_state;
	u32			  done_cnt;
	u32			  ready_cnt;
	u32			  status;
	u32			  rcode;
	int			  busy_threshold;
	u32			  interval_min;
	u32			  interval_max;
	struct kds_command	 *old_cmd;

	struct mutex		  ev_lock;
	struct list_head	  events;

	struct timer_list	  timer;
	atomic_t		  tick;
	u32			  start_tick;
	u32			  force_intr;

	struct xrt_cu_stats        stats;
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
	u32			   poll_count;
	u32                        poll_threshold;
	u32			   interrupt_used;
	/* Good for debug */
	u32			   sleep_cnt;
	u32			   max_running;

	/* support user management CU interrupt */
	DECLARE_BITMAP(is_ucu, 1);
	wait_queue_head_t	   ucu_waitq;
	atomic_t		   ucu_event;
	int (* user_manage_irq)(struct xrt_cu *xcu, bool user_manage);
	int (* configure_irq)(struct xrt_cu *xcu, bool enable);

	/* For debug/analysis */
	char			   debug;
	char			   log_buf[CIRC_BUF_SIZE] __aligned(sizeof(u64));
	struct circ_buf		   crc_buf;
};

static inline char *prot2str(enum CU_PROTOCOL prot)
{
	switch (prot) {
	case CTRL_HS:		return "CTRL_HS";
	case CTRL_CHAIN:	return "CTRL_CHAIN";
	case CTRL_NONE:		return "CTRL_NONE";
	case CTRL_ME:		return "CTRL_ME";
	case CTRL_ACC:		return "CTRL_ACC";
	case CTRL_FA:		return "CTRL_FA";
	default:		return "UNKNOWN";
	}
}

static inline void xrt_cu_enable_intr(struct xrt_cu *xcu, u32 intr_type)
{
	if (xcu->funcs)
		xcu->funcs->enable_intr(xcu->core, intr_type);
}

static inline void xrt_cu_disable_intr(struct xrt_cu *xcu, u32 intr_type)
{
	if (xcu->funcs)
		xcu->funcs->disable_intr(xcu->core, intr_type);
}

static inline u32 xrt_cu_clear_intr(struct xrt_cu *xcu)
{
	return xcu->funcs ? xcu->funcs->clear_intr(xcu->core) : 0;
}

static inline int xrt_cu_config(struct xrt_cu *xcu, u32 *data, size_t sz, int type)
{
	return xcu->funcs->configure(xcu->core, data, sz, type);
}

static inline void xrt_cu_start(struct xrt_cu *xcu)
{
	xcu->funcs->start(xcu->core);
}

static inline int xrt_cu_submit_config(struct xrt_cu *xcu, struct kds_command *xcmd)
{
	if (!xcu->funcs->submit_config)
		return -EINVAL;

	return xcu->funcs->submit_config(xcu->core, xcmd);
}

static inline struct kds_command *xrt_cu_get_complete(struct xrt_cu *xcu)
{
	if (!xcu->funcs->get_complete)
		return NULL;

	return xcu->funcs->get_complete(xcu->core);
}

static inline int
xrt_cu_cmd_abort(struct xrt_cu *xcu, void *cond,
		 bool (*match)(struct kds_command *xcmd, void *cond))
{
	if (!xcu->funcs->abort)
		return -EINVAL;

	return xcu->funcs->abort(xcu->core, cond, match);
}

static inline void xrt_cu_reset(struct xrt_cu *xcu)
{
	xcu->funcs->reset(xcu->core);
}

static inline bool xrt_cu_reset_done(struct xrt_cu *xcu)
{
	return xcu->funcs->reset_done(xcu->core);
}

static inline void __xrt_cu_check(struct xrt_cu *xcu, bool force)
{
	struct xcu_status status;

	status.num_done = 0;
	status.num_ready = 0;
	status.new_status = 0;
	status.rcode = 0;
	xcu->funcs->check(xcu->core, &status, force);
	/* XRT CU assume command finished in order */
	xcu->done_cnt += status.num_done;
	xcu->ready_cnt += status.num_ready;
	// Do not update CU status if it has crashed
	if (xcu->status != CU_AP_CRASHED)
	  xcu->status = status.new_status;
	xcu->rcode  = status.rcode;
}

static inline void xrt_cu_check(struct xrt_cu *xcu)
{
	__xrt_cu_check(xcu, false);
}

static inline void xrt_cu_check_force(struct xrt_cu *xcu)
{
	__xrt_cu_check(xcu, true);
}

static inline int xrt_cu_get_credit(struct xrt_cu *xcu)
{
	return xcu->funcs->alloc_credit(xcu->core);
}

static inline int is_zero_credit(struct xrt_cu *xcu)
{
	return (xcu->funcs->peek_credit(xcu->core) == 0);
}

static inline int xrt_cu_peek_credit(struct xrt_cu *xcu)
{
	return xcu->funcs->peek_credit(xcu->core);
}

static inline void xrt_cu_put_credit(struct xrt_cu *xcu, u32 count)
{
	xcu->funcs->free_credit(xcu->core, count);
}

/* This is a bit trick to calculate next highest power of 2.
 * If size is a power of 2, the return would be the same as size.
 */
static __attribute__((unused))
u32 round_up_to_next_power2(u32 size)
{
	u32 v = --size;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return ++v;
}

/* 1. Move commands from pending command queue to running queue
 * 2. If CU is ready, submit command(Configure hardware)
 * 3. Check if submitted command is completed or not
 */
void xrt_cu_submit(struct xrt_cu *xcu, struct kds_command *xcmd);
void xrt_cu_hpq_submit(struct xrt_cu *xcu, struct kds_command *xcmd);
void xrt_cu_abort(struct xrt_cu *xcu, struct kds_client *client);
bool xrt_cu_abort_done(struct xrt_cu *xcu, struct kds_client *client);
bool xrt_cu_intr_supported(struct xrt_cu *xcu);
int xrt_cu_intr_thread(void *data);
int xrt_cu_start_thread(struct xrt_cu *xcu);
void xrt_cu_stop_thread(struct xrt_cu *xcu);
int xrt_cu_cfg_update(struct xrt_cu *xcu, int intr);
int xrt_fa_cfg_update(struct xrt_cu *xcu, u64 bar, u64 dev, void __iomem *vaddr, u32 num_slots);
int xrt_is_fa(struct xrt_cu *xcu, u32 *size);
int xrt_cu_get_protocol(struct xrt_cu *xcu);
u32 xrt_cu_get_status(struct xrt_cu *xcu);
int xrt_cu_regmap_size(struct xrt_cu *xcu);

int  xrt_cu_init(struct xrt_cu *xcu);
void xrt_cu_fini(struct xrt_cu *xcu);

ssize_t show_cu_stat(struct xrt_cu *xcu, char *buf);
ssize_t show_cu_info(struct xrt_cu *xcu, char *buf);
ssize_t show_formatted_cu_stat(struct xrt_cu *xcu, char *buf);
ssize_t show_stats_begin(struct xrt_cu *xcu, char *buf);
ssize_t show_stats_end(struct xrt_cu *xcu, char *buf);

void xrt_cu_incr_sq_count(struct xrt_cu *xcu);
u64 xrt_cu_get_iops(struct xrt_cu *xcu, u64 last_timestamp, u64 incre_ecmds, u64 new_ts);
u64 xrt_cu_get_average_sq(struct xrt_cu *xcu, u32 sq_total, u32 sq_count);
u64 xrt_cu_get_idle(struct xrt_cu *xcu, u64 last_timestamp, u64 idle_start, u64 last_read_idle_start, u64 delta_idle_time, u32 idle, u64 new_ts);

void xrt_cu_circ_produce(struct xrt_cu *xcu, u32 stage, uintptr_t cmd);
ssize_t xrt_cu_circ_consume_all(struct xrt_cu *xcu, char *buf, size_t size);

int xrt_cu_process_queues(struct xrt_cu *xcu);

/* CU Implementations */
int xrt_cu_hls_init(struct xrt_cu *xcu);
void xrt_cu_hls_fini(struct xrt_cu *xcu);

typedef struct {
	u32 argOffset;
	u32 argSize;
	u32 argValue[];
} descEntry_t;

typedef struct {
	u32 status;
	u32 numInputEntries;
	u32 inputEntryBytes;
	u32 numOutputEntries;
	u32 outputEntryBytes;
	u32 data[];
} descriptor_t;

#define to_cu_fa(core) ((struct xrt_cu_fa *)(core))
struct xrt_cu_fa {
	void __iomem		*vaddr;
	void __iomem		*cmdmem;
	u64			 paddr;
	u32			 slot_sz;
	u32			 num_slots;
	u32			 head_slot;
	u32			 desc_msw;
	u32			 task_cnt;
	int			 max_credits;
	int			 credits;
	int			 run_cnts;
	u64			 check_count;

	struct list_head	 submitted;
	struct list_head	 completed;
};

int xrt_cu_fa_init(struct xrt_cu *xcu);
void xrt_cu_fa_fini(struct xrt_cu *xcu);

int xrt_cu_scu_init(struct xrt_cu *xcu, void *vaddr, struct semaphore *sem);
void xrt_cu_scu_fini(struct xrt_cu *xcu);
void xrt_cu_scu_crashed(struct xrt_cu *xcu);

#endif /* _XRT_CU_H */
