/*
 * Copyright (C) 2018-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Soren Soe <soren.soe@xilinx.com>
 *    Jan Stephan <j.stephan@hzdr.de>
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
#include <linux/ktime.h>
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
# define SCHED_PRINT_PACKET(packet, size) sched_debug_packet(packet, size)
#else
# define SCHED_DEBUG(msg)
# define SCHED_DEBUGF(format, ...)
# define SCHED_PRINTF(format, ...) DRM_INFO(format, ##__VA_ARGS__)
# define SCHED_DEBUG_PACKET(packet, size)
# define SCHED_PRINT_PACKET(packet, size) sched_debug_packet(packet, size)
#endif

#define csr_read32(base, r_off)			\
	ioread32((base) + (r_off) - ERT_CSR_ADDR)
#define csr_write32(val, base, r_off)		\
	iowrite32((val), (base) + (r_off) - ERT_CSR_ADDR)

/* Highest bit in ip_reference indicate if it's exclusively reserved. */
#define	IP_EXCL_RSVD_MASK	(~(1 << 31))

#define	CU_ADDR_HANDSHAKE_MASK	(0xff)
#define	CU_ADDR_VALID(addr)	(((addr) | CU_ADDR_HANDSHAKE_MASK) != -1)

#if defined(XOCL_UUID)
static xuid_t uuid_null = NULL_UUID_LE;
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
static void scheduler_incr_poll(struct xocl_scheduler *xs);

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
 * mask_idx32() - Slot mask idx index for a given slot_idx
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of the slot mask containing the slot_idx
 */
static inline unsigned int
mask_idx32(unsigned int idx)
{
	return idx >> 5;
}

/**
 * idx_in_mask32() - Index of command queue slot within the mask that contains it
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of slot within the mask that contains it
 */
static inline unsigned int
idx_in_mask32(unsigned int idx, unsigned int mask_idx)
{
	return idx - (mask_idx << 5);
}

/**
 * Command data used by scheduler
 *
 * @cq_list: command object in scheduler command queue
 * @cu_list: command object is executing (penguin and dataflow mode only)
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
	struct list_head cu_list; // exec core running queue

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

	bool          aborted;  // set to true if CU aborts the command 
	unsigned long uid;      // unique id for this command
	unsigned int  cu_idx;   // index of CU running this cmd
	unsigned int  slot_idx; // index in exec core running queue

	bool timestamp_enabled;
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
 * Return: Size in number of uint32_t of command packet
 */
static inline unsigned int
cmd_packet_size(struct xocl_cmd *xcmd)
{
	return cmd_payload_size(xcmd) +
		sizeof(xcmd->ert_pkt->header) / sizeof(uint32_t);
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

static inline void
cmd_record_timestamp(struct xocl_cmd *xcmd, enum ert_cmd_state state)
{
	if (!xcmd->timestamp_enabled)
		return;

	ert_start_kernel_timestamps(xcmd->ert_cu)->
		skc_timestamps[state] = ktime_to_ns(ktime_get());
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
	cmd_record_timestamp(xcmd, state);
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
	SCHED_DEBUGF("-> %s(%lu,%d)\n", __func__, xcmd->uid, state);
	cmd_record_timestamp(xcmd, state);
	xcmd->state = state;
	xcmd->ert_pkt->state = state;
	SCHED_DEBUGF("<- %s\n", __func__);
}

/*
 * update_state() - Update command state if client has aborted
 */
static enum ert_cmd_state
cmd_update_state(struct xocl_cmd *xcmd)
{
	if (xcmd->state != ERT_CMD_STATE_RUNNING && xcmd->client->abort) {
		userpf_info(xcmd->xdev, "aborting stale client pid(%d) cmd(%lu)"
			    ,pid_nr(xcmd->client->pid),xcmd->uid);
		cmd_set_state(xcmd, ERT_CMD_STATE_ABORT);
	}
	if (exec_is_flush(xcmd->exec)) {
		userpf_info(xcmd->xdev, "aborting stale exec pid (%d) cmd(%lu)"
			    ,pid_nr(xcmd->client->pid),xcmd->uid);
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
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xcmd->bo->base);
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
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&dbo->base);
		xcmd->deps[didx] = NULL;
		if (!chain_to) { // command may have completed already
			--xcmd->wait_count;
			continue;
		}
		if (chain_to->chain_count >= MAX_DEPS) {
			userpf_err(xcmd->xdev,
				   "cmd (%lu) chain count (%d) exceeds maximum allowed (%d)",
				   chain_to->uid, chain_to->chain_count, MAX_DEPS);
			return 1;
		}
		SCHED_DEBUGF("+ xcmd(%lu)->chain[%d]=xcmd(%lu)",
			     chain_to->uid, chain_to->chain_count, xcmd->uid);
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
	INIT_LIST_HEAD(&xcmd->cq_list);
	INIT_LIST_HEAD(&xcmd->cu_list);
	xcmd->aborted = false;
	xcmd->uid = count++;
	xcmd->exec = exec;
	xcmd->cu_idx = no_index;
	xcmd->slot_idx = no_index;
	xcmd->xs = xs;
	xcmd->xdev = client->xdev;
	xcmd->client = client;
	xcmd->bo = NULL;
	xcmd->ert_pkt = NULL;
	xcmd->chain_count = 0;
	xcmd->wait_count = 0;
	xcmd->timestamp_enabled = false;
	atomic_inc(&client->outstanding_execs);
	SCHED_DEBUGF("xcmd(%lu) xcmd(%p) [-> new ]\n", xcmd->uid, xcmd);
	return xcmd;
}

/**
 * cmd_free() - free a command object
 *
 * @xcmd: command object to free (move to freelist)
 */
