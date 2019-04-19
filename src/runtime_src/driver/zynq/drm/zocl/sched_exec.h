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

#define MAX_SLOTS 128
#define MAX_U32_SLOT_MASKS (((MAX_SLOTS-1)>>5) + 1)
/* MAX_CU_NUM are defined in zocl_util.h */
#define MAX_U32_CU_MASKS (((MAX_CU_NUM-1)>>5) + 1)
#define U32_MASK 0xFFFFFFFF

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

/**
 * Command state
 *
 * @CMD_STATE_NEW:      Set by host before submitting a command to scheduler
 * @CMD_STATE_QUEUED:   Internal scheduler state
 * @CMD_STATE_RUNNING:  Internal scheduler state
 * @CMD_STATE_COMPLETE: Set by scheduler when command completes
 * @CMD_STATE_ERROR:    Set by scheduler if command failed
 * @CMD_STATE_ABORT:    Set by scheduler if command abort
 */
enum cmd_state {
	CMD_STATE_NEW = 1,
	CMD_STATE_QUEUED = 2,
	CMD_STATE_RUNNING = 3,
	CMD_STATE_COMPLETED = 4,
	CMD_STATE_ERROR = 5,
	CMD_STATE_ABORT = 6,
};

/**
 * Opcode types for commands
 *
 * @OP_START_CU:       start a workgroup on a CU
 * @OP_START_KERNEL:   currently aliased to ERT_START_CU
 * @OP_CONFIGURE:      configure command scheduler
 * @OP_CONFIG_SKERNEL: configure soft kernel
 * @OP_START_SKERNEL:  start soft kernel
 * @OP_SK_UNCONFIG:    unconfigure a soft kernel
 */
enum cmd_opcode {
	OP_START_CU		= 0,
	OP_START_KERNEL		= 0,
	OP_CONFIGURE		= 2,
	OP_STOP			= 3,
	OP_ABORT		= 4,
	OP_CONFIG_SKERNEL	= 8,
	OP_START_SKERNEL	= 9,
	OP_UNCONFIG_SKERNEL	= 10,
};

/**
 * struct sched_packet: scheduler generic packet format
 *
 * @state:   [3-0] current state of a command
 * @custom:  [11-4] custom per specific commands
 * @count:   [22-12] number of words in payload (data)
 * @opcode:  [27-23] opcode identifying specific command
 * @type:    [31-27] type of command (currently 0)
 * @data:    count number of words representing packet payload
 */
struct sched_packet {
	union {
		struct {
			uint32_t state:4;   /* [3-0]   */
			uint32_t custom:8;  /* [11-4]  */
			uint32_t count:11;  /* [22-12] */
			uint32_t opcode:5;  /* [27-23] */
			uint32_t type:4;    /* [31-27] */
		};
		uint32_t header;
	};
	uint32_t data[1];   /* count number of words */
};

/**
 * struct start_kernel_cmd: start kernel command format
 *
 * @state:           [3-0] current state of a command
 * @extra_cu_masks:  [11-10] extra CU masks in addition to mandatory mask
 * @count:           [22-12] number of words in payload (data)
 * @opcode:          [27-23] 0, opcode for start_kernel
 * @type:            [31-27] 0, type of start_kernel
 *
 * @cu_mask:         first mandatory CU mask
 * @data:            count number of words representing command payload
 *
 * The packet payload is comprised of 1 mandatory CU mask plus
 * extra_cu_masks per header field, followed a CU register map of size
 * (count - (1 + extra_cu_masks)) uint32_t words.
 */
struct start_kernel_cmd {
	union {
		struct {
			uint32_t state:4;          /* [3-0]   */
			uint32_t unused:6;         /* [9-4]  */
			uint32_t extra_cu_masks:2; /* [11-10]  */
			uint32_t count:11;         /* [22-12] */
			uint32_t opcode:5;         /* [27-23] */
			uint32_t type:4;           /* [31-27] */
		};
		uint32_t header;
	};

	/* payload */
	uint32_t cu_mask;          /* mandatory cu mask */
	uint32_t data[1];          /* count-1 number of words */
};

