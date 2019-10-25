/* SPDX-License-Identifier: GPL-2.0 */
/** * Compute unit execution, interrupt management and
 * client context core data structures.
 *
 * Copyright (C) 2017-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
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

#ifndef _XCL_SCHE_EXEC_H_
#define _XCL_SCHE_EXEC_H_

#include <linux/mutex.h>
#include <linux/init_task.h>
#include <linux/list.h>
#include <linux/wait.h>
#include "zocl_drv.h"
#include "zocl_cu.h"
#include "ert.h"

#define MAX_SLOTS 128
#define MAX_U32_SLOT_MASKS (((MAX_SLOTS-1)>>5) + 1)
/* MAX_CU_NUM are defined in zocl_util.h */
#define MAX_U32_CU_MASKS (((MAX_CU_NUM-1)>>5) + 1)

/* Timer thread wake up interval in Millisecond */
#define	ZOCL_CU_TIMER_INTERVAL		(500)

/* Reset timer interval in Microsecond */
#define	ZOCL_CU_RESET_TIMER_INTERVAL	(1000)

/*
 * For zocl cu version 1. The done counter will have risk to overflow
 * if more than 31 commands done but kds still not able to read done counter.
 * TBD. This is related to the hardware implementation.
 */
#define MAX_PENDING_CMD         31

/**
 * Timestamp only use in set_cmd_ext_timestamp()
 */
enum zocl_ts_type {
	CU_START_TIME,
	CU_DONE_TIME,
};

enum zocl_cu_type {
	ZOCL_HARD_CU,
	ZOCL_SOFT_CU,
};

enum zocl_exec_status {
	ZOCL_EXEC_NORMAL = 0,
	ZOCL_EXEC_STOP,
	ZOCL_EXEC_FLUSH,
};

struct sched_dev;
struct sched_ops;

/*
 * struct sched_client_ctx: Manage user space client attached to device
 *
 * @link: Client context is added to list in device
 * @trigger:
 * @lock:
 */
struct sched_client_ctx {
	struct list_head   link;
	atomic_t           trigger;
	atomic_t           outstanding_execs;
	struct mutex       lock;
	int		   num_cus;
	struct pid	   *pid;
	unsigned int	   abort;
};
#define CLIENT_NUM_CU_CTX(client) ((client)->num_cus)

/**
 * struct sched_exec_core: Core data structure for command execution on a device
 *
 * @ctx_list: Context list populated with device context
 * @ctx_list_lock: Context list lock
 * @poll_wait_queue: Wait queue for device polling
 * @scheduler: Command queue scheduler
 * @submitted_cmds: Tracking of command submitted for execution on this device
 * @num_slots: Number of command queue slots
 * @num_cus: Number of CUs in loaded program
 * @cu_shift_offset: CU idx to CU address shift value
 * @cu_base_addr: Base address of CU address space
 * @polling_mode: If set then poll for command completion
 * @cq_interrupt: If set then X86 host will trigger interrupt to PS
 * @configured: Flag of the core data structure has been initialized
 * @slot_status: Status (busy(1)/free(0)) of slots in command queue
 * @num_slot_masks: Number of slots status masks used
 * @cu_status: Status (busy(1)/free(0)) of CUs. Unused in ERT mode.
 * @num_cu_masks: Number of CU masks used (computed from @num_cus)
 * @cu_addr_phy: Physical address of CUs.
 * @cu: Per CU structure.
 * @ops: Scheduler operations vtable
 * @cq_thread: Kernel thread to check the ert command queue.
 * @timer_task: Kernel thread as a timer interrupt.
 */
struct sched_exec_core {
	void __iomem               *base;
	struct list_head           ctx_list;
	spinlock_t                 ctx_list_lock;
	wait_queue_head_t          poll_wait_queue;

	struct scheduler          *scheduler;

	struct sched_cmd          *submitted_cmds[MAX_SLOTS];

	unsigned int               num_slots;
	unsigned int               num_cus;
	unsigned int               cu_shift_offset;
	u32                        cu_base_addr;
	unsigned int               polling_mode;
	unsigned int               cq_interrupt;
	unsigned int               cu_dma;
	unsigned int               cu_isr;
	unsigned int               configured;

	/* Bitmap tracks busy(1)/free(0) slots in cmd_slots*/
	u32                        slot_status[MAX_U32_SLOT_MASKS];
	unsigned int               num_slot_masks; /* ((num_slots-1)>>5)+1 */

	/* Bitmap tracks CU busy(1)/free(0) */
	u32                        cu_status[MAX_U32_CU_MASKS];
	unsigned int               num_cu_masks; /* ((num_cus-1)>>5+1 */