static void
cmd_free(struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s xcmd(%lu)\n", __func__, xcmd->uid);

	cmd_release_gem_object_reference(xcmd);

	mutex_lock(&free_cmds_mutex);
	list_move_tail(&xcmd->cq_list, &free_cmds);
	mutex_unlock(&free_cmds_mutex);

	SCHED_DEBUGF("<- %s\n", __func__);
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

static inline bool
cmd_can_enable_timestamps(struct xocl_cmd *xcmd)
{
	struct ert_start_kernel_cmd *pkt = xcmd->ert_cu;

	if (cmd_type(xcmd) != ERT_CU || !xcmd->ert_cu->stat_enabled)
		return false;

	if ((char *)ert_start_kernel_timestamps(pkt) +
		sizeof(struct cu_cmd_state_timestamps) >
		(char *)pkt + xcmd->bo->base.size) {
		userpf_err(xcmd->xdev, "no space for timestamps in exec buf");
		return false;
	}
	return true;
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

	xcmd->timestamp_enabled = cmd_can_enable_timestamps(xcmd);

	// copy pkt cus to command object cu bitmap
	if (cmd_type(xcmd) == ERT_CU) {
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

/**
 * cmd_has_cu() - Check if this command object can execute on CU
 *
 * @cuidx: the index of the CU.	 Note that CU indicies start from 0.
 */
static inline bool
cmd_has_cu(struct xocl_cmd *xcmd, unsigned int cuidx)
{
	SCHED_DEBUGF("%s(%lu,%d) = %d\n", __func__, xcmd->uid, cuidx, test_bit(cuidx, xcmd->cu_bitmap));
	return test_bit(cuidx, xcmd->cu_bitmap);
}

/**
 * cmd_first_cu() - Get index of first CU this command can use
 */
static inline unsigned int
cmd_first_cu(struct xocl_cmd *xcmd)
{
	return find_first_bit(xcmd->cu_bitmap, MAX_CUS);
}

/**
 * cmd_next_cu() - Get index of CU after @prev this command can use
 *
 * @prev: index of previous CU
 */
static inline unsigned int
cmd_next_cu(struct xocl_cmd *xcmd, unsigned int prev)
{
	return find_next_bit(xcmd->cu_bitmap, MAX_CUS, prev + 1);
}

/**
 * cmd_set_cu() - Lock command to one specific CU
 *
 * @cuidx: Selected specific CU that this command can use
 */
static inline void
cmd_set_cu(struct xocl_cmd *xcmd, unsigned int cuidx)
{
	SCHED_DEBUGF("-> %s cmd(%lu) cuidx(%d)\n", __func__, xcmd->uid, cuidx);

	xcmd->cu_idx = cuidx;
	bitmap_zero(xcmd->cu_bitmap, MAX_CUS);
	set_bit(cuidx, xcmd->cu_bitmap);

	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * cmd_ctx_in() - Get the context/queue ID from the command
 *
 * Applicable only for ERT_CU commands when the command targets a CU
 * that has context / queue feature enabled, this is checked by caller.
 */
static uint32_t
cmd_ctx_read(struct xocl_cmd *xcmd)
{
	u32 *regmap = cmd_regmap(xcmd);

	// ctx-in 0x10, ctx-out 0x14
	if (cmd_regmap_size(xcmd) < 6) {
		userpf_err(xcmd->xdev,"cmd(%lu) regmap size (%d) is too small for context/queue parameters\n",
			   xcmd->uid,cmd_regmap_size(xcmd));
		return 0;
	}
	return regmap[4];
}

/**
 * struct xocl_cu: Represents a compute unit in penguin or dataflow mode
 *
 * @done_queue: a fifo of cmds completed by CU, popped off by scheduler
 * @xdev: the xrt device with this CU
 * @idx: index of this CU
 * @dataflow: true when running in dataflow mode
 * @base: exec base address of this CU
 * @addr: base address of this CU
 * @polladdr: address of CU poll request register
 * @ctrlreg: state of the CU (value of AXI-lite control register)
 * @done_cnt: number of commands that have completed (<=running_queue.size())
 * @run_cnt: number of commands that have bee started (<=running_queue.size())
 *
 * A compute unit is configured with a number of context it supports.  Each 
 * context manages command execution separate from other contexts.  A command
 * started in some context finished in order in that context, but a context
 * executes out of order with respect to another context.
 *
 * By default a compute unit supports one implicit context.  This one context
 * is used always in AP_CTRL_HS and by default in AP_CTRL_CHAIN unless the
 * kernel with the compute unit explicitly advertise suppport for contexts.
 *
 * When a kernel supports explicit context (only AP_CTRL_CHAIN has this
 * option), then the command register map at offset 0x10 contains the context
 * number identifying the context on which the command should execute. When
 * the CU raises AP_DONE, cu_poll() reads the CU register map at offset 0x14
 * to obtain the context number that corresponds to the AP_DONE.  After
 * reading the context register at 0x14, then cu_poll() acknowledges AP_DONE
 * by writing AP_CONTINUE.
 *
 * When a command finishes, it is moved from the ctx list to the done_queue
 * in the CU.  The scheduler picks command off the done list in the order
 * in which they are inserted into the list.
 *
 * A context error occurs in either of following cases
 *  1. Command explicit context (ctx_in) exceeds CU configured contexts.
 *     If this error occurs, the cmd is aborted (never started on CU)
 *  2. CU output context (ctx_out) exceeds CU configured contexts
 *     If this error occurs, then all cmds are aborted, the CU is put in 
 *     error state and will not accept new commands, likely xbutil reset
 *     will be necessary.
 *  3. When ctx queue has no cmd for corresponding ctx_out
 *     Same error handling as for case 2.
 */
struct xocl_cu {
	struct list_head   done_queue;
	struct xocl_dev    *xdev;
	unsigned int       idx;
	unsigned int       uid;
	u32                control;
	void __iomem       *base;
	u32                addr;
	void __iomem       *polladdr;
	u32                ap_check;
	bool               error;

	u32                ctrlreg;
	unsigned int       done_cnt;
	unsigned int       run_cnt;

	// context handling
	u16                ctx_cfg;  // configured contexts
	u16                ctx_size;    // allocated contexts
	struct list_head   *ctx;
};

/*
 * Allocate queues for requested number of contexts
 * By default all CUs have one context / queue
 */
static int
cu_alloc_ctx(struct xocl_cu *xcu, unsigned int nctx)
{
	unsigned int idx;
	if (xcu->ctx_size < nctx) {
		kfree(xcu->ctx);
		xcu->ctx = kmalloc(sizeof(struct list_head) * nctx, GFP_KERNEL);
		xcu->ctx_size = xcu->ctx ? nctx : 0;
	}
	for (idx=0; idx < xcu->ctx_size; ++idx)
		INIT_LIST_HEAD(&xcu->ctx[idx]);

	// A CU must have at least one context even if it doesn't
	// support context execution
	xcu->error = xcu->error || (xcu->ctx_size == 0);

	return xcu->error;
}

/*
 */
static int
cu_reset(struct xocl_cu *xcu, unsigned int idx, void __iomem *base, u32 addr, void *polladdr)
{
	xcu->error = false;
	xcu->ctx_cfg = ((addr & 0xF8) >> 3); // bits [7-3]
	xcu->idx = idx;
	xcu->control = (addr & 0x7); // bits [2-0]
	xcu->base = base;
	xcu->addr = addr & ~CU_ADDR_HANDSHAKE_MASK;  // clear encoded handshake and context
	xcu->polladdr = polladdr;
	xcu->ap_check = (xcu->control == AP_CTRL_CHAIN) ? (AP_DONE) : (AP_DONE | AP_IDLE);
	xcu->ctrlreg = 0;
	xcu->done_cnt = 0;
	xcu->run_cnt = 0;
	cu_alloc_ctx(xcu, xcu->ctx_cfg);
	userpf_info(xcu->xdev, "configured cu(%d) base@0x%x poll@0x%p control(%d) ctx(%d)\n",
		    xcu->idx, xcu->addr, xcu->polladdr, xcu->control, xcu->ctx_cfg);

	return xcu->error;
}

/**
 */
struct xocl_cu *
cu_create(struct xocl_dev *xdev)
{
	struct xocl_cu *xcu = kmalloc(sizeof(struct xocl_cu), GFP_KERNEL);
	static unsigned int uid;

	INIT_LIST_HEAD(&xcu->done_queue);
	xcu->xdev = xdev;
	xcu->uid = uid++;
	xcu->ctx_size = 0;
	xcu->ctx_cfg = 0;
	xcu->ctx = NULL;
	cu_alloc_ctx(xcu, 1);  // one ctx by default
	SCHED_DEBUGF("%s(uid:%d)\n", __func__, xcu->uid);
	return xcu;
}

static inline u32
cu_base_addr(struct xocl_cu *xcu)
{
	return xcu->addr;
}

static inline bool
cu_dataflow(struct xocl_cu *xcu)
{
	return xcu->control == AP_CTRL_CHAIN;
}

static inline bool
cu_valid(struct xocl_cu *xcu)
{
	return CU_ADDR_VALID(xcu->addr);
}

static void
cu_abort_cmd(struct xocl_cu *xcu, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s\n", __func__);
	userpf_err(xcu->xdev,"aborting cu(%d) cmd(%lu)\n", xcu->uid, xcmd->uid);
	list_move_tail(&xcmd->cu_list, &xcu->done_queue);
	xcmd->aborted = true;
	++xcu->done_cnt;  // cmd was moved to done queue
	SCHED_DEBUGF("<- %s cu(%d) done(%d) run(%d)\n", __func__, xcu->uid, xcu->done_cnt, xcu->run_cnt);
}

static void
cu_abort_ctx(struct xocl_cu *xcu, uint32_t ctxid)
{
	struct list_head *pos, *next;
	SCHED_DEBUGF("-> %s\n", __func__);
	list_for_each_safe(pos, next, &xcu->ctx[ctxid]) {
		struct xocl_cmd *xcmd = list_entry(pos, struct xocl_cmd, cu_list);
		cu_abort_cmd(xcu, xcmd);
		--xcu->run_cnt; // cmd was moved from ctx queue
	}
	SCHED_DEBUGF("<- %s cu(%d) done(%d) run(%d)\n", __func__, xcu->uid, xcu->done_cnt, xcu->run_cnt);
}

static void
cu_abort(struct xocl_cu *xcu)
{
	uint32_t ctxid;
	SCHED_DEBUGF("-> %s\n", __func__);
	for (ctxid = 0; ctxid < xcu->ctx_size; ++ctxid)
		cu_abort_ctx(xcu,ctxid);
	xcu->error = true;
	SCHED_DEBUGF("<- %s cu marked in error\n", __func__);
}

/**
 * cu_ctx_out() - Read back context from CU
 *
 * @return: If the CU is not configured with explicit context, then 
 *   return the default ctx id (0), otherwise read CU @ 0x14 offset
 */
static inline uint32_t
cu_ctx_out(struct xocl_cu *xcu)
{
	uint32_t ctxid;

	if (!xcu->ctx_cfg)
		return 0;  // default ctx

	ctxid = ioread32(xcu->base + xcu->addr + 0x14);
	if (ctxid < xcu->ctx_cfg) {
		SCHED_DEBUGF("%s cu(%d) ctx_out(%d)\n",__func__, xcu->uid, ctxid);
		return ctxid; // explicit context
	}

	userpf_err(xcu->xdev,"invalid output ctx(%d) for cu(%d) with max ctx(%d)\n",
		   xcu->uid,ctxid,xcu->ctx_cfg);
	cu_abort(xcu);
	return no_index;
}

static inline uint32_t
cu_ctx_in(struct xocl_cu *xcu, struct xocl_cmd *xcmd)
{
	uint32_t ctxid;

	if (!xcu->ctx_cfg) 
		return 0;  // default ctx

	ctxid = cmd_ctx_read(xcmd);
	if (ctxid < xcu->ctx_cfg) {
		SCHED_DEBUGF("%s cu(%d) cmd(%lu) ctx_in(%d)\n",__func__, xcu->uid, xcmd->uid, ctxid);
		return ctxid;  // explicit context
	}

	userpf_err(xcu->xdev,"invalid input ctx(%d) in cmd(%lu) for cu(%d) with max ctx(%d)\n",
		   ctxid, xcmd->uid, xcu->uid, xcu->ctx_cfg);
	return no_index;
}

/**
 */
static void
cu_destroy(struct xocl_cu *xcu)
{
	SCHED_DEBUGF("%s(uid:%d)\n", __func__, xcu->uid);
	kfree(xcu);
}

/**
 * cu_pop_ctx() - Move command from ctx list to CU end of done list
 */
static int
cu_pop_ctx(struct xocl_cu *xcu)
{
	struct xocl_cmd *xcmd;
	uint32_t ctxid = cu_ctx_out(xcu);

	if (ctxid == no_index)
		return 1;
	
	xcmd = list_first_entry_or_null(&xcu->ctx[ctxid], struct xocl_cmd, cu_list);
	if (!xcmd) {
		userpf_err(xcu->xdev,"missing cmd in cu(%d) for ctx(%d)\n",
			   xcu->uid,ctxid);
		cu_abort(xcu);
		return 1;
		
	}

        SCHED_DEBUGF("%s xcu(%d) ctx(%d) pops xcmd(%lu)\n"
		     , __func__, xcu->uid, ctxid, xcmd->uid);
	list_move_tail(&xcmd->cu_list, &xcu->done_queue);
	++xcu->done_cnt; // assert done_cnt <= |running_queue|
	--xcu->run_cnt;
	return 0;
}

/**
 * cu_push_ctx() - Save command on running queue
 */
static int
cu_push_ctx(struct xocl_cu *xcu, struct xocl_cmd *xcmd)
{
	uint32_t ctxid;
	if (xcu->error || (ctxid = cu_ctx_in(xcu, xcmd)) == no_index) {
		// immididately abort cmd, by marking it done
		cu_abort_cmd(xcu, xcmd);
		return 1;
	}
		
	SCHED_DEBUGF("%s cu(%d) ctx(%d) pushes cmd(%lu)\n",
		     __func__, xcu->uid, ctxid, xcmd->uid);
	list_add_tail(&xcmd->cu_list, &xcu->ctx[ctxid]);
	++xcu->run_cnt;
	return 0;
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
	if (!cu_dataflow(xcu))
		return;

	SCHED_DEBUGF("-> %s cu(%d) @0x%x\n", __func__, xcu->idx, xcu->addr);

	// acknowledge done directly to CU (xcu->addr)
	iowrite32(AP_CONTINUE, xcu->base + xcu->addr);

	// in ert_poll mode acknowlegde done to ERT
	if (xcu->polladdr && xcu->run_cnt) {
		SCHED_DEBUGF("+ @0x%p\n", xcu->polladdr);
		iowrite32(AP_CONTINUE, xcu->polladdr);
	}

	SCHED_DEBUGF("<- %s\n", __func__);
}

static inline u32
cu_status(struct xocl_cu *xcu)
{
	return ioread32(xcu->base + xcu->addr);
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

	xcu->ctrlreg = cu_status(xcu);

	SCHED_DEBUGF("+ ctrlreg(0x%x)\n", xcu->ctrlreg);

	if (xcu->run_cnt && (xcu->ctrlreg & xcu->ap_check)) {
		cu_pop_ctx(xcu);
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

	if ((xcu->ctrlreg & AP_START) || (!cu_dataflow(xcu) && xcu->run_cnt))
		cu_poll(xcu);

	SCHED_DEBUGF("<- %s returns %d\n", __func__,
		     cu_dataflow(xcu) ? !(xcu->ctrlreg & AP_START) : xcu->run_cnt == 0);

	return cu_dataflow(xcu) ? !(xcu->ctrlreg & AP_START) : xcu->run_cnt == 0;
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
		? list_first_entry(&xcu->done_queue, struct xocl_cmd, cu_list)
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

	xcmd = list_first_entry(&xcu->done_queue, struct xocl_cmd, cu_list);
	list_del(&xcmd->cu_list);
	--xcu->done_cnt;

	SCHED_DEBUGF("%s(%d) xcmd(%lu) done(%d) run(%d)\n", __func__,
		     xcu->idx, xcmd->uid, xcu->done_cnt, xcu->run_cnt);
}

/**
 * cu_configure_ooo() - Configure a CU with {addr,val} pairs (out-of-order)
 */
static void
cu_configure_ooo(struct xocl_cu *xcu, struct xocl_cmd *xcmd)
{
	unsigned int size = cmd_regmap_size(xcmd);
	u32 *regmap = cmd_regmap(xcmd);
	unsigned int idx;

	SCHED_DEBUGF("-> %s cu(%d) xcmd(%lu)\n", __func__, xcu->idx, xcmd->uid);
	// past reserved 4 ctrl + 2 ctx 
	for (idx = 6; idx < size - 1; idx += 2) {
		u32 offset = *(regmap + idx);
		u32 val = *(regmap + idx + 1);

		SCHED_DEBUGF("+ base[0x%x] = 0x%x\n", offset, val);
		iowrite32(val, xcu->base + xcu->addr + offset);
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * cu_configure_ino() - Configure a CU with consecutive layout (in-order)
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

	// Push command on context.  If bad cmd ctx, the command is
	// immediately marked done so that cmd can be processed next
	if (cu_push_ctx(xcu, xcmd))
		return true;

	// past header, past cumasks
	SCHED_DEBUG_PACKET(cmd_regmap(xcmd), cmd_regmap_size(xcmd));

	// write register map, starting at base + 0x10
	// 0x0 used for control register
	// 0x4, 0x8 used for interrupt, which is initialized in setup of ERT
	// 0xC used for interrupt status, which is set by hardware
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
		SCHED_DEBUGF("+ @0x%p\n", xcu->polladdr);
		iowrite32(AP_START, xcu->polladdr);
	}

	SCHED_DEBUGF("<- %s cu(%d) started xcmd(%lu) done(%d) run(%d)\n",
		     __func__, xcu->idx, xcmd->uid, xcu->done_cnt, xcu->run_cnt);

	return true;
}


/**
 * struct xocl_ert: Represents embedded scheduler in ert mode
 *
 * @cq_size: Size of HW command queue
 * @num_slots: Number of slot in CQ
 * @slot_size: Size of a CQ slot
 * @cq_intr: Enable interrupts host -> MB for new commands
 * @command_queue: Command queue with commands that have been sent to ERT
 * @slot_status: Bitmap to track status (busy(1)/free(0)) slots in command queue
 * @ctrl_busy: Flag to indicate that slot 0 (ctrl commands) is busy
 * @version: Version per HW ERT
 * @cu_usage: CU usage count per ERT FW (since last reset)
 * @cu_status: CU status per ERT FW
 * @cq_slot_status: CQ (command_queue) slot status per ERT FW
 * @cq_slot_usage: CQ (command_queue) usage count since last reset
 */
struct xocl_ert {
	struct xocl_dev * xdev;
	void __iomem *    csr_base;
	void __iomem *    cq_base;
	unsigned int      uid;

	unsigned int      cq_size;
	unsigned int      num_slots;

	unsigned int      slot_size;
	bool              cq_intr;
	
	struct xocl_cmd * command_queue[MAX_SLOTS];

	// Bitmap tracks busy(1)/free(0) slots in command_queue
	DECLARE_BITMAP(slot_status, MAX_SLOTS);
	unsigned int	  ctrl_busy;

	// stats
	u32               version;
	u32               cu_usage[MAX_CUS];
	u32               cu_status[MAX_CUS];
	u32               cq_slot_status[MAX_SLOTS];
	unsigned int      cq_slot_usage[MAX_SLOTS];
};

/*
 */
struct xocl_ert *
ert_create(struct xocl_dev * xdev, void __iomem *csr_base, void __iomem *cq_base)
{
	struct xocl_ert *xert = kmalloc(sizeof(struct xocl_ert), GFP_KERNEL);
	static unsigned int uid;

	xert->xdev = xdev;
	xert->csr_base = csr_base;
	xert->cq_base = cq_base;
	xert->uid = uid++;
	xert->num_slots = 0;
	xert->slot_size = 0;
	xert->cq_intr = false;
	SCHED_DEBUGF("%s(%d,0x%p)\n", __func__, xert->uid, xert->cq_base);
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
ert_cfg(struct xocl_ert *xert, unsigned int cq_size, unsigned int num_slots, bool cq_intr)
{
	unsigned int idx;

	SCHED_DEBUGF("%s ert(%d) cq_size(%d) slots(%d) slot_size(%d) cq_intr(%d)\n",
		     __func__, xert->uid, cq_size, num_slots, cq_size / num_slots, cq_intr);
	xert->cq_size = cq_size;
	xert->num_slots = num_slots;
	xert->slot_size = cq_size / num_slots;
	xert->cq_intr = cq_intr;
	xert->version = 0;

	for (idx = 0; idx < MAX_CUS; ++idx) {
		xert->cu_usage[idx] = 0;
		xert->cu_status[idx] = 0;
	}

	for (idx = 0; idx < MAX_SLOTS; ++idx) {
		xert->command_queue[idx] = NULL;
		xert->cq_slot_status[idx] = 0;
		xert->cq_slot_usage[idx] = 0;
	}

	bitmap_zero(xert->slot_status, MAX_SLOTS);
	set_bit(0, xert->slot_status); // reserve for control command
	xert->ctrl_busy = false;
}

/*
 * acquire_slot_idx() - First available slot index
 */
static unsigned int
ert_acquire_slot_idx(struct xocl_ert *xert)
{
	unsigned int idx = find_first_zero_bit(xert->slot_status, MAX_SLOTS);

	SCHED_DEBUGF("%s(%d) returns %d\n", __func__, xert->uid, idx < xert->num_slots ? idx : no_index);
	if (idx < xert->num_slots) {
		set_bit(idx, xert->slot_status);
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
ert_acquire_slot(struct xocl_ert *xert, struct xocl_cmd *xcmd)
{
	// slot 0 is reserved for ctrl commands
	if (cmd_type(xcmd) == ERT_CTRL) {
		SCHED_DEBUGF("%s ctrl cmd(%lu)\n", __func__, xcmd->uid);
		if (xert->ctrl_busy) {
			userpf_info(xert->xdev, "ctrl slot is busy\n");
			return -1;
		}
		xert->ctrl_busy = true;
		return (xcmd->slot_idx = 0);
	}

	return (xcmd->slot_idx = ert_acquire_slot_idx(xert));
}

/*
 * release_slot_idx() - Release specified slot idx
 */
static void
ert_release_slot_idx(struct xocl_ert *xert, unsigned int slot_idx)
{
	clear_bit(slot_idx, xert->slot_status);
}

/**
 * release_slot() - Release a slot index for a command
 *
 * Special case for control commands that execute in slot 0.  This
 * slot cannot be marked free ever.
 */
static void
ert_release_slot(struct xocl_ert *xert, struct xocl_cmd *xcmd)
{
	if (xcmd->slot_idx == no_index)
		return; // already released

	SCHED_DEBUGF("-> %s(%d) xcmd(%lu) slotidx(%d)\n",
		     __func__, xert->uid, xcmd->uid, xcmd->slot_idx);
	if (cmd_type(xcmd) == ERT_CTRL) {
		SCHED_DEBUG("+ ctrl cmd\n");
		xert->ctrl_busy = false;
	} else {
		ert_release_slot_idx(xert, xcmd->slot_idx);
	}
	xert->command_queue[xcmd->slot_idx] = NULL;
	xcmd->slot_idx = no_index;
	SCHED_DEBUGF("<- %s\n", __func__);
}

static inline struct xocl_cmd *
ert_get_cmd(struct xocl_ert *xert, unsigned int slotidx)
{
	return xert->command_queue[slotidx];
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
	u32 slot_addr = 0;
	struct ert_packet *ecmd = cmd_packet(xcmd);

	SCHED_DEBUGF("-> %s ert(%d) cmd(%lu)\n", __func__, xert->uid, xcmd->uid);

	if (ert_acquire_slot(xert, xcmd) == no_index) {
		SCHED_DEBUGF("<- %s returns false (noindex)\n", __func__);
		return false;
	}

	slot_addr = xcmd->slot_idx * xert->slot_size;

	SCHED_DEBUG_PACKET(ecmd, cmd_packet_size(xcmd));

	// write packet minus header
	if (cmd_type(xcmd) == ERT_CU && !XOCL_DSA_IS_VERSAL(xcmd->xdev)) {
		// write kds selected cu_idx in first cumask (first word after header)
		iowrite32(xcmd->cu_idx, xert->cq_base + slot_addr + 4);

		// write remaining packet (past header and cuidx)
		xocl_memcpy_toio(xert->cq_base + slot_addr + 8,
				 ecmd->data + 1, (ecmd->count - 1) * sizeof(u32));
	}
	else
		xocl_memcpy_toio(xert->cq_base + slot_addr + 4,
				 ecmd->data, ecmd->count * sizeof(u32));

	// write header
	iowrite32(ecmd->header, xert->cq_base + slot_addr);

	// trigger interrupt to embedded scheduler if feature is enabled
	if (xert->cq_intr) {
		u32 mask_idx = mask_idx32(xcmd->slot_idx);
		u32 cq_int_addr = ERT_CQ_STATUS_REGISTER_ADDR + (mask_idx << 2);
		u32 mask = 1 << idx_in_mask32(xcmd->slot_idx, mask_idx);

		SCHED_DEBUGF("++ mb_submit writes slot mask 0x%x to CQ_INT register at addr 0x%x\n",
			     mask, cq_int_addr);
		csr_write32(mask, xert->csr_base, cq_int_addr);
	}

	// success
	++xert->cq_slot_usage[xcmd->slot_idx];
	xert->command_queue[xcmd->slot_idx] = xcmd;
	
	SCHED_DEBUGF("<- %s returns true\n", __func__);
	return true;
}

/**
 * New ERT populates:
 * [1  ]      : header
 * [1  ]      : custat version
 * [1  ]      : ert git version
 * [1  ]      : number of cq slots
 * [1  ]      : number of cus
 * [#numcus]  : cu execution stats (number of executions)
 * [#numcus]  : cu status (1: running, 0: idle)
 * [#slots]   : command queue slot status
 *
 * Old ERT populates
 * [1  ]      : header
 * [#numcus]  : cu execution stats (number of executions)
 */
static void
ert_read_custat(struct xocl_ert *xert, struct xocl_cmd *xcmd, unsigned int num_cus)
{
	u32 slot_addr = xcmd->slot_idx*xert->slot_size;

	// cu stat version is 1 word past header
	u32 custat_version = ioread32(xert->cq_base + slot_addr + 4);

	xert->version = -1;
	memset(xert->cu_usage, -1, MAX_CUS * sizeof(u32));
	memset(xert->cu_status, -1, MAX_CUS * sizeof(u32));
	memset(xert->cq_slot_status, -1, MAX_SLOTS * sizeof(u32));

	// New command style from ERT firmware
	if (custat_version == 0x51a10000) {
		unsigned int idx = 2; // packet word index past header and version
		unsigned int max_idx = (xert->slot_size >> 2);
		u32 git = ioread32(xert->cq_base + slot_addr + (idx++ << 2));
		u32 ert_num_cq_slots = ioread32(xert->cq_base + slot_addr + (idx++ << 2));
		u32 ert_num_cus = ioread32(xert->cq_base + slot_addr + (idx++ << 2));
		unsigned int words = 0; 

		xert->version = git;

		// bogus data in command, avoid oob writes to local arrays
		if (ert_num_cus > MAX_CUS || ert_num_cq_slots > MAX_CUS)
			return;

		// cu execution stat
		words = min(ert_num_cus, max_idx - idx);
		xocl_memcpy_fromio(xert->cu_usage, xert->cq_base + slot_addr + (idx << 2),
			      words * sizeof(u32));
		idx += words;

		// ert cu status
		words = min(ert_num_cus, max_idx - idx);
		xocl_memcpy_fromio(xert->cu_status, xert->cq_base + slot_addr + (idx << 2),
			      words * sizeof(u32));
		idx += words;

		// ert cq status
		words = min(ert_num_cq_slots, max_idx - idx);
		xocl_memcpy_fromio(xert->cq_slot_status, xert->cq_base + slot_addr + (idx << 2),
			      words * sizeof(u32));
		idx += words;
	}
	else {
		// Old ERT command style populates only cu usage past header
		xocl_memcpy_fromio(xert->cu_usage, xert->cq_base + slot_addr + 4,
			      num_cus * sizeof(u32));
	}
}

/**
 */
static inline u32
ert_version(struct xocl_ert *xert)
{
	return xert->version;
}

/**
 */
static inline u32
ert_cu_usage(struct xocl_ert *xert, unsigned int cuidx)
{
	return xert->cu_usage[cuidx];
}

/**
 */
static inline u32
ert_cu_status(struct xocl_ert *xert, unsigned int cuidx)
{
	return xert->cu_status[cuidx];
}

/**
 */
static inline bool
ert_cq_slot_busy(struct xocl_ert *xert, unsigned int slotidx)
{
	return xert->command_queue[slotidx] != NULL;
}

/**
 */
static inline u32
ert_cq_slot_status(struct xocl_ert *xert, unsigned int slotidx)
{
	return xert->cq_slot_status[slotidx];
}

static inline u32
ert_cq_slot_usage(struct xocl_ert *xert, unsigned int slotidx)
{
	return xert->cq_slot_usage[slotidx];
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
 * @pdev: Platform device associated with this execution core
 * @exec_lock: Lock for synchronizing external access
 * @base: BAR address
 * @csr_base: Status register base address
 * @cq_base: CQ base address
 * @intr_base:
 * @intr_num:
 * @ert_cfg_priv: Private data for scheduler subdevice
 * @poll_wait_queue: Wait queue for device polling
 * @scheduler: Command queue scheduler
 * @core_list: List head for storing in scheduler
 * @xclbin_id: UUID of current loaded xclbin
 * @num_cus: Number of CUs in loaded program
 * @num_cdma: Number of CDMAs in hardware
 * @polling_mode: If set then poll for command completion
 * @cq_interrupt: If set then trigger interrupt to MB on new commands
 * @configure_active: Flag to indicate that a configure command is active
 * @configured: Flag to indicate that the core data structure has been initialized
 * @stopped: Flag to indicate that the core data structure cannot be used
 * @flush: Flag to indicate that commands for this device should be flushed
 * @pending_cu_queue: num_cus array of commands submitted but pending execution on a CU
 * @cu_load_count: num_cus array of number of pending + running (submitted) commands for a CU
 * @pending_ctrl_queue: Pending control commands
 * @pending_kds_queue: Pending kds (local) commands
 * @pending_cmd_queue: Staging queue for commands that are queued but not yet submitted
 * @cu_usage: CU usage count since last reset
 * @cu_status: AP_CTRL status of CU, updated by ERT_CU_STAT
 * @sr0: If set, then status register [0..31] is pending with completed commands (ERT only).
 * @sr1: If set, then status register [32..63] is pending with completed commands (ERT only).
 * @sr2: If set, then status register [64..95] is pending with completed commands (ERT only).
 * @sr3: If set, then status register [96..127] is pending with completed commands (ERT only).
 * @ops: Scheduler operations vtable
 *
 * The execution core receives commands from scheduler when it transfers
 * execbuf command objects to execution cores where they are queued.  When the
 * scheduler services an execution core, the queued commands are submitted to
 * matching pending queue depending on command type.  A CU command is
 * submitted to the matching CU queue with fewest entries.  Pending CU
 * commands are started when the CU is available (kds mode) or when there is
 * room in the running command queue (ert mode).  When checking command
 * completion only the commands in the running queue need to be checked.
 */
struct exec_core {
	struct platform_device *   pdev;

	struct mutex		   exec_lock;

	void __iomem *		   base;
	void __iomem *		   csr_base;
	void __iomem *		   cq_base;
	unsigned int               cq_size;

	u32			   intr_base;
	u32			   intr_num;
	char			   ert_cfg_priv;
	bool			   needs_reset;

	wait_queue_head_t	   poll_wait_queue;

	struct xocl_scheduler *    scheduler;
	struct list_head           core_list;

	xuid_t			   xclbin_id;

	unsigned int		   num_cus;
	unsigned int		   num_cdma;
	
	bool		           polling_mode;
	bool		           cq_interrupt;
	bool		           configure_active;
	bool		           configured;
	bool		           stopped;
	bool		           flush;

	struct list_head           pending_cu_queue[MAX_CUS];
	struct list_head           pending_ctrl_queue;
	struct list_head           pending_kds_queue;

	struct list_head           running_cmd_queue;
	struct list_head           pending_cmd_queue;

	unsigned int               num_running_cmds;
	unsigned int               num_pending_cmds;
	unsigned int               cu_load_count[MAX_CUS];

	struct xocl_cu *	   cus[MAX_CUS];
	struct xocl_ert	*	   ert;

	u32			   cu_usage[MAX_CUS];
	u32			   cu_status[MAX_CUS];

	// Status register pending complete.  Written by ISR, cleared by
	// scheduler
	atomic_t		   sr0;
	atomic_t		   sr1;
	atomic_t		   sr2;
	atomic_t		   sr3;

	// Operations for dynamic indirection dependt on MB or kernel
	// scheduler
	struct exec_ops *	   ops;

	unsigned int		   uid;

	// For each CU, ip_reference contains either number of shared users
	// when the MSB is not set, or the PID of the process that exclusively
	// reserved it when MSB is set.
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

static inline u32
exec_cu_status(struct exec_core *exec, unsigned int cuidx)
{
	return exec->cu_status[cuidx];
}

static inline unsigned int
exec_num_running(struct exec_core *exec)
{
	return exec->num_running_cmds;
}

static inline unsigned int
exec_num_pending(struct exec_core *exec)
{
	return exec->num_pending_cmds;
}

static bool
exec_valid_cu(struct exec_core *exec, unsigned int cuidx)
{
	struct xocl_cu *xcu = exec->cus[cuidx];
	return xcu ? cu_valid(xcu) : false;
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
	uint32_t *cdma = xocl_rom_cdma_addr(xdev);
	unsigned int dsa = exec->ert_cfg_priv;
	struct ert_configure_cmd *cfg = xcmd->ert_cfg;
	bool ert = XOCL_DSA_IS_VERSAL(xdev) ? 1 : xocl_mb_sched_on(xdev);
	bool ert_full = (ert && cfg->ert && !cfg->dataflow);
	bool ert_poll = (ert && cfg->ert && cfg->dataflow);
	unsigned int ert_num_slots = 0;
	int cuidx = 0;

	/* Only allow configuration with one live ctx */
	if (exec->configured) {
		DRM_INFO("command scheduler is already configured for this device\n");
		return 1;
	}

	userpf_info(xdev, "ert per feature rom = %d", ert);
	userpf_info(xdev, "dsa52 = %d", dsa);

	if (XOCL_DSA_IS_VERSAL(xdev)) {
		userpf_info(xdev, "force polling mode for versal");
		cfg->polling = true;

		/*
		 * For versal device, we will use ert_full if we are
		 * configured as ert mode even dataflow is configured.
		 * And we do not support ert_poll.
		 */
		ert_full = cfg->ert;
		ert_poll = false;
	}

	/* Mark command as control command to force slot 0 execution */
	cfg->type = ERT_CTRL;

	if (cfg->count != 5 + cfg->num_cus) {
		userpf_err(xdev, "invalid configure command, count=%d expected 5+num_cus(%d)\n",
			   cfg->count, cfg->num_cus);
		return 1;
	}

	SCHED_DEBUGF("configuring scheduler cq_size(%d)\n", exec->cq_size);
	ert_num_slots = exec->cq_size / cfg->slot_size;
	exec->num_cus = cfg->num_cus;
	exec->num_cdma = 0;

	if (ert_poll)
		// Adjust slot size for ert poll mode
		cfg->slot_size = exec->cq_size / MAX_CUS;

	if (ert_full && cfg->cu_dma && ert_num_slots > 32) {
		// Max slot size is 32 because of cudma bug
		userpf_info(xdev, "Limitting CQ size to 32 due to ERT CUDMA bug\n");
		ert_num_slots = 32;
		cfg->slot_size = exec->cq_size / ert_num_slots;
	}

	// Create CUs for regular CUs
	for (cuidx = 0; cuidx < exec->num_cus; ++cuidx) {
		struct xocl_cu *xcu = exec->cus[cuidx];
		void *polladdr = (ert_poll)
			// cuidx+1 to reserve slot 0 for ctrl => max 127 CUs in ert_poll mode
			? (char *)exec->cq_base + (cuidx+1) * cfg->slot_size
			: NULL;

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
				void *polladdr = (ert_poll)
					? (char *)exec->cq_base + (cuidx+1) *
					cfg->slot_size : 0;

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
		exec->ert = ert_create(exec_get_xdev(exec), exec->csr_base, exec->cq_base);

	if (ert_poll) {
		userpf_info(xdev, "configuring dataflow mode with ert polling\n");
		cfg->slot_size = exec->cq_size / MAX_CUS;
		cfg->cu_isr = 0;
		cfg->cu_dma = 0;
		ert_cfg(exec->ert, exec->cq_size, MAX_CUS, cfg->cq_int);
		exec->ops = &ert_poll_ops;
		exec->polling_mode = cfg->polling;
	} else if (ert_full) {
		userpf_info(xdev, "configuring embedded scheduler mode\n");
		ert_cfg(exec->ert, exec->cq_size, ert_num_slots, cfg->cq_int);
		exec->ops = &ert_ops;
		exec->polling_mode = cfg->polling;
		exec->cq_interrupt = cfg->cq_int;
		cfg->dsa52 = dsa;
		cfg->cdma = cdma ? 1 : 0;
	} else {
		userpf_info(xdev, "configuring penguin scheduler mode\n");
		exec->ops = &penguin_ops;
		exec->polling_mode = true;
	}

	if (XDEV(xdev)->priv.flags & XOCL_DSAFLAG_CUDMA_OFF)
		cfg->cu_dma = 0;

	// The KDS side of of the scheduler is now configured.  If ERT is
	// enabled, then the configure command will be started asynchronously
	// on ERT.  The shceduler is not marked configured until ERT has
	// completed (exec_finish_cmd); this prevents other processes from
	// submitting commands to same xclbin.  However we must also stop
	// other processes from submitting configure command on this same
	// xclbin while ERT asynchronous configure is running.
	exec->configure_active = true;

	userpf_info(xdev, "scheduler config ert(%d), dataflow(%d), slots(%d), cudma(%d), cuisr(%d), cdma(%d), cus(%d)\n"
		 , ert_poll | ert_full
		 , cfg->dataflow
		 , ert_num_slots
		 , cfg->cu_dma ? 1 : 0
		 , cfg->cu_isr ? 1 : 0
		 , exec->num_cdma
		 , exec->num_cus);

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
exec_reset(struct exec_core *exec, const xuid_t *xclbin_id)
{
	struct xocl_dev *xdev = exec_get_xdev(exec);
	unsigned int idx;

	mutex_lock(&exec->exec_lock);

	userpf_info(xdev, "%s(%d) cfg(%d)\n", __func__, exec->uid, exec->configured);

	// only reconfigure the scheduler on new xclbin
	if (!xclbin_id || (uuid_equal(&exec->xclbin_id, xclbin_id) && exec->configured)) {
		exec->stopped = false;
		goto out;
	}

	userpf_info(xdev, "%s resets", __func__);
	userpf_info(xdev, "exec->xclbin(%pUb),xclbin(%pUb)\n", &exec->xclbin_id, xclbin_id);
	uuid_copy(&exec->xclbin_id, xclbin_id);
	exec->num_cus = 0;
	exec->num_cdma = 0;

	exec->polling_mode = true;
	exec->cq_interrupt = false;
	exec->configure_active = false;
	exec->configured = false;
	exec->stopped = false;
	exec->flush = false;
	exec->ops = &penguin_ops;

	for (idx = 0; idx < MAX_CUS; ++idx) {
		INIT_LIST_HEAD(&exec->pending_cu_queue[idx]);
		exec->cu_load_count[idx] = 0;
		exec->cu_usage[idx] = 0;
	}

	exec->num_running_cmds = 0;
	exec->num_pending_cmds = 0;

	INIT_LIST_HEAD(&exec->pending_ctrl_queue);
	INIT_LIST_HEAD(&exec->pending_kds_queue);
	INIT_LIST_HEAD(&exec->pending_cmd_queue);
	INIT_LIST_HEAD(&exec->running_cmd_queue);

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
		exec->flush = true;
		// Wake up the scheduler to force one iteration flushing stale
		// commands for this device
		scheduler_intr(exec->scheduler);

		// Wait a second
		msleep(1000);
	}

	outstanding = atomic_read(&xdev->outstanding_execs);
	if (outstanding)
		userpf_err(xdev, "unexpected outstanding commands %d after flush", outstanding);
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
	} else if (exec) {
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
	struct resource *res;
	static unsigned int count;
	unsigned int i;

	if (!exec)
		return NULL;

	mutex_init(&exec->exec_lock);
	exec->base = xdev->core.bar_addr;
	if (XOCL_GET_SUBDEV_PRIV(&pdev->dev))
		exec->ert_cfg_priv = *(char *)XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	else
		xocl_info(&pdev->dev, "did not get private data");

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res) {
		exec->intr_base = res->start;
		exec->intr_num = res->end - res->start + 1;
	} else
		xocl_info(&pdev->dev, "did not get IRQ resource");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_info(&pdev->dev, "did not get CSR resource");
	} else {
		exec->csr_base = ioremap_nocache(res->start,
			res->end - res->start + 1);
		if (!exec->csr_base) {
			xocl_err(&pdev->dev, "map CSR resource failed");
			return NULL;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		xocl_info(&pdev->dev, "did not get CQ resource");
	} else {
		exec->cq_size = res->end - res->start + 1;
		exec->cq_size = min(exec->cq_size, (unsigned int)ERT_CQ_SIZE);
		exec->cq_base = ioremap_nocache(res->start, exec->cq_size);
		if (!exec->cq_base) {
			if (exec->csr_base)
				iounmap(exec->csr_base);
			xocl_err(&pdev->dev, "map CQ resource failed");
			return NULL;
		}
		xocl_info(&pdev->dev, "CQ size is %d\n", exec->cq_size);
	}

	exec->pdev = pdev;
	if (XOCL_GET_SUBDEV_PRIV(&pdev->dev))
		exec->ert_cfg_priv = *(char *)XOCL_GET_SUBDEV_PRIV(&pdev->dev);

	init_waitqueue_head(&exec->poll_wait_queue);
	exec->scheduler = xs;
	exec->uid = count++;

	for (i = 0; i < exec->intr_num; i++) {
		xocl_user_interrupt_reg(xdev, i+exec->intr_base, exec_isr, exec);
		xocl_user_interrupt_config(xdev, i + exec->intr_base, true);
	}

	exec_reset(exec, &uuid_null);
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
	if (exec->csr_base) 
		iounmap(exec->csr_base);
	if (exec->cq_base)
		iounmap(exec->cq_base);

	list_del(&exec->core_list);

	devm_kfree(&exec->pdev->dev, exec);
}

/*
 */
static inline struct xocl_scheduler *
exec_scheduler(struct exec_core *exec)
{
	return exec->scheduler;
}


static void
exec_update_custatus(struct exec_core *exec)
{
	unsigned int cuidx;
	// ignore kdma which on least at u200_2018_30_1 is not BAR mapped
	for (cuidx = 0; cuidx < exec->num_cus - exec->num_cdma; ++cuidx) {
		// skip free running kernels which is not BAR mapped
		if (!exec_valid_cu(exec, cuidx))
			exec->cu_status[cuidx] = 0;
		else if (exec_is_ert(exec))
			exec->cu_status[cuidx] = ert_cu_status(exec->ert, cuidx)
				? AP_START : AP_IDLE;
		else 
			exec->cu_status[cuidx] = cu_status(exec->cus[cuidx]);
	}

	// reset cdma status
	for (; cuidx < exec->num_cus; ++cuidx)
		exec->cu_status[cuidx] = 0;
}

/**
 * finish_cmd() - Special post processing of commands after execution
 */
static int
exec_finish_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	if (cmd_opcode(xcmd) == ERT_CONFIGURE) {
		exec->configured = true;
		exec->configure_active = false;
		return 0;
	}

	if (cmd_opcode(xcmd) != ERT_CU_STAT)
		return 0;
	
	if (exec_is_ert(exec))
		ert_read_custat(exec->ert, xcmd, exec->num_cus);

	exec_update_custatus(exec);

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
 *
 * Update outstanding execs count for client and device.
 */
static void
exec_notify_host(struct exec_core *exec, struct xocl_cmd* xcmd)
{
	struct client_ctx *client = xcmd->client;
	struct xocl_dev *xdev = exec_get_xdev(exec);

	SCHED_DEBUGF("-> %s(%d) cmd(%lu)\n", __func__, exec->uid, xcmd->uid);

	mutex_lock(&xdev->dev_lock);  // not sure this lock is needed any longer
	atomic_dec(&xdev->outstanding_execs);
	atomic_dec(&client->outstanding_execs);
	atomic_inc(&client->trigger);
	mutex_unlock(&xdev->dev_lock); // eliminate ?
	wake_up_interruptible(&exec->poll_wait_queue);

	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * exec_cmd_mark_complete() - Move a command to specified state and notify host
 *
 * Commands are marked complete in two ways
 *  1. Through polling (of CUs or of MB status register)
 *  2. Through interrupts from MB
 *
 * @xcmd: Command to mark complete
 * @state: New command state
 *
 * The external command state is changed to @state and the host is notified
 * that some command has completed.  The calling code is responsible for
 * recycling / freeing the command, this function *cannot* call cmd_free
 * because when ERT is enabled multiple commands can complete in one shot and
 * list iterations of running cmds (@exec_running_to_complete) would not work.
 */
static void
exec_mark_cmd_state(struct exec_core *exec, struct xocl_cmd *xcmd, enum ert_cmd_state state)
{
	SCHED_DEBUGF("-> %s exec(%d) xcmd(%lu) state(%d)\n",
		     __func__, exec->uid, xcmd->uid, state);
	if (cmd_type(xcmd) == ERT_CTRL)
		exec_finish_cmd(exec, xcmd);

	if (xcmd->cu_idx != no_index)
		--exec->cu_load_count[xcmd->cu_idx];

	cmd_set_state(xcmd, state);

	if (exec->polling_mode)
		scheduler_decr_poll(exec->scheduler);

	if (exec->ert)
		ert_release_slot(exec->ert, xcmd);

	exec_notify_host(exec, xcmd);

	// Deactivate command and trigger chain of waiting commands
	cmd_mark_deactive(xcmd);
	cmd_trigger_chain(xcmd);

	SCHED_DEBUGF("<- %s\n", __func__);
}

static inline void
exec_mark_cmd_complete(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	exec_mark_cmd_state(exec, xcmd,
			    xcmd->aborted ? ERT_CMD_STATE_ABORT : ERT_CMD_STATE_COMPLETED);
}

static inline void
exec_mark_cmd_error(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	exec_mark_cmd_state(exec, xcmd,
			    xcmd->aborted ? ERT_CMD_STATE_ABORT : ERT_CMD_STATE_ERROR);
}

/**
 * process_cmd_mask() - Move all commands in mask to complete state
 *
 * @mask: Bitmask with queried statuses of commands
 * @mask_idx: Index of the command mask. Used to offset the actual cmd slot index
 *
 * scheduler_ops ERT mode callback function
 *
 * Used in ERT mode only.
 */
static void
exec_process_cmd_mask(struct exec_core *exec, u32 mask, unsigned int mask_idx)
{
	int bit_idx = 0, cmd_idx = 0;

	SCHED_DEBUGF("-> %s(0x%x,%d)\n", __func__, mask, mask_idx);

	for (bit_idx = 0, cmd_idx = mask_idx<<5; bit_idx < 32; mask >>= 1, ++bit_idx, ++cmd_idx) {
		struct xocl_cmd *xcmd = (mask & 0x1)
			? ert_get_cmd(exec->ert, cmd_idx)
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
 * scheduler_ops ERT poll mode callback function
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

	// assert(mask_idx > 0 || mask > 0x1); // 0x1 is ctrl commands not cus
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
 * exec_penguin_start_cu_cmd() - Callback in penguin and dataflow mode
 *
 * @xcmd: command to start
 *
 * scheduler_ops penguin and ert poll callback function for CU type commands
 *
 * Used in penguin and ert poll mode where KDS schedules and starts
 * compute units.
 */
static bool
exec_penguin_start_cu_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	struct xocl_cu *xcu = NULL;

	SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

	// CU was selected when command was submitted
	xcu = exec->cus[xcmd->cu_idx];
	if (cu_ready(xcu) && cu_start(xcu, xcmd)) {
		cmd_set_int_state(xcmd, ERT_CMD_STATE_RUNNING);
		list_move_tail(&xcmd->cq_list, &exec->running_cmd_queue);
		++exec->num_running_cmds;
		++exec->cu_usage[xcmd->cu_idx];
		SCHED_DEBUGF("<- %s -> true\n", __func__);
		return true;
	}
	
	SCHED_DEBUGF("<- %s -> false\n", __func__);
	return false;
}

/**
 * exec_penguin_start_ctrl_cmd() - Callback in penguin mode for ctrl commands
 * 
 * In penguin mode ctrl commands run synchronously, so mark them complete when
 * done, e.g. there is nothihng to poll for completion as there is nothing
 * left running
 */
static bool
exec_penguin_start_ctrl_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);

	// Nothting to do for currently supported ctrl commands
	// Just mark the command as complete and free it.
	exec_mark_cmd_complete(exec, xcmd);
	cmd_free(xcmd);

	SCHED_DEBUGF("<- %s returns true\n", __func__);

	return true;
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

	if (cmdtype == ERT_CU) {
		struct xocl_cu *xcu = exec->cus[xcmd->cu_idx];

		 if (cu_first_done(xcu) == xcmd) {
			cu_pop_done(xcu);
			exec_mark_cmd_complete(exec, xcmd);
		}
	}

	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * ert_ert_start_cmd() - Start a command in ERT mode
 *
 * @xcmd: command to start
 *
 * scheduler_ops ERT mode callback function
 *
 * Used in ert mode where ERT schedules, starts, and polls compute units.
 */
static bool
exec_ert_start_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
		     exec->uid, xcmd->uid, cmd_opcode(xcmd));
	
	if (ert_start_cmd(exec->ert, xcmd)) {
		cmd_set_int_state(xcmd, ERT_CMD_STATE_RUNNING);
		list_move_tail(&xcmd->cq_list, &exec->running_cmd_queue);
		++exec->num_running_cmds;
		SCHED_DEBUGF("<- %s returns true\n", __func__);
		return true;
	}

	// start failed
	SCHED_DEBUGF("<- %s returns false\n", __func__);
	return false;
}

/**
 * exec_ert_start_ctrl_cmd() - Callback in ERT mode for ctrl commands
 *
 * In ERT poll mode cu stats are managed by kds itself, nothing
 * to retrieve from ERT.  This could be split to two functions 
 * through scheduler_ops, but not really critical.
 */
static bool
exec_ert_start_ctrl_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) xcmd(%lu)\n", __func__, exec->uid, xcmd->uid);

	// For CU_STAT in ert polling mode (dataflow assisted polling) there
	// is nothing to do, mark complete immediately
	if (cmd_opcode(xcmd) == ERT_CU_STAT && exec_is_ert_poll(exec)) {
		exec_mark_cmd_complete(exec, xcmd);
		return true;
	}

	// Pass all other control commands to ERT
	if (exec_ert_start_cmd(exec, xcmd)) {
		SCHED_DEBUGF("<- %s returns true\n", __func__);
		return true;
	}
		
	SCHED_DEBUGF("<- %s returns false\n", __func__);
	return false;
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
		u32 val = csr_read32(exec->csr_base, csr_addr);

		if (val)
			userpf_info(exec_get_xdev(exec),
				    "csr[%d]=0x%x cleared\n", idx, val);
	}
}

/**
 * exec_ert_query_mailbox() - Check ERT CQ completion mailbox
 *
 * @exec: device
 * @xcmd: command to check
 *
 * This function is for ERT and ERT polling mode.  When KDS is configured to
 * poll, this function polls the ert->host mailbox.
 *
 * The function checks all available entries in the mailbox so more than one
 * command may be marked complete by this function.
 */
static void
exec_ert_query_mailbox(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	u32 mask;
	u32 cmdtype = cmd_type(xcmd);
	u32 slot;
	int mask_idx;
	u32 slots[MAX_SLOTS];
	u32 cnt = 0;
	int i;

	SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

	while (!(xocl_mailbox_versal_get(xcmd->xdev, &slot)))
		slots[cnt++] = slot;

	if (!cnt)
		return;

	for (i = 0; i < cnt; i++) {
		// special case for control commands which are in slot 0
		if (cmdtype == ERT_CTRL && (slots[i] == 0)) {
			exec_process_cmd_mask(exec, 0x1, 0);
			continue;
		}

		mask = 1 << (slots[i] % sizeof (u32));
		mask_idx = slots[i] >> 5;

		exec->ops->process_mask(exec, mask, mask_idx);
	}

	SCHED_DEBUGF("<- %s\n", __func__);
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

	if (exec->polling_mode
	    || (mask_idx == 0 && atomic_xchg(&exec->sr0, 0))
	    || (mask_idx == 1 && atomic_xchg(&exec->sr1, 0))
	    || (mask_idx == 2 && atomic_xchg(&exec->sr2, 0))
	    || (mask_idx == 3 && atomic_xchg(&exec->sr3, 0))) {
		u32 csr_addr = ERT_STATUS_REGISTER_ADDR + (mask_idx<<2);

		mask = csr_read32(exec->csr_base, csr_addr);
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
 * exec_ert_query_cu() - Callback for ERT poll mode
 *
 * @xcmd: command to check
 *
 * ERT assisted polling in dataflow mode
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
	exec_ert_query_csr(exec, xcmd, mask_idx32(xcmd->cu_idx+1)); // note offset
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * exec_ert_query_cmd() - Callback for cmd completion when ERT mode
 *
 * @xcmd: command to check
 *
 * ERT CU scheduling mode
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
	struct xocl_dev *xdev = xocl_get_xdev(exec->pdev);

	SCHED_DEBUGF("-> %s cmd(%lu), slot_idx(%d)\n", __func__, xcmd->uid, xcmd->slot_idx);

	if (XOCL_DSA_IS_VERSAL(xdev)) {
		exec_ert_query_mailbox(exec, xcmd);
	} else
		exec_ert_query_csr(exec, xcmd, mask_idx32(xcmd->slot_idx));

	SCHED_DEBUGF("<- %s\n", __func__);
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

static void
exec_abort_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, exec->uid, xcmd->uid);
	exec_notify_host(exec, xcmd);
	cmd_free(xcmd);
	SCHED_DEBUGF("<- %s\n", __func__);
}

/**
 * start_cmd() - Start execution of a command
 *
 * Return: true if succesfully started, false otherwise
 *
 * Function dispatches based on penguin vs ert mode
 */
static inline bool
exec_start_cu_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
		     exec->uid, xcmd->uid, cmd_opcode(xcmd));

	if (exec->ops->start_cmd(exec, xcmd)) {
		SCHED_DEBUGF("<- %s returns true\n", __func__);
		return true;
	}

	SCHED_DEBUGF("<- %s returns false\n", __func__);
	return false;
}

/**
 * start_start_ctrl_cmd() - Start execution of a command
 *
 * Return: true if succesfully started, false otherwise
 *
 * Function dispatches based on penguin vs ert mode
 */
static bool
exec_start_ctrl_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
		     exec->uid, xcmd->uid, cmd_opcode(xcmd));

	// Let scheduler mode determine the needed processing
        if (exec->ops->start_ctrl(exec, xcmd)) {
		SCHED_DEBUGF("<- %s returns true\n", __func__);
		return true;
	}
	
	SCHED_DEBUGF("<- %s returns false\n", __func__);
	return false;
}



/**
 * exec_start_kds_cmd() - KDS commands run synchronously
 */
static inline bool
exec_start_kds_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu) opcode(%d)\n", __func__,
		     exec->uid, xcmd->uid, cmd_opcode(xcmd));

	// Let scheduler mode determine the needed processing (currently none)
	// if (exec->ops->start_kds(exec, xcmd) {...}

	// kds commands are locally processed so are now complete
	exec_mark_cmd_complete(exec, xcmd);
	cmd_free(xcmd);
	SCHED_DEBUGF("<- %s returns true\n", __func__);
	return true;
}