/**
 * struct configure_cmd: configure command format
 *
 * @state:           [3-0] current state of a command
 * @count:           [22-12] number of words in payload (5 + num_cus)
 * @opcode:          [27-23] 1, opcode for configure
 * @type:            [31-27] 0, type of configure
 *
 * @slot_size:       command queue slot size
 * @num_cus:         number of compute units in program
 * @cu_shift:        shift value to convert CU idx to CU addr
 * @cu_base_addr:    base address to add to CU addr for actual physical address
 *
 * @ert:1            enable embedded HW scheduler
 * @polling:1        poll for command completion
 * @cu_dma:1         enable CUDMA custom module for HW scheduler
 * @cu_isr:1         enable CUISR custom module for HW scheduler
 * @cq_int:1         enable interrupt from host to HW scheduler
 * @unused:26
 * @dsa52:1          reserved for internal use
 *
 * @data             addresses of @num_cus_CUs
 */
struct configure_cmd {
	union {
		struct {
			uint32_t state:4;          /* [3-0]   */
			uint32_t unused:8;         /* [11-4]  */
			uint32_t count:11;         /* [22-12] */
			uint32_t opcode:5;         /* [27-23] */
			uint32_t type:4;           /* [31-27] */
		};
		uint32_t header;
	};

	/* payload */
	uint32_t slot_size;
	uint32_t num_cus;
	uint32_t cu_shift;
	uint32_t cu_base_addr;

	/* features */
	uint32_t ert:1;
	uint32_t polling:1;
	uint32_t cu_dma:1;
	uint32_t cu_isr:1;
	uint32_t cq_int:1;
	uint32_t unusedf:26;
	uint32_t dsa52:1;

	/* cu addresses map size is num_cus */
	uint32_t data[1];
};

/**
 * struct configure_sk_cmd: configure soft kernel command format
 *
 * @state:           [3-0] current state of a command
 * @count:           [22-12] number of words in payload (5 + num_cus)
 * @opcode:          [27-23] 1, opcode for configure
 * @type:            [31-27] 0, type of configure
 *
 * @start_cuidx:     start index of compute units
 * @num_cus:         number of compute units in program
 * @sk_size:         size in bytes of soft kernel image
 * @sk_name:         symbol name of soft kernel
 * @sk_addr:         soft kernel image's physical address (little endian)
 */
struct configure_sk_cmd {
	union {
		struct {
			uint32_t state:4;          /* [3-0]   */
			uint32_t unused:8;         /* [11-4]  */
			uint32_t count:11;         /* [22-12] */
			uint32_t opcode:5;         /* [27-23] */
			uint32_t type:4;           /* [31-27] */
		};
		uint32_t header;
	};

	/* payload */
	uint32_t start_cuidx;
	uint32_t num_cus;
	uint32_t sk_size;
	uint32_t sk_name[8];
	uint64_t sk_addr;
};

/**
 * struct unconfigure_sk_cmd: unconfigure soft kernel command format
 *
 * @state:           [3-0] current state of a command
 * @count:           [22-12] number of words in payload (5 + num_cus)
 * @opcode:          [27-23] 1, opcode for configure
 * @type:            [31-27] 0, type of configure
 *
 * @start_cuidx:     start index of compute units
 * @num_cus:         number of compute units in program
 */
struct unconfigure_sk_cmd {
	union {
		struct {
			uint32_t state:4;          /* [3-0]   */
			uint32_t unused:8;         /* [11-4]  */
			uint32_t count:11;         /* [22-12] */
			uint32_t opcode:5;         /* [27-23] */
			uint32_t type:4;           /* [31-27] */
		};
		uint32_t header;
	};

	/* payload */
	uint32_t start_cuidx;
	uint32_t num_cus;
};

/**
 * struct abort_cmd: abort command format.
 *
 * @idx: The slot index of command to abort
 */
struct abort_cmd {
	union {
		struct {
			uint32_t state:4;          /* [3-0]   */
			uint32_t unused:11;        /* [14-4]  */
			uint32_t idx:8;            /* [22-15] */
			uint32_t opcode:5;         /* [27-23] */
			uint32_t type:4;           /* [31-27] */
		};
		uint32_t header;
	};
};

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

	u32                        cu_status[MAX_U32_CU_MASKS];
	unsigned int               num_cu_masks; /* ((num_cus-1)>>5+1 */

	/* Soft kernel definitions */
	u32                        scu_status[MAX_U32_CU_MASKS];

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
	enum cmd_state state;
	int cu_idx; /* running cu, initialized to -1 */
	int slot_idx;
	int cq_slot_idx;
	void *buffer;
	void (*free_buffer)(struct sched_cmd *xcmd);

	/* The actual cmd object representation */
	struct sched_packet *packet;
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
