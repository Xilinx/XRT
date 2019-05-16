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
#include "ert.h"
#include "zocl_drv.h"

#define MAX_SLOTS 128
#define MAX_U32_SLOT_MASKS (((MAX_SLOTS-1)>>5) + 1)
/* MAX_CU_NUM are defined in zocl_util.h */
#define MAX_U32_CU_MASKS (((MAX_CU_NUM-1)>>5) + 1)
#define U32_MASK 0xFFFFFFFF

#define ZOCL_KDS_MASK		(~0xFF)
#define ZOCL_CU_FREE_RUNNING	(U32_MASK & ZOCL_KDS_MASK)

/**
 * Timestamp only use in set_cmd_ext_timestamp()
 */
enum zocl_ts_type {
	CU_START_TIME,
	CU_DONE_TIME,
};

/**
 * Address constants per spec
 */
#define WORD_SIZE                     4          /* 4 bytes */
#define CQ_SIZE                       0x10000    /* 64K */
#define CQ_BASE_ADDR                  0x190000
#define CSR_ADDR                      0x180000

enum zocl_cu_type {
	ZOCL_HARD_CU,
	ZOCL_SOFT_CU,
};

struct sched_dev;
struct sched_ops;

struct sched_client_ctx {
	struct list_head    link;
	atomic_t            trigger;
	struct mutex        lock;
};

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
 * @cu_usage: How many times the CUs are excecuted.
 * @ops: Scheduler operations vtable
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
	u32 			   cu_valid[MAX_U32_CU_MASKS];

	u32                        cu_addr_phy[MAX_CU_NUM];
	void __iomem              *cu_addr_virt[MAX_CU_NUM];
	u32                        cu_usage[MAX_CU_NUM];

	struct sched_ops          *ops;
	struct task_struct        *cq_thread;
	wait_queue_head_t          cq_wait_queue;
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
 * @packet: mapped ert packet object from user space
 */
struct sched_cmd {
	struct list_head list;
	struct drm_device *ddev;
	struct scheduler *sched;
	struct sched_exec_core *exec;
	enum ert_cmd_state state;
	int cu_idx; /* running cu, initialized to -1 */
	int slot_idx;
	int cq_slot_idx;
	void *buffer;
	void (*free_buffer)(struct sched_cmd *xcmd);

	/* The actual cmd object representation */
	struct ert_packet *packet;
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

void zocl_track_ctx(struct drm_device *dev, struct sched_client_ctx *fpriv);
void zocl_untrack_ctx(struct drm_device *dev, struct sched_client_ctx *fpriv);

#endif