static int
exec_start_cu_range(struct exec_core *exec, unsigned int start, unsigned int end)
{
	unsigned int started = 0;
	unsigned int cuidx;

	for (cuidx = start; cuidx < end; ++cuidx) {
		struct list_head *cu_queue = &exec->pending_cu_queue[cuidx];
		struct xocl_cmd *xcmd = list_first_entry_or_null(cu_queue, struct xocl_cmd, cq_list);
		if (!xcmd || !exec_start_cu_cmd(exec, xcmd))
			continue;
		++started;
	}

	return started;
}

static int
exec_start_cus(struct exec_core *exec)
{
	static unsigned int first_cu = -1;
	unsigned int start_cu = (first_cu < exec->num_cus) ? ++first_cu : (first_cu = 0);

	unsigned int total = 0;
	unsigned int prev = 0;

	//SCHED_PRINTF("-> %s first_cu(%d) start_cu(%d)\n", __func__, first_cu, start_cu);

	do {
		prev = total;
		total += exec_start_cu_range(exec, start_cu, exec->num_cus);
		total += exec_start_cu_range(exec, 0, start_cu);
	} while (total > prev);

	return total;
}

static int
exec_start_ctrl(struct exec_core *exec)
{
	struct list_head *ctrl_queue = &exec->pending_ctrl_queue;
	struct xocl_cmd *xcmd = list_first_entry_or_null(ctrl_queue, struct xocl_cmd, cq_list);
	return (xcmd && exec_start_ctrl_cmd(exec, xcmd)) ? 1 : 0;
}

