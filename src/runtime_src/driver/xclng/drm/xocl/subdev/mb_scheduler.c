/*
 * Copyright (C) 2018-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Soren Soe <soren.soe@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

/*
 * Kernel Driver Scheduler (KDS) for XRT
 *
 * struct xocl_cmd
 *  - wraps exec BOs create from user space
 *  - transitions through a number of states
 *  - initially added to pending command queue
 *  - consumed by scheduler which manages its execution (state transition)
 * struct xcol_cu
 *  - compute unit for executing commands
 *  - used only without embedded scheduler (ert)
 *  - talks to HW compute units
 * struct xocl_ert
 *  - embedded scheduler for executing commands on ert
 *  - talks to HW ERT
 * struct exec_core
 *  - execution core managing execution on one device
 * struct xocl_scheduler
 *  - manages execution of cmds on one or more exec cores
 *  - executed in a separate kernel thread
 *  - loops repeatedly when there is work to do
 *  - moves pending commands into a scheduler command queue
 *
 * [new -> pending]. The xocl API adds exec BOs to KDS.	 The exec BOs are
 * wrapped in a xocl_cmd object and added to a pending command queue.
 *
 * [pending -> queued]. Scheduler loops repeatedly and copies pending commands
 * to its own command queue, then managaes command execution on one or more
 * execution cores.
 *
 * [queued -> submitted]. Commands are submitted for execution on execution
 * core when the core has room for new commands.
 *
 * [submitted -> running]. Once submitted, a command is transition by
 * scheduler into running state when there is an available compute unit (no
 * ert) or if ERT is used, then when ERT has room.
 *
 * [running -> complete]. Commands running on ERT complete by sending an
 * interrupt to scheduler.  When ERT is not used, commands are running on a
 * compute unit and are polled for completion.
 */

#include <linux/bitmap.h>
#include <linux/list.h>
#include <linux/eventfd.h>
#include <linux/kthread.h>
#include <ert.h>
#include "../xocl_drv.h"
#include "../userpf/common.h"

//#define SCHED_VERBOSE

#if defined(__GNUC__)
#define SCHED_UNUSED __attribute__((unused))
#endif

#define sched_debug_packet(packet, size)				\
({									\
	int i;								\
	u32 *data = (u32 *)packet;					\
	for (i = 0; i < size; ++i)					    \
		DRM_INFO("packet(0x%p) data[%d] = 0x%x\n", data, i, data[i]); \
})

#ifdef SCHED_VERBOSE
# define SCHED_DEBUG(msg) DRM_INFO(msg)
# define SCHED_DEBUGF(format, ...) DRM_INFO(format, ##__VA_ARGS__)
# define SCHED_PRINTF(format, ...) DRM_INFO(format, ##__VA_ARGS__)
# define SCHED_DEBUG_PACKET(packet, size) sched_debug_packet(packet, size)
#else
# define SCHED_DEBUG(msg)
# define SCHED_DEBUGF(format, ...)
# define SCHED_PRINTF(format, ...) DRM_INFO(format, ##__VA_ARGS__)
# define SCHED_DEBUG_PACKET(packet, size)
#endif

/* constants */
static const unsigned int no_index = -1;

/* FFA	handling */
static const u32 AP_START    = 0x1;
static const u32 AP_DONE     = 0x2;
static const u32 AP_IDLE     = 0x4;
static const u32 AP_READY    = 0x8;
static const u32 AP_CONTINUE = 0x10;

/* Forward declaration */
struct exec_core;
struct exec_ops;
struct xocl_scheduler;

static int validate(struct platform_device *pdev, struct client_ctx *client,
		    const struct drm_xocl_bo *bo);
static bool exec_is_flush(struct exec_core *exec);
static void exec_ert_clear_csr(struct exec_core *exec);
static void scheduler_wake_up(struct xocl_scheduler *xs);
static void scheduler_intr(struct xocl_scheduler *xs);
static void scheduler_decr_poll(struct xocl_scheduler *xs);

/*
 */
static void
xocl_bitmap_to_arr32(u32 *buf, const unsigned long *bitmap, unsigned int nbits)
{
	unsigned int i, halfwords;

	halfwords = DIV_ROUND_UP(nbits, 32);
	for (i = 0; i < halfwords; i++) {
		buf[i] = (u32) (bitmap[i/2] & UINT_MAX);
		if (++i < halfwords)
			buf[i] = (u32) (bitmap[i/2] >> 32);
	}

	/* Clear tail bits in last element of array beyond nbits. */
	if (nbits % BITS_PER_LONG)
		buf[halfwords - 1] &= (u32) (UINT_MAX >> ((-nbits) & 31));
}

static void
xocl_bitmap_from_arr32(unsigned long *bitmap, const u32 *buf, unsigned int nbits)
{
	unsigned int i, halfwords;

	halfwords = DIV_ROUND_UP(nbits, 32);
	for (i = 0; i < halfwords; i++) {
		bitmap[i/2] = (unsigned long) buf[i];
		if (++i < halfwords)
			bitmap[i/2] |= ((unsigned long) buf[i]) << 32;
	}

	/* Clear tail bits in last word beyond nbits. */
	if (nbits % BITS_PER_LONG)
		bitmap[(halfwords - 1) / 2] &= BITMAP_LAST_WORD_MASK(nbits);
}


/**
 * slot_mask_idx() - Slot mask idx index for a given slot_idx
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of the slot mask containing the slot_idx
 */
static inline unsigned int
slot_mask_idx(unsigned int slot_idx)
{
	return slot_idx >> 5;
}

/**
 * slot_idx_in_mask() - Index of command queue slot within the mask that contains it
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of slot within the mask that contains it
 */
static inline unsigned int
slot_idx_in_mask(unsigned int slot_idx)
{
	return slot_idx - (slot_mask_idx(slot_idx) << 5);
}

/**
 * Command data used by scheduler
 *
 * @list: command object moves from pending to commmand queue list
 * @cu_list: command object is added to CU list when running (penguin only)
 *
 * @bo: underlying drm buffer object
 * @exec: execution device associated with this command
 * @client: client (user process) context that created this command
 * @xs: command scheduler responsible for schedulint this command
 * @state: state of command object per scheduling
 * @id: unique id for an active command object
 * @cu_idx: index of CU executing this cmd object; used in penguin mode only
 * @slot_idx: command queue index of this command object
 * @wait_count: number of commands that must trigger this command before it can start
 * @chain_count: number of commands that this command must trigger when it completes
 * @chain: list of commands to trigger upon completion; maximum chain depth is 8
 * @deps: list of commands this object depends on, converted to chain when command is queued
 * @packet: mapped ert packet object from user space
 */

struct xocl_cmd {
	struct list_head cq_list; // scheduler command queue
	struct list_head rq_list; // exec core running queue

	/* command packet */
	struct drm_xocl_bo *bo;
	union {
		struct ert_packet	    *ert_pkt;
		struct ert_configure_cmd    *ert_cfg;
		struct ert_start_kernel_cmd *ert_cu;
		struct ert_start_copybo_cmd *ert_cp;
	};

	DECLARE_BITMAP(cu_bitmap, MAX_CUS);

	struct xocl_dev	  *xdev;
	struct exec_core  *exec;
	struct client_ctx *client;
	struct xocl_scheduler *xs;
	enum ert_cmd_state state;

	/* dependency handling */
	unsigned int chain_count;
	unsigned int wait_count;
	union {
		struct xocl_cmd *chain[8];
		struct drm_xocl_bo *deps[8];
	};

	unsigned long uid;     // unique id for this command
	unsigned int cu_idx;   // index of CU running this cmd (penguin mode)
	unsigned int slot_idx; // index in exec core submit queue
};

/*
 * List of free xocl_cmd objects.
 *
 * @free_cmds: populated with recycled xocl_cmd objects
 * @cmd_mutex: mutex lock for cmd_list
 *
 * Command objects are recycled for later use and only freed when kernel
 * module is unloaded.
 */
static LIST_HEAD(free_cmds);
static DEFINE_MUTEX(free_cmds_mutex);

/**
 * delete_cmd_list() - reclaim memory for all allocated command objects
 */
static void
cmd_list_delete(void)
{
	struct xocl_cmd *xcmd;
	struct list_head *pos, *next;

	mutex_lock(&free_cmds_mutex);
	list_for_each_safe(pos, next, &free_cmds) {
		xcmd = list_entry(pos, struct xocl_cmd, cq_list);
		list_del(pos);
		kfree(xcmd);
	}
	mutex_unlock(&free_cmds_mutex);
}

/*
 * opcode() - Command opcode
 *
 * @cmd: Command object
 * Return: Opcode per command packet
 */
static inline u32
cmd_opcode(struct xocl_cmd *xcmd)
{
	return xcmd->ert_pkt->opcode;
}

/*
 * type() - Command type
 *
 * @cmd: Command object
 * Return: Type of command
 */
static inline u32
cmd_type(struct xocl_cmd *xcmd)
{
	return xcmd->ert_pkt->type;
}

/*
 * exec() - Get execution core
 */
static inline struct exec_core *
cmd_exec(struct xocl_cmd *xcmd)
{
	return xcmd->exec;
}

/*
 * uid() - Get unique id of command
 */
static inline unsigned long
cmd_uid(struct xocl_cmd *xcmd)
{
	return xcmd->uid;
}

/*
 */
static inline unsigned int
cmd_wait_count(struct xocl_cmd *xcmd)
{
	return xcmd->wait_count;
}

/**
 * payload_size() - Command payload size
 *
 * @xcmd: Command object
 * Return: Size in number of words of command packet payload
 */
static inline unsigned int
cmd_payload_size(struct xocl_cmd *xcmd)
{
	return xcmd->ert_pkt->count;
}

/**
 * cmd_packet_size() - Command packet size
 *
 * @xcmd: Command object
 * Return: Size in number of words of command packet
 */
static inline unsigned int
cmd_packet_size(struct xocl_cmd *xcmd)
{
	return cmd_payload_size(xcmd) + 1;
}

/**
 * cu_masks() - Number of command packet cu_masks
 *
 * @xcmd: Command object
 * Return: Total number of CU masks in command packet
 */
static inline unsigned int
cmd_cumasks(struct xocl_cmd *xcmd)
{
	return 1 + xcmd->ert_cu->extra_cu_masks;
}

/**
 * regmap_size() - Size of regmap is payload size (n) minus the number of cu_masks
 *
 * @xcmd: Command object
 * Return: Size of register map in number of words
 */
static inline unsigned int
cmd_regmap_size(struct xocl_cmd *xcmd)
{
	return cmd_payload_size(xcmd) - cmd_cumasks(xcmd);
}

/*
 */
static inline struct ert_packet*
cmd_packet(struct xocl_cmd *xcmd)
{
	return xcmd->ert_pkt;
}

/*
 */
static inline u32*
cmd_regmap(struct xocl_cmd *xcmd)
{
	return xcmd->ert_cu->data + xcmd->ert_cu->extra_cu_masks;
}

/**
 * cmd_set_int_state() - Set internal command state used by scheduler only
 *
 * @xcmd: command to change internal state on
 * @state: new command state per ert.h
 */