	/* Bitmap tracks CU initialization initialized(1)/uninitialized(0) */
	u32                        cu_init[MAX_U32_CU_MASKS];

	/* Soft kernel definitions */
	u32                        scu_status[MAX_U32_CU_MASKS];

	/* Bitmap tracks valid CU valid(1)/invalid(0) */
	u32			   cu_valid[MAX_U32_CU_MASKS];

	struct zocl_cu		  *zcu;

	struct sched_ops          *ops;
	struct task_struct        *cq_thread;
	wait_queue_head_t          cq_wait_queue;

	struct task_struct        *timer_task;

	/* Context switch */
	atomic_t		  exec_status;
};

/**
 * struct scheduler: scheduler for sched_cmd objects
 *
 * @sched_thread: thread associated with this scheduler
 * @use_count: use count for this scheduler
 * @wait_queue: conditional wait queue for scheduler thread
 * @error: set to 1 to indicate scheduler error
 * @stop: set to 1 to indicate scheduler should stop
 * @cq: list of command objects managed by scheduler
 * @intc: set when there is a pending interrupt for command completion
 * @poll: number of running commands in polling mode
 * @check: flag to indicate a CU timerout check
 */
struct scheduler {
	struct task_struct        *sched_thread;
	unsigned int               use_count;

	wait_queue_head_t          wait_queue;
	unsigned int               error;
	unsigned int               stop;

	struct list_head           cq;
	unsigned int               intc; /* pending intr shared with isr*/
	unsigned int               poll; /* number of cmds to poll */
	atomic_t                   check;
};

/**
 * Command data used by scheduler
 *
 * @list: command object moves from list to list
 * @exec: core data structure for scheduler
 * @state: state of command object per scheduling
 * @cu_idx: index of CU executing this cmd object; used in penguin mode only
 * @slot_idx: command queue index of this command object
 * @buffer: underlying buffer (ex. drm buffer object)
 * @exectime: time unit elapsed after CU is triggered to run
 * @packet: mapped ert packet object from user space
 * @check_timeout: flag to indicate if we should check timeout on this CU
 */
struct sched_cmd {
	struct list_head list;
	struct list_head rq_list;
	struct drm_device *ddev;
	struct scheduler *sched;
	struct sched_exec_core *exec;
	struct sched_client_ctx *client;
	enum ert_cmd_state state;
	int cu_idx; /* running cu, initialized to -1 */
	int slot_idx;
	int cq_slot_idx;
	void *buffer;
	void (*free_buffer)(struct sched_cmd *xcmd);

	/*
	 * This is a rough time unit elapsed after the CU is running.
	 * It is set to a initial value configured by init CU command
	 * and decreased every time unit. If it reaches zero, this CU
	 * timeouts.
	 *
	 * The time unit is 500 Milliseconds now.
	 */
	uint32_t exectime;

	/*
	 * If this flag is set, we should check the timeout of this
	 * command. It is set based on the CU timeout value. If the
	 * timeout value is 0, we should not set this flag.
	 */
	int check_timeout;

	/*
	 * If this flag is set, record time stamps in the user's command
	 * package when the command state change.
	 */
	bool timestamp_enabled;

	/* The actual cmd object representation */
	union {
		struct ert_packet *packet;
		struct ert_start_copybo_cmd *ert_cp;
		struct ert_start_kernel_cmd *ert_cu;
	};

	zocl_dma_handle_t dma_handle;
};

/**
 * struct sched_ops: scheduler specific operations
 *
 * Scheduler can operate in MicroBlaze mode (mb/ert) or in penguin mode. This
 * struct differentiates specific operations.  The struct is per device node,
 * meaning that one device can operate in ert mode while another can operate in
 * penguin mode.
 */
struct sched_ops {
	int (*submit)(struct sched_cmd *xcmd);
	void (*query)(struct sched_cmd *xcmd);
};

int sched_init_exec(struct drm_device *drm);
int sched_fini_exec(struct drm_device *drm);
int sched_reset_exec(struct drm_device *drm);

void zocl_track_ctx(struct drm_device *dev, struct sched_client_ctx *fpriv);
void zocl_untrack_ctx(struct drm_device *dev, struct sched_client_ctx *fpriv);

int zocl_exec_valid_cu(struct sched_exec_core *exec, unsigned int cuid);
u32 sched_is_busy(struct drm_zocl_dev *zdev);
u32 sched_live_clients(struct drm_zocl_dev *zdev, pid_t **plist);
#endif