static int
exec_start_kds(struct exec_core *exec)
{
	struct list_head *local_queue = &exec->pending_kds_queue;
	struct xocl_cmd *xcmd = list_first_entry_or_null(local_queue, struct xocl_cmd, cq_list);
	return (xcmd && exec_start_kds_cmd(exec, xcmd)) ? 1 : 0;
}

static bool
exec_submit_cu_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	// Append cmd to end of shortest CU list
	unsigned int min_load_count = -1;
	unsigned int cuidx = -1;
	unsigned int bit;
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, exec->uid, xcmd->uid);
	for (bit = cmd_first_cu(xcmd); bit < exec->num_cus; bit = cmd_next_cu(xcmd, bit)) {
		unsigned int load_count = exec->cu_load_count[bit];
		if (load_count >= min_load_count)
			continue;
		cuidx = bit;
		if ((min_load_count = load_count) == 0)
			break;
	}

	list_move_tail(&xcmd->cq_list, &exec->pending_cu_queue[cuidx]);
	cmd_set_cu(xcmd, cuidx);
	++exec->cu_load_count[cuidx];
	SCHED_DEBUGF("<- %s cuidx(%d) load(%d)\n", __func__, cuidx, exec->cu_load_count[cuidx]);
	return true;
}

static inline bool
exec_submit_ctrl_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, exec->uid, xcmd->uid);

	// configure command should configure kds succesfully or be abandoned
	if (cmd_opcode(xcmd) == ERT_CONFIGURE && (exec->configure_active || exec_cfg_cmd(exec, xcmd))) {
		cmd_set_state(xcmd, ERT_CMD_STATE_ERROR);
		exec_abort_cmd(exec, xcmd);
		SCHED_DEBUGF("<- %s returns false\n", __func__);
		return false;
	}

	// move to pending ctrl list
	list_move_tail(&xcmd->cq_list, &exec->pending_ctrl_queue);
	
	SCHED_DEBUGF("<- %s true\n", __func__);
	return true;
}