static inline void
cmd_set_int_state(struct xocl_cmd *xcmd, enum ert_cmd_state state)
{
	SCHED_DEBUGF("-> %s(%lu,%d)\n", __func__, xcmd->uid, state);
	xcmd->state = state;
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * cmd_set_state() - Set both internal and external state of a command
 *
 * The state is reflected externally through the command packet
 * as well as being captured in internal state variable
 *
 * @xcmd: command object
 * @state: new state
 */
static inline void
cmd_set_state(struct xocl_cmd *xcmd, enum ert_cmd_state state)
{
	SCHED_DEBUGF("->%s(%lu,%d)\n", __func__, xcmd->uid, state);
	xcmd->state = state;
	xcmd->ert_pkt->state = state;
	SCHED_DEBUGF("<-%s\n", __func__);
}

/*
 * update_state() - Update command state if client has aborted
 */
static enum ert_cmd_state
cmd_update_state(struct xocl_cmd *xcmd)
{
	if (xcmd->state != ERT_CMD_STATE_RUNNING && xcmd->client->abort) {
		userpf_info(xcmd->xdev, "aborting stale client cmd(%lu)", xcmd->uid);
		cmd_set_state(xcmd, ERT_CMD_STATE_ABORT);
	}
	if (exec_is_flush(xcmd->exec)) {
		userpf_info(xcmd->xdev, "aborting stale exec cmd(%lu)", xcmd->uid);
		cmd_set_state(xcmd, ERT_CMD_STATE_ABORT);
	}
	return xcmd->state;
}

/*
 * release_gem_object_reference() -
 */
static inline void
cmd_release_gem_object_reference(struct xocl_cmd *xcmd)
{
	if (xcmd->bo)
		drm_gem_object_unreference_unlocked(&xcmd->bo->base);
}

/*
 */
static inline void
cmd_mark_active(struct xocl_cmd *xcmd)
{
	if (xcmd->bo)
		xcmd->bo->metadata.active = xcmd;
}

/*
 */
static inline void
cmd_mark_deactive(struct xocl_cmd *xcmd)
{
	if (xcmd->bo)
		xcmd->bo->metadata.active = NULL;
}

/**
 * chain_dependencies() - Chain this command to its dependencies
 *
 * @xcmd: Command to chain to its dependencies
 *
 * This function looks at all incoming explicit BO dependencies, checks if a
 * corresponding xocl_cmd object exists (is active) in which case that command
 * object must chain argument xcmd so that it (xcmd) can be triggered when
 * dependency completes.  The chained command has a wait count correponding to
 * the number of dependencies that are active.
 */
static int
cmd_chain_dependencies(struct xocl_cmd *xcmd)
{
	int didx;
	int dcount = xcmd->wait_count;

	SCHED_DEBUGF("-> chain_dependencies of xcmd(%lu)\n", xcmd->uid);
	for (didx = 0; didx < dcount; ++didx) {
		struct drm_xocl_bo *dbo = xcmd->deps[didx];
		struct xocl_cmd *chain_to = dbo->metadata.active;
		// release reference created in ioctl call when dependency was looked up
		// see comments in xocl_ioctl.c:xocl_execbuf_ioctl()
		drm_gem_object_unreference_unlocked(&dbo->base);
		xcmd->deps[didx] = NULL;
		if (!chain_to) { // command may have completed already
			--xcmd->wait_count;
			continue;
		}
		if (chain_to->chain_count >= MAX_DEPS) {
			DRM_INFO("chain count exceeded");
			return 1;
		}
		SCHED_DEBUGF("+ xcmd(%lu)->chain[%d]=xcmd(%lu)", chain_to->uid, chain_to->chain_count, xcmd->uid);
		chain_to->chain[chain_to->chain_count++] = xcmd;
	}
	SCHED_DEBUG("<- chain_dependencies\n");
	return 0;
}

/**
 * trigger_chain() - Trigger the execution of any commands chained to argument command
 *
 * @xcmd: Completed command that must trigger its chained (waiting) commands
 *
 * The argument command has completed and must trigger the execution of all
 * chained commands whos wait_count is 0.
 */
static void
cmd_trigger_chain(struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> trigger_chain xcmd(%lu)\n", xcmd->uid);
	while (xcmd->chain_count) {
		struct xocl_cmd *trigger = xcmd->chain[--xcmd->chain_count];

		SCHED_DEBUGF("+ cmd(%lu) triggers cmd(%lu) with wait_count(%d)\n",
			     xcmd->uid, trigger->uid, trigger->wait_count);
		// decrement trigger wait count
		// scheduler will submit when wait count reaches zero
		--trigger->wait_count;
	}
	SCHED_DEBUG("<- trigger_chain\n");
}


/**
 * cmd_get() - Get a free command object
 *
 * Get from free/recycled list or allocate a new command if necessary.
 *
 * Return: Free command object
 */
static struct xocl_cmd*
cmd_get(struct xocl_scheduler *xs, struct exec_core *exec, struct client_ctx *client)
{
	struct xocl_cmd *xcmd;
	static unsigned long count;

	mutex_lock(&free_cmds_mutex);
	xcmd = list_first_entry_or_null(&free_cmds, struct xocl_cmd, cq_list);
	if (xcmd)
		list_del(&xcmd->cq_list);
	mutex_unlock(&free_cmds_mutex);
	if (!xcmd)
		xcmd = kmalloc(sizeof(struct xocl_cmd), GFP_KERNEL);
	if (!xcmd)
		return ERR_PTR(-ENOMEM);
	xcmd->uid = count++;
	xcmd->exec = exec;
	xcmd->cu_idx = no_index;
	xcmd->slot_idx = no_index;
	xcmd->xs = xs;
	xcmd->xdev = client->xdev;
	xcmd->client = client;
	xcmd->bo = NULL;
	xcmd->ert_pkt = NULL;
	atomic_inc(&client->outstanding_execs);
	SCHED_DEBUGF("xcmd(%lu) xcmd(%p) [-> new ]\n", xcmd->uid, xcmd);
	return xcmd;
}

/**
 * cmd_free() - free a command object
 *
 * @xcmd: command object to free (move to freelist)
 *
 * The command *is* in some current list (scheduler command queue)
 */
static void
cmd_free(struct xocl_cmd *xcmd)
{
	cmd_release_gem_object_reference(xcmd);

	mutex_lock(&free_cmds_mutex);
	list_move_tail(&xcmd->cq_list, &free_cmds);
	mutex_unlock(&free_cmds_mutex);

	atomic_dec(&xcmd->xdev->outstanding_execs);
	atomic_dec(&xcmd->client->outstanding_execs);
	SCHED_DEBUGF("xcmd(%lu) [-> free]\n", xcmd->uid);
}

/**
 * abort_cmd() - abort command object before it becomes pending
 *
 * @xcmd: command object to abort (move to freelist)
 *
 * Command object is *not* in any current list
 *
 * Return: 0
 */
static void
cmd_abort(struct xocl_cmd *xcmd)
{
	mutex_lock(&free_cmds_mutex);
	list_add_tail(&xcmd->cq_list, &free_cmds);
	mutex_unlock(&free_cmds_mutex);

	atomic_dec(&xcmd->client->outstanding_execs);
	SCHED_DEBUGF("xcmd(%lu) [-> abort]\n", xcmd->uid);
}

/*
 * cmd_bo_init() - Initialize a command object with an exec BO
 *
 * In penguin mode, the command object caches the CUs available
 * to execute the command.  When ERT is enabled, the CU info
 * is not used.
 */
static void
cmd_bo_init(struct xocl_cmd *xcmd, struct drm_xocl_bo *bo,
	    int numdeps, struct drm_xocl_bo **deps, bool penguin)
{
	SCHED_DEBUGF("%s(%lu,bo,%d,deps,%d)\n", __func__, xcmd->uid, numdeps, penguin);
	xcmd->bo = bo;
	xcmd->ert_pkt = (struct ert_packet *)bo->vmapping;

	// in kds mode copy pkt cus to command object cu bitmap
	if (penguin && cmd_type(xcmd) == ERT_CU) {
		unsigned int i = 0;
		u32 cumasks[4] = {0};

		cumasks[0] = xcmd->ert_cu->cu_mask;
		SCHED_DEBUGF("+ xcmd(%lu) cumask[0]=0x%x\n", xcmd->uid, cumasks[0]);
		for (i = 0; i < xcmd->ert_cu->extra_cu_masks; ++i) {
			cumasks[i+1] = xcmd->ert_cu->data[i];
			SCHED_DEBUGF("+ xcmd(%lu) cumask[%d]=0x%x\n", xcmd->uid, i+1, cumasks[i+1]);
		}
		xocl_bitmap_from_arr32(xcmd->cu_bitmap, cumasks, MAX_CUS);
	}

	// dependencies are copied here, the anticipated wait_count is number
	// of specified dependencies.  The wait_count is adjusted when the
	// command is queued in the scheduler based on whether or not a
	// dependency is active (managed by scheduler)
	memcpy(xcmd->deps, deps, numdeps*sizeof(struct drm_xocl_bo *));
	xcmd->wait_count = numdeps;
	xcmd->chain_count = 0;
}

/*
 */
static void
cmd_packet_init(struct xocl_cmd *xcmd, struct ert_packet *packet)
{
	SCHED_DEBUGF("%s(%lu,packet)\n", __func__, xcmd->uid);
	xcmd->ert_pkt = packet;
}

/*
 * cmd_has_cu() - Check if this command object can execute on CU
 *
 * @cuidx: the index of the CU.	 Note that CU indicies start from 0.
 */
static int
cmd_has_cu(struct xocl_cmd *xcmd, unsigned int cuidx)
{
	SCHED_DEBUGF("%s(%lu,%d) = %d\n", __func__, xcmd->uid, cuidx, test_bit(cuidx, xcmd->cu_bitmap));
	return test_bit(cuidx, xcmd->cu_bitmap);
}

/**
 * struct xocl_cu: Represents a compute unit in penguin mode
 *
 * @running_queue: a fifo representing commands running on this CU
 * @xdev: the xrt device with this CU
 * @idx: index of this CU
 * @dataflow: true when running in dataflow mode
 * @base: exec base address of this CU
 * @addr: base address of this CU
 * @polladdr: address of CU poll request register
 * @ctrlreg: state of the CU (value of AXI-lite control register)
 * @done_cnt: number of commands that have completed (<=running_queue.size())
 * @run_cnt: number of commands that have bee started (<=running_queue.size())
 */
struct xocl_cu {
	struct list_head   running_queue;
	struct xocl_dev    *xdev;
	unsigned int       idx;
	unsigned int       uid;
	bool               dataflow;
	void __iomem       *base;
	u32                addr;
	u32                polladdr;

	u32                ctrlreg;
	unsigned int       done_cnt;
	unsigned int       run_cnt;
};

/*
 */
void
cu_reset(struct xocl_cu *xcu, unsigned int idx, void __iomem *base, u32 addr, u32 polladdr)
{
	xcu->idx = idx;
	xcu->dataflow = (addr & 0xFF) == AP_CTRL_CHAIN;
	xcu->base = base;
	xcu->addr = addr & ~(0xFF); // clear encoded handshake
	xcu->polladdr = polladdr;
	xcu->ctrlreg = 0;
	xcu->done_cnt = 0;
	xcu->run_cnt = 0;
	userpf_info(xcu->xdev, "configured cu(%d) base@0x%x poll@0x%x dataflow(%d)\n",
		    xcu->idx, xcu->addr, xcu->polladdr, xcu->dataflow);
}

/**
 */
struct xocl_cu *
cu_create(struct xocl_dev *xdev)
{
	struct xocl_cu *xcu = kmalloc(sizeof(struct xocl_cu), GFP_KERNEL);
	static unsigned int uid;

	INIT_LIST_HEAD(&xcu->running_queue);
	xcu->xdev = xdev;
	xcu->uid = uid++;
	SCHED_DEBUGF("%s(uid:%d)\n", __func__, xcu->uid);
	return xcu;
}

static inline u32
cu_base_addr(struct xocl_cu *xcu)
{
	return xcu->addr;
}

/**
 */
void
cu_destroy(struct xocl_cu *xcu)
{
	SCHED_DEBUGF("%s(uid:%d)\n", __func__, xcu->uid);
	kfree(xcu);
}

/**
 * cu_continue() - Acknowledge AP_DONE by sending AP_CONTINUE
 *
 * Applicable to dataflow only.
 *
 * In ERT poll mode, also write to the CQ slot corresponding to the CU.  ERT
 * prevents host notification of next AP_DONE until first AP_DONE is
 * acknowledged by host.  Do not acknowledge ERT if no outstanding jobs on CU;
 * this prevents stray notifications from ERT.
 */
void
cu_continue(struct xocl_cu *xcu)
{
	if (!xcu->dataflow)
		return;

	SCHED_DEBUGF("-> %s cu(%d) @0x%x\n", __func__, xcu->idx, xcu->addr);

	// acknowledge done directly to CU (xcu->addr)
	iowrite32(AP_CONTINUE, xcu->base + xcu->addr);

	// in ert_poll mode acknowlegde done to ERT
	if (xcu->polladdr && xcu->run_cnt) {
		SCHED_DEBUGF("+ @0x%x\n", xcu->polladdr);
		iowrite32(AP_CONTINUE, xcu->base + xcu->polladdr);
	}

	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * cu_poll() - Poll a CU for its status
 *
 * Used in penguin and ert_poll mode only. Read the CU control register and
 * update run and done count as necessary.  Acknowledge any AP_DONE received
 * from kernel.  Check for AP_IDLE since ERT in poll mode will also read the
 * kernel control register and AP_DONE is COR.
 */
void
cu_poll(struct xocl_cu *xcu)
{
	// assert !list_empty(&running_queue)
	SCHED_DEBUGF("-> %s cu(%d) @0x%x done(%d) run(%d)\n", __func__,
		     xcu->idx, xcu->addr, xcu->done_cnt, xcu->run_cnt);

	xcu->ctrlreg = ioread32(xcu->base + xcu->addr);

	SCHED_DEBUGF("+ ctrlreg(0x%x)\n", xcu->ctrlreg);

	if (xcu->run_cnt && (xcu->ctrlreg & (AP_DONE|AP_IDLE))) {
		++xcu->done_cnt; // assert done_cnt <= |running_queue|
		--xcu->run_cnt;
		cu_continue(xcu);
	}

	SCHED_DEBUGF("<- %s cu(%d) done(%d) run(%d)\n", __func__,
		     xcu->idx, xcu->done_cnt, xcu->run_cnt);
}

/**
 * cu_ready() - Check if CU is ready to start another command
 *
 * Return: True if ready false otherwise.
 * The CU is ready when AP_START is low.  Poll the CU if necessary.
 */
static bool
cu_ready(struct xocl_cu *xcu)
{
	SCHED_DEBUGF("-> %s cu(%d)\n", __func__, xcu->idx);

	if ((xcu->ctrlreg & AP_START) || (!xcu->dataflow && xcu->run_cnt))
		cu_poll(xcu);

	SCHED_DEBUGF("<- %s returns %d\n", __func__,
		     xcu->dataflow ? !(xcu->ctrlreg & AP_START) : xcu->run_cnt == 0);

	return xcu->dataflow ? !(xcu->ctrlreg & AP_START) : xcu->run_cnt == 0;
}

/**
 * cu_first_done() - Get the first completed command from the running queue
 *
 * Return: The first command that has completed or nullptr if none
 */
static struct xocl_cmd*
cu_first_done(struct xocl_cu *xcu)
{
	SCHED_DEBUGF("-> %s cu(%d) done(%d) run(%d)\n", __func__, xcu->idx, xcu->done_cnt, xcu->run_cnt);

	if (!xcu->done_cnt && xcu->run_cnt)
		cu_poll(xcu);

	SCHED_DEBUGF("<- %s done(%d) run(%d)\n", __func__, xcu->done_cnt, xcu->run_cnt);

	return xcu->done_cnt
		? list_first_entry(&xcu->running_queue, struct xocl_cmd, rq_list)
		: NULL;
}

/**
 * cu_pop_done() - Remove first element from running queue
 */
static void
cu_pop_done(struct xocl_cu *xcu)
{
	struct xocl_cmd *xcmd;

	if (!xcu->done_cnt)
		return;

	xcmd = list_first_entry(&xcu->running_queue, struct xocl_cmd, rq_list);
	list_del(&xcmd->rq_list);
	--xcu->done_cnt;

	SCHED_DEBUGF("%s(%d) xcmd(%lu) done(%d) run(%d)\n", __func__,
		     xcu->idx, xcmd->uid, xcu->done_cnt, xcu->run_cnt);
}

/**
 * cu_configure_ooo() - Configure a CU with {addr,val} pairs
 */
static void
cu_configure_ooo(struct xocl_cu *xcu, struct xocl_cmd *xcmd)
{
	unsigned int size = cmd_regmap_size(xcmd);
	u32 *regmap = cmd_regmap(xcmd);
	unsigned int idx;

	SCHED_DEBUGF("-> %s cu(%d) xcmd(%lu)\n", __func__, xcu->idx, xcmd->uid);
	for (idx = 4; idx < size - 1; idx += 2) {
		u32 offset = *(regmap + idx);
		u32 val = *(regmap + idx + 1);

		SCHED_DEBUGF("+ base[0x%x] = 0x%x\n", offset, val);
		iowrite32(val, xcu->base + offset);
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * cu_configure_ino() - Configure a CU with consecutive layout
 */
static void
cu_configure_ino(struct xocl_cu *xcu, struct xocl_cmd *xcmd)
{
	unsigned int size = cmd_regmap_size(xcmd);
	u32 *regmap = cmd_regmap(xcmd);
	unsigned int idx;

	SCHED_DEBUGF("-> %s cu(%d) xcmd(%lu)\n", __func__, xcu->idx, xcmd->uid);
	for (idx = 4; idx < size; ++idx)
		iowrite32(*(regmap + idx), xcu->base + xcu->addr + (idx << 2));
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * cu_start() - Start the CU with a new command.
 *
 * The command is pushed onto the running queue
 */
static bool
cu_start(struct xocl_cu *xcu, struct xocl_cmd *xcmd)
{
	// assert(!(ctrlreg & AP_START), "cu not ready");
	SCHED_DEBUGF("-> %s cu(%d) cmd(%lu)\n", __func__, xcu->idx, xcmd->uid);

	// past header, past cumasks
	SCHED_DEBUG_PACKET(cmd_regmap(xcmd), cmd_regmap_size(xcmd));

	/* write register map, starting at base + 0x10
	 * 0x0 used for control register
	 * 0x4, 0x8 used for interrupt, which is initialized in setup of ERT
	 * 0xC used for interrupt status, which is set by hardware
	 */
	if (cmd_opcode(xcmd) == ERT_EXEC_WRITE)
		cu_configure_ooo(xcu, xcmd);
	else
		cu_configure_ino(xcu, xcmd);

	// start cu.  update local state as we may not be polling prior
	// to next ready check.
	xcu->ctrlreg |= AP_START;
	iowrite32(AP_START, xcu->base + xcu->addr);

	// in ert poll mode request ERT to poll CU
	if (xcu->polladdr) {
		SCHED_DEBUGF("+ @0x%x\n", xcu->polladdr);
		iowrite32(AP_START, xcu->base + xcu->polladdr);
	}

	// add cmd to end of running queue
	list_add_tail(&xcmd->rq_list, &xcu->running_queue);
	++xcu->run_cnt;

	SCHED_DEBUGF("<- %s cu(%d) started xcmd(%lu) done(%d) run(%d)\n",
		     __func__, xcu->idx, xcmd->uid, xcu->done_cnt, xcu->run_cnt);

	return true;
}


/**
 * sruct xocl_ert: Represents embedded scheduler in ert mode
 */
struct xocl_ert {
	void __iomem *base;
	u32	     cq_addr;
	unsigned int uid;

	unsigned int slot_size;
	bool         cq_intr;
};

/*
 */
struct xocl_ert *
ert_create(void __iomem *base, u32 cq_addr)
{
	struct xocl_ert *xert = kmalloc(sizeof(struct xocl_ert), GFP_KERNEL);
	static unsigned int uid;

	xert->base = base;
	xert->cq_addr = cq_addr;
	xert->uid = uid++;
	xert->slot_size = 0;
	xert->cq_intr = false;
	SCHED_DEBUGF("%s(%d,0x%x)\n", __func__, xert->uid, xert->cq_addr);
	return xert;
}

/**
 */
static void
ert_destroy(struct xocl_ert *xert)
{
	SCHED_DEBUGF("%s(%d)\n", __func__, xert->uid);
	kfree(xert);
}

/**
 */
static void
ert_cfg(struct xocl_ert *xert, unsigned int slot_size, bool cq_intr)
{
	SCHED_DEBUGF("%s(%d) slot_size(%d) cq_intr(%d)\n", __func__, xert->uid, slot_size, cq_intr);
	xert->slot_size = slot_size;
	xert->cq_intr = cq_intr;
}

/**
 * ert_start_cmd() - Start a command in ERT mode
 *
 * @xcmd: command to start
 *
 * Write command packet to ERT command queue
 */
static bool
ert_start_cmd(struct xocl_ert *xert, struct xocl_cmd *xcmd)
{
	u32 slot_addr = xert->cq_addr + xcmd->slot_idx * xert->slot_size;
	struct ert_packet *ecmd = cmd_packet(xcmd);

	SCHED_DEBUG_PACKET(ecmd, cmd_packet_size(xcmd));

	SCHED_DEBUGF("-> %s ert(%d) cmd(%lu)\n", __func__, xert->uid, xcmd->uid);

	// write packet minus header
	SCHED_DEBUGF("++ slot_idx=%d, slot_addr=0x%x\n", xcmd->slot_idx, slot_addr);
	memcpy_toio(xert->base + slot_addr + 4, ecmd->data, (cmd_packet_size(xcmd) - 1) * sizeof(u32));

	// write header
	iowrite32(ecmd->header, xert->base + slot_addr);

	// trigger interrupt to embedded scheduler if feature is enabled
	if (xert->cq_intr) {
		u32 cq_int_addr = ERT_CQ_STATUS_REGISTER_ADDR + (slot_mask_idx(xcmd->slot_idx) << 2);
		u32 mask = 1 << slot_idx_in_mask(xcmd->slot_idx);

		SCHED_DEBUGF("++ mb_submit writes slot mask 0x%x to CQ_INT register at addr 0x%x\n",
			     mask, cq_int_addr);
		iowrite32(mask, xert->base + cq_int_addr);
	}
	SCHED_DEBUGF("<- %s returns true\n", __func__);
	return true;
}

/**
 */
static void
ert_read_custat(struct xocl_ert *xert, unsigned int num_cus, u32 *cu_usage, struct xocl_cmd *xcmd)
{
	u32 slot_addr = xert->cq_addr + xcmd->slot_idx*xert->slot_size;

	memcpy_fromio(cu_usage, xert->base + slot_addr + 4, num_cus * sizeof(u32));
}

/**
 * struct exec_ops: scheduler specific operations
 *
 * @start_cmd: start a command on a device
 * @start_ctrl: starts a control command
 * @query_cmd: check if a command has completed
 * @query_ctrl: check if a control command has completed
 * @process_mask: process command status register from ERT
 *
 * Virtual dispatch table for different modes of operation for a
 * specific execution core (device)
 */
struct exec_ops {
	bool (*start_cmd)(struct exec_core *exec, struct xocl_cmd *xcmd);
	bool (*start_ctrl)(struct exec_core *exec, struct xocl_cmd *xcmd);
	void (*query_cmd)(struct exec_core *exec, struct xocl_cmd *xcmd);
	void (*query_ctrl)(struct exec_core *exec, struct xocl_cmd *xcmd);
	void (*process_mask)(struct exec_core *exec, u32 mask, unsigned int maskidx);
};

static struct exec_ops ert_ops;       // ert mode
static struct exec_ops ert_poll_ops;  // ert polling mode
static struct exec_ops penguin_ops;   // kds mode (no ert)

/**
 * struct exec_core: Core data structure for command execution on a device
 *
 * @ctx_list: Context list populated with device context
 * @exec_lock: Lock for synchronizing external access
 * @poll_wait_queue: Wait queue for device polling
 * @scheduler: Command queue scheduler
 * @submitted_cmds: Tracking of command submitted for execution on this device
 * @num_slots: Number of command queue slots
 * @num_cus: Number of CUs in loaded program
 * @num_cdma: Number of CDMAs in hardware
 * @polling_mode: If set then poll for command completion
 * @cq_interrupt: If set then trigger interrupt to MB on new commands
 * @configured: Flag to indicate that the core data structure has been initialized
 * @stopped: Flag to indicate that the core data structure cannot be used
 * @flush: Flag to indicate that commands for this device should be flushed
 * @cu_usage: Usage count since last reset
 * @slot_status: Bitmap to track status (busy(1)/free(0)) slots in command queue
 * @ctrl_busy: Flag to indicate that slot 0 (ctrl commands) is busy
 * @cu_status: Bitmap to track status (busy(1)/free(0)) of CUs. Unused in ERT mode.
 * @sr0: If set, then status register [0..31] is pending with completed commands (ERT only).
 * @sr1: If set, then status register [32..63] is pending with completed commands (ERT only).
 * @sr2: If set, then status register [64..95] is pending with completed commands (ERT only).
 * @sr3: If set, then status register [96..127] is pending with completed commands (ERT only).
 * @ops: Scheduler operations vtable
 */
struct exec_core {
	struct platform_device	   *pdev;

	struct mutex		   exec_lock;

	void __iomem		   *base;
	u32			   intr_base;
	u32			   intr_num;
	char			   ert_cfg_priv;

	wait_queue_head_t	   poll_wait_queue;

	struct xocl_scheduler	   *scheduler;

	xuid_t			   xclbin_id;

	unsigned int		   num_slots;
	unsigned int		   num_cus;
	unsigned int		   num_cdma;
	unsigned int		   polling_mode;
	unsigned int		   cq_interrupt;
	unsigned int		   configured;
	unsigned int		   stopped;
	unsigned int		   flush;

	struct xocl_cu		   *cus[MAX_CUS];
	struct xocl_ert		   *ert;

	u32			   cu_usage[MAX_CUS];

	// Bitmap tracks busy(1)/free(0) slots in cmd_slots
	struct xocl_cmd		   *submitted_cmds[MAX_SLOTS];
	DECLARE_BITMAP(slot_status, MAX_SLOTS);
	unsigned int		   ctrl_busy;

	// Status register pending complete.  Written by ISR,
	// cleared by scheduler
	atomic_t		   sr0;
	atomic_t		   sr1;
	atomic_t		   sr2;
	atomic_t		   sr3;

	// Operations for dynamic indirection dependt on MB
	// or kernel scheduler
	struct exec_ops		   *ops;

	unsigned int		   uid;
	unsigned int		   ip_reference[MAX_CUS];
};

/**
 * exec_get_pdev() -
 */
static inline struct platform_device *
exec_get_pdev(struct exec_core *exec)
{
	return exec->pdev;
}

/**
 * exec_get_xdev() -
 */
static inline struct xocl_dev *
exec_get_xdev(struct exec_core *exec)
{
	return xocl_get_xdev(exec->pdev);
}

/**
 * Check if exec core is in full ERT mode
 */
static inline bool
exec_is_ert(struct exec_core *exec)
{
	return exec->ops == &ert_ops;
}

/**
 * Check if exec core is in full ERT poll mode
 */
static inline bool
exec_is_ert_poll(struct exec_core *exec)
{
	return exec->ops == &ert_poll_ops;
}

/**
 * Check if exec core is in penguin mode
 */
static inline bool
exec_is_penguin(struct exec_core *exec)
{
	return exec->ops == &penguin_ops;
}

/**
 * Check if exec core is in polling mode
 */
static inline bool
exec_is_polling(struct exec_core *exec)
{
	return exec->polling_mode;
}

/**
 * Check if exec core has been requested to flush commands
 */
static inline bool
exec_is_flush(struct exec_core *exec)
{
	return exec->flush;
}

/**
 * Get base address of a CU
 */
static inline u32
exec_cu_base_addr(struct exec_core *exec, unsigned int cuidx)
{
	return cu_base_addr(exec->cus[cuidx]);
}

/**
 */
static inline u32
exec_cu_usage(struct exec_core *exec, unsigned int cuidx)
{
	return exec->cu_usage[cuidx];
}


/**
 */
static void
exec_cfg(struct exec_core *exec)
{
}

/*
 * to be automated
 */
static int
exec_cfg_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	struct xocl_dev *xdev = exec_get_xdev(exec);
	uint32_t *cdma = xocl_cdma_addr(xdev);
	unsigned int dsa = exec->ert_cfg_priv;
	struct ert_configure_cmd *cfg = xcmd->ert_cfg;
	bool ert = xocl_mb_sched_on(xdev);
	bool ert_full = (ert && cfg->ert && !cfg->dataflow);
	bool ert_poll = (ert && cfg->ert && cfg->dataflow);
	int cuidx = 0;

	/* Only allow configuration with one live ctx */
	if (exec->configured) {
		DRM_INFO("command scheduler is already configured for this device\n");
		return 1;
	}

	userpf_info(xdev, "ert per feature rom = %d", ert);
	userpf_info(xdev, "dsa52 = %d", dsa);

	/* Mark command as control command to force slot 0 execution */
	cfg->type = ERT_CTRL;

	if (cfg->count != 5 + cfg->num_cus) {
		userpf_err(xdev, "invalid configure command, count=%d expected 5+num_cus(%d)\n",
			   cfg->count, cfg->num_cus);
		return 1;
	}

	SCHED_DEBUG("configuring scheduler\n");
	exec->num_slots = ERT_CQ_SIZE / cfg->slot_size;
	exec->num_cus = cfg->num_cus;
	exec->num_cdma = 0;

	if (ert_poll)
		// Adjust slot size for ert poll mode
		cfg->slot_size = ERT_CQ_SIZE / MAX_CUS;

	// Create CUs for regular CUs
	for (cuidx = 0; cuidx < exec->num_cus; ++cuidx) {
		struct xocl_cu *xcu = exec->cus[cuidx];
		u32 polladdr = (ert_poll)
			// cuidx+1 to reserve slot 0 for ctrl => max 127 CUs in ert_poll mode
			? ERT_CQ_BASE_ADDR + (cuidx+1) * cfg->slot_size
			: 0;

		if (!xcu)
			xcu = exec->cus[cuidx] = cu_create(xdev);
		cu_reset(xcu, cuidx, exec->base, cfg->data[cuidx], polladdr);
	}

	// Create KDMA CUs
	if (cdma) {
		uint32_t *addr = 0;

		for (addr = cdma; addr < cdma+4; ++addr) { /* 4 is from xclfeatures.h */
			if (*addr) {
				struct xocl_cu *xcu = exec->cus[cuidx];
				u32 polladdr = (ert_poll)
					? ERT_CQ_BASE_ADDR + (cuidx+1) * cfg->slot_size
					: 0;

				if (!xcu)
					xcu = exec->cus[cuidx] = cu_create(xdev);
				cu_reset(xcu, cuidx, exec->base, *addr, polladdr);
				++exec->num_cus;
				++exec->num_cdma;
				++cfg->num_cus;
				++cfg->count;
				cfg->data[cuidx] = *addr;
				++cuidx;
			}
		}
	}

	if ((ert_full || ert_poll) && !exec->ert)
		exec->ert = ert_create(exec->base, ERT_CQ_BASE_ADDR);

	if (ert_poll) {
		userpf_info(xdev, "configuring dataflow mode with ert polling\n");
		cfg->slot_size = ERT_CQ_SIZE / MAX_CUS;
		cfg->cu_isr = 0;
		cfg->cu_dma = 0;
		ert_cfg(exec->ert, cfg->slot_size, cfg->cq_int);
		exec->ops = &ert_poll_ops;
		exec->polling_mode = cfg->polling;
	} else if (ert_full) {
		userpf_info(xdev, "configuring embedded scheduler mode\n");
		ert_cfg(exec->ert, cfg->slot_size, cfg->cq_int);
		exec->ops = &ert_ops;
		exec->polling_mode = cfg->polling;
		exec->cq_interrupt = cfg->cq_int;
		cfg->dsa52 = dsa;
		cfg->cdma = cdma ? 1 : 0;
	} else {
		userpf_info(xdev, "configuring penguin scheduler mode\n");
		exec->ops = &penguin_ops;
		exec->polling_mode = 1;
	}

	// reserve slot 0 for control commands
	set_bit(0, exec->slot_status);

	userpf_info(xdev, "scheduler config ert(%d), dataflow(%d), slots(%d), cudma(%d), cuisr(%d), cdma(%d), cus(%d)\n"
		 , ert_poll | ert_full
		 , cfg->dataflow
		 , exec->num_slots
		 , cfg->cu_dma ? 1 : 0
		 , cfg->cu_isr ? 1 : 0
		 , exec->num_cdma
		 , exec->num_cus);

	exec->configured = true;
	return 0;
}

/**
 * exec_reset() - Reset the scheduler
 *
 * @exec: Execution core (device) to reset
 *
 * TODO: Perform scheduler configuration based on current xclbin
 *	 rather than relying of cfg command
 */
static void
exec_reset(struct exec_core *exec)
{
	struct xocl_dev *xdev = exec_get_xdev(exec);
	xuid_t *xclbin_id;

	mutex_lock(&exec->exec_lock);

	xclbin_id = XOCL_XCLBIN_ID(xdev);

	userpf_info(xdev, "%s(%d) cfg(%d)\n", __func__, exec->uid, exec->configured);

	// only reconfigure the scheduler on new xclbin
	if (!xclbin_id || (uuid_equal(&exec->xclbin_id, xclbin_id) && exec->configured)) {
		exec->stopped = false;
		goto out;
	}

	userpf_info(xdev, "exec->xclbin(%pUb),xclbin(%pUb)\n", &exec->xclbin_id, xclbin_id);
	userpf_info(xdev, "%s resets for new xclbin", __func__);
	memset(exec->cu_usage, 0, MAX_CUS * sizeof(u32));
	uuid_copy(&exec->xclbin_id, xclbin_id);
	exec->num_cus = 0;
	exec->num_cdma = 0;

	exec->num_slots = 16;
	exec->polling_mode = 1;
	exec->cq_interrupt = 0;
	exec->configured = false;
	exec->stopped = false;
	exec->flush = false;
	exec->ops = &penguin_ops;

	bitmap_zero(exec->slot_status, MAX_SLOTS);
	set_bit(0, exec->slot_status); // reserve for control command
	exec->ctrl_busy = false;

	atomic_set(&exec->sr0, 0);
	atomic_set(&exec->sr1, 0);
	atomic_set(&exec->sr2, 0);
	atomic_set(&exec->sr3, 0);

	exec_cfg(exec);

out:
	mutex_unlock(&exec->exec_lock);
}

/**
 * exec_stop() - Stop the scheduler from scheduling commands on this core
 *
 * @exec:  Execution core (device) to stop
 *
 * Block access to current exec_core (device).	This API must be called prior
 * to performing an AXI reset and downloading of a new xclbin.	Calling this
 * API flushes the commands running on current device and prevents new
 * commands from being scheduled on the device.	 This effectively prevents any
 * further commands from running on the device
 */
static void
exec_stop(struct exec_core *exec)
{
	int idx;
	struct xocl_dev *xdev = exec_get_xdev(exec);
	unsigned int outstanding = 0;
	unsigned int wait_ms = 100;
	unsigned int retry = 20;  // 2 sec

	mutex_lock(&exec->exec_lock);
	userpf_info(xdev, "%s(%p)\n", __func__, exec);
	exec->stopped = true;
	exec_ert_clear_csr(exec);
	mutex_unlock(&exec->exec_lock);

	// Wait for commands to drain if any
	outstanding = atomic_read(&xdev->outstanding_execs);
	while (--retry && outstanding) {
		userpf_info(xdev, "Waiting for %d outstanding commands to finish", outstanding);
		msleep(wait_ms);
		outstanding = atomic_read(&xdev->outstanding_execs);
	}

	// Last gasp, flush any remaining commands for this device exec core
	// This is an abnormal case.  All exec clients have been destroyed
	// prior to exec_stop being called (per contract), this implies that
	// all regular client commands have been flushed.
	if (outstanding) {
		// Wake up the scheduler to force one iteration flushing stale
		// commands for this device
		exec->flush = true;
		scheduler_intr(exec->scheduler);

		// Wait a second
		msleep(1000);
	}

	outstanding = atomic_read(&xdev->outstanding_execs);
	if (outstanding)
		userpf_err(xdev, "unexpected outstanding commands %d after flush", outstanding);

	// stale commands were flushed, reset submitted command state
	for (idx = 0; idx < MAX_SLOTS; ++idx)
		exec->submitted_cmds[idx] = NULL;

	bitmap_zero(exec->slot_status, MAX_SLOTS);
	set_bit(0, exec->slot_status); // reserve for control command
	exec->ctrl_busy = false;
}

/*
 */
static irqreturn_t
exec_isr(int irq, void *arg)
{
	struct exec_core *exec = (struct exec_core *)arg;

	SCHED_DEBUGF("-> xocl_user_event %d\n", irq);
	if (exec && !exec->polling_mode) {

		irq -= exec->intr_base;
		if (irq == 0)
			atomic_set(&exec->sr0, 1);
		else if (irq == 1)
			atomic_set(&exec->sr1, 1);
		else if (irq == 2)
			atomic_set(&exec->sr2, 1);
		else if (irq == 3)
			atomic_set(&exec->sr3, 1);

		/* wake up all scheduler ... currently one only */
		scheduler_intr(exec->scheduler);
	} else {
		userpf_err(exec_get_xdev(exec), "unhandled isr irq %d", irq);
	}
	SCHED_DEBUGF("<- xocl_user_event\n");
	return IRQ_HANDLED;
}

/*
 */
struct exec_core *
exec_create(struct platform_device *pdev, struct xocl_scheduler *xs)
{
	struct exec_core *exec = devm_kzalloc(&pdev->dev, sizeof(struct exec_core), GFP_KERNEL);
	struct xocl_dev *xdev = xocl_get_xdev(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	static unsigned int count;
	unsigned int i;

	if (!exec)
		return NULL;

	mutex_init(&exec->exec_lock);
	exec->base = xdev->core.bar_addr;

	exec->intr_base = res->start;
	exec->intr_num = res->end - res->start + 1;
	exec->pdev = pdev;
	exec->ert_cfg_priv = *(char *)XOCL_GET_SUBDEV_PRIV(&pdev->dev);

	init_waitqueue_head(&exec->poll_wait_queue);
	exec->scheduler = xs;
	exec->uid = count++;

	for (i = 0; i < exec->intr_num; i++) {
		xocl_user_interrupt_reg(xdev, i+exec->intr_base, exec_isr, exec);
		xocl_user_interrupt_config(xdev, i + exec->intr_base, true);
	}

	exec_reset(exec);
	platform_set_drvdata(pdev, exec);

	SCHED_DEBUGF("%s(%d)\n", __func__, exec->uid);

	return exec;
}

/*
 */
static void
exec_destroy(struct exec_core *exec)
{
	int idx;

	SCHED_DEBUGF("%s(%d)\n", __func__, exec->uid);
	for (idx = 0; idx < exec->num_cus; ++idx)
		cu_destroy(exec->cus[idx]);
	if (exec->ert)
		ert_destroy(exec->ert);
	devm_kfree(&exec->pdev->dev, exec);
}

/*
 */
static inline struct xocl_scheduler *
exec_scheduler(struct exec_core *exec)
{
	return exec->scheduler;
}

/*
 * acquire_slot_idx() - First available slot index
 */
static unsigned int
exec_acquire_slot_idx(struct exec_core *exec)
{
	unsigned int idx = find_first_zero_bit(exec->slot_status, MAX_SLOTS);

	SCHED_DEBUGF("%s exec(%d) returns %d\n", __func__, exec->uid, idx < exec->num_slots ? idx : no_index);
	if (idx < exec->num_slots) {
		set_bit(idx, exec->slot_status);
		return idx;
	}
	return no_index;
}


/**
 * acquire_slot() - Acquire a slot index for a command
 *
 * This function makes a special case for control commands which
 * must always dispatch to slot 0, otherwise normal acquisition
 */
static int
exec_acquire_slot(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	// slot 0 is reserved for ctrl commands
	if (cmd_type(xcmd) == ERT_CTRL) {
		SCHED_DEBUGF("%s ctrl cmd(%lu)\n", __func__, xcmd->uid);
		if (exec->ctrl_busy)
			return -1;
		exec->ctrl_busy = true;
		return (xcmd->slot_idx = 0);
	}

	return (xcmd->slot_idx = exec_acquire_slot_idx(exec));
}

/*
 * release_slot_idx() - Release specified slot idx
 */
static void
exec_release_slot_idx(struct exec_core *exec, unsigned int slot_idx)
{
	clear_bit(slot_idx, exec->slot_status);
}

/**
 * release_slot() - Release a slot index for a command
 *
 * Special case for control commands that execute in slot 0.  This
 * slot cannot be marked free ever.
 */
static void
exec_release_slot(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	if (xcmd->slot_idx == no_index)
		return; // already released

	SCHED_DEBUGF("-> %s(%d) xcmd(%lu) slotidx(%d)\n",
		     __func__, exec->uid, xcmd->uid, xcmd->slot_idx);
	if (cmd_type(xcmd) == ERT_CTRL) {
		SCHED_DEBUG("+ ctrl cmd\n");
		exec->ctrl_busy = false;
	} else {
		exec_release_slot_idx(exec, xcmd->slot_idx);
	}
	xcmd->slot_idx = no_index;
	SCHED_DEBUGF("<- %s\n", __func__);
}

/*
 * submit_cmd() - Submit command for execution on this core
 *
 * Return: true on success, false if command could not be submitted
 */
static bool
exec_submit_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	unsigned int slotidx = exec_acquire_slot(exec, xcmd);

	if (slotidx == no_index)
		return false;
	SCHED_DEBUGF("%s cmd(%lu) slotidx(%d)\n", __func__, xcmd->uid, slotidx);
	exec->submitted_cmds[slotidx] = xcmd;
	cmd_set_int_state(xcmd, ERT_CMD_STATE_SUBMITTED);
	return true;
}

/*
 * finish_cmd() - Special post processing of commands after execution
 */
static int
exec_finish_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	if (cmd_opcode(xcmd) == ERT_CU_STAT && exec_is_ert(exec))
		ert_read_custat(exec->ert, exec->num_cus, exec->cu_usage, xcmd);
	return 0;
}

/*
 * execute_copbo_cmd() - Execute ERT_START_COPYBO commands
 *
 * This is special case for copying P2P
 */
static int
exec_execute_copybo_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	int ret;
	struct ert_start_copybo_cmd *ecmd = xcmd->ert_cp;
	struct drm_file *filp = (struct drm_file *)ecmd->arg;
	struct drm_device *ddev = filp->minor->dev;

	SCHED_DEBUGF("-> %s(%d,%lu)\n", __func__, exec->uid, xcmd->uid);
	ret = xocl_copy_import_bo(ddev, filp, ecmd);
	SCHED_DEBUGF("<- %s\n", __func__);
	return ret == 0 ? 0 : 1;
}

/*
 * notify_host() - Notify user space that a command is complete.
 */
static void
exec_notify_host(struct exec_core *exec)
{
	struct list_head *ptr;
	struct client_ctx *entry;
	struct xocl_dev *xdev = exec_get_xdev(exec);

	SCHED_DEBUGF("-> %s(%d)\n", __func__, exec->uid);

	/* now for each client update the trigger counter in the context */
	mutex_lock(&xdev->dev_lock);
	list_for_each(ptr, &xdev->ctx_list) {
		entry = list_entry(ptr, struct client_ctx, link);
		atomic_inc(&entry->trigger);
	}
	mutex_unlock(&xdev->dev_lock);
	/* wake up all the clients */
	wake_up_interruptible(&exec->poll_wait_queue);
	SCHED_DEBUGF("<- %s\n", __func__);
}

/*
 * exec_cmd_mark_complete() - Move a command to complete state
 *
 * Commands are marked complete in two ways
 *  1. Through polling of CUs or polling of MB status register
 *  2. Through interrupts from MB
 *
 * @xcmd: Command to mark complete
 *
 * The external command state is changed to complete and the host
 * is notified that some command has completed.
 */
static void
exec_mark_cmd_complete(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s(%d,%lu)\n", __func__, exec->uid, xcmd->uid);
	if (cmd_type(xcmd) == ERT_CTRL)
		exec_finish_cmd(exec, xcmd);

	cmd_set_state(xcmd, ERT_CMD_STATE_COMPLETED);

	if (exec->polling_mode)
		scheduler_decr_poll(exec->scheduler);

	exec_release_slot(exec, xcmd);
	exec_notify_host(exec);

	// Deactivate command and trigger chain of waiting commands
	cmd_mark_deactive(xcmd);
	cmd_trigger_chain(xcmd);

	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * process_cmd_mask() - Move all commands in mask to complete state
 *
 * @mask: Bitmask with queried statuses of commands
 * @mask_idx: Index of the command mask. Used to offset the actual cmd slot index
 *
 * scheduler_ops ERT callback function
 *
 * Used in ERT mode only.  Currently ERT submitted commands remain in exec
 * submitted queue as ERT doesn't support data flow
 */
static void
exec_process_cmd_mask(struct exec_core *exec, u32 mask, unsigned int mask_idx)
{
	int bit_idx = 0, cmd_idx = 0;

	// assert(mask)
	SCHED_DEBUGF("-> %s(0x%x,%d)\n", __func__, mask, mask_idx);

	for (bit_idx = 0, cmd_idx = mask_idx<<5; bit_idx < 32; mask >>= 1, ++bit_idx, ++cmd_idx) {
		struct xocl_cmd *xcmd = (mask & 0x1)
			? exec->submitted_cmds[cmd_idx]
			: NULL;

		if (xcmd)
			exec_mark_cmd_complete(exec, xcmd);
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * process_cu_mask() - Check status of compute units per mask
 *
 * @mask: Bitmask with CUs to check
 * @mask_idx: Index of the CU mask. Used to offset the actual CU index
 *
 * scheduler_ops ERT poll callback function
 *
 * Used in ERT CU polling mode only.  When ERT interrupts host it is because
 * some CUs changed state when ERT polled it.  These CUs must be checked by
 * KDS and if a command has completed then it must be marked complete.
 *
 * CU indices in mask are offset by 1 to reserve CQ slot 0 for ctrl cmds
 */
static void
exec_process_cu_mask(struct exec_core *exec, u32 mask, unsigned int mask_idx)
{
	int bit_idx = 0, cu_idx = 0;

	// assert(mask > 0x1); // 0x1 is ctrl commands not cus
	SCHED_DEBUGF("-> %s(0x%x,%d)\n", __func__, mask, mask_idx);
	for (bit_idx = 0, cu_idx = mask_idx<<5; bit_idx < 32; mask >>= 1, ++bit_idx, ++cu_idx) {
		struct xocl_cu *xcu;
		struct xocl_cmd *xcmd;

		if (!(mask & 0x1))
			continue;

		xcu = exec->cus[cu_idx-1]; // note offset

		// poll may have been done outside of ERT when a CU was
		// started; alas there can be more than one completed cmd
		while ((xcmd = cu_first_done(xcu))) {
			cu_pop_done(xcu);
			exec_mark_cmd_complete(exec, xcmd);
		}
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * penguin_start_cmd() - Start a command in penguin mode
 *
 * @exec: device
 * @xcmd: command to start
 *
 * scheduler_ops penguin and ert poll callback function
 *
 * Used in penguin and ert poll mode where KDS schedules and starts
 * compute units.
 */
static bool
exec_penguin_start_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	unsigned int cuidx;
	u32 opcode = cmd_opcode(xcmd);

	SCHED_DEBUGF("-> %s cmd(%lu) opcode(%d)\n", __func__, xcmd->uid, opcode);

	if (opcode == ERT_START_COPYBO && exec_execute_copybo_cmd(exec, xcmd)) {
		cmd_set_state(xcmd, ERT_CMD_STATE_ERROR);
		return false;
	}

	if (cmd_type(xcmd) != ERT_CU) {
		SCHED_DEBUGF("<- %s not a CU style command -> true\n", __func__);
		return true;
	}

	// Find a ready CU
	for (cuidx = 0; cuidx < exec->num_cus; ++cuidx) {
		struct xocl_cu *xcu = exec->cus[cuidx];

		if (cmd_has_cu(xcmd, cuidx) && cu_ready(xcu) && cu_start(xcu, xcmd)) {
			exec->submitted_cmds[xcmd->slot_idx] = NULL;
			++exec->cu_usage[cuidx];
			exec_release_slot(exec, xcmd);
			xcmd->cu_idx = cuidx;
			SCHED_DEBUGF("<- %s -> true\n", __func__);
			return true;
		}
	}
	SCHED_DEBUGF("<- %s -> false\n", __func__);
	return false;
}

/**
 * penguin_query() - Check command status of argument command
 *
 * @exec: device
 * @xcmd: command to check
 *
 * scheduler_ops penguin mode callback function
 *
 * Function is called in penguin mode where KDS polls CUs for completion
 */
static void
exec_penguin_query_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	u32 cmdtype = cmd_type(xcmd);

	SCHED_DEBUGF("-> %s cmd(%lu) opcode(%d) type(%d) slot_idx=%d\n",
		     __func__, xcmd->uid, cmd_opcode(xcmd), cmdtype, xcmd->slot_idx);

	if (cmdtype == ERT_KDS_LOCAL || cmdtype == ERT_CTRL)
		exec_mark_cmd_complete(exec, xcmd);
	else if (cmdtype == ERT_CU) {
		struct xocl_cu *xcu = exec->cus[xcmd->cu_idx];

		if (cu_first_done(xcu) == xcmd) {
			cu_pop_done(xcu);
			exec_mark_cmd_complete(exec, xcmd);
		}
	}

	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * ert_start_cmd() - Start a command in ERT mode
 *
 * @exec: device
 * @xcmd: command to start
 *
 * scheduler_ops ERT mode callback function
 *
 * Used in ert mode where ERT schedules, starts, and polls compute units.
 */
static bool
exec_ert_start_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	if (cmd_type(xcmd) == ERT_KDS_LOCAL)
		return exec_penguin_start_cmd(exec, xcmd);

	return ert_start_cmd(exec->ert, xcmd);
}

/**
 * ert_start_ctrl() - Start a control command in ERT mode
 *
 * In ERT poll mode cu stats are managed by kds itself, nothing
 * to retrieve from ERT.
 */
static bool
exec_ert_start_ctrl(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	if (cmd_opcode(xcmd) == ERT_CU_STAT && exec_is_ert_poll(exec))
		return exec_penguin_start_cmd(exec, xcmd);

	return ert_start_cmd(exec->ert, xcmd);
}

/**
 * Clear the ERT command queue status register
 *
 * This can be necessary in ert polling mode, where KDS itself
 * can be ahead of ERT, so stale interrupts are possible which
 * is bad during reconfig.
 */
static void
exec_ert_clear_csr(struct exec_core *exec)
{
	unsigned int idx;

	if (!exec_is_ert(exec) && !exec_is_ert_poll(exec))
		return;

	for (idx = 0; idx < 4; ++idx) {
		u32 csr_addr = ERT_STATUS_REGISTER_ADDR + (idx<<2);
		u32 val = ioread32(exec->base + csr_addr);

		if (val)
			userpf_info(exec_get_xdev(exec),
				    "csr[%d]=0x%x cleared\n", idx, val);
	}
}

/**
 * ert_query_csr() - Check ERT CQ completion register
 *
 * @exec: device
 * @xcmd: command to check
 * @mask_idx: index of status register to check
 *
 * This function is for ERT and ERT polling mode.  When KDS is configured to
 * poll, this function polls the command queue completion register from
 * ERT. In interrupt mode check the interrupting status register.
 *
 * The function checks all entries in the same command queue status register as
 * argument command so more than one command may be marked complete by this
 * function.
 */
static void
exec_ert_query_csr(struct exec_core *exec, struct xocl_cmd *xcmd, unsigned int mask_idx)
{
	u32 mask = 0;
	u32 cmdtype = cmd_type(xcmd);

	SCHED_DEBUGF("-> %s cmd(%lu), mask_idx(%d)\n", __func__, xcmd->uid, mask_idx);

	if (cmdtype == ERT_KDS_LOCAL) {
		exec_mark_cmd_complete(exec, xcmd);
		SCHED_DEBUGF("<- %s local command\n", __func__);
		return;
	}

	if (exec->polling_mode
	    || (mask_idx == 0 && atomic_xchg(&exec->sr0, 0))
	    || (mask_idx == 1 && atomic_xchg(&exec->sr1, 0))
	    || (mask_idx == 2 && atomic_xchg(&exec->sr2, 0))
	    || (mask_idx == 3 && atomic_xchg(&exec->sr3, 0))) {
		u32 csr_addr = ERT_STATUS_REGISTER_ADDR + (mask_idx<<2);

		mask = ioread32(exec->base + csr_addr);
		SCHED_DEBUGF("++ %s csr_addr=0x%x mask=0x%x\n", __func__, csr_addr, mask);
	}

	if (!mask) {
		SCHED_DEBUGF("<- %s mask(0x0)\n", __func__);
		return;
	}

	// special case for control commands which are in slot 0
	if (cmdtype == ERT_CTRL && (mask & 0x1)) {
		exec_process_cmd_mask(exec, 0x1, mask_idx);
		mask ^= 0x1;
	}

	if (mask)
		exec->ops->process_mask(exec, mask, mask_idx);

	SCHED_DEBUGF("<- %s\n", __func__);
}


/**
 * ert_query_cu() - Check command completion based on ERT CQ completion register
 *
 * @exec: device
 * @xcmd: command to check
 * @maskidx:
 *
 * scheduler_ops ERT poll mode callback function
 *
 * NOTE: in ERT poll mode the CQ slot indices are offset by 1 for cu indices,
 * this is done so as to reserve slot 0 for control commands.
 *
 * In ERT poll mode, the command completion register corresponds to compute
 * units, which ERT is monitoring / polling for completion.
 *
 * If a CU status has changed, ERT will notify host via 4 interrupt registers
 * each representing 32 CUs.  This function checks the interrupt register
 * containing the CU on which argument cmd was started.
 *
 * The function checks all entries in the same status register as argument
 * command so more than one command may be marked complete by this function.
 */
static void
exec_ert_query_cu(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s cmd(%lu), cu_idx(%d)\n", __func__, xcmd->uid, xcmd->cu_idx);
	exec_ert_query_csr(exec, xcmd, slot_mask_idx(xcmd->cu_idx+1)); // note offset
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * ert_query_cu() - Check command completion based on ERT CQ completion register
 *
 * @exec: device
 * @xcmd: command to check
 * @maskidx:
 *
 * scheduler_ops ERT mode callback function
 *
 * In ERT mode, the command completion register corresponds to ERT commands,
 * which KDS wrote to the ERT command queue when a command was started.
 *
 * If a command has completed, ERT will notify host via 4 interrupt registers
 * each representing 32 CUs.  This function checks the interrupt register
 * containing the CU on which argument cmd was started.
 *
 * If a CU status has changed, ERT will notify host via 4 interrupt registers
 * each representing 32 commands.  This function checks the interrupt register
 * containing the argument command.
 *
 * The function checks all entries in the same status register as argument
 * command so more than one command may be marked complete by this function.
 */
static void
exec_ert_query_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s cmd(%lu), slot_idx(%d)\n", __func__, xcmd->uid, xcmd->slot_idx);
	exec_ert_query_csr(exec, xcmd, slot_mask_idx(xcmd->slot_idx));
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * ert_query_ctrl() - Check command completion for control command
 *
 * In ERT poll mode cu stats are managed by kds itself, nothing
 * to retrieve from ERT.
 */
static void
exec_ert_query_ctrl(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s cmd(%lu), slot_idx(%d)\n", __func__, xcmd->uid, xcmd->slot_idx);
	if (cmd_opcode(xcmd) == ERT_CU_STAT && exec_is_ert_poll(exec))
		exec_penguin_query_cmd(exec, xcmd);
	else
		exec_ert_query_cmd(exec, xcmd);
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * start_cmd() - Start execution of a command
 *
 * Return: true if succesfully started, false otherwise
 *
 * Function dispatches based on penguin vs ert mode
 */
static bool
exec_start_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	u32 cmdtype = cmd_type(xcmd);

	// assert cmd had been submitted
	SCHED_DEBUGF("-> %s cmd(%lu) opcode(%d)\n", __func__, xcmd->uid, cmd_opcode(xcmd));

	// ctrl commands may need special attention
	if (cmdtype == ERT_CTRL && exec->ops->start_ctrl(exec, xcmd)) {
		cmd_set_int_state(xcmd, ERT_CMD_STATE_RUNNING);
		SCHED_DEBUGF("<- %s returns true for cmd type(%d)\n", __func__, cmdtype);
		return true;
	}

	if (cmdtype != ERT_CTRL && exec->ops->start_cmd(exec, xcmd)) {
		cmd_set_int_state(xcmd, ERT_CMD_STATE_RUNNING);
		SCHED_DEBUGF("<- %s returns true for cmd type(%d)\n", __func__, cmdtype);
		return true;
	}

	SCHED_DEBUGF("<- %s returns false\n", __func__);
	return false;
}

/**
 * query_cmd() - Check status of command
 *
 * Function dispatches based on penguin vs ert mode.  In ERT mode
 * multiple commands can be marked complete by this function.
 */
static void
exec_query_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	u32 cmdtype = cmd_type(xcmd);

	SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

	// ctrl commands may need special attention
	if (cmdtype == ERT_CTRL)
		exec->ops->query_ctrl(exec, xcmd);
	else
		exec->ops->query_cmd(exec, xcmd);

	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * ert_ops: operations for ERT scheduling
 */
static struct exec_ops ert_ops = {
	.start_cmd = exec_ert_start_cmd,
	.start_ctrl = exec_ert_start_cmd,
	.query_cmd = exec_ert_query_cmd,
	.query_ctrl = exec_ert_query_cmd,
	.process_mask = exec_process_cmd_mask,
};

/**
 * penguin_ops: operations for kernel mode scheduling
 */
static struct exec_ops penguin_ops = {
	.start_cmd = exec_penguin_start_cmd,
	.start_ctrl = exec_penguin_start_cmd,
	.query_cmd = exec_penguin_query_cmd,
	.query_ctrl = exec_penguin_query_cmd,
	.process_mask = NULL,
};

/**
 * dataflow_ops: operations for kernel mode scheduling
 */
static struct exec_ops ert_poll_ops = {
	.start_cmd = exec_penguin_start_cmd,
	.start_ctrl = exec_ert_start_ctrl,
	.query_cmd = exec_ert_query_cu,
	.query_ctrl = exec_ert_query_ctrl,
	.process_mask = exec_process_cu_mask,
};

/*
 */
static inline struct exec_core *
pdev_get_exec(struct platform_device *pdev)
{
	return platform_get_drvdata(pdev);
}

/*
 */
static inline struct exec_core *
dev_get_exec(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return pdev ? pdev_get_exec(pdev) : NULL;
}

/*
 */
static inline struct xocl_dev *
dev_get_xdev(struct device *dev)
{
	struct exec_core *exec = dev_get_exec(dev);

	return exec ? exec_get_xdev(exec) : NULL;
}

/**
 * List of new pending xocl_cmd objects
 *
 * @pending_cmds: populated from user space with new commands for buffer objects
 * @num_pending: number of pending commands
 *
 * Scheduler copies pending commands to its private queue when necessary
 */
static LIST_HEAD(pending_cmds);
static DEFINE_MUTEX(pending_cmds_mutex);
static atomic_t num_pending = ATOMIC_INIT(0);

static void
pending_cmds_reset(void)
{
	/* clear stale command objects if any */
	while (!list_empty(&pending_cmds)) {
		struct xocl_cmd *xcmd = list_first_entry(&pending_cmds, struct xocl_cmd, cq_list);

		DRM_INFO("deleting stale pending cmd\n");
		cmd_free(xcmd);
	}
	atomic_set(&num_pending, 0);
}

/**
 * struct xocl_sched: scheduler for xocl_cmd objects
 *
 * @scheduler_thread: thread associated with this scheduler
 * @use_count: use count for this scheduler
 * @wait_queue: conditional wait queue for scheduler thread
 * @error: set to 1 to indicate scheduler error
 * @stop: set to 1 to indicate scheduler should stop
 * @reset: set to 1 to reset the scheduler
 * @command_queue: list of command objects managed by scheduler
 * @intc: boolean flag set when there is a pending interrupt for command completion
 * @poll: number of running commands in polling mode
 */
struct xocl_scheduler {
	struct task_struct	  *scheduler_thread;
	unsigned int		   use_count;

	wait_queue_head_t	   wait_queue;
	unsigned int		   error;
	unsigned int		   stop;
	unsigned int		   reset;

	struct list_head	   command_queue;

	unsigned int		   intc; /* pending intr shared with isr, word aligned atomic */
	unsigned int		   poll; /* number of cmds to poll */
};

static struct xocl_scheduler scheduler0;

static void
scheduler_reset(struct xocl_scheduler *xs)
{
	xs->error = 0;
	xs->stop = 0;
	xs->poll = 0;
	xs->reset = false;
	xs->intc = 0;
}

static void
scheduler_cq_reset(struct xocl_scheduler *xs)
{
	while (!list_empty(&xs->command_queue)) {
		struct xocl_cmd *xcmd = list_first_entry(&xs->command_queue, struct xocl_cmd, cq_list);

		DRM_INFO("deleting stale scheduler cmd\n");
		cmd_free(xcmd);
	}
}

static void
scheduler_wake_up(struct xocl_scheduler *xs)
{
	wake_up_interruptible(&xs->wait_queue);
}

static void
scheduler_intr(struct xocl_scheduler *xs)
{
	xs->intc = 1;
	scheduler_wake_up(xs);
}

static inline void
scheduler_decr_poll(struct xocl_scheduler *xs)
{
	--xs->poll;
}


/**
 * scheduler_queue_cmds() - Queue any pending commands
 *
 * The scheduler copies pending commands to its internal command queue where
 * is is now in queued state.
 */
static void
scheduler_queue_cmds(struct xocl_scheduler *xs)
{
	struct xocl_cmd *xcmd;
	struct list_head *pos, *next;

	SCHED_DEBUGF("-> %s\n", __func__);
	mutex_lock(&pending_cmds_mutex);
	list_for_each_safe(pos, next, &pending_cmds) {
		xcmd = list_entry(pos, struct xocl_cmd, cq_list);
		if (xcmd->xs != xs)
			continue;
		SCHED_DEBUGF("+ queueing cmd(%lu)\n", xcmd->uid);
		list_del(&xcmd->cq_list);
		list_add_tail(&xcmd->cq_list, &xs->command_queue);

		/* chain active dependencies if any to this command object */
		if (cmd_wait_count(xcmd) && cmd_chain_dependencies(xcmd))
			cmd_set_state(xcmd, ERT_CMD_STATE_ERROR);
		else
			cmd_set_int_state(xcmd, ERT_CMD_STATE_QUEUED);

		/* this command is now active and can chain other commands */
		cmd_mark_active(xcmd);
		atomic_dec(&num_pending);
	}
	mutex_unlock(&pending_cmds_mutex);
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * queued_to_running() - Move a command from queued to running state if possible
 *
 * @xcmd: Command to start
 *
 * Upon success, the command is not necessarily running. In ert mode the
 * command will have been submitted to the embedded scheduler, whereas in
 * penguin mode the command has been started on a CU.
 *
 * Return: %true if command was submitted to device, %false otherwise
 */
static bool
scheduler_queued_to_submitted(struct xocl_scheduler *xs, struct xocl_cmd *xcmd)
{
	struct exec_core *exec = cmd_exec(xcmd);
	bool retval = false;

	if (cmd_wait_count(xcmd))
		return false;

	SCHED_DEBUGF("-> %s(%lu) opcode(%d)\n", __func__, xcmd->uid, cmd_opcode(xcmd));

	// configure prior to using the core
	if (cmd_opcode(xcmd) == ERT_CONFIGURE && exec_cfg_cmd(exec, xcmd)) {
		cmd_set_state(xcmd, ERT_CMD_STATE_ERROR);
		return false;
	}

	// submit the command
	if (exec_submit_cmd(exec, xcmd)) {
		if (exec->polling_mode)
			++xs->poll;
		retval = true;
	}

	SCHED_DEBUGF("<- queued_to_submitted returns %d\n", retval);

	return retval;
}

static bool
scheduler_submitted_to_running(struct xocl_scheduler *xs, struct xocl_cmd *xcmd)
{
	return exec_start_cmd(cmd_exec(xcmd), xcmd);
}

/**
 * running_to_complete() - Check status of running commands
 *
 * @xcmd: Command is in running state
 *
 * When ERT is enabled this function may mark more than just argument
 * command as complete based on content of command completion register.
 * Without ERT, only argument command is checked for completion.
 */
static void
scheduler_running_to_complete(struct xocl_scheduler *xs, struct xocl_cmd *xcmd)
{
	exec_query_cmd(cmd_exec(xcmd), xcmd);
}

/**
 * complete_to_free() - Recycle a complete command objects
 *
 * @xcmd: Command is in complete state
 */
static void
scheduler_complete_to_free(struct xocl_scheduler *xs, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s(%lu)\n", __func__, xcmd->uid);
	cmd_free(xcmd);
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
scheduler_error_to_free(struct xocl_scheduler *xs, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s(%lu)\n", __func__, xcmd->uid);
	exec_notify_host(cmd_exec(xcmd));
	scheduler_complete_to_free(xs, xcmd);
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
scheduler_abort_to_free(struct xocl_scheduler *xs, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s(%lu)\n", __func__, xcmd->uid);
	scheduler_error_to_free(xs, xcmd);
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * scheduler_iterator_cmds() - Iterate all commands in scheduler command queue
 */
static void
scheduler_iterate_cmds(struct xocl_scheduler *xs)
{
	struct list_head *pos, *next;

	SCHED_DEBUGF("-> %s\n", __func__);
	list_for_each_safe(pos, next, &xs->command_queue) {
		struct xocl_cmd *xcmd = list_entry(pos, struct xocl_cmd, cq_list);

		cmd_update_state(xcmd);
		SCHED_DEBUGF("+ processing cmd(%lu)\n", xcmd->uid);

		if (xcmd->state == ERT_CMD_STATE_QUEUED)
			scheduler_queued_to_submitted(xs, xcmd);
		if (xcmd->state == ERT_CMD_STATE_SUBMITTED)
			scheduler_submitted_to_running(xs, xcmd);
		if (xcmd->state == ERT_CMD_STATE_RUNNING)
			scheduler_running_to_complete(xs, xcmd);
		if (xcmd->state == ERT_CMD_STATE_COMPLETED)
			scheduler_complete_to_free(xs, xcmd);
		if (xcmd->state == ERT_CMD_STATE_ERROR)
			scheduler_error_to_free(xs, xcmd);
		if (xcmd->state == ERT_CMD_STATE_ABORT)
			scheduler_abort_to_free(xs, xcmd);
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * scheduler_wait_condition() - Check status of scheduler wait condition
 *
 * Scheduler must wait (sleep) if
 *   1. there are no pending commands
 *   2. no pending interrupt from embedded scheduler
 *   3. no pending complete commands in polling mode
 *
 * Return: 1 if scheduler must wait, 0 othewise
 */
static int
scheduler_wait_condition(struct xocl_scheduler *xs)
{
	if (kthread_should_stop()) {
		xs->stop = 1;
		SCHED_DEBUG("scheduler wakes kthread_should_stop\n");
		return 0;
	}

	if (atomic_read(&num_pending)) {
		SCHED_DEBUG("scheduler wakes to copy new pending commands\n");
		return 0;
	}

	if (xs->intc) {
		SCHED_DEBUG("scheduler wakes on interrupt\n");
		xs->intc = 0;
		return 0;
	}

	if (xs->poll) {
		SCHED_DEBUG("scheduler wakes to poll\n");
		return 0;
	}

	SCHED_DEBUG("scheduler waits ...\n");
	return 1;
}

/**
 * scheduler_wait() - check if scheduler should wait
 *
 * See scheduler_wait_condition().
 */
static void
scheduler_wait(struct xocl_scheduler *xs)
{
	wait_event_interruptible(xs->wait_queue, scheduler_wait_condition(xs) == 0);
}

/**
 * scheduler_loop() - Run one loop of the scheduler
 */
static void
scheduler_loop(struct xocl_scheduler *xs)
{
	static unsigned int loop_cnt;

	SCHED_DEBUGF("%s\n", __func__);
	scheduler_wait(xs);

	if (xs->error)
		DRM_INFO("scheduler encountered unexpected error\n");

	if (xs->stop)
		return;

	if (xs->reset) {
		SCHED_DEBUG("scheduler is resetting after timeout\n");
		scheduler_reset(xs);
	}

	/* queue new pending commands */
	scheduler_queue_cmds(xs);

	/* iterate all commands */
	scheduler_iterate_cmds(xs);

	// loop 8 times before explicitly yielding
	if (++loop_cnt == 8) {
		loop_cnt = 0;
		schedule();
	}
}

/**
 * scheduler() - Command scheduler thread routine
 */
static int
scheduler(void *data)
{
	struct xocl_scheduler *xs = (struct xocl_scheduler *)data;

	while (!xs->stop)
		scheduler_loop(xs);
	DRM_INFO("%s:%d %s thread exits with value %d\n", __FILE__, __LINE__, __func__, xs->error);
	return xs->error;
}



/**
 * add_xcmd() - Add initialized xcmd object to pending command list
 *
 * @xcmd: Command to add
 *
 * Scheduler copies pending commands to its internal command queue.
 *
 * Return: 0 on success
 */
static int
add_xcmd(struct xocl_cmd *xcmd)
{
	struct exec_core *exec = xcmd->exec;
	struct xocl_dev *xdev = xocl_get_xdev(exec->pdev);

	// Prevent stop and reset
	mutex_lock(&exec->exec_lock);

	SCHED_DEBUGF("-> %s(%lu) pid(%d)\n", __func__, xcmd->uid, pid_nr(task_tgid(current)));
	SCHED_DEBUGF("+ exec stopped(%d) configured(%d)\n", exec->stopped, exec->configured);

	if (exec->stopped || (!exec->configured && cmd_opcode(xcmd) != ERT_CONFIGURE))
		goto err;

	cmd_set_state(xcmd, ERT_CMD_STATE_NEW);
	mutex_lock(&pending_cmds_mutex);
	list_add_tail(&xcmd->cq_list, &pending_cmds);
	atomic_inc(&num_pending);
	mutex_unlock(&pending_cmds_mutex);

	/* wake scheduler */
	atomic_inc(&xdev->outstanding_execs);
	atomic64_inc(&xdev->total_execs);
	scheduler_wake_up(xcmd->xs);

	SCHED_DEBUGF("<- %s ret(0) opcode(%d) type(%d) num_pending(%d)\n",
		     __func__, cmd_opcode(xcmd), cmd_type(xcmd), atomic_read(&num_pending));
	mutex_unlock(&exec->exec_lock);
	return 0;

err:
	SCHED_DEBUGF("<- %s ret(1) opcode(%d) type(%d) num_pending(%d)\n",
		     __func__, cmd_opcode(xcmd), cmd_type(xcmd), atomic_read(&num_pending));
	mutex_unlock(&exec->exec_lock);
	return 1;
}


/**
 * add_bo_cmd() - Add a new buffer object command to pending list
 *
 * @exec: Targeted device
 * @client: Client context
 * @bo: Buffer objects from user space from which new command is created
 * @numdeps: Number of dependencies for this command
 * @deps: List of @numdeps dependencies
 *
 * Scheduler copies pending commands to its internal command queue.
 *
 * Return: 0 on success, 1 on failure
 */
static int
add_bo_cmd(struct exec_core *exec, struct client_ctx *client, struct drm_xocl_bo *bo,
	   int numdeps, struct drm_xocl_bo **deps)
{
	struct xocl_cmd *xcmd = cmd_get(exec_scheduler(exec), exec, client);

	if (!xcmd)
		return 1;

	SCHED_DEBUGF("-> %s(%lu)\n", __func__, xcmd->uid);

	cmd_bo_init(xcmd, bo, numdeps, deps, (exec_is_penguin(exec) || exec_is_ert_poll(exec)));

	if (add_xcmd(xcmd))
		goto err;

	SCHED_DEBUGF("<- %s ret(0) opcode(%d) type(%d)\n", __func__, cmd_opcode(xcmd), cmd_type(xcmd));
	return 0;
err:
	cmd_abort(xcmd);
	SCHED_DEBUGF("<- %s ret(1) opcode(%d) type(%d)\n", __func__, cmd_opcode(xcmd), cmd_type(xcmd));
	return 1;
}

static int
add_ctrl_cmd(struct exec_core *exec, struct client_ctx *client, struct ert_packet *packet)
{
	struct xocl_cmd *xcmd = cmd_get(exec_scheduler(exec), exec, client);

	if (!xcmd)
		return 1;

	SCHED_DEBUGF("-> %s(%lu)\n", __func__, xcmd->uid);

	cmd_packet_init(xcmd, packet);

	if (add_xcmd(xcmd))
		goto err;

	SCHED_DEBUGF("<- %s ret(0) opcode(%d) type(%d)\n", __func__, cmd_opcode(xcmd), cmd_type(xcmd));
	return 0;
err:
	cmd_abort(xcmd);
	SCHED_DEBUGF("<- %s ret(1) opcode(%d) type(%d)\n", __func__, cmd_opcode(xcmd), cmd_type(xcmd));
	return 1;
}


/**
 * init_scheduler_thread() - Initialize scheduler thread if necessary
 *
 * Return: 0 on success, -errno otherwise
 */
static int
init_scheduler_thread(struct xocl_scheduler *xs)
{
	SCHED_DEBUGF("%s use_count=%d\n", __func__, xs->use_count);
	if (xs->use_count++)
		return 0;

	init_waitqueue_head(&xs->wait_queue);
	INIT_LIST_HEAD(&xs->command_queue);
	scheduler_reset(xs);

	xs->scheduler_thread = kthread_run(scheduler, (void *)xs, "xocl-scheduler-thread0");
	if (IS_ERR(xs->scheduler_thread)) {
		int ret = PTR_ERR(xs->scheduler_thread);

		DRM_ERROR(__func__);
		return ret;
	}
	return 0;
}

/**
 * fini_scheduler_thread() - Finalize scheduler thread if unused
 *
 * Return: 0 on success, -errno otherwise
 */
static int
fini_scheduler_thread(struct xocl_scheduler *xs)
{
	int retval = 0;

	SCHED_DEBUGF("%s use_count=%d\n", __func__, xs->use_count);
	if (--xs->use_count)
		return 0;

	retval = kthread_stop(xs->scheduler_thread);

	/* clear stale command objects if any */
	pending_cmds_reset();
	scheduler_cq_reset(xs);

	/* reclaim memory for allocate command objects */
	cmd_list_delete();

	return retval;
}

static void client_release_implicit_cus(struct exec_core *exec,
	struct client_ctx *client)
{
	int i;

	SCHED_DEBUGF("-> %s", __func__);
	for (i = exec->num_cus - exec->num_cdma; i < exec->num_cus; i++) {
		SCHED_DEBUGF("+ cu(%d)", i);
		clear_bit(i, client->cu_bitmap);
	}
	SCHED_DEBUGF("<- %s", __func__);
}

static void client_reserve_implicit_cus(struct exec_core *exec,
	struct client_ctx *client)
{
	int i;

	SCHED_DEBUGF("-> %s", __func__);
	for (i = exec->num_cus - exec->num_cdma; i < exec->num_cus; i++) {
		SCHED_DEBUGF("+ cu(%d)", i);
		set_bit(i, client->cu_bitmap);
	}
	SCHED_DEBUGF("<- %s", __func__);
}

/**
 * Entry point for exec buffer.
 *
 * Function adds exec buffer to the pending list of commands
 */
int
add_exec_buffer(struct platform_device *pdev, struct client_ctx *client, void *buf,
		int numdeps, struct drm_xocl_bo **deps)
{
	struct exec_core *exec = platform_get_drvdata(pdev);
	// Add the command to pending list
	return add_bo_cmd(exec, client, buf, numdeps, deps);
}

static int
create_client(struct platform_device *pdev, void **priv)
{
	struct client_ctx	*client;
	struct xocl_dev		*xdev = xocl_get_xdev(pdev);
	int			ret = 0;

	client = devm_kzalloc(XDEV2DEV(xdev), sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	mutex_lock(&xdev->dev_lock);

	if (!xdev->offline) {
		client->pid = task_tgid(current);
		client->abort = false;
		atomic_set(&client->trigger, 0);
		atomic_set(&client->outstanding_execs, 0);
		client->num_cus = 0;
		client->xdev = xocl_get_xdev(pdev);
		list_add_tail(&client->link, &xdev->ctx_list);
		*priv =	client;
	} else {
		/* Do not allow new client to come in while being offline. */
		devm_kfree(XDEV2DEV(xdev), client);
		ret = -EBUSY;
	}

	mutex_unlock(&xdev->dev_lock);

	DRM_INFO("creating scheduler client for pid(%d), ret: %d\n",
		 pid_nr(task_tgid(current)), ret);

	return ret;
}

static void destroy_client(struct platform_device *pdev, void **priv)
{
	struct client_ctx *client = (struct client_ctx *)(*priv);
	struct exec_core *exec = platform_get_drvdata(pdev);
	struct xocl_scheduler *xs = exec_scheduler(exec);
	struct xocl_dev	*xdev = xocl_get_xdev(pdev);
	unsigned int	outstanding;
	unsigned int	timeout_loops = 20;
	unsigned int	loops = 0;
	int pid;
	unsigned int bit;
	struct ip_layout *layout;
	xuid_t *xclbin_id;

	// force scheduler to abort execs for this client
	client->abort = true;
	// wait for outstanding execs to finish
	outstanding = atomic_read(&client->outstanding_execs);
	while (outstanding) {
		unsigned int new;

		userpf_info(xdev, "waiting for %d outstanding execs to finish",
			outstanding);
		msleep(500);
		new = atomic_read(&client->outstanding_execs);
		loops = (new == outstanding ? (loops + 1) : 0);
		if (loops == timeout_loops) {
			userpf_err(xdev,
				   "Giving up with %d outstanding execs.\n",
				   outstanding);
			userpf_err(xdev,
				   "Please reset device with 'xbutil reset'\n");
			xdev->needs_reset = true;
			// reset the scheduler loop
			xs->reset = true;
			break;
		}
		outstanding = new;
	}

	mutex_lock(&xdev->dev_lock);

	pid = pid_nr(client->pid);
	layout = XOCL_IP_LAYOUT(xdev);
	xclbin_id = XOCL_XCLBIN_ID(xdev);

	bit = layout
	  ? find_first_bit(client->cu_bitmap, layout->m_count)
	  : MAX_CUS;

	/*
	 * This happens when application exists without formally releasing the
	 * contexts on CUs. Give up our contexts on CUs and our lock on xclbin.
	 * Note, that implicit CUs (such as CDMA) do not add to ip_reference.
	 */
	while (layout && (bit < layout->m_count)) {
		if (exec->ip_reference[bit]) {
			userpf_info(xdev, "CTX reclaim (%pUb, %d, %u)",
				xclbin_id, pid, bit);
			exec->ip_reference[bit]--;
		}
		bit = find_next_bit(client->cu_bitmap, layout->m_count, bit + 1);
	}

	/* Unlock xclbin */
	if (CLIENT_NUM_CU_CTX(client) != 0)
		(void) xocl_icap_unlock_bitstream(xdev, xclbin_id, pid);

	client->virt_cu_ref = 0;
	client_release_implicit_cus(exec, client);
	bitmap_zero(client->cu_bitmap, MAX_CUS);

	list_del(&client->link);
	DRM_INFO("client exits pid(%d)\n", pid);

	mutex_unlock(&xdev->dev_lock);

	devm_kfree(XDEV2DEV(xdev), client);
	*priv = NULL;
}

static uint poll_client(struct platform_device *pdev, struct file *filp,
	poll_table *wait, void *priv)
{
	struct client_ctx	*client = (struct client_ctx *)priv;
	struct exec_core	*exec = platform_get_drvdata(pdev);
	int			counter;

	poll_wait(filp, &exec->poll_wait_queue, wait);
	counter = atomic_dec_if_positive(&client->trigger);
	if (counter == -1)
		return 0;
	return POLLIN;
}

static int client_ioctl_ctx(struct platform_device *pdev,
			    struct client_ctx *client, void *data)
{
	struct drm_xocl_ctx *args = data;
	int ret = 0;
	int pid = pid_nr(task_tgid(current));
	struct xocl_dev	*xdev = xocl_get_xdev(pdev);
	struct exec_core *exec = platform_get_drvdata(pdev);
	xuid_t *xclbin_id;
	u32 cu_idx = args->cu_index;

	mutex_lock(&xdev->dev_lock);

	xclbin_id = XOCL_XCLBIN_ID(xdev);
	if (!xclbin_id || !uuid_equal(xclbin_id, &args->xclbin_id)) {
		ret = -EBUSY;
		goto out;
	}

	if (cu_idx != XOCL_CTX_VIRT_CU_INDEX &&
		cu_idx >= XOCL_IP_LAYOUT(xdev)->m_count) {
		userpf_err(xdev, "cuidx(%d) >= numcus(%d)\n",
			cu_idx, XOCL_IP_LAYOUT(xdev)->m_count);
		ret = -EINVAL;
		goto out;
	}

	if (args->op == XOCL_CTX_OP_FREE_CTX) {
		if (cu_idx == XOCL_CTX_VIRT_CU_INDEX) {
			if (client->virt_cu_ref == 0) {
				ret = -EINVAL;
				goto out;
			}
			--client->virt_cu_ref;
			if (client->virt_cu_ref == 0)
				client_release_implicit_cus(exec, client);
		} else {
			ret = test_and_clear_bit(cu_idx, client->cu_bitmap) ?
				0 : -EINVAL;
			if (ret) // Try to release unreserved CU
				goto out;
			--client->num_cus;
			--exec->ip_reference[cu_idx];
		}

		// We just gave up the last context, unlock the xclbin
		if (CLIENT_NUM_CU_CTX(client) == 0)
			(void) xocl_icap_unlock_bitstream(xdev, xclbin_id, pid);

		goto out;
	}

	if (args->op != XOCL_CTX_OP_ALLOC_CTX) {
		ret = -EINVAL;
		goto out;
	}

	if (args->flags != XOCL_CTX_SHARED) {
		userpf_err(xdev, "only support shared contexts");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (cu_idx != XOCL_CTX_VIRT_CU_INDEX &&
		test_and_set_bit(cu_idx, client->cu_bitmap)) {
		// Context was previously allocated for the same CU,
		// cannot allocate again. Need to implement per CU ref
		// counter to make it work.
		userpf_info(xdev, "CTX already allocated by this process");
		ret = -EINVAL;
		goto out;
	}

	if (CLIENT_NUM_CU_CTX(client) == 0) {
		// This is the first context on any CU for this process,
		// lock the xclbin
		ret = xocl_icap_lock_bitstream(xdev, xclbin_id, pid);
		if (ret) {
			if (cu_idx != XOCL_CTX_VIRT_CU_INDEX)
				clear_bit(cu_idx, client->cu_bitmap);
			goto out;
		}
	}

	if (cu_idx == XOCL_CTX_VIRT_CU_INDEX) {
		if (client->virt_cu_ref == 0)
			client_reserve_implicit_cus(exec, client);
		++client->virt_cu_ref;
	} else {
		++client->num_cus;
		++exec->ip_reference[cu_idx];
	}

out:
	xocl_info(&pdev->dev,
		"CTX %s(%pUb, pid %d, cu_idx 0x%x) = %d, ctx=%d",
		args->op == XOCL_CTX_OP_FREE_CTX ? "del" : "add",
		xclbin_id, pid, cu_idx, ret, CLIENT_NUM_CU_CTX(client));

	mutex_unlock(&xdev->dev_lock);
	return ret;
}

static int
get_bo_paddr(struct xocl_dev *xdev, struct drm_file *filp,
	     uint32_t bo_hdl, size_t off, size_t size, uint64_t *paddrp)
{
	struct drm_device *ddev = filp->minor->dev;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;

	obj = xocl_gem_object_lookup(ddev, filp, bo_hdl);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO 0x%x\n", bo_hdl);
		return -ENOENT;
	}

	xobj = to_xocl_bo(obj);
	if (!xobj->mm_node) {
		/* Not a local BO */
		drm_gem_object_unreference_unlocked(obj);
		return -EADDRNOTAVAIL;
	}

	if (obj->size <= off || obj->size < off + size) {
		userpf_err(xdev, "Failed to get paddr for BO 0x%x\n", bo_hdl);
		drm_gem_object_unreference_unlocked(obj);
		return -EINVAL;
	}

	*paddrp = xobj->mm_node->start + off;
	drm_gem_object_unreference_unlocked(obj);
	return 0;
}

static int convert_execbuf(struct xocl_dev *xdev, struct drm_file *filp,
			   struct exec_core *exec, struct drm_xocl_bo *xobj)
{
	int i;
	int ret_src, ret_dst;
	size_t src_off;
	size_t dst_off;
	size_t sz;
	uint64_t src_addr;
	uint64_t dst_addr;
	struct ert_start_copybo_cmd *scmd = (struct ert_start_copybo_cmd *)xobj->vmapping;

	/* CU style commands must specify CU type */
	if (scmd->opcode == ERT_START_CU || scmd->opcode == ERT_EXEC_WRITE)
		scmd->type = ERT_CU;

	/* Only convert COPYBO cmd for now. */
	if (scmd->opcode != ERT_START_COPYBO)
		return 0;

	sz = ert_copybo_size(scmd);

	src_off = ert_copybo_src_offset(scmd);
	ret_src = get_bo_paddr(xdev, filp, scmd->src_bo_hdl, src_off, sz, &src_addr);
	if (ret_src != 0 && ret_src != -EADDRNOTAVAIL)
		return ret_src;

	dst_off = ert_copybo_dst_offset(scmd);
	ret_dst = get_bo_paddr(xdev, filp, scmd->dst_bo_hdl, dst_off, sz, &dst_addr);
	if (ret_dst != 0 && ret_dst != -EADDRNOTAVAIL)
		return ret_dst;

	/* We need at least one local BO for copy */
	if (ret_src == -EADDRNOTAVAIL && ret_dst == -EADDRNOTAVAIL)
		return -EINVAL;

	/* One of them is not local BO, perform P2P copy */
	if (ret_src != ret_dst) {
		/* Not a ERT cmd, make sure KDS will handle it. */
		scmd->type = ERT_KDS_LOCAL;
		scmd->arg = filp;
		return 0;
	}

	/* Both BOs are local, copy via KDMA CU */
	ert_fill_copybo_cmd(scmd, 0, 0, src_addr, dst_addr, sz);

	for (i = exec->num_cus - exec->num_cdma; i < exec->num_cus; i++)
		scmd->cu_mask[i / 32] |= 1 << (i % 32);

	scmd->opcode = ERT_START_CU;
	scmd->type = ERT_CU;

	return 0;
}

static int
client_ioctl_execbuf(struct platform_device *pdev,
		     struct client_ctx *client, void *data, struct drm_file *filp)
{
	struct drm_xocl_execbuf *args = data;
	struct drm_xocl_bo *xobj;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *deps[8] = {0};
	int numdeps = -1;
	int ret = 0;
	struct xocl_dev	*xdev = xocl_get_xdev(pdev);
	struct drm_device *ddev = filp->minor->dev;

	if (xdev->needs_reset) {
		userpf_err(xdev, "device needs reset, use 'xbutil reset -h'");
		return -EBUSY;
	}

	/* Look up the gem object corresponding to the BO handle.
	 * This adds a reference to the gem object.  The refernece is
	 * passed to kds or released here if errors occur.
	 */
	obj = xocl_gem_object_lookup(ddev, filp, args->exec_bo_handle);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO %d\n",
		args->exec_bo_handle);
		return -ENOENT;
	}

	/* Convert gem object to xocl_bo extension */
	xobj = to_xocl_bo(obj);
	if (!xocl_bo_execbuf(xobj) || convert_execbuf(xdev, filp,
		platform_get_drvdata(pdev), xobj) != 0) {
		ret = -EINVAL;
		goto out;
	}

	ret = validate(pdev, client, xobj);
	if (ret) {
		userpf_err(xdev, "Exec buffer validation failed\n");
		ret = -EINVAL;
		goto out;
	}

	/* Copy dependencies from user.	 It is an error if a BO handle specified
	 * as a dependency does not exists. Lookup gem object corresponding to bo
	 * handle.  Convert gem object to xocl_bo extension.  Note that the
	 * gem lookup acquires a reference to the drm object, this reference
	 * is passed on to the the scheduler via xocl_exec_add_buffer.
	 */
	for (numdeps = 0; numdeps < 8 && args->deps[numdeps]; ++numdeps) {
		struct drm_gem_object *gobj =
		  xocl_gem_object_lookup(ddev, filp, args->deps[numdeps]);
		struct drm_xocl_bo *xbo = gobj ? to_xocl_bo(gobj) : NULL;

		if (!gobj)
			userpf_err(xdev, "Failed to look up GEM BO %d\n",
				   args->deps[numdeps]);
		if (!xbo) {
			ret = -EINVAL;
			goto out;
		}
		deps[numdeps] = xbo;
	}

	/* Add exec buffer to scheduler (kds).	The scheduler manages the
	 * drm object references acquired by xobj and deps.  It is vital
	 * that the references are released properly.
	 */
	ret = add_exec_buffer(pdev, client, xobj, numdeps, deps);
	if (ret) {
		userpf_err(xdev, "Failed to add exec buffer to scheduler\n");
		ret = -EINVAL;
		goto out;
	}

	/* Return here, noting that the gem objects passed to kds have
	 * references that must be released by kds itself.  User manages
	 * a regular reference to all BOs returned as file handles.  These
	 * references are released with the BOs are freed.
	 */
	return ret;

out:
	for (--numdeps; numdeps >= 0; numdeps--)
		drm_gem_object_unreference_unlocked(&deps[numdeps]->base);
	drm_gem_object_unreference_unlocked(&xobj->base);
	return ret;
}

int client_ioctl(struct platform_device *pdev,
		 int op, void *data, void *drm_filp)
{
	struct drm_file *filp = drm_filp;
	struct client_ctx *client = filp->driver_priv;
	int ret;

	switch (op) {
	case DRM_XOCL_CTX:
		ret = client_ioctl_ctx(pdev, client, data);
		break;
	case DRM_XOCL_EXECBUF:
		ret = client_ioctl_execbuf(pdev, client, data, drm_filp);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
/**
 * reset() - Reset device exec data structure
 *
 * @pdev: platform device to reset
 *
 * [Current 2018.3 situation:]
 * This function is currently called from mgmt icap on every AXI is
 * freeze/unfreeze.  It ensures that the device exec_core state is reset to
 * same state as was when scheduler was originally probed for the device.
 * The callback from icap, ensures that scheduler resets the exec core when
 * multiple processes are already attached to the device but AXI is reset.
 *
 * Even though the very first client created for this device also resets the
 * exec core, it is possible that further resets are necessary.	 For example
 * in multi-process case, there can be 'n' processes that attach to the
 * device.  On first client attach the exec core is reset correctly, but now
 * assume that 'm' of these processes finishes completely before any remaining
 * (n-m) processes start using the scheduler.  In this case, the n-m clients have
 * already been created, but icap resets AXI because the xclbin has no
 * references (arguably this AXI reset is wrong)
 *
 * [Work-in-progress:]
 * Proper contract:
 *  Pre-condition: xocl_exec_stop has been called before xocl_exec_reset.
 *  Pre-condition: new bitstream has been downloaded and AXI has been reset
 */
static int
reset(struct platform_device *pdev)
{
	struct exec_core *exec = platform_get_drvdata(pdev);

	exec_stop(exec);   // remove when upstream explicitly calls stop()
	exec_reset(exec);
	return 0;
}

/**
 * stop() - Reset device exec data structure
 *
 * This API must be called prior to performing an AXI reset and downloading of
 * a new xclbin.  Calling this API flushes the commands running on current
 * device and prevents new commands from being scheduled on the device.	 This
 * effectively prevents 'xbutil top' from issuing CU_STAT commands while
 * programming is performed.
 *
 * Pre-condition: xocl_client_release has been called, e.g there are no
 *		  current clients using the bitstream
 */
static int
stop(struct platform_device *pdev)
{
	struct exec_core *exec = platform_get_drvdata(pdev);

	exec_stop(exec);
	return 0;
}

/**
 * validate() - Check if requested cmd is valid in the current context
 */
static int
validate(struct platform_device *pdev, struct client_ctx *client, const struct drm_xocl_bo *bo)
{
	struct ert_packet *ecmd = (struct ert_packet *)bo->vmapping;
	struct ert_start_kernel_cmd *scmd = (struct ert_start_kernel_cmd *)bo->vmapping;
	unsigned int i = 0;
	u32 ctx_cus[4] = {0};
	u32 cumasks = 0;
	int err = 0;

	SCHED_DEBUGF("-> %s(%d)\n", __func__, ecmd->opcode);

	/* cus for start kernel commands only */
	if (ecmd->type != ERT_CU)
		return 0; /* ok */

	/* client context cu bitmap may not change while validating */
	mutex_lock(&client->xdev->dev_lock);

	/* no specific CUs selected, maybe ctx is not used by client */
	if (bitmap_empty(client->cu_bitmap, MAX_CUS)) {
		userpf_err(xocl_get_xdev(pdev), "%s found no CUs in ctx\n", __func__);
		err = 1;
		goto out;
	}

	/* Check CUs in cmd BO against CUs in context */
	cumasks = 1 + scmd->extra_cu_masks;
	xocl_bitmap_to_arr32(ctx_cus, client->cu_bitmap, cumasks * 32);

	for (i = 0; i < cumasks; ++i) {
		uint32_t cmd_cus = ecmd->data[i];
		/* cmd_cus must be subset of ctx_cus */
		if (cmd_cus & ~ctx_cus[i]) {
			SCHED_DEBUGF("<- %s(1), CU mismatch in mask(%d) cmd(0x%x) ctx(0x%x)\n",
				     __func__, i, cmd_cus, ctx_cus[i]);
			err = 1;
			goto out; /* error */
		}
	}


out:
	mutex_unlock(&client->xdev->dev_lock);
	SCHED_DEBUGF("<- %s(%d) cmd and ctx CUs match\n", __func__, err);
	return err;

}

struct xocl_mb_scheduler_funcs sche_ops = {
	.create_client = create_client,
	.destroy_client = destroy_client,
	.poll_client = poll_client,
	.client_ioctl = client_ioctl,
	.stop = stop,
	.reset = reset,
};

/* sysfs */
static ssize_t
kds_numcus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exec_core *exec = dev_get_exec(dev);
	unsigned int cus = exec ? exec->num_cus - exec->num_cdma : 0;

	return sprintf(buf, "%d\n", cus);
}
static DEVICE_ATTR_RO(kds_numcus);

static ssize_t
kds_cucounts_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exec_core *exec = dev_get_exec(dev);
	unsigned int cus = exec ? exec->num_cus - exec->num_cdma : 0;
	unsigned int sz = 0;
	unsigned int idx;

	for (idx = 0; idx < cus; ++idx) {
		struct xocl_cu *xcu = exec->cus[idx];

		sz += sprintf(buf, "cu[%d] done(%d) run(%d)\n", idx, xcu->done_cnt, xcu->run_cnt);
	}
	if (sz)
		buf[sz++] = 0;
	return sz;
}
static DEVICE_ATTR_RO(kds_cucounts);

static ssize_t
kds_numcdmas_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_xdev(dev);
	uint32_t *cdma = xocl_cdma_addr(xdev);
	unsigned int cdmas = cdma ? 1 : 0; //TBD

	return sprintf(buf, "%d\n", cdmas);
}
static DEVICE_ATTR_RO(kds_numcdmas);


static ssize_t
kds_custat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exec_core *exec = dev_get_exec(dev);
	struct xocl_dev *xdev = exec_get_xdev(exec);
	struct client_ctx client;
	struct ert_packet packet;
	unsigned int count = 0;
	ssize_t sz = 0;

	// minimum required initialization of client
	client.abort = false;
	client.xdev = xdev;
	atomic_set(&client.outstanding_execs, 0);

	packet.opcode = ERT_CU_STAT;
	packet.type = ERT_CTRL;
	packet.count = 1;  // data[1]

	if (add_ctrl_cmd(exec, &client, &packet) == 0) {
		int retry = 5;

		SCHED_DEBUGF("-> custat waiting for command to finish\n");
		// wait for command completion
		while (--retry && atomic_read(&client.outstanding_execs))
			msleep(100);
		if (retry == 0 && atomic_read(&client.outstanding_execs))
			userpf_info(xdev, "custat unexpected timeout\n");
		SCHED_DEBUGF("<- custat retry(%d)\n", retry);
	}

	for (count = 0; count < exec->num_cus; ++count)
		sz += sprintf(buf+sz, "CU[@0x%x] : %d\n",
			      exec_cu_base_addr(exec, count),
			      exec_cu_usage(exec, count));
	if (sz)
		buf[sz++] = 0;

	return sz;
}
static DEVICE_ATTR_RO(kds_custat);

static struct attribute *kds_sysfs_attrs[] = {
	&dev_attr_kds_numcus.attr,
	&dev_attr_kds_cucounts.attr,
	&dev_attr_kds_numcdmas.attr,
	&dev_attr_kds_custat.attr,
	NULL
};

static const struct attribute_group kds_sysfs_attr_group = {
	.attrs = kds_sysfs_attrs,
};

static void
user_sysfs_destroy_kds(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &kds_sysfs_attr_group);
}

static int
user_sysfs_create_kds(struct platform_device *pdev)
{
	int err = sysfs_create_group(&pdev->dev.kobj, &kds_sysfs_attr_group);

	if (err)
		userpf_err(xocl_get_xdev(pdev), "create kds attr failed: 0x%x", err);
	return err;
}

/**
 * Init scheduler
 */
static int mb_scheduler_probe(struct platform_device *pdev)
{
	struct exec_core *exec = exec_create(pdev, &scheduler0);

	if (!exec)
		return -ENOMEM;

	if (user_sysfs_create_kds(pdev))
		goto err;

	init_scheduler_thread(&scheduler0);
	xocl_subdev_register(pdev, XOCL_SUBDEV_MB_SCHEDULER, &sche_ops);
	platform_set_drvdata(pdev, exec);

	DRM_INFO("command scheduler started\n");

	return 0;

err:
	devm_kfree(&pdev->dev, exec);
	return 1;
}

/**
 * Fini scheduler
 */
static int mb_scheduler_remove(struct platform_device *pdev)
{
	struct xocl_dev *xdev;
	int i;
	struct exec_core *exec = platform_get_drvdata(pdev);

	SCHED_DEBUGF("-> %s\n", __func__);
	fini_scheduler_thread(exec_scheduler(exec));

	xdev = xocl_get_xdev(pdev);
	for (i = 0; i < exec->intr_num; i++) {
		xocl_user_interrupt_config(xdev, i + exec->intr_base, false);
		xocl_user_interrupt_reg(xdev, i + exec->intr_base,
			NULL, NULL);
	}
	mutex_destroy(&exec->exec_lock);

	user_sysfs_destroy_kds(pdev);
	exec_destroy(exec);
	platform_set_drvdata(pdev, NULL);

	SCHED_DEBUGF("<- %s\n", __func__);
	DRM_INFO("command scheduler removed\n");
	return 0;
}

static struct platform_device_id mb_sche_id_table[] = {
	{ XOCL_MB_SCHEDULER, 0 },
	{ },
};

static struct platform_driver	mb_scheduler_driver = {
	.probe		= mb_scheduler_probe,
	.remove		= mb_scheduler_remove,
	.driver		= {
		.name = "xocl_mb_sche",
	},
	.id_table	= mb_sche_id_table,
};

int __init xocl_init_mb_scheduler(void)
{
	return platform_driver_register(&mb_scheduler_driver);
}

void xocl_fini_mb_scheduler(void)
{
	SCHED_DEBUGF("-> %s\n", __func__);
	platform_driver_unregister(&mb_scheduler_driver);
	SCHED_DEBUGF("<- %s\n", __func__);
}