static inline bool
exec_submit_kds_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, exec->uid, xcmd->uid);
	
	// If preprocessing fails, then abandon
	if (cmd_opcode(xcmd) == ERT_START_COPYBO && exec_execute_copybo_cmd(exec, xcmd)) {
		cmd_set_state(xcmd, ERT_CMD_STATE_ERROR);
		exec_abort_cmd(exec, xcmd);
		SCHED_DEBUGF("<- %s returns false\n", __func__);
		return false;
	}

	// move to pending kds list
	list_move_tail(&xcmd->cq_list, &exec->pending_kds_queue);
	
	SCHED_DEBUGF("<- %s returns true\n", __func__);
	return true;
}

static bool
exec_submit_cmd(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	bool ret = false;

	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, exec->uid, xcmd->uid);
	if (cmd_update_state(xcmd) == ERT_CMD_STATE_ABORT)
		exec_abort_cmd(exec, xcmd);
	else if (cmd_type(xcmd) == ERT_CU)
		ret = exec_submit_cu_cmd(exec, xcmd);
	else if (cmd_type(xcmd) == ERT_KDS_LOCAL)
		ret = exec_submit_kds_cmd(exec, xcmd);
	else if (cmd_type(xcmd) == ERT_CTRL)
		ret = exec_submit_ctrl_cmd(exec, xcmd);
	else
		userpf_err(xcmd->xdev,"Unknown command type %d\n",cmd_type(xcmd));

	if (ret && exec->polling_mode)
		scheduler_incr_poll(exec->scheduler);

	if (ret)
		++exec->num_pending_cmds;
	
	SCHED_DEBUGF("<- %s ret(%d)\n", __func__, ret);
	return ret;
}

static void
exec_error_to_free(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	exec_notify_host(exec, xcmd);
	cmd_free(xcmd);
}

static inline void
exec_new_to_queued(struct exec_core *exec, struct xocl_cmd *xcmd)
{
	SCHED_DEBUGF("-> %s exec(%d) cmd(%lu)\n", __func__, exec->uid, xcmd->uid);
	if (cmd_update_state(xcmd) == ERT_CMD_STATE_ABORT) {
		exec_abort_cmd(exec, xcmd);
		SCHED_DEBUGF("<- %s aborting\n", __func__);
		return;
	}

	// add to core command queue
	list_move_tail(&xcmd->cq_list, &exec->pending_cmd_queue);
	cmd_set_int_state(xcmd, ERT_CMD_STATE_QUEUED);
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
exec_queued_to_submitted(struct exec_core *exec)
{
	struct list_head *pos, *next;

	list_for_each_safe(pos, next, &exec->pending_cmd_queue) {
		struct xocl_cmd *xcmd = list_entry(pos, struct xocl_cmd, cq_list);
		exec_submit_cmd(exec, xcmd);
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
exec_submitted_to_running(struct exec_core *exec)
{
	unsigned int started = 0;
	
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);
	started += exec_start_ctrl(exec);
	started += exec_start_cus(exec);
	started += exec_start_kds(exec);
	exec->num_pending_cmds -= started;
	SCHED_DEBUGF("<- %s started(%d)\n", __func__, started);
}

static void
exec_running_to_complete(struct exec_core *exec)
{
	struct list_head *pos, *next;
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);
	list_for_each_safe(pos, next, &exec->running_cmd_queue) {
		struct xocl_cmd *xcmd = list_entry(pos, struct xocl_cmd, cq_list);
		cmd_update_state(xcmd);

		// guard against exec_query_cmd completing multiple commands
		// in one call when ert is enabled.
		if (xcmd->state == ERT_CMD_STATE_RUNNING)
			exec_query_cmd(exec, xcmd);

		if (xcmd->state >= ERT_CMD_STATE_COMPLETED) {
			--exec->num_running_cmds;
			cmd_free(xcmd);
		}
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
exec_reset_cmd_queue(struct exec_core *exec, struct list_head *cmd_queue)
{
	struct list_head *pos, *next;
	list_for_each_safe(pos, next, cmd_queue) {
		struct xocl_cmd *xcmd = list_entry(pos, struct xocl_cmd, cq_list);
		cmd_set_state(xcmd, ERT_CMD_STATE_ABORT);
		exec_error_to_free(exec, xcmd);
	}
}

static void
exec_reset_pending_cu_cmds(struct exec_core *exec)
{
	unsigned int cuidx;
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);
	for (cuidx = 0; cuidx < exec->num_cus; ++cuidx) {
		SCHED_DEBUGF("+ %s cu_queue(%d)\n", __func__, cuidx);
		exec_reset_cmd_queue(exec, &exec->pending_cu_queue[cuidx]);
	}
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
exec_reset_pending_ctrl_cmds(struct exec_core *exec)
{
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);
	exec_reset_cmd_queue(exec, &exec->pending_ctrl_queue);
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
exec_reset_pending_kds_cmds(struct exec_core *exec)
{
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);
	exec_reset_cmd_queue(exec, &exec->pending_kds_queue);
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
exec_reset_cmds(struct exec_core *exec)
{
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);
	exec_reset_pending_cu_cmds(exec);
	exec_reset_pending_ctrl_cmds(exec);
	exec_reset_pending_kds_cmds(exec);
	SCHED_DEBUGF("<- %s\n", __func__);
}

static void
exec_service_cmds(struct exec_core *exec)
{
	SCHED_DEBUGF("-> %s exec(%d)\n", __func__, exec->uid);
	// Baby sit running commands
	exec_running_to_complete(exec);

	// Submit new commands for execution
	exec_queued_to_submitted(exec);

	// Start commands
	exec_submitted_to_running(exec);
	SCHED_DEBUGF("<- %s\n", __func__);
}


/**
 * struct ert_ops: ERT scheduling
 *
 * Callback functions used in regular (no dataflow) ERT mode
 */
static struct exec_ops ert_ops = {
	.start_cmd = exec_ert_start_cmd,
	.start_ctrl = exec_ert_start_ctrl_cmd,
	.query_cmd = exec_ert_query_cmd,
	.query_ctrl = exec_ert_query_cmd,
	.process_mask = exec_process_cmd_mask,
};

/**
 * struct penguin_ops: kernel mode scheduling (penguin)
 *
 * Callback functions used in regular (no dataflow) penguin mode
 */
static struct exec_ops penguin_ops = {
	.start_cmd = exec_penguin_start_cu_cmd,
	.start_ctrl = exec_penguin_start_ctrl_cmd,
	.query_cmd = exec_penguin_query_cmd,
	.query_ctrl = exec_penguin_query_cmd,
	.process_mask = NULL,
};

/**
 * struct dataflow_ops: kernel mode scheduling with ert polling
 *
 * Callback functions used in dataflow mode only when ERT is 
 * assisting in polling for CU completion.
 */
static struct exec_ops ert_poll_ops = {
	.start_cmd = exec_penguin_start_cu_cmd,
	.start_ctrl = exec_ert_start_ctrl_cmd,
	.query_cmd = exec_ert_query_cu,
	.query_ctrl = exec_ert_query_cmd,
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
	SCHED_DEBUGF("-> %s\n", __func__);
	// clear stale command objects if any
	while (!list_empty(&pending_cmds)) {
		struct xocl_cmd *xcmd = list_first_entry(&pending_cmds, struct xocl_cmd, cq_list);

		DRM_INFO("deleting stale pending cmd\n");
		cmd_free(xcmd);
	}
	atomic_set(&num_pending, 0);
	SCHED_DEBUGF("<- %s\n", __func__);
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
 * @cores: list of execution cores (devices)
 * @intc: boolean flag set when there is a pending interrupt for command completion
 * @poll: number of running commands in polling mode
 */
struct xocl_scheduler {
	struct task_struct	  *scheduler_thread;
	unsigned int		   use_count;

	wait_queue_head_t	   wait_queue;
	bool  		           error;
	bool 		           stop;
	bool		           reset;

	struct list_head           cores; // executuion cores

	unsigned int		   intc; /* pending intr shared with isr, word aligned atomic */
	unsigned int		   poll; /* number of cmds to poll */
};

static struct xocl_scheduler scheduler0;

static void
scheduler_reset(struct xocl_scheduler *xs)
{
	xs->error = false;
	xs->stop = false;
	xs->reset = false;
	xs->poll = 0;
	xs->intc = 0;
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

static inline void
scheduler_incr_poll(struct xocl_scheduler *xs)
{
	++xs->poll;
}

/**
 * scheduler_queue_cmds() - Dispatch pending commands to cores
 */
static void
scheduler_queue_cmds(struct xocl_scheduler *xs)
{
	struct list_head *pos, *next;

	SCHED_DEBUGF("-> %s\n", __func__);
	mutex_lock(&pending_cmds_mutex);
	list_for_each_safe(pos, next, &pending_cmds) {
		struct xocl_cmd *xcmd = list_entry(pos, struct xocl_cmd, cq_list);
		if (xcmd->xs != xs)
			continue;
		SCHED_DEBUGF("+ dispatching cmd(%lu)\n", xcmd->uid);

		// chain active dependencies if any to this command object
		if (cmd_wait_count(xcmd) && cmd_chain_dependencies(xcmd))
			cmd_set_state(xcmd, ERT_CMD_STATE_ERROR);
		else
			cmd_set_int_state(xcmd, ERT_CMD_STATE_QUEUED);

		// move command to proper execution core
		exec_new_to_queued(xcmd->exec, xcmd);

		// this command is now active and can chain other commands
		cmd_mark_active(xcmd);
		atomic_dec(&num_pending);
	}
	mutex_unlock(&pending_cmds_mutex);
	SCHED_DEBUGF("<- %s\n", __func__);
}


/**
 * scheduler_service_cores() - Iterate all devices
 */
static void
scheduler_service_cores(struct xocl_scheduler *xs)
{
	struct list_head *pos, *next;

	SCHED_DEBUGF("-> %s\n", __func__);
	list_for_each_safe(pos, next, &xs->cores) {
		struct exec_core *exec = list_entry(pos, struct exec_core, core_list);
		exec_service_cmds(exec);
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
		xs->stop = true;
		SCHED_DEBUG("scheduler wakes kthread_should_stop\n");
		return 0;
	}

	if (atomic_read(&num_pending)) {
		SCHED_DEBUGF("scheduler wakes to copy new pending commands(%d)\n", atomic_read(&num_pending));
		return 0;
	}

	if (xs->intc) {
		SCHED_DEBUG("scheduler wakes on interrupt\n");
		xs->intc = 0;
		return 0;
	}

	if (xs->poll) {
		SCHED_DEBUGF("scheduler wakes to poll(%d)\n", xs->poll);
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

	// queue new pending commands
	scheduler_queue_cmds(xs);

	// iterate all execution cores
	scheduler_service_cores(xs);

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
	return xs->error ? 1 : 0;
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

	SCHED_DEBUGF("-> %s cmd(%lu) pid(%d)\n", __func__, xcmd->uid, pid_nr(task_tgid(current)));
	SCHED_DEBUGF("+ exec stopped(%d) configured(%d)\n", exec->stopped, exec->configured);

	if (exec->stopped || (!exec->configured && cmd_opcode(xcmd) != ERT_CONFIGURE)) {
		userpf_err(xdev, "scheduler can't add cmd(%lu) opcode(%d)\n",
			   xcmd->uid, cmd_opcode(xcmd));
		goto err;
	}

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

	SCHED_DEBUGF("-> %s cmd(%lu)\n", __func__, xcmd->uid);

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
	INIT_LIST_HEAD(&xs->cores);
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

	// clear stale command objects if any
	pending_cmds_reset();

	// reclaim memory for allocate command objects
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

static void
client_reserve_implicit_cus(struct exec_core *exec, struct client_ctx *client)
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
		client->pid = get_pid(task_pid(current));
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

static inline bool ip_excl_held(u32 ip_ref)
{
	return ((ip_ref & ~IP_EXCL_RSVD_MASK) != 0);
}

static inline pid_t ip_excl_holder(struct exec_core *exec, u32 ip_idx)
{
	u32 ref = exec->ip_reference[ip_idx];

	if (ip_excl_held(ref))
		return (ref & IP_EXCL_RSVD_MASK);
	return 0;
}

static int add_ip_ref(struct xocl_dev *xdev, struct exec_core *exec,
	u32 ip_idx, pid_t pid, bool shared)
{
	u32 ref = exec->ip_reference[ip_idx];

	BUG_ON(ip_idx >= MAX_CUS);
	BUG_ON(!mutex_is_locked(&xdev->dev_lock));

	if (ip_excl_held(ref)) {
		userpf_err(xdev, "CU(%d) is exclusively held by process %d",
			ip_idx, ip_excl_holder(exec, ip_idx));
		return -EBUSY;
	}
	if (!shared && ref) {
		userpf_err(xdev, "CU(%d) has %d shared users", ip_idx, ref);
		return -EBUSY;
	}

	if (shared) {
		BUG_ON(ref >= IP_EXCL_RSVD_MASK);
		exec->ip_reference[ip_idx]++;
	} else {
		exec->ip_reference[ip_idx] = ~IP_EXCL_RSVD_MASK | pid;
	}
	return 0;
}

static int rem_ip_ref(struct xocl_dev *xdev, struct exec_core *exec, u32 ip_idx)
{
	u32 ref = exec->ip_reference[ip_idx];

	BUG_ON(ip_idx >= MAX_CUS);
	BUG_ON(!mutex_is_locked(&xdev->dev_lock));

	if (ref == 0) {
		userpf_err(xdev, "CU(%d) has never been reserved", ip_idx);
		return -EINVAL;
	}

	if (ip_excl_held(ref))
		exec->ip_reference[ip_idx] = 0;
	else
		exec->ip_reference[ip_idx]--;
	return 0;
}

static void 
destroy_client(struct platform_device *pdev, void **priv)
{
	struct client_ctx *client = (struct client_ctx *)(*priv);
	struct exec_core *exec = platform_get_drvdata(pdev);
	struct xocl_scheduler *xs = exec_scheduler(exec);
	struct xocl_dev	*xdev = xocl_get_xdev(pdev);
	unsigned int	outstanding;
	unsigned int	timeout_loops = 20;
	unsigned int	loops = 0;
	int pid = pid_nr(client->pid);
	unsigned int bit;
	struct ip_layout *layout;
	xuid_t *xclbin_id;

	// force scheduler to abort execs for this client
	client->abort = true;

	// wait for outstanding execs to finish
	outstanding = atomic_read(&client->outstanding_execs);
	while (outstanding) {
		unsigned int new;

		userpf_info(xdev, "pid(%d) waiting for %d outstanding execs to finish",
			    pid,outstanding);
		msleep(500);
		new = atomic_read(&client->outstanding_execs);
		loops = (new == outstanding ? (loops + 1) : 0);
		if (loops == timeout_loops) {
			userpf_err(xdev,
				   "pid(%d) gives up with %d outstanding execs.\n",
				   pid,outstanding);
			userpf_err(xdev,
				   "Please reset device with 'xbutil reset'\n");
			exec->needs_reset = true;
			// reset the scheduler loop
			xs->reset = true;
			break;
		}
		outstanding = new;
	}

	mutex_lock(&xdev->dev_lock);
	put_pid(client->pid);
	client->pid = NULL;

	list_del(&client->link);
	DRM_INFO("client exits pid(%d)\n", pid);

	if (CLIENT_NUM_CU_CTX(client) == 0)
		goto done;

	/*
	 * This happens when application exits without formally releasing the
	 * contexts on CUs. Give up our contexts on CUs and our lock on xclbin.
	 * Note, that implicit CUs (such as CDMA) do not add to ip_reference.
	 */

	layout = XOCL_IP_LAYOUT(xdev);
	xclbin_id = XOCL_XCLBIN_ID(xdev);

	client_release_implicit_cus(exec, client);
	client->virt_cu_ref = 0;

	bit = layout
	  ? find_first_bit(client->cu_bitmap, layout->m_count)
	  : MAX_CUS;
	while (layout && (bit < layout->m_count)) {
		if (rem_ip_ref(xdev, exec, bit) == 0) {
			userpf_info(xdev, "CTX reclaim (%pUb, %d, %u)",
				xclbin_id, pid, bit);
		}
		bit = find_next_bit(client->cu_bitmap, layout->m_count, bit + 1);
	}
	bitmap_zero(client->cu_bitmap, MAX_CUS);

	(void) xocl_icap_unlock_bitstream(xdev, xclbin_id);

done:
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
	pid_t pid = pid_nr(task_tgid(current));
	struct xocl_dev	*xdev = xocl_get_xdev(pdev);
	struct exec_core *exec = platform_get_drvdata(pdev);
	xuid_t *xclbin_id;
	u32 cu_idx = args->cu_index;
	bool shared;

	/* bypass ctx check for versal for now */
	if (XOCL_DSA_IS_VERSAL(xdev))
		return 0;

	mutex_lock(&xdev->dev_lock);

	/* Sanity check arguments for add/rem CTX */
	xclbin_id = XOCL_XCLBIN_ID(xdev);
	if (!xclbin_id || !uuid_equal(xclbin_id, &args->xclbin_id)) {
		userpf_err(xdev, "try to add/rem CTX on wrong xclbin");
		ret = -EBUSY;
		goto out;
	}

	if (cu_idx != XOCL_CTX_VIRT_CU_INDEX
	    && cu_idx >= XOCL_IP_LAYOUT(xdev)->m_count) {
		userpf_err(xdev, "cuidx(%d) >= numcus(%d)\n",
			cu_idx, XOCL_IP_LAYOUT(xdev)->m_count);
		ret = -EINVAL;
		goto out;
	}

	if (cu_idx != XOCL_CTX_VIRT_CU_INDEX && !exec_valid_cu(exec,cu_idx)) {
		userpf_err(xdev, "invalid CU(%d)",cu_idx);
		ret = -EINVAL;
		goto out;
	}

	/* Handle CTX removal */
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
			(void) rem_ip_ref(xdev, exec, cu_idx);
		}

		// We just gave up the last context, unlock the xclbin
		if (CLIENT_NUM_CU_CTX(client) == 0)
			(void) xocl_icap_unlock_bitstream(xdev, xclbin_id);

		goto out;
	}

	/* Handle CTX add */
	if (args->op != XOCL_CTX_OP_ALLOC_CTX) {
		ret = -EINVAL;
		goto out;
	}

	shared = (args->flags == XOCL_CTX_SHARED);
	if (!shared && cu_idx == XOCL_CTX_VIRT_CU_INDEX) {
		userpf_err(xdev,
			"exclusively reserve virtual CU is not allowed");
		ret = -EINVAL;
		goto out;
	}

	if (cu_idx != XOCL_CTX_VIRT_CU_INDEX) {
		if (test_and_set_bit(cu_idx, client->cu_bitmap)) {
			// Context was previously allocated for the same CU,
			// cannot allocate again. Need to implement per CU ref
			// counter to make it work.
			userpf_err(xdev, "CTX already added by this process");
			ret = -EINVAL;
			goto out;
		}
		if (add_ip_ref(xdev, exec, cu_idx, pid, shared) != 0) {
			clear_bit(cu_idx, client->cu_bitmap);
			ret = -EBUSY;
			goto out;
		}
	}


	if (CLIENT_NUM_CU_CTX(client) == 0) {
		// This is the first context on any CU for this process,
		// lock the xclbin
		ret = xocl_icap_lock_bitstream(xdev, xclbin_id);
		if (ret) {
			if (cu_idx != XOCL_CTX_VIRT_CU_INDEX) {
				(void) rem_ip_ref(xdev, exec, cu_idx);
				clear_bit(cu_idx, client->cu_bitmap);
			}
			goto out;
		}
	}

	if (cu_idx == XOCL_CTX_VIRT_CU_INDEX) {
		if (client->virt_cu_ref == 0)
			client_reserve_implicit_cus(exec, client);
		++client->virt_cu_ref;
	} else {
		++client->num_cus;
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
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
		return -EADDRNOTAVAIL;
	}

	if (obj->size <= off || obj->size < off + size) {
		userpf_err(xdev, "Failed to get paddr for BO 0x%x\n", bo_hdl);
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
		return -EINVAL;
	}

	*paddrp = xobj->mm_node->start + off;
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
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
	if (exec->num_cdma == 0)
		return -EINVAL;

	userpf_info(xdev,"checking alignment requirments for KDMA sz(%lu)",sz);
	if ((dst_addr + dst_off) % KDMA_BLOCK_SIZE ||
	    (src_addr + src_off) % KDMA_BLOCK_SIZE ||
	    sz % KDMA_BLOCK_SIZE) {
		userpf_err(xdev,"improper alignment, cannot use KDMA");
		return -EINVAL;
	}

	ert_fill_copybo_cmd(scmd, 0, 0, src_addr, dst_addr, sz / KDMA_BLOCK_SIZE);

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
	struct exec_core *exec = platform_get_drvdata(pdev);

	if (exec->needs_reset) {
		userpf_err(xdev, "device needs reset, use 'xbutil reset'");
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

	/* bypass exec buffer valication for versal for now */
	if (!XOCL_DSA_IS_VERSAL(xdev)) { 
		ret = validate(pdev, client, xobj);
		if (ret) {
			userpf_err(xdev, "Exec buffer validation failed\n");
			ret = -EINVAL;
			goto out;
		}
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
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&deps[numdeps]->base);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);

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
reset(struct platform_device *pdev, const xuid_t *xclbin_id)
{
	struct exec_core *exec = platform_get_drvdata(pdev);

	exec_reset(exec, xclbin_id);
	exec->needs_reset = false;
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
 * reconfig() - Force scheduler to reconfigure on next ERT_CONFIGURE command
 *
 * Adding of commands will fail until next command is a configure command
 */
static int
reconfig(struct platform_device *pdev)
{
	struct exec_core *exec = platform_get_drvdata(pdev);
	exec->configure_active = false;
	exec->configured = false;
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
	unsigned int maskidx = 0;
	u32 ctx_cus[4] = {0};
	u32 cumasks = 0;
	int err = 0;
	bool cus_specified = false;
	u64 bo_size = bo->base.size;

	SCHED_DEBUGF("-> %s opcode(%d)\n", __func__, ecmd->opcode);

	// Before accessing content of exec buf, make sure the size makes sense
	if (bo_size < sizeof(*ecmd) ||
		bo_size < sizeof(ecmd->header) + ecmd->count * sizeof(u32)) {
		userpf_err(xocl_get_xdev(pdev), "exec buf is too small\n");
		return 1;
	}

	// cus for start kernel commands only
	if (ecmd->type != ERT_CU)
		return 0; /* ok */

	// payload count must be at least 1 for mandatory cumask
	if (scmd->count < 1 + scmd->extra_cu_masks) {
		userpf_err(xocl_get_xdev(pdev), "exec buf payload count is too small\n");
		return 1;
	}

	// client context cu bitmap may not change while validating
	mutex_lock(&client->xdev->dev_lock);

	// no specific CUs selected, maybe ctx is not used by client
	if (bitmap_empty(client->cu_bitmap, MAX_CUS)) {
		userpf_err(xocl_get_xdev(pdev), "%s found no CUs in ctx\n", __func__);
		err = 1;
		goto out;
	}

	// Check CUs in cmd BO against CUs in context
	cumasks = 1 + scmd->extra_cu_masks;
	xocl_bitmap_to_arr32(ctx_cus, client->cu_bitmap, cumasks * 32);

	for (maskidx = 0; maskidx < cumasks; ++maskidx) {
		uint32_t cmd_cus = ecmd->data[maskidx];

		if (!cmd_cus) // no cus in mask
			continue;

		cus_specified = true;

		// cmd_cus must be subset of ctx_cus
		if (cmd_cus & ~ctx_cus[maskidx]) {
			userpf_err(client->xdev,
				"CU mismatch in mask(%d) cmd(0x%x) ctx(0x%x)\n",
				maskidx, cmd_cus, ctx_cus[maskidx]);
			err = 1;
			goto out; /* error */
		}
	}

	if (!cus_specified) {
		userpf_err(client->xdev, "No CUs specified for command\n");
		err = 1;
	}

out:
	mutex_unlock(&client->xdev->dev_lock);
	SCHED_DEBUGF("<- %s err(%d) cmd and ctx CUs match\n", __func__, err);
	return err;

}

int cu_map_addr(struct platform_device *pdev, u32 cu_idx, void *drm_filp,
	u32 *addrp)
{
	struct xocl_dev	*xdev = xocl_get_xdev(pdev);
	struct exec_core *exec = platform_get_drvdata(pdev);
	struct drm_file *filp = drm_filp;
	struct client_ctx *client = filp->driver_priv;
	struct xocl_cu *xcu = NULL;

	mutex_lock(&xdev->dev_lock);

	if (cu_idx >= MAX_CUS) {
		userpf_err(xdev, "cu index (%d) is too big\n", cu_idx);
		mutex_unlock(&xdev->dev_lock);
		return -EINVAL;
	}
	if (!test_bit(cu_idx, client->cu_bitmap)) {
		userpf_err(xdev, "cu(%d) isn't reserved\n", cu_idx);
		mutex_unlock(&xdev->dev_lock);
		return -EINVAL;
	}
	if (ip_excl_holder(exec, cu_idx) == 0) {
		userpf_err(xdev, "cu(%d) isn't exclusively reserved\n", cu_idx);
		mutex_unlock(&xdev->dev_lock);
		return -EINVAL;
	}

	xcu = exec->cus[cu_idx];
	BUG_ON(xcu == NULL);
	*addrp = xcu->addr;
	mutex_unlock(&xdev->dev_lock);
	return 0;
}

struct xocl_mb_scheduler_funcs sche_ops = {
	.create_client = create_client,
	.destroy_client = destroy_client,
	.poll_client = poll_client,
	.client_ioctl = client_ioctl,
	.stop = stop,
	.reset = reset,
	.reconfig = reconfig,
	.cu_map_addr = cu_map_addr,
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
	uint32_t *cdma = xocl_rom_cdma_addr(xdev);
	unsigned int cdmas = cdma ? 1 : 0; //TBD

	return sprintf(buf, "%d\n", cdmas);
}
static DEVICE_ATTR_RO(kds_numcdmas);


static ssize_t
kds_custat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exec_core *exec = dev_get_exec(dev);
	struct xocl_ert *xert = exec_is_ert(exec) ? exec->ert : NULL;
	unsigned int idx = 0;
	ssize_t sz = 0;

	// No need to lock exec, cu stats are computed and cached.
	// Even if xclbin is swapped, the data reflects the xclbin on
	// which is was computed above.
	for (idx = 0; idx < exec->num_cus; ++idx)
		sz += sprintf(buf+sz, "CU[@0x%x] : %d status : %d\n",
			      exec_cu_base_addr(exec, idx),
			      xert ? ert_cu_usage(xert, idx) : exec_cu_usage(exec, idx),
			      exec_cu_status(exec, idx));

	sz += sprintf(buf+sz, "KDS number of pending commands: %d\n", exec_num_pending(exec));

	if (!xert) {
		sz += sprintf(buf+sz, "KDS number of running commands: %d\n", exec_num_running(exec));
		goto out;
	}

	sz += sprintf(buf+sz, "CQ usage : {");
	for (idx = 0; idx < xert->num_slots; ++idx)
		sz += sprintf(buf+sz, "%s%d", (idx > 0 ? "," : ""), ert_cq_slot_usage(xert, idx));
	sz += sprintf(buf+sz, "}\n");

	sz += sprintf(buf+sz, "CQ mirror state : {");
	for (idx = 0; idx < xert->num_slots; ++idx) {
		if (idx == 0) { // ctrl slot should be ignored
			sz += sprintf(buf+sz, "-");
			continue;
		}
		sz += sprintf(buf+sz, ",%d", ert_cq_slot_busy(xert, idx));
	}
	sz += sprintf(buf+sz, "}\n");

	sz += sprintf(buf+sz, "ERT scheduler version : 0x%x\n", ert_version(xert));
	sz += sprintf(buf+sz, "ERT number of submitted commands: %d\n", exec_num_running(exec));
	sz += sprintf(buf+sz, "ERT scheduler CU state : {");
	for (idx = 0; idx < exec->num_cus; ++idx) {
		if (idx > 0)
			sz += sprintf(buf+sz, ",");
		sz += sprintf(buf+sz, "%d", ert_cu_status(xert, idx));
	}
		
	sz += sprintf(buf+sz, "}\nERT scheduler CQ state : {");
	for (idx = 0; idx < xert->num_slots; ++idx) {
		if (idx == 0) { // ctrl slot should be ignored
			sz += sprintf(buf+sz, "-");
			continue;
		}
		sz += sprintf(buf+sz, ",%d", ert_cq_slot_status(xert, idx));
	}
	sz += sprintf(buf+sz, "}\n");

out:
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
	list_add_tail(&exec->core_list, &scheduler0.cores);
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
	struct xocl_dev *xdev = xocl_get_xdev(pdev);
	struct exec_core *exec = platform_get_drvdata(pdev);
	unsigned int i;

	SCHED_DEBUGF("-> %s\n", __func__);
	exec_reset_cmds(exec);
	fini_scheduler_thread(exec_scheduler(exec));

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

struct xocl_drv_private sche_priv = {
	.ops = &sche_ops,
};

static struct platform_device_id mb_sche_id_table[] = {
	{ XOCL_DEVNAME(XOCL_MB_SCHEDULER), (kernel_ulong_t)&sche_priv },
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
