/*
 * A GEM style device manager for MPSoC based OpenCL accelerators.
 *
 * Copyright (C) 2017-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Soren Soe   <soren.soe@xilinx.com>
 *    Min Ma      <min.ma@xilinx.com>
 *    Jan Stephan <j.stephan@hzdr.de>
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
#include <linux/bitmap.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "ert.h"
#include "sched_exec.h"
#include "zocl_sk.h"
#include "zocl_xclbin.h"

/*
 * The get_cu_support_intr needs this but we will move it to zocl_xclbin.c
 * in next code change.
 */
#include "xclbin.h"

/* #define SCHED_VERBOSE */

#if defined(__GNUC__)
#define SCHED_UNUSED __attribute__((unused))
#endif

#define sched_error_on(exec, expr) \
({ \
	unsigned int ret = 0; \
	if ((expr)) { \
		DRM_ERROR("Assertion failed: %s:%s\n",  __func__, #expr); \
		exec->scheduler->error = 1; \
		ret = 1; \
	} \
	(ret); \
})

#ifdef SCHED_VERBOSE
# define SCHED_DEBUG(format, ...) DRM_INFO(format, ##__VA_ARGS__)
#else
# define SCHED_DEBUG(format, ...)
#endif

static int cq_check(void *data);
static irqreturn_t sched_cq_isr(int irq, void *arg);

/* Scheduler call schedule() every MAX_SCHED_LOOP loop*/
#define MAX_SCHED_LOOP 8
static int    sched_loop_cnt;

static struct scheduler g_sched0;
static struct sched_ops penguin_ops;
static struct sched_ops ps_ert_ops;

/**
 * List of free sched_cmd objects.
 *
 * @free_cmds: populated with recycled sched_cmd objects
 * @cmd_mutex: mutex lock for cmd_list
 *
 * Command objects are recycled for later use and only freed when kernel
 * module is unloaded.
 */
static LIST_HEAD(free_cmds);
static DEFINE_MUTEX(free_cmds_mutex);

/**
 * List of new pending sched_cmd objects
 *
 * @pending_cmds: populated from user space with new commands for buffer objects
 * @num_pending: number of pending commands
 *
 * Scheduler copies pending commands to its private queue when necessary
 */
static LIST_HEAD(pending_cmds);
static DEFINE_SPINLOCK(pending_cmds_lock);
static atomic_t num_pending = ATOMIC_INIT(0);
static atomic_t num_running = ATOMIC_INIT(0);

/**
 * is_ert() - Check if running in embedded (ert) mode.
 *
 * Return: %true of ert mode, %false otherwise
 */
inline unsigned int
is_ert(struct drm_device *dev)
{
	struct drm_zocl_dev *zdev = dev->dev_private;

	return zdev->exec->ops == &ps_ert_ops;
}

/**
 * ffs_or_neg_one() - Find first set bit in a 32 bit mask.
 *
 * @mask: mask to check
 *
 * First LSBit is at position 0.
 *
 * Return: Position of first set bit, or -1 if none
 */
inline int
ffs_or_neg_one(u32 mask)
{
	if (!mask)
		return -1;
	return ffs(mask)-1;
}

/**
 * ffz_or_neg_one() - Find first zero bit in bit mask
 *
 * @mask: mask to check
 * Return: Position of first zero bit, or -1 if none
 */
inline int
ffz_or_neg_one(u32 mask)
{
	if (mask == U32_MASK)
		return -1;
	return ffz(mask);
}

/**
 * slot_size() - slot size per device configuration
 *
 * Return: Command queue slot size
 */
inline unsigned int
slot_size(struct drm_device *dev)
{
	struct drm_zocl_dev *zdev = dev->dev_private;

	return CQ_SIZE / zdev->exec->num_slots;
}

/**
 * cu_mask_idx() - CU mask index for a given cu index
 *
 * @cu_idx: Global [0..127] index of a CU
 * Return: Index of the CU mask containing the CU with cu_idx
 */
static inline unsigned int
cu_mask_idx(unsigned int cu_idx)
{
	return cu_idx >> 5; /* 32 cus per mask */
}

/**
 * cu_idx_in_mask() - CU idx within its mask
 *
 * @cu_idx: Global [0..127] index of a CU
 * Return: Index of the CU within the mask that contains it
 */
static inline unsigned int
cu_idx_in_mask(unsigned int cu_idx)
{
	return cu_idx - (cu_mask_idx(cu_idx) << 5);
}

/**
 * cu_idx_from_mask() - Get CU's global idx [0..127] by CU idx in a mask
 *
 * @cu_idx: Index of CU with mask identified by mask_idx
 * @mask_idx: Mask index of the has CU with cu_idx
 * Return: Global cu_idx [0..127]
 */
static inline unsigned int
cu_idx_from_mask(unsigned int cu_idx, unsigned int mask_idx)
{
	return cu_idx + (mask_idx << 5);
}

/* explicitly set cucu_valid bitmap to valid(1) for cuid */
static inline void
zocl_cu_set_valid(struct sched_exec_core *exec_core, unsigned int cu_idx)
{
	exec_core->cu_valid[cu_mask_idx(cu_idx)] |=
		(1 << cu_idx_in_mask(cu_idx));
}

/* explicitly set cu_valid bitmap to invalid(0) for cuid */
static inline void
zocl_cu_set_invalid(struct sched_exec_core *exec_core, unsigned int cu_idx)
{
	exec_core->cu_valid[cu_mask_idx(cu_idx)] ^=
		(1 << cu_idx_in_mask(cu_idx));
}

/* return values: valid(1)/invalid(0) */
static inline unsigned int
zocl_cu_is_valid(struct sched_exec_core *exec_core, unsigned int cu_idx)
{
	return (exec_core->cu_valid[cu_mask_idx(cu_idx)] &
		(1 << cu_idx_in_mask(cu_idx))) > 0;
}

int
zocl_exec_valid_cu(struct sched_exec_core *exec, unsigned int cuid)
{
	return zocl_cu_is_valid(exec, cuid);
}

/**
 * slot_mask_idx() - Slot mask idx index for a given slot_idx
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of the slot mask containing the slot_idx
 */
inline unsigned int
slot_mask_idx(unsigned int slot_idx)
{
	return slot_idx >> 5;
}

/**
 * slot_idx_in_mask() - Index of CQ slot within the mask that contains it
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of slot within the mask that contains it
 */
inline unsigned int
slot_idx_in_mask(unsigned int slot_idx)
{
	return slot_idx - (slot_mask_idx(slot_idx) << 5);
}

/**
 * slot_idx_from_mask_idx() - Get slot global idx [0..127] by slot idx in mask
 *
 * @slot_idx: Index of slot with mask identified by mask_idx
 * @mask_idx: Mask index of the mask hat has slot with slot_idx
 * Return: Global slot_idx [0..127]
 */
inline unsigned int
slot_idx_from_mask_idx(unsigned int slot_idx, unsigned int mask_idx)
{
	return slot_idx + (mask_idx << 5);
}

/**
 * opcode() - Command opcode
 *
 * @cmd: Command object
 * Return: Opcode per command packet
 */
inline u32
opcode(struct sched_cmd *cmd)
{
	return cmd->packet->opcode;
}

/**
 * payload_size() - Command payload size
 *
 * @cmd: Command object
 * Return: Size in number of words of command packet payload
 */
inline u32
payload_size(struct sched_cmd *cmd)
{
	return cmd->packet->count;
}

/**
 * packet_size() - Command packet size
 *
 * @cmd: Command object
 * Return: Size in number of words of command packet
 */
inline u32
packet_size(struct sched_cmd *cmd)
{
	return payload_size(cmd) + 1;
}

/**
 * cu_masks() - Number of command packet cu_masks
 *
 * @cmd: Command object
 * Return: Total number of CU masks in command packet
 */
inline u32
cu_masks(struct sched_cmd *cmd)
{
	struct ert_start_kernel_cmd *sk;
	u32 op = opcode(cmd);

	if (op != ERT_START_KERNEL && op != ERT_SK_START &&
	    op != ERT_INIT_CU && op != ERT_EXEC_WRITE)
		return 0;
	sk = (struct ert_start_kernel_cmd *)cmd->packet;
	return 1 + sk->extra_cu_masks;
}

/**
 * regmap_size() - Size of regmap
 *                 It is calculated by payload size + one packet header size -
 *                 offset of cu_mask field - cu_masks
 *                 This is based on the assumption that regmap is located
 *                 right after the cu_masks (including extra_cu_masks).
 * @xcmd: Command object
 * Return: Size of register map in number of words
 */
inline u32
regmap_size(struct sched_cmd *cmd)
{
	switch (opcode(cmd)) {

	case ERT_INIT_CU:
		return
		    payload_size(cmd) + 1 -
		    offsetof(struct ert_init_kernel_cmd, cu_mask) / WORD_SIZE -
		    cu_masks(cmd);

	case ERT_START_CU:
	case ERT_EXEC_WRITE:
	case ERT_SK_START:
		return
		    payload_size(cmd) + 1 -
		    offsetof(struct ert_start_kernel_cmd, cu_mask) / WORD_SIZE -
		    cu_masks(cmd);

	default:
		DRM_WARN("Command %d does not support regmap.\n", opcode(cmd));
		return 0;
	}
}

/**
 * cu_idx_to_timeout() - Get CU's timeout value for CU index.
 *                       CU timeout is in Microsecond. Our timer thread wakes
 *                       up every ZOCL_CU_TIMER_INTERVAL (500) Milliseconds.
 *                       So we return the timeout value in our timer interval
 *                       unit plus one in case we just miss one timer slot.
 * @dev: Device handle
 * @cu_idx: Global CU idx
 *
 * Return: run timeout value of CU
 */
inline uint32_t
cu_idx_to_timeout(struct drm_device *dev, unsigned int cu_idx)
{
	struct drm_zocl_dev *zdev = dev->dev_private;

	if (!zdev->exec->zcu[cu_idx].run_timeout)
		return 0;

	return zdev->exec->zcu[cu_idx].run_timeout /
	    (ZOCL_CU_TIMER_INTERVAL * 1000) + 1;
}

/**
 * cu_idx_to_reset_timeout() - Get CU's reset timeout value for CU index.
 *                             CU reset timeout is in Microsecond. We busy
 *                             check the reset status every
 *                             ZOCL_CU_RESET_TIMER_INTERVAL (1000) Microsecond.
 * @dev: Device handle
 * @cu_idx: Global CU idx
 *
 * Return: reset timeout value of CU in Microsecond
 */
inline uint32_t
cu_idx_to_reset_timeout(struct drm_device *dev, unsigned int cu_idx)
{
	struct drm_zocl_dev *zdev = dev->dev_private;

	return zdev->exec->zcu[cu_idx].reset_timeout /
	    (ZOCL_CU_RESET_TIMER_INTERVAL) + 1;
}

/**
 * enable_cmd_timestamps() - Enable timestamps for start CU commands
 *
 * @cmd: Command Object
 */
static inline bool
enable_cmd_timestamps(struct sched_cmd *cmd)
{
	struct ert_start_kernel_cmd *sk = cmd->ert_cu;
	struct cu_cmd_state_timestamps *ts;
	size_t ts_sz = sizeof(struct cu_cmd_state_timestamps);
	struct drm_zocl_bo *bo = cmd->buffer;

	if (!(opcode(cmd) == ERT_START_CU ||
	    opcode(cmd) == ERT_EXEC_WRITE) ||
	    !sk->stat_enabled)
		return false;

	ts = ert_start_kernel_timestamps(sk);
	if ((char *)ts + ts_sz > (char *)sk + bo->cma_base.base.size) {
		DRM_ERROR("No enough space for timestamps in command.\n");
		return false;
	}
	return true;
}

/**
 * set_cmd_timestamp() - Set time stamp for specific state
 *
 * @cmd: Command Object
 * @state: Command state
 */
static inline void
set_cmd_timestamp(struct sched_cmd *cmd, enum ert_cmd_state state)
{
	struct cu_cmd_state_timestamps *ts;

	if (!cmd->timestamp_enabled)
		return;

	ts = ert_start_kernel_timestamps(cmd->ert_cu);
	/* The command state enum is using default integer value.
	 * So we could use it for index.
	 */
	ts->skc_timestamps[state] = ktime_to_ns(ktime_get());
}

/**
 * set_cmd_int_state() - Set internal command state used by scheduler only
 *
 * @xcmd: command to change internal state on
 * @state: new command state per ert.h
 */
static inline void
set_cmd_int_state(struct sched_cmd *cmd, enum ert_cmd_state state)
{
	SCHED_DEBUG("-> %s (,%d)\n", __func__, state);
	set_cmd_timestamp(cmd, state);
	cmd->state = state;
	SCHED_DEBUG("<- %s\n", __func__);
}

/*
 * This is the timer thread function. In the case that a CU timeout
 * value is initialized, we start a timer thread. This thread wakes
 * up every ZOCL_CU_TIMER_INTERVAL (500) Milliseconds. And each time,
 * it sets a flag in scheduler and wakes up scheduler so that scheduler
 * can check if a CU timeouts.
 */
static int zocl_cu_timer_thread(void *data)
{
	struct drm_zocl_dev *zdev = (struct drm_zocl_dev *)data;
	struct sched_exec_core *exec = zdev->exec;
	struct scheduler *sched = exec->scheduler;

	while (1) {
		if (kthread_should_stop())
			break;

		msleep(ZOCL_CU_TIMER_INTERVAL);

		atomic_set(&sched->check, 1);
		wake_up_interruptible(&g_sched0.wait_queue);
	}

	return 0;
}

int zocl_init_cu_timer(struct drm_zocl_dev *zdev)
{
	struct sched_exec_core *exec = zdev->exec;
	char thread_name[256] = "zocl-cu-timer-thread";

	/* If the timer is initialized, return here */
	if (exec->timer_task)
		return 0;

	exec->timer_task = kthread_run(zocl_cu_timer_thread, zdev, thread_name);
	if (IS_ERR(exec->timer_task)) {
		int ret = PTR_ERR(exec->timer_task);

		DRM_ERROR("Unable to create CU timer.\n");
		return ret;
	}

	return 0;
}

void zocl_cleanup_cu_timer(struct drm_zocl_dev *zdev)
{
	struct sched_exec_core *exec = zdev->exec;

	if (!exec)
		return;

	if (!exec->timer_task)
		return;

	kthread_stop(exec->timer_task);
}

static irqreturn_t sched_exec_isr(int irq, void *arg)
{
	struct drm_zocl_dev *zdev = arg;
	int cu_idx;

	SCHED_DEBUG("-> %s irq %d", __func__, irq);
	for (cu_idx = 0; cu_idx < zdev->cu_num; cu_idx++) {
		if (zdev->irq[cu_idx] == irq)
			break;
	}

	SCHED_DEBUG("cu_idx %d interrupt handle", cu_idx);
	if (cu_idx >= zdev->cu_num) {
		/* This should never happen */
		DRM_ERROR("Unknown isr irq %d, polling %d\n",
			  irq, zdev->exec->polling_mode);
		return IRQ_NONE;
	}

	/* This function returns the value of the interrupt status register
	 * No need to check the interrupt type for now.
	 */
	zocl_cu_clear_intr(&zdev->exec->zcu[cu_idx]);

	/* wake up all scheduler ... currently one only
	 *
	 * This might have race with sched_wait_cond(), which will read then set
	 * intc to 0; This race should has no impact on the functionality.
	 *
	 * 1. If scheduler thread is on sleeping, there is no race.
	 * 2. If scheduler thread has waked up, and returned from
	 * sched_wait_cond(), there is no race.
	 * 3. Only when scheduler thread waked up and accessing sched->intc,
	 * race might happen. It has two results:
	 *	a. intc set to 0 here. But then the scheduler thread will still
	 *	iterate all submitted commands.
	 *	b. intc set to 1 here. The scheduler thread failed to reset intc
	 *	to 0. In this case, after iterate all submitted commands, the
	 *	scheduler loop will go to sched_wait_cond() again and try to
	 *	reset intc and start the second time iteration.
	 *
	 */
	g_sched0.intc = 1;
	wake_up_interruptible(&g_sched0.wait_queue);

	SCHED_DEBUG("<- %s", __func__);
	return IRQ_HANDLED;
}

static void
init_cu_by_idx(struct sched_cmd *cmd, int cu_idx)
{
	u32 size = regmap_size(cmd);
	struct ert_init_kernel_cmd *ik;
	struct zocl_cu *cu = &cmd->exec->zcu[cu_idx];

	ik = (struct ert_init_kernel_cmd *)cmd->packet;

	zocl_cu_configure(cu, ik->data + ik->extra_cu_masks, size, CONSECUTIVE);

}

/**
 * init_cus() - Initialize CUs from user space command.
 *
 * Process the initialize CUs command sent from user space. Only one process
 * can initialize a given CU, so if a CU in the CU masks is initialized,
 * this function ignores the initialize request.
 *
 * The initialization is done by copying the user regmap to register map.
 */
static void
init_cus(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	struct ert_init_kernel_cmd *ik;
	unsigned int i;
	uint32_t *cmp;
	uint32_t run_timeout;
	uint32_t reset_timeout;
	int num_masks = cu_masks(cmd);
	int mask_idx;
	int warn_flag = 0;

	ik = (struct ert_init_kernel_cmd *)cmd->packet;
	cmp = &ik->cu_mask;

	run_timeout = ik->cu_run_timeout;
	reset_timeout = ik->cu_reset_timeout;
	if (run_timeout != 0 && reset_timeout == 0) {
		DRM_WARN("Init CU fail: invalid cu reset timeout.\n");
		goto done;
	}

	for (mask_idx = 0; mask_idx < num_masks; ++mask_idx) {
		u32 cmd_mask = cmp[mask_idx];
		u32 inited_mask = zdev->exec->cu_init[mask_idx];
		u32 uninited_mask = (cmd_mask | inited_mask) ^ inited_mask;
		u32 busy_mask = zdev->exec->cu_status[mask_idx];

		/*
		 * If some requested CUs are initialized already,
		 * record a flag and post some log later.
		 */
		if (!warn_flag && (inited_mask & cmd_mask))
			warn_flag = 1;

		/*
		 * We don't have uninitialized CU for this 32 mask_id.
		 * Move to next 32 CUs.
		 */
		if (!uninited_mask)
			continue;

		for (i = 0; i < 32; ++i) {
			int cu_idx;

			if (!(uninited_mask & (1 << i)))
				continue;

			cu_idx = cu_idx_from_mask(i, mask_idx);

			if (busy_mask & (1 << i)) {
				DRM_WARN("Can not init CU %d while running.\n",
				   cu_idx);
				continue;
			}

			if (!zocl_cu_is_valid(zdev->exec, cu_idx)) {
				DRM_WARN("Init CU %d fail: NOT a valid CU.\n",
				    cu_idx);
				continue;
			}

			if (cu_idx >= zdev->exec->num_cus) {
				/*
				 * Requested CU index exceeds the configured
				 * CU numbers.
				 */
				DRM_WARN("Init CU %d fail: NOT configured.\n",
				    cu_idx);
				goto done;
			}

			zdev->exec->zcu[cu_idx].run_timeout = run_timeout;
			zdev->exec->zcu[cu_idx].reset_timeout = reset_timeout;

			init_cu_by_idx(cmd, cu_idx);

			zdev->exec->cu_init[mask_idx] ^= (1 << i);
		}
	}

	/*
	 * If CU is initialized with a timeout value, we start the timer
	 * thread to track CU timeout.
	 */
	if (run_timeout != 0)
		zocl_init_cu_timer(zdev);

done:
	if (warn_flag)
		DRM_INFO("CU can only be initialized once.\n");
}

/*
 * returns false if any of the cu doesnt support interrupt
 */
bool
get_cus_support_intr(struct drm_zocl_dev *zdev)
{
	struct ip_data *ip;
        int i;
        if (!zdev->ip)
                return false;

        for (i = 0; i < zdev->ip->m_count; ++i) {
                ip = &zdev->ip->m_ip_data[i];
                if(!(ip->properties & 0x1)) {
                        return false;
                }
        }
        return true;
}

/**
 * configure() - Configure the scheduler from user space command
 *
 * Process the configure command sent from user space. Only one process can
 * configure the scheduler, so if scheduler is already configured, the
 * function should verify that another process doesn't expect different
 * configuration.
 *
 * Future may need ability to query current configuration so as to keep
 * multiple processes in sync.
 *
 * Return: 0 on success, 1 on failure
 */
static int
configure(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	struct sched_exec_core *exec = zdev->exec;
	struct ert_configure_cmd *cfg;
	unsigned int i, j;
	phys_addr_t cu_addr;
	char name[256] = "zocl-ert-thread";
	int cq_irq;
	int acc_cu = 0;
	int has_acc_cu = 0;
	int ret;

	if (sched_error_on(exec, opcode(cmd) != ERT_CONFIGURE))
		return 1;

	if (!list_empty(&pending_cmds)) {
		DRM_ERROR("Pending commands list not empty\n");
		return 1;
	}

	if (!list_is_singular(&g_sched0.cq)) {
		DRM_ERROR("Queued commands list not empty\n");
		return 1;
	}

	cfg = (struct ert_configure_cmd *)(cmd->packet);

	if (exec->configured != 0) {
		DRM_WARN("Reconfiguration not supported\n");
		return 1;
	}

	SCHED_DEBUG("Configuring scheduler\n");
	exec->num_slots       = CQ_SIZE / cfg->slot_size;
	write_lock(&zdev->attr_rwlock);
	exec->num_cus         = cfg->num_cus;
	exec->cu_shift_offset = cfg->cu_shift;
	exec->cu_base_addr    = cfg->cu_base_addr;
	exec->num_cu_masks    = ((exec->num_cus - 1)>>5) + 1;

	if (!zdev->ert) {
		if (cfg->ert)
			DRM_INFO("No ERT scheduler on MPSoC, using KDS\n");
		SCHED_DEBUG("++ configuring penguin scheduler mode\n");
		exec->ops = &penguin_ops;
		exec->polling_mode = cfg->polling;
		/*
		 * Interrupt may not be enabled for some of the kernel,
		 * Need to use polling mode in that case
		 */
		if (!get_cus_support_intr(zdev)) {
			DRM_WARN("Interrupt is not enabled for at least one "
			    "kernel. Fall back to polling mode.");
			exec->polling_mode = 1;
		}
		exec->configured = 1;
	} else {
		SCHED_DEBUG("++ configuring PS ERT mode\n");
		exec->ops = &ps_ert_ops;
		exec->polling_mode = cfg->polling;
		exec->cq_interrupt = cfg->cq_int;
		exec->cu_dma = cfg->cu_dma;
		exec->cu_isr = cfg->cu_isr;
		DRM_INFO("PS ERT enabled features:");
		DRM_INFO("  cu_dma(%d)", exec->cu_dma);
		DRM_INFO("  cu_isr(%d)", exec->cu_isr);
		DRM_INFO("  host_polling_mode(%d)", exec->polling_mode);
		DRM_INFO("  cq_interrupt(%d)", exec->cq_interrupt);
		zdev->ert->ops->config(zdev->ert, cfg);
		exec->configured = 1;
	}
	write_unlock(&zdev->attr_rwlock);

	/* Enable interrupt from host to PS when new commands are ready */
	if (zdev->ert && exec->cq_interrupt) {
		/* Stop CQ check thread */
		if (zdev->exec->cq_thread)
			kthread_stop(zdev->exec->cq_thread);

		/* At this point we are good. No one is polling CQ */
		cq_irq = zdev->ert->irq[ERT_CQ_IRQ];
		ret = request_irq(cq_irq, sched_cq_isr, 0, "zocl_cq", zdev);
		if (ret) {
			DRM_WARN("Failed to initial CQ interrupt. "
			    "Fall back to polling\n");
			exec->cq_interrupt = 0;
			exec->cq_thread = kthread_run(cq_check, zdev, name);
		}
	}
	/* TODO: let's consider how to support reconfigurable KDS/ERT later.
	 * At that time, ERT should be able to change back to CQ polling mode.
	 */

	exec->zcu = vzalloc(sizeof(struct zocl_cu) * exec->num_cus);
	if (!exec->zcu) {
		DRM_ERROR("Could not allocate CU objects\n");
		return -ENOMEM;
	}

	for (i = 0; i < exec->num_cus; i++) {
		if (zocl_xclbin_accel_adapter(cfg->data[i] & ~ZOCL_KDS_MASK)) {
			/* If the ACCEL adapter is used */
			acc_cu = 1;
			if (has_acc_cu == 0)
				has_acc_cu = 1;
		}

		/* CU address should be masked by encoded handshake for KDS. */
		cu_addr = cfg->data[i] & ZOCL_KDS_MASK;
		if (cu_addr == ZOCL_CU_FREE_RUNNING) {
			DRM_INFO("CU %x is free-running.", cfg->data[i]);
			continue;
		}
		/*
		 * Explicity set cu is valid,
		 * only valid cu will be processed below.
		 */
		zocl_cu_set_valid(exec, i);

		/* If it is in ert mode, there is no XCLBIN parsed now
		 * Trust configuration from host. Once host download XCLBIN to
		 * PS side, verify host configuration in the same way.
		 *
		 * Now the zdev->ert is heavily used in configure()
		 * Need cleanup.
		 */
		if (!zdev->ert && get_apt_index(zdev, cu_addr) < 0) {
			DRM_ERROR("CU address %x is not found in XCLBIN\n",
				  cfg->data[i]);
			return 1;
		}

		/* For MPSoC as PCIe device, the CU address for PS = base
		 * address + PCIe offset.
		 *
		 * For Pure MPSoC device, the base address is always 0
		 */
		SCHED_DEBUG(
		    "++ config cu(%d) at res_start: 0x%llx + cu_addr: 0x%llx\n",
		    i, (uint64_t)zdev->res_start, (uint64_t)cu_addr);
		cu_addr = zdev->res_start + cu_addr;

		if (!acc_cu)
			zocl_cu_init(&exec->zcu[i], MODEL_HLS, cu_addr);
		else {
			zocl_cu_init(&exec->zcu[i], MODEL_ACC, cu_addr);
			/* ACCEL adapter CU initial finished.
			 * The next CU might be HLS CU.
			 */
			acc_cu = 0;
		}
	}

	/* If ERT mode or ACC adapters is used, only support polling mode */
	if (zdev->ert || has_acc_cu) {
		if (has_acc_cu) {
			DRM_WARN("Interrupt is not support for ACC adapter. "
				"    Fall back to polling mode\n");
		}
		exec->polling_mode = 1;
		goto print_and_out;
	}

	/* Right now only support 32 CUs interrupts
	 * If there are more than 32 CUs, fall back to polling mode
	 */
	if (!exec->polling_mode && exec->num_cus > 32) {
		DRM_WARN("Only support up to 32 CUs interrupts, "
		    "request %d CUs. Fall back to polling mode\n",
		    exec->num_cus);
		exec->polling_mode = 1;
	}

	/* If user prefer polling_mode, skip interrupt setup */
	if (exec->polling_mode)
		goto set_cu_and_print;

	/* If re-config KDS is supported, should free old irq and disable cu
	 * interrupt according to the command
	 */
	for (i = 0; i < exec->num_cus; i++) {
		if (!zocl_cu_is_valid(exec, i))
			continue;

		ret = request_irq(zdev->irq[i], sched_exec_isr, 0,
		    "zocl", zdev);
		if (ret) {
			/* Fail to install at least one interrupt
			 * handler. We need to free the handler(s)
			 * already installed and fall back to
			 * polling mode.
			 */
			for (j = 0; j < i; j++) {
				if (zocl_cu_is_valid(exec, i))
					free_irq(zdev->irq[j], zdev);
			}
			DRM_WARN("request_irq failed on CU %d error: %d."
			    "Fall back to polling mode.\n", i, ret);
			exec->polling_mode = 1;
			break;
		}
	}

set_cu_and_print:
	/* Do not trust user's interrupt enable setting in the start cu cmd */
	for (i = 0; i < exec->num_cus; i++) {
		if (!zocl_cu_is_valid(exec, i))
			continue;

		if (exec->polling_mode)
			zocl_cu_disable_intr(&exec->zcu[i], CU_INTR_DONE);
		else
			zocl_cu_enable_intr(&exec->zcu[i], CU_INTR_DONE);
	}

print_and_out:
	DRM_INFO("scheduler config ert(%d)", is_ert(cmd->ddev));
	DRM_INFO("  cus(%d)", exec->num_cus);
	DRM_INFO("  slots(%d)", exec->num_slots);
	DRM_INFO("  num_cu_masks(%d)", exec->num_cu_masks);
	DRM_INFO("  cu_shift(%d)", exec->cu_shift_offset);
	DRM_INFO("  cu_base(0x%x)", exec->cu_base_addr);
	DRM_INFO("  polling(%d)", exec->polling_mode);
	return 0;
}

static int
configure_soft_kernel(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct ert_configure_sk_cmd *cfg;
	u32 i;
	struct soft_krnl_cmd *scmd;
	int ret;

	SCHED_DEBUG("-> %s", __func__);

	cfg = (struct ert_configure_sk_cmd *)(cmd->packet);

	mutex_lock(&sk->sk_lock);

	/* Check if the CU configuration exceeds maximum CU number */
	if (cfg->start_cuidx + cfg->num_cus > MAX_CU_NUM) {
		DRM_WARN("Soft kernel CU %d exceed maximum cu number %d.\n",
		    cfg->start_cuidx + cfg->num_cus, MAX_CU_NUM);
		mutex_unlock(&sk->sk_lock);
		return -EINVAL;
	}

	/* Check if any CU is configured already */
	for (i = cfg->start_cuidx; i < cfg->start_cuidx + cfg->num_cus; i++)
		if (sk->sk_cu[i]) {
			DRM_WARN("Soft Kernel CU %d is configured already.\n",
			    i);
			mutex_unlock(&sk->sk_lock);
			return -EINVAL;
		}

	sk->sk_ncus += cfg->num_cus;

	mutex_unlock(&sk->sk_lock);

	/* NOTE: any failure after this point needs to resume sk_ncus */

	/* Fill up a soft kernel command and add to soft kernel command list */
	scmd = kmalloc(sizeof(struct soft_krnl_cmd), GFP_KERNEL);
	if (!scmd) {
		ret = -ENOMEM;
		goto fail;
	}

	scmd->skc_packet = (struct ert_packet *)cfg;

	if (cfg->sk_type == SOFTKERNEL_TYPE_XCLBIN) {
		void *xclbin_buffer = NULL;

		/* remap device physical addr to kernel virtual addr */
		xclbin_buffer =
		    memremap(cfg->sk_addr, cfg->sk_size, MEMREMAP_WB);
		if (xclbin_buffer == NULL) {
			ret = -ENOMEM;
			goto fail;
		}
		ret = zocl_xclbin_load_pdi(zdev, xclbin_buffer);
		memunmap(xclbin_buffer);
		if (ret)
			goto fail;
	}

	mutex_lock(&sk->sk_lock);
	list_add_tail(&scmd->skc_list, &sk->sk_cmd_list);
	mutex_unlock(&sk->sk_lock);

	/* start CU by waking up Soft Kernel handler */
	wake_up_interruptible(&sk->sk_wait_queue);

	SCHED_DEBUG("<- %s\n", __func__);

	return 0;

fail:
	mutex_lock(&sk->sk_lock);
	sk->sk_ncus -= cfg->num_cus;
	mutex_unlock(&sk->sk_lock);
	return ret;
}

static int
unconfigure_soft_kernel(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct soft_cu *scu;
	struct ert_unconfigure_sk_cmd *cfg;
	u32 i;

	SCHED_DEBUG("-> %s\n", __func__);

	cfg = (struct ert_unconfigure_sk_cmd *)(cmd->packet);

	mutex_lock(&sk->sk_lock);

	/* Check if the CU unconfiguration exceeds maximum CU number */
	if (cfg->start_cuidx + cfg->num_cus > MAX_CU_NUM) {
		DRM_WARN("Soft kernel CU %d exceed maximum cu number %d.\n",
		    cfg->start_cuidx + cfg->num_cus, MAX_CU_NUM);
		mutex_unlock(&sk->sk_lock);
		return -EINVAL;
	}

	/* Check if any CU is not configured */
	for (i = cfg->start_cuidx; i < cfg->start_cuidx + cfg->num_cus; i++)
		if (!sk->sk_cu[i]) {
			DRM_WARN("Soft Kernel CU %d is not configured.\n", i);
			mutex_unlock(&sk->sk_lock);
			return -EINVAL;
		}

	sk->sk_ncus -= cfg->num_cus;

	/*
	 * For each soft kernel, we set the RELEASE flag and wake up
	 * waiting thread to release soft kenel.
	 */
	for (i = cfg->start_cuidx; i < cfg->start_cuidx + cfg->num_cus; i++) {
		scu = sk->sk_cu[i];
		scu->sc_flags |= ZOCL_SCU_FLAGS_RELEASE;
		up(&scu->sc_sem);
	}

	mutex_unlock(&sk->sk_lock);

	SCHED_DEBUG("<- %s\n", __func__);

	return 0;
}

/**
 * set_cmd_state() - Set both internal and external state of a command
 *
 * The state is reflected externally through the command packet
 * as well as being captured in internal state variable
 *
 * @cmd: command object
 * @state: new state
 */
static inline void
set_cmd_state(struct sched_cmd *cmd, enum ert_cmd_state state)
{
	SCHED_DEBUG("-> %s (,%d)\n", __func__, state);
	set_cmd_timestamp(cmd, state);
	cmd->state = state;
	cmd->packet->state = state;
	SCHED_DEBUG("<- %s\n", __func__);
}

/**
 * set_cmd_ext_cu_idx() - Set external cu_idx of a command
 *
 * The cu_idx is reflected externally through the command packet
 *
 * @cmd: command object
 * @cu_idx: CU idex
 */
static inline void
set_cmd_ext_cu_idx(struct sched_cmd *cmd, int cu_idx)
{
	int mask_idx = cu_mask_idx(cu_idx);
	int mask_cu_idx = cu_idx_in_mask(cu_idx);

	cmd->packet->data[mask_idx] &= 1 << mask_cu_idx;
}

/**
 * set_cmd_ext_timestamp() - Set timestamp in the command packet
 *
 * The timestamp is reflected externally through the command packet
 * TODO: Should have scheduler profiling solution in the future.
 *
 * @cmd: command object
 * @ts: timestamp type
 */
static inline void
set_cmd_ext_timestamp(struct sched_cmd *cmd, enum zocl_ts_type ts)
{
	u32 opc = opcode(cmd);
	ZOCL_TIMESPEC tv;
	struct ert_start_kernel_cmd *sk;

	sk = (struct ert_start_kernel_cmd *)cmd->packet;

	/* Simply skip if it is not start CU and exec write command */
	if (opc != ERT_START_CU && opc != ERT_EXEC_WRITE)
		return;
	/* Use the first 4 32bits in the regmap to record CU start/end time.
	 * To let user application know the CU start/end time.
	 * The first  32 bits - CU start seconds
	 * The second 32 bits - CU start microseconds
	 * The third  32 bits - CU done  seconds
	 * The fourth 32 bits - CU done  microseconds
	 * Use 32 bits timestamp is good enough for this purpose for now.
	 */
	ZOCL_GETTIME(&tv);
	if (ts == CU_START_TIME) {
		*(sk->data + sk->extra_cu_masks) = (u32)tv.tv_sec;
		*(sk->data + sk->extra_cu_masks + 1) = (u32)tv.ZOCL_USEC;
	} else if (ts == CU_DONE_TIME) {
		*(sk->data + sk->extra_cu_masks + 2) = (u32)tv.tv_sec;
		*(sk->data + sk->extra_cu_masks + 3) = (u32)tv.ZOCL_USEC;
	}
}

/**
 * acquire_slot_idx() - Acquire a slot index if available.
 *                      Update slot status to busy so it cannot be reacquired.
 *
 * This function is called from scheduler thread
 *
 * Return: Command queue slot index, or -1 if none available
 */
static int
acquire_slot_idx(struct drm_device *dev)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	unsigned int mask_idx = 0, slot_idx = -1, tmp_idx;
	u32 mask;

	SCHED_DEBUG("-> %s\n", __func__);
	for (mask_idx = 0; mask_idx < zdev->exec->num_slot_masks; ++mask_idx) {
		mask = zdev->exec->slot_status[mask_idx];
		slot_idx = ffz_or_neg_one(mask);
		tmp_idx = slot_idx_from_mask_idx(slot_idx, mask_idx);
		if (slot_idx == -1 || tmp_idx >= zdev->exec->num_slots)
			continue;
		zdev->exec->slot_status[mask_idx] ^= (1<<slot_idx);
		SCHED_DEBUG("<- %s returns %d\n", __func__,
			    slot_idx_from_mask_idx(slot_idx, mask_idx));
		return slot_idx_from_mask_idx(slot_idx, mask_idx);
	}
	SCHED_DEBUG("<- %s returns -1\n", __func__);
	return -1;
}

/**
 * release_slot_idx() - Release a slot index
 *
 * Update slot status mask for slot index. Notify scheduler in case
 * release is via ISR
 *
 * @dev: scheduler
 * @slot_idx: the slot index to release
 */
static void
release_slot_idx(struct drm_device *dev, unsigned int slot_idx)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	unsigned int mask_idx = slot_mask_idx(slot_idx);
	unsigned int pos = slot_idx_in_mask(slot_idx);

	SCHED_DEBUG("<-> %s slot_status[%d]=0x%x, pos=%d\n",
	    __func__, mask_idx, zdev->exec->slot_status[mask_idx], pos);
	zdev->exec->slot_status[mask_idx] ^= (1<<pos);
}

inline struct cu_ctrl_reg *
to_cu_ctrl_reg(u32 *reg)
{
	return (struct cu_ctrl_reg *)reg;
}

/**
 * cu_done() - Check status of CU which execute cmd
 *
 * @cmd: submmited command
 *
 * This function is called to check if the CU which execute cmd is done.
 * The cmd should be guaranteed to have been submitted.
 *
 * Return: %true if cu done, %false otherwise
 */
inline int
cu_done(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	int cu_idx = cmd->cu_idx;
	struct zocl_cu *cu = &cmd->exec->zcu[cu_idx];
	struct sched_cmd *fc;

	SCHED_DEBUG("-> %s (,%d)\n", __func__, cu_idx);

	fc = list_first_entry(&cu->running_queue, struct sched_cmd, rq_list);
	/* Check if this cmd is the first one running on the CU */
	if (fc != cmd) {
		SCHED_DEBUG("<- %s returns 0\n", __func__);
		return false;
	}

	zocl_cu_check(cu);

	if (cu->done_cnt > 0) {
		unsigned int mask_idx = cu_mask_idx(cu_idx);
		unsigned int pos = cu_idx_in_mask(cu_idx);

		set_cmd_ext_timestamp(cmd, CU_DONE_TIME);
		/* if cu is busy, change the status to free */
		if (zdev->exec->cu_status[mask_idx] & 1<<pos)
			zdev->exec->cu_status[mask_idx] ^= 1<<pos;

		/* This command is completed, refund credit and delete it */
		zocl_cu_refund_credit(cu, 1);
		list_del(&cmd->rq_list);
		cu->done_cnt--;

		SCHED_DEBUG("<- %s returns 1\n", __func__);
		return true;
	}
	SCHED_DEBUG("<- %s returns 0\n", __func__);
	return false;
}

inline int
scu_done(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	int cu_idx = cmd->cu_idx;
	struct soft_krnl *sk = zdev->soft_kernel;
	u32 *virt_addr = sk->sk_cu[cu_idx]->sc_vregs;

	SCHED_DEBUG("-> %s (,%d) checks scu at address 0x%p\n",
	    __func__, cu_idx, virt_addr);
	/* We simulate hard CU here.
	 * done is indicated by AP_DONE(2) alone or by AP_DONE(2) | AP_IDLE(4)
	 * but not by AP_IDLE itself.  Since 0x10 | (0x10 | 0x100) = 0x110
	 * checking for 0x10 is sufficient.
	 */
	mutex_lock(&sk->sk_lock);
	if (*virt_addr & 2) {
		unsigned int mask_idx = cu_mask_idx(cu_idx);
		unsigned int pos = cu_idx_in_mask(cu_idx);

		zdev->exec->scu_status[mask_idx] ^= 1 << pos;
		*virt_addr &= ~2;
		mutex_unlock(&sk->sk_lock);
		SCHED_DEBUG("<- %s returns 1\n", __func__);
		return true;
	}
	mutex_unlock(&sk->sk_lock);
	SCHED_DEBUG("<- %s returns 0\n", __func__);
	return false;
}

inline int
scu_configure_done(struct sched_cmd *cmd)
{
	struct drm_device *dev = cmd->ddev;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct ert_configure_sk_cmd *cfg;
	int i;

	cfg = (struct ert_configure_sk_cmd *)(cmd->packet);

	mutex_lock(&sk->sk_lock);

	for (i = cfg->start_cuidx; i < cfg->start_cuidx + cfg->num_cus; i++)
		if (sk->sk_cu[i] == NULL) {
			/*
			 * If we have any unconfigured soft kernel CU, this
			 * configure command is not completed yet.
			 */
			mutex_unlock(&sk->sk_lock);
			return false;
		}

	mutex_unlock(&sk->sk_lock);

	return true;
}

inline int
scu_unconfig_done(struct sched_cmd *cmd)
{
	struct drm_device *dev = cmd->ddev;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct ert_unconfigure_sk_cmd *cfg;
	int i;

	cfg = (struct ert_unconfigure_sk_cmd *)(cmd->packet);

	mutex_lock(&sk->sk_lock);
	for (i = cfg->start_cuidx; i < cfg->start_cuidx + cfg->num_cus; i++)
		if (sk->sk_cu[i]) {
			/*
			 * If we have any configured soft kernel CU, this
			 * unconfigure command is not completed yet.
			 */
			mutex_unlock(&sk->sk_lock);
			return false;
		}

	mutex_unlock(&sk->sk_lock);

	return true;
}

/**
 * notify_host() - Notify user space that a command is complete.
 */
static void
notify_host(struct sched_cmd *cmd)
{
	struct list_head *ptr;
	struct sched_client_ctx *entry;
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	unsigned long flags = 0;

	SCHED_DEBUG("-> %s from num_running: %d\n",
	    __func__, atomic_read(&num_running));

	atomic_dec(&num_running);

	if (!zdev->ert) {
		/* for each client update the trigger counter in the context */
		spin_lock_irqsave(&zdev->exec->ctx_list_lock, flags);
		list_for_each(ptr, &zdev->exec->ctx_list) {
			entry = list_entry(ptr, struct sched_client_ctx, link);
			atomic_inc(&entry->trigger);
			SCHED_DEBUG("-> %s trigger %d\n", __func__,
			    atomic_read(&entry->trigger));
		}
		spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, flags);
		/* wake up all the clients */
		wake_up_interruptible(&zdev->exec->poll_wait_queue);
	} else {
		zdev->ert->ops->notify_host(zdev->ert, cmd->cq_slot_idx);
	}
	SCHED_DEBUG("<- %s to num_running: %d\n",
	    __func__, atomic_read(&num_running));
}

/**
 * Note: zocl copy bo will use built-in dma without
 * using any real ERT CU kernel, we will need to increase
 * the poll count to wake up scheduler when dma is done
 */
static inline void
polling_cnt_inc(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;

	if (zdev->ert || zdev->exec->polling_mode ||
	    (opcode(cmd) == ERT_START_COPYBO))
		++cmd->sched->poll;
}

static inline void
polling_cnt_dec(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;

	if (zdev->ert || zdev->exec->polling_mode ||
	    (opcode(cmd) == ERT_START_COPYBO))
		--cmd->sched->poll;
}

/**
 * mark_cmd_complete() - Move a command to complete state
 *
 * Commands are marked complete in two ways
 *  1. Through polling of CUs or polling of MB status register
 *  2. Through interrupts from MB
 * In both cases, the completed commands are residing in the completed_cmds
 * list and the number of completed commands is reflected in num_completed.
 *
 * @xcmd: Command to mark complete
 *
 * The command is removed from the slot it occupies in the device command
 * queue. The slot is released so new commands can be submitted. The host
 * is notified that some command has completed.
 */
static void
mark_cmd_complete(struct sched_cmd *cmd, enum ert_cmd_state cmd_state)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;

	SCHED_DEBUG("-> %s(,%d)\n", __func__, cmd->slot_idx);
	zdev->exec->submitted_cmds[cmd->slot_idx] = NULL;
	set_cmd_state(cmd, cmd_state);
	polling_cnt_dec(cmd);
	release_slot_idx(cmd->ddev, cmd->slot_idx);
	notify_host(cmd);
	SCHED_DEBUG("<- %s\n", __func__);
}

/**
 * During cmd submit within queued_to_running, we need to handle cases that
 * submit failed then we want to bail out this cmd by setting it as ERROR.
 * We will need to notify host as well.
 */
static void
mark_cmd_submit_error(struct sched_cmd *cmd)
{
	set_cmd_state(cmd, ERT_CMD_STATE_ERROR);
	notify_host(cmd);
}

/**
 * For pending cmds before or during cmd submit, scheduler might has been
 * set to stop. We want to bail out those cmds by set it to ABORT. We will
 * need to notify host as well.
 */
static void
mark_cmd_submit_abort(struct sched_cmd *cmd)
{
	set_cmd_state(cmd, ERT_CMD_STATE_ABORT);
	notify_host(cmd);
}

/**
 * get_free_sched_cmd() - Get a free command object
 *
 * Get from free/recycled list or allocate a new command if necessary.
 *
 * Return: Free command object
 */
static struct sched_cmd*
get_free_sched_cmd(void)
{
	struct sched_cmd *cmd;

	SCHED_DEBUG("-> %s\n", __func__);
	mutex_lock(&free_cmds_mutex);
	cmd = list_first_entry_or_null(&free_cmds, struct sched_cmd, list);
	if (cmd)
		list_del(&cmd->list);
	mutex_unlock(&free_cmds_mutex);
	if (!cmd)
		cmd = kmalloc(sizeof(struct sched_cmd), GFP_KERNEL);
	if (!cmd)
		return ERR_PTR(-ENOMEM);
	memset(&cmd->dma_handle, 0, sizeof(zocl_dma_handle_t));
	SCHED_DEBUG("<- %s %p\n", __func__, cmd);
	return cmd;
}

/**
 * zocl_gem_object_unref() - unreference a drm object
 *
 * @cmd: cmd owning the drm object to be unreference,
 *       it could be CMA or normal gem buffer
 *
 * Use the correct way to unreference a gem object
 *
 */
void zocl_gem_object_unref(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	struct drm_zocl_bo *bo = cmd->buffer;

	if (zdev->domain)
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&bo->gem_base);
	else
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&bo->cma_base.base);
}

/*
 * add_cmd() - Add a new command to the pending list
 *
 * @ddev: drm device owning adding the buffer object
 * @bo: buffer objects from user space from which new command is created
 *
 * Scheduler copies pending commands to its internal command queue.
 *
 * Return: 0 on success, -errno on failure
 */
static int
add_cmd(struct sched_cmd *cmd)
{
	int ret = 0;
	unsigned long flags;

	SCHED_DEBUG("-> %s\n", __func__);

	if (atomic_read(&cmd->exec->exec_status) != ZOCL_EXEC_NORMAL)
		return -EBUSY;

	cmd->cu_idx = -1;
	cmd->slot_idx = -1;
	DRM_DEBUG("packet header 0x%08x, data 0x%08x\n",
		  cmd->packet->header, cmd->packet->data[0]);
	set_cmd_state(cmd, ERT_CMD_STATE_NEW);
	spin_lock_irqsave(&pending_cmds_lock, flags);
	list_add_tail(&cmd->list, &pending_cmds);
	spin_unlock_irqrestore(&pending_cmds_lock, flags);

	/* wake scheduler */
	atomic_inc(&num_pending);
	wake_up_interruptible(&cmd->sched->wait_queue);

	SCHED_DEBUG("<- %s\n", __func__);
	return ret;
}

/*
 * add_gem_bo_cmd() - add a command by gem buffer object
 *
 * @ddev: drm device owning adding the buffer object
 * @bo: buffer objects from user space from which new command is created
 *
 * Get a free scheduler command and initial it by gem buffer object.
 * After all, add this command to pending list.
 *
 * Return: 0 on success, -errno on failure
 */
static int
add_gem_bo_cmd(struct drm_device *dev, struct drm_zocl_bo *bo,
	struct sched_client_ctx *client)
{
	struct sched_cmd *cmd;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct ert_packet *packet;
	int ret;

	cmd = get_free_sched_cmd();
	if (!cmd)
		return -ENOMEM;

	SCHED_DEBUG("-> %s\n", __func__);
	cmd->ddev = dev;
	cmd->sched = zdev->exec->scheduler;
	cmd->buffer = (void *)bo;
	cmd->exec = zdev->exec;
	cmd->client = client;
	if (zdev->domain)
		packet = (struct ert_packet *)bo->vmapping;
	else
		packet = (struct ert_packet *)bo->cma_base.vaddr;
	cmd->packet = packet;
	cmd->timestamp_enabled = enable_cmd_timestamps(cmd);
	cmd->cq_slot_idx = 0;
	cmd->free_buffer = zocl_gem_object_unref;
	atomic_inc(&client->outstanding_execs);

	ret = add_cmd(cmd);

	SCHED_DEBUG("<- %s\n", __func__);
	return ret;
}

/**
 * recycle_cmd() - recycle a command objects
 *
 * @cmd: command object to recycle
 *
 * Command object is added to the freelist
 *
 * Return: 0
 */
static int
recycle_cmd(struct sched_cmd *cmd)
{
	SCHED_DEBUG("%s %p\n", __func__, cmd);
	mutex_lock(&free_cmds_mutex);
	list_move_tail(&cmd->list, &free_cmds);
	mutex_unlock(&free_cmds_mutex);

	atomic_dec(&cmd->client->outstanding_execs);
	return 0;
}

/**
 * delete_cmd_list() - reclaim memory for all allocated command objects
 */
static void
delete_cmd_list(void)
{
	struct sched_cmd *cmd;
	struct list_head *pos, *next;

	mutex_lock(&free_cmds_mutex);
	list_for_each_safe(pos, next, &free_cmds) {
		cmd = list_entry(pos, struct sched_cmd, list);
		list_del(pos);
		kfree(cmd);
	}
	mutex_unlock(&free_cmds_mutex);
}

static inline void
zocl_print_stale_cmd(struct sched_cmd *cmd)
{
	DRM_INFO("stale cmd state[%d], cu[%d], slot[%d], cq_slot[%d]",
	    cmd->state, cmd->cu_idx, cmd->slot_idx, cmd->cq_slot_idx);
	DRM_INFO("          check_timeout=%d, client pid %d.",
	    cmd->check_timeout, pid_nr(cmd->client->pid));
}

/**
 * reset_all() - Reset the scheduler
 *
 * Clear stale command objects if any. This can occur if the HW for
 * some reason hangs.
 */
static void
reset_all(void)
{
	struct sched_cmd *cmd;

	/* clear stale command object if any */
	while (!list_empty(&pending_cmds)) {
		cmd = list_first_entry(&pending_cmds, struct sched_cmd, list);
		DRM_INFO("deleting stale pending cmd\n");
		zocl_print_stale_cmd(cmd);
		cmd->free_buffer(cmd);
		recycle_cmd(cmd);
	}
	while (!list_empty(&g_sched0.cq)) {
		cmd = list_first_entry(&g_sched0.cq, struct sched_cmd, list);
		DRM_INFO("deleting stale scheduler cmd\n");
		zocl_print_stale_cmd(cmd);
		cmd->free_buffer(cmd);
		recycle_cmd(cmd);
	}
}

/**
 * get_free_cu() - get index of first available CU per command cu mask
 *
 * @cmd:    command containing CUs to check for availability
 * @cu_type: ZOCL_SOFT_CU(1) to get free soft CU,
 *           ZOCL_HARD_CU(0) to get free hard CU
 *
 * This function is called kernel software scheduler mode only, in embedded
 * scheduler mode, the hardware scheduler handles the commands directly.
 *
 * Return: Index of free CU,
 *         -1 of no CU is available,
 *         -EINVAL cu_mask is invalid.
 */
static int
get_free_cu(struct sched_cmd *cmd, enum zocl_cu_type cu_type)
{
	int mask_idx = 0;
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	int num_masks = cu_masks(cmd);
	struct sched_exec_core *exec = zdev->exec;
	int cu_idx = -1;
	int valid_found = 0;

	SCHED_DEBUG("-> %s\n", __func__);

	for (mask_idx = 0; mask_idx < num_masks; ++mask_idx) {
		u32 cmd_mask = cmd->packet->data[mask_idx]; /* skip header */
		u32 busy_mask = cu_type == ZOCL_SOFT_CU ?
		    exec->scu_status[mask_idx]
		    : exec->cu_status[mask_idx];
		u32 free_mask = (cmd_mask | busy_mask) ^ busy_mask;
		/* for soft cu, cu is always valid */
		u32 valid_mask = cu_type == ZOCL_SOFT_CU ?
		    (u32)-1 : exec->cu_valid[mask_idx];

		if (cmd_mask & valid_mask)
			valid_found = 1;

		/* For hardware cu, we have to search on valid CUs only. */
		if (cu_type == ZOCL_HARD_CU)
			free_mask &= exec->cu_valid[mask_idx];

		cu_idx = ffs_or_neg_one(free_mask);

		if (cu_idx < 0)
			continue;

		if (cu_type == ZOCL_HARD_CU) {
			 /* KDS should not over spending credits */
			if (!zocl_cu_get_credit(&exec->zcu[cu_idx]))
				exec->cu_status[mask_idx] ^= 1 << cu_idx;
		} else
			exec->scu_status[mask_idx] ^= 1 << cu_idx;
		SCHED_DEBUG("<- %s returns %d\n", __func__,
			    cu_idx_from_mask(cu_idx, mask_idx));
		return cu_idx_from_mask(cu_idx, mask_idx);
	}

	if (!valid_found)
		DRM_WARN("Cannot find valid cu from cu_mask");
	valid_found = valid_found ? -1 : -EINVAL;
	SCHED_DEBUG("<- %s exhausted returns %d\n", __func__, valid_found);
	return valid_found;
}

/**
 * configure_cu() - transfer command register map to specified CU and
 *                  start the CU.
 *
 * @cmd: command with register map to transfer to CU
 * @cu_idx: index of CU to configure
 *
 * This function is called in kernel software scheduler mode only.
 */
static void
configure_cu(struct sched_cmd *cmd, int cu_idx)
{
	u32 size = regmap_size(cmd);
	struct ert_start_kernel_cmd *sk;
	struct zocl_cu *cu = &cmd->exec->zcu[cu_idx];
	int type;

	SCHED_DEBUG("-> %s cu_idx=%d, regmap_size=%d\n",
	    __func__, cu_idx, size);

	sk = (struct ert_start_kernel_cmd *)cmd->packet;

	if (opcode(cmd) == ERT_EXEC_WRITE)
		type = PAIRS;
	else
		type = CONSECUTIVE;

	zocl_cu_configure(cu, sk->data + sk->extra_cu_masks, size, type);

	/* Let user know which CU execute this command */
	set_cmd_ext_cu_idx(cmd, cu_idx);

	set_cmd_ext_timestamp(cmd, CU_START_TIME);

	/* Set the initial command execute time */
	cmd->exectime = cu_idx_to_timeout(cmd->ddev, cmd->cu_idx);
	cmd->check_timeout = cmd->exectime > 0 ? 1 : 0;

	zocl_cu_start(cu);
	list_add_tail(&cmd->rq_list, &cu->running_queue);

	SCHED_DEBUG("<- %s\n", __func__);
}

/**
 * ert_configure_cu() - transfer command register map to specified CU and
 *                      start the CU.
 *
 * @cmd: command with register map to transfer to CU
 * @cu_idx: index of CU to configure
 *
 * This function is called in kernel software scheduler mode only.
 */
static void
ert_configure_cu(struct sched_cmd *cmd, int cu_idx)
{
	u32 size = regmap_size(cmd);
	struct ert_start_kernel_cmd *sk;
	struct zocl_cu *cu = &cmd->exec->zcu[cu_idx];
	int type = CONSECUTIVE;

	SCHED_DEBUG("-> %s cu_idx=%d, regmap_size=%d\n",
	    __func__, cu_idx, size);

	sk = (struct ert_start_kernel_cmd *)cmd->packet;

	zocl_cu_configure(cu, sk->data + sk->extra_cu_masks, size, type);

	zocl_cu_start(cu);
	list_add_tail(&cmd->rq_list, &cu->running_queue);

	SCHED_DEBUG("<- %s\n", __func__);
}

static int
ert_configure_scu(struct sched_cmd *cmd, int cu_idx)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct soft_cu *scu;
	u32 i, size = regmap_size(cmd);
	u32 *cu_regfile;
	struct ert_start_kernel_cmd *skc;

	skc = (struct ert_start_kernel_cmd *)cmd->packet;

	SCHED_DEBUG("-> %s", __func__);

	mutex_lock(&sk->sk_lock);
	scu = sk->sk_cu[cu_idx];
	if (!scu) {
		DRM_ERROR("Error: soft cu does not exist.\n");
		mutex_unlock(&sk->sk_lock);
		return -ENXIO;
	}

	cu_regfile = scu->sc_vregs;

	SCHED_DEBUG("cu_idx=%d, cu_addr=0x%p, regmap_size=%d\n",
		    cu_idx, cu_regfile, size);

	/* Copy payload to soft CU register */
	for (i = 1; i < size; ++i)
		cu_regfile[i] = *(skc->data + skc->extra_cu_masks + i);

	up(&scu->sc_sem);
	mutex_unlock(&sk->sk_lock);

	SCHED_DEBUG("<- %s", __func__);

	return 0;
}

/**
 * zocl_reset_cu() - Reset a running command and CU.
 *
 * @cmd: Command to reset. CU is pointed by cmd->cu_idx.
 *
 * Reset a CU when detecting CU timeout. After resetting CU, we will
 * busy wait a preset reset_timeout Microseconds. If CU is reset
 * successfully, we set cmd state to TIMEOUT so that this CU can be
 * restarted. If reset is not done after this period of time, we set
 * cmd state to NORESPONSE to indicate the CU timeout and reset fails.
 */
static void
zocl_reset_cu(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	int cu_idx = cmd->cu_idx;
	uint32_t reset_timeout = cu_idx_to_reset_timeout(cmd->ddev, cu_idx);
	unsigned int mask_idx = cu_mask_idx(cu_idx);
	unsigned int pos = cu_idx_in_mask(cu_idx);
	enum ert_cmd_state cmd_state;

	SCHED_DEBUG("-> %s (,%d)\n", __func__, cmd->slot_idx);

	zocl_cu_reset(&zdev->exec->zcu[cu_idx]);
	while (reset_timeout) {
		if (zocl_cu_reset_done(&zdev->exec->zcu[cu_idx]))
			break;
		udelay(ZOCL_CU_RESET_TIMER_INTERVAL);
		reset_timeout--;
	}

	if (reset_timeout == 0) {
		DRM_ERROR("CU %d timeouts and reset fails.\n", cmd->cu_idx);
		cmd_state = ERT_CMD_STATE_NORESPONSE;
	} else {
		DRM_WARN("CU %d timeouts and has been reset successfully.\n",
		    cmd->cu_idx);
		cmd_state = ERT_CMD_STATE_TIMEOUT;
	}

	zdev->exec->cu_status[mask_idx] ^= 1<<pos;
	mark_cmd_complete(cmd, cmd_state);

	SCHED_DEBUG("<- %s (,%d)\n", __func__, cmd->slot_idx);
}

/**
 * check_cmds_timeout() - Decrease the execute time and check cmds timeout.
 *
 * We reach the checkpoint indicated by the timer thread. So we do a
 * special check for every command in the running state. Decrease the
 * command's run time and check if it goes to zero.
 */
static void
check_cmds_timeout(struct scheduler *sched)
{
	struct sched_cmd *cmd;
	struct list_head *pos, *next;

	list_for_each_safe(pos, next, &sched->cq) {
		cmd = list_entry(pos, struct sched_cmd, list);

		if (cmd->check_timeout && cmd->state == ERT_CMD_STATE_RUNNING) {
			cmd->exectime--;
			if (cmd->exectime == 0) {
				/*
				 * Detected command timeout. We mark the
				 * internal command state to TIMEOUT so that
				 * we can perform reset operation on this
				 * command and CU.
				 */
				set_cmd_int_state(cmd, ERT_CMD_STATE_TIMEOUT);
			}
		}
	}
}

/**
 * queued_to_running() - Move a command from queued to running state if possible
 *
 * @cmd: Command to start
 *
 * Upon success, the command is not necessarily running. In ert mode the
 * command will have been submitted to the embedded scheduler, whereas in
 * penguin mode the command has been started on a CU.
 *
 * Return: %true if command was submitted to device, %false otherwise
 */
static int
queued_to_running(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;
	int retval = false;

	SCHED_DEBUG("-> %s\n", __func__);
	if (opcode(cmd) == ERT_CONFIGURE)
		configure(cmd);

	if (opcode(cmd) == ERT_INIT_CU)
		init_cus(cmd);

	if (zdev->exec->ops->submit(cmd)) {
		set_cmd_int_state(cmd, ERT_CMD_STATE_RUNNING);
		polling_cnt_inc(cmd);
		zdev->exec->submitted_cmds[cmd->slot_idx] = cmd;
		retval = true;
	}
	SCHED_DEBUG("<- %s returns %d\n", __func__, retval);

	return retval;
}

/**
 * running_to_complete() - Check status of running commands
 *
 * @cmd: Command is in running state
 *
 * If a command is found to be complete, it marked complete prior to return
 * from this function.
 */
static void
running_to_complete(struct sched_cmd *cmd)
{
	struct drm_zocl_dev *zdev = cmd->ddev->dev_private;

	SCHED_DEBUG("-> %s\n", __func__);
	zdev->exec->ops->query(cmd);
	SCHED_DEBUG("<- %s\n", __func__);
}

/**
 * complete_to_free() - Recycle a complete command objects
 *
 * @xcmd: Command is in complete state
 */
static void
complete_to_free(struct sched_cmd *cmd)
{

	SCHED_DEBUG("-> %s\n", __func__);
	cmd->free_buffer(cmd);
	recycle_cmd(cmd);
	SCHED_DEBUG("<- %s\n", __func__);
}


/**
 * scheduler_queue_cmds() - Queue any pending commands
 *
 * The scheduler copies pending commands to its internal command queue where
 * is is now in queued state.
 */
static void
scheduler_queue_cmds(struct scheduler *sched)
{
	struct sched_cmd *cmd;
	struct list_head *pos, *next;
	unsigned long flags;

	SCHED_DEBUG("-> %s\n", __func__);

	spin_lock_irqsave(&pending_cmds_lock, flags);
	list_for_each_safe(pos, next, &pending_cmds) {
		cmd = list_entry(pos, struct sched_cmd, list);
		if (cmd->sched != sched)
			continue;
		list_del(&cmd->list);
		atomic_dec(&num_pending);
		list_add_tail(&cmd->list, &sched->cq);
		atomic_inc(&num_running);
		set_cmd_int_state(cmd, ERT_CMD_STATE_QUEUED);
	}
	spin_unlock_irqrestore(&pending_cmds_lock, flags);
	SCHED_DEBUG("<- %s\n", __func__);
}

static void
cmd_update_state(struct sched_cmd *cmd)
{
	if (cmd->state != ERT_CMD_STATE_RUNNING && cmd->client->abort) {
		DRM_INFO("Aborting cmds for closing pid(%d)",
		    pid_nr(cmd->client->pid));
		zocl_print_stale_cmd(cmd);
		mark_cmd_submit_abort(cmd);
	}
	/* Abort stuck CUs, but still let 'healthy' CU continue */
	if ((atomic_read(&cmd->exec->exec_status) ==
	    ZOCL_EXEC_FLUSH) && (cmd->cu_idx < 0)) {
		zocl_print_stale_cmd(cmd);
		mark_cmd_submit_abort(cmd);
	}
}

/**
 * scheduler_iterate_cmds() - Iterate all commands in scheduler command queue
 */
static void
scheduler_iterate_cmds(struct scheduler *sched)
{
	struct sched_cmd *cmd;
	struct list_head *pos, *next;

	SCHED_DEBUG("-> %s\n", __func__);

	if (atomic_read(&sched->check)) {
		atomic_set(&sched->check, 0);
		check_cmds_timeout(sched);
	}

	list_for_each_safe(pos, next, &sched->cq) {
		cmd = list_entry(pos, struct sched_cmd, list);

		cmd_update_state(cmd);

		if (cmd->state == ERT_CMD_STATE_QUEUED)
			queued_to_running(cmd);
		if (cmd->state == ERT_CMD_STATE_RUNNING)
			running_to_complete(cmd);
		if (cmd->state == ERT_CMD_STATE_TIMEOUT)
			zocl_reset_cu(cmd);
		if (cmd->state == ERT_CMD_STATE_COMPLETED ||
		    cmd->state == ERT_CMD_STATE_ABORT ||
		    cmd->state == ERT_CMD_STATE_TIMEOUT ||
		    cmd->state == ERT_CMD_STATE_ERROR ||
		    cmd->state == ERT_CMD_STATE_NORESPONSE)
			complete_to_free(cmd);
	}

	SCHED_DEBUG("<- %s\n", __func__);
}

/**
 * sched_wait_cond() - Check status of scheduler wait condition
 *
 * Scheduler must wait (sleep) if
 *  1. there are no pending commands
 *  2. no pending interrupt from embedded scheduler
 *  3. no pending complete commands in polling mode
 *
 * Return: 1 if scheduler must wait, 0 othewise
 */
static int
sched_wait_cond(struct scheduler *sched)
{
	if (kthread_should_stop() || sched->error) {
		sched->stop = 1;
		SCHED_DEBUG("scheduler wakes kthread_should_stop\n");
		return 0;
	}

	if (atomic_read(&num_pending)) {
		SCHED_DEBUG("scheduler wakes to copy new pending commands\n");
		return 0;
	}

	if (sched->intc) {
		SCHED_DEBUG("scheduler wakes on interrupt\n");
		sched->intc = 0;
		return 0;
	}

	if (sched->poll) {
		SCHED_DEBUG("scheduler wakes to poll\n");
		return 0;
	}

	if (atomic_read(&sched->check)) {
		SCHED_DEBUG("scheduler wakes on timer\n");
		return 0;
	}

	SCHED_DEBUG("scheduler waits ...\n");
	return 1;
}

/**
 * scheduler_wait() - check if scheduler should wait
 *
 * See sched_wait_cond().
 */
static void
scheduler_wait(struct scheduler *sched)
{
	wait_event_interruptible(sched->wait_queue, !sched_wait_cond(sched));
}

/**
 * scheduler_loop() - Run one loop of the scheduler
 */
static void
scheduler_loop(struct scheduler *sched)
{
	SCHED_DEBUG("-> %s\n", __func__);

	scheduler_wait(sched);

	if (sched->stop) {
		if (sched->error)
			DRM_ERROR("Unexpected error and exits\n");
		return;
	}

	/* queue new pending commands */
	scheduler_queue_cmds(sched);

	/* iterate all commands */
	scheduler_iterate_cmds(sched);

	if (sched_loop_cnt < MAX_SCHED_LOOP)
		sched_loop_cnt++;
	else {
		sched_loop_cnt = 0;
		schedule();
	}
}

/**
 * scheduler() - Command scheduler thread routine
 */
static int
scheduler(void *data)
{
	struct scheduler *sched = (struct scheduler *)data;

	while (!sched->stop)
		scheduler_loop(sched);
	DRM_DEBUG("%s thread exits with value %d\n", __func__, sched->error);
	return sched->error;
}

/**
 * init_scheduler_thread() - Initialize scheduler thread if necessary
 *
 * Return: 0 on success, -errno otherwise
 */
static int
init_scheduler_thread(void)
{
	char name[256] = "zocl-scheduler-thread0";

	SCHED_DEBUG("%s use_count=%d\n", __func__, g_sched0.use_count);
	if (g_sched0.use_count++)
		return 0;

	sched_loop_cnt = 0;

	init_waitqueue_head(&g_sched0.wait_queue);
	g_sched0.error = 0;
	g_sched0.stop = 0;

	INIT_LIST_HEAD(&g_sched0.cq);
	g_sched0.intc = 0;
	g_sched0.poll = 0;
	atomic_set(&g_sched0.check, 0);

	g_sched0.sched_thread = kthread_run(scheduler, &g_sched0, name);
	if (IS_ERR(g_sched0.sched_thread)) {
		int ret = PTR_ERR(g_sched0.sched_thread);

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
fini_scheduler_thread(void)
{
	int retval = 0;

	SCHED_DEBUG("%s use_count=%d\n", __func__, g_sched0.use_count);
	if (--g_sched0.use_count)
		return 0;

	retval = kthread_stop(g_sched0.sched_thread);

	/* clear stale command objects if any */
	reset_all();

	/* reclaim memory for allocate command objects */
	delete_cmd_list();

	return retval;
}

static inline int
dma_done(struct sched_cmd *cmd)
{
	return (cmd->dma_handle.dma_flags & ZOCL_DMA_DONE) != 0;
}

static inline bool
dma_error(struct sched_cmd *cmd)
{
	return (cmd->dma_handle.dma_flags & ZOCL_DMA_ERROR) != 0;
}

static inline void
dma_clear(struct sched_cmd *cmd)
{
	cmd->dma_handle.dma_flags = 0;
}

/**
 * penguin_query() - Check command status of argument command
 *
 * @cmd: Command to check
 *
 * Function is called in penguin mode (no embedded scheduler).
 */
static void
penguin_query(struct sched_cmd *cmd)
{
	u32 opc = opcode(cmd);

	SCHED_DEBUG("-> %s() slot_idx=%d\n", __func__, cmd->slot_idx);
	switch (opc) {
	case ERT_START_COPYBO:
		if (dma_done(cmd)) {
			dma_error(cmd) ?
			    mark_cmd_complete(cmd, ERT_CMD_STATE_ERROR) :
			    mark_cmd_complete(cmd, ERT_CMD_STATE_COMPLETED);
			dma_clear(cmd);
		}
		break;
	case ERT_START_CU:
	case ERT_EXEC_WRITE:
		if (cu_done(cmd))
			mark_cmd_complete(cmd, ERT_CMD_STATE_COMPLETED);
		break;
	case ERT_INIT_CU:
	case ERT_CONFIGURE:
		mark_cmd_complete(cmd, ERT_CMD_STATE_COMPLETED);
		break;
	default:
		DRM_ERROR("unknown opcode %d", opc);
	}
	SCHED_DEBUG("<- %s\n", __func__);
}

/**
 * callback function provided to the dma engine, when dma complete
 * this function will be called. scheduler should have the knowledge
 * to update its internal status.
 */
static void zocl_dma_complete(void *arg, int ret)
{
	struct sched_cmd *cmd = (struct sched_cmd *)arg;

	cmd->dma_handle.dma_flags |= ZOCL_DMA_DONE;
	if (ret != 0)
		cmd->dma_handle.dma_flags |= ZOCL_DMA_ERROR;

	wake_up_interruptible(&cmd->sched->wait_queue);
}

static int
zocl_dma_channel_instance(zocl_dma_handle_t *dma_handle,
	struct drm_zocl_dev *zdev)
{
	dma_cap_mask_t dma_mask;

	if (!dma_handle->dma_chan && ZOCL_PLATFORM_ARM64) {
		/* If zdev_dma_chan is NULL, we haven't initialized it yet. */
		if (!zdev->zdev_dma_chan) {
			dma_cap_zero(dma_mask);
			dma_cap_set(DMA_MEMCPY, dma_mask);
			zdev->zdev_dma_chan =
			    dma_request_channel(dma_mask, 0, NULL);
			if (!zdev->zdev_dma_chan) {
				DRM_WARN("no DMA Channel available.\n");
				return -EBUSY;
			}
		}
		dma_handle->dma_chan = zdev->zdev_dma_chan;
	}

	return dma_handle->dma_chan ? 0 : -EINVAL;
}

static int
zocl_copy_bo_submit(struct sched_cmd *cmd)
{
	struct ert_start_copybo_cmd *ecmd = cmd->ert_cp;
	struct drm_file *filp = (struct drm_file *)ecmd->arg;
	struct drm_device *ddev = cmd->ddev;
	struct drm_zocl_dev *zdev = ddev->dev_private;
	zocl_dma_handle_t *dma_handle = &cmd->dma_handle;
	struct drm_zocl_copy_bo args = {
		.dst_handle = ecmd->dst_bo_hdl,
		.src_handle = ecmd->src_bo_hdl,
		.size = ert_copybo_size(ecmd),
		.dst_offset = ert_copybo_dst_offset(ecmd),
		.src_offset = ert_copybo_src_offset(ecmd),
	};
	int err = 0;

	/* Get single dma channel instance. */
	err = zocl_dma_channel_instance(dma_handle, zdev);
	if (err)
		return err;

	/* We must set up callback for async dma operations. */
	dma_handle->dma_func = zocl_dma_complete;
	dma_handle->dma_arg = cmd;

	return zocl_copy_bo_async(ddev, filp, dma_handle, &args);
}

/**
 * penguin_submit() - penguin submit of a command
 *
 * @cmd: command to submit
 *
 * Special processing for configure and init command.
 * Configuration and initialization are done/called by queued_to_running
 * before calling penguin_submit. In penguin mode configuration and
 * initialization need to ensure that the commands are retired properly by
 * scheduler, so assign them slot indexes and let normal flow continue.
 *
 * Return: %true on successful submit, %false otherwise
 */
static int
penguin_submit(struct sched_cmd *cmd)
{
	SCHED_DEBUG("-> %s\n", __func__);

	/**
	 * copy bo will be handled as ert command via execbuf. If submission
	 * failed, we should bail out and notify polling waiters.
	 */
	if (opcode(cmd) == ERT_START_COPYBO) {
		if (zocl_copy_bo_submit(cmd) != 0) {
			mark_cmd_submit_error(cmd);
			return false;
		}
		cmd->slot_idx = acquire_slot_idx(cmd->ddev);
		return true;
	}

	if (opcode(cmd) == ERT_CONFIGURE) {
		cmd->slot_idx = acquire_slot_idx(cmd->ddev);
		SCHED_DEBUG("<- %s (configure)\n", __func__);
		return true;
	}

	if (opcode(cmd) == ERT_INIT_CU) {
		cmd->slot_idx = acquire_slot_idx(cmd->ddev);
		SCHED_DEBUG("<- %s (init CU)\n", __func__);
		return true;
	}

	if (opcode(cmd) != ERT_START_CU && opcode(cmd) != ERT_EXEC_WRITE)
		return false;

	/* extract cu list */
	cmd->cu_idx = get_free_cu(cmd, ZOCL_HARD_CU);
	if (cmd->cu_idx < 0) {
		if (cmd->cu_idx == -EINVAL)
			mark_cmd_submit_error(cmd);
		return false;
	}

	/* track cu executions */
	++(cmd->exec->zcu[cmd->cu_idx].usage);

	cmd->slot_idx = acquire_slot_idx(cmd->ddev);
	if (cmd->slot_idx < 0)
		return false;

	/* found free cu, transfer regmap and start it */
	configure_cu(cmd, cmd->cu_idx);

	SCHED_DEBUG("<- %s cu_idx=%d slot=%d\n",
	    __func__, cmd->cu_idx, cmd->slot_idx);

	return true;
}

/**
 * penguin_ops: operations for kernel mode scheduling
 */
static struct sched_ops penguin_ops = {
	.submit = penguin_submit,
	.query = penguin_query,
};

/**
 * ps_ert_query() - Check command status of argument command
 *
 * @cmd: Command to check
 *
 * Function is called in penguin mode (no embedded scheduler).
 */
static void
ps_ert_query(struct sched_cmd *cmd)
{
	u32 opc = opcode(cmd);

	SCHED_DEBUG("-> %s() slot_idx=%d\n", __func__, cmd->slot_idx);
	switch (opc) {

	case ERT_SK_CONFIG:
		if (scu_configure_done(cmd))
			mark_cmd_complete(cmd, ERT_CMD_STATE_COMPLETED);
		break;

	case ERT_SK_UNCONFIG:
		if (scu_unconfig_done(cmd))
			mark_cmd_complete(cmd, ERT_CMD_STATE_COMPLETED);
		break;

	case ERT_SK_START:
		if (scu_done(cmd))
			mark_cmd_complete(cmd, ERT_CMD_STATE_COMPLETED);
		break;

	case ERT_START_CU:
	case ERT_EXEC_WRITE:
		if (!cu_done(cmd))
			break;
		/* pass through */

	case ERT_CONFIGURE:
		mark_cmd_complete(cmd, ERT_CMD_STATE_COMPLETED);
		break;

	default:
		DRM_ERROR("unknown opcode %d", opc);
	}
	SCHED_DEBUG("<- %s()", __func__);
}

/**
 * ps_ert_submit() - PS ERT submit of a command
 *
 * @cmd: command to submit
 *
 * Special processing for configure command. Configuration itself is
 * done/called by queued_to_running before calling penguin_submit. In penguin
 * mode configuration need to ensure that the command is retired properly by
 * scheduler, so assign it a slot index and let normal flow continue.
 *
 * Return: %true on successful submit, %false otherwise
 */
static int
ps_ert_submit(struct sched_cmd *cmd)
{
	int ret;

	SCHED_DEBUG("-> %s()", __func__);

	cmd->slot_idx = acquire_slot_idx(cmd->ddev);
	if (cmd->slot_idx < 0)
		return false;

	switch (opcode(cmd)) {
	case ERT_CONFIGURE:
		SCHED_DEBUG("<- %s (configure)\n", __func__);
		break;

	case ERT_SK_CONFIG:
		SCHED_DEBUG("<- %s (configure soft kernel)\n", __func__);
		ret = configure_soft_kernel(cmd);
		if (ret) {
			release_slot_idx(cmd->ddev, cmd->slot_idx);
			if (ret != -ENOMEM)
				mark_cmd_submit_error(cmd);
			return false;
		}
		break;

	case ERT_SK_UNCONFIG:
		SCHED_DEBUG("<- %s (unconfigure soft kernel)\n", __func__);
		if (unconfigure_soft_kernel(cmd)) {
			release_slot_idx(cmd->ddev, cmd->slot_idx);
			mark_cmd_submit_error(cmd);
			return false;
		}
		break;

	case ERT_SK_START:
		cmd->cu_idx = get_free_cu(cmd, ZOCL_SOFT_CU);
		if (cmd->cu_idx < 0) {
			release_slot_idx(cmd->ddev, cmd->slot_idx);
			if (cmd->cu_idx == -EINVAL)
				mark_cmd_submit_error(cmd);
			return false;
		}
		if (ert_configure_scu(cmd, cmd->cu_idx)) {
			release_slot_idx(cmd->ddev, cmd->slot_idx);
			return false;
		}

		SCHED_DEBUG("<- %s() cu_idx=%d slot=%d cq_slot=%d\n",
		    __func__, cmd->cu_idx, cmd->slot_idx, cmd->cq_slot_idx);
		break;

	case ERT_START_CU:
	case ERT_EXEC_WRITE:
		/* extract cu list */
		cmd->cu_idx = get_free_cu(cmd, ZOCL_HARD_CU);
		if (cmd->cu_idx < 0) {
			release_slot_idx(cmd->ddev, cmd->slot_idx);
			if (cmd->cu_idx == -EINVAL)
				mark_cmd_submit_error(cmd);
			return false;
		}

		/* found free cu, transfer regmap and start it */
		ert_configure_cu(cmd, cmd->cu_idx);

		SCHED_DEBUG("<- %s() cu_idx=%d slot=%d cq_slot=%d\n",
		    __func__, cmd->cu_idx, cmd->slot_idx, cmd->cq_slot_idx);
		break;

	default:
		release_slot_idx(cmd->ddev, cmd->slot_idx);
		return false;
	}

	return true;
}

/**
 * ps_ert_ops: operations for ps scheduling
 */
static struct sched_ops ps_ert_ops = {
	.submit = ps_ert_submit,
	.query = ps_ert_query,
};

/**
 * Only process ERT_START_COPYBO command.
 * On MPSoC ARM64 platforms, the DMA engine is not a real HLS CU,
 * thus the cmd->arg is not being used. We use it to preserve the
 * filp.
 */
static bool
zocl_execbuf_to_ert(struct drm_zocl_bo *bo, struct drm_file *filp)
{
	struct ert_start_copybo_cmd *scmd =
	    (struct ert_start_copybo_cmd *)bo->cma_base.vaddr;

	if (scmd->opcode != ERT_START_COPYBO)
		return true;

	if (!ZOCL_PLATFORM_ARM64) {
		DRM_WARN("only support built-in copybo for ARM64");
		return false;
	}

	/* preserve filp for looking up bo */
	scmd->arg = filp;
	return true;
}

/**
 * zocl_dma_check() - Checks whether DMA can be performed or not
 *
 * @dev: Device node calling execbuf
 * @bo: buffer objects from user space from which new command is created
 *
*/
static bool
zocl_dma_check(struct drm_device *dev, struct drm_zocl_bo *bo)
{
	uint64_t dst_paddr, src_paddr;
	struct ert_start_copybo_cmd *cmd =
	  (struct ert_start_copybo_cmd *)bo->cma_base.vaddr;
	struct drm_file *filp = cmd->arg;
	struct drm_zocl_copy_bo args = {
	  .dst_handle = cmd->dst_bo_hdl,
	  .src_handle = cmd->src_bo_hdl,
	  .size = ert_copybo_size(cmd),
	  .dst_offset = ert_copybo_dst_offset(cmd),
	  .src_offset = ert_copybo_src_offset(cmd),
	};

	if (cmd->opcode != ERT_START_COPYBO)
		return true;

	return zocl_can_dma_performed(dev, filp, &args, &dst_paddr, &src_paddr);
}

/**
 * zocl_execbuf_ioctl() - Entry point for exec buffer.
 *
 * @dev: Device node calling execbuf
 * @data: Payload
 * @filp:
 *
 * Function adds exec buffer to the pending list of commands
 *
 * Return: 0 on success, -errno otherwise
 */
int
zocl_execbuf_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_gem_object *gem_obj;
	struct drm_zocl_bo *zocl_bo;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct drm_zocl_execbuf *args = data;
	struct sched_client_ctx *client;
	int ret = 0;

	SCHED_DEBUG("-> %s", __func__);
	gem_obj = zocl_gem_object_lookup(dev, filp, args->exec_bo_handle);
	if (!gem_obj) {
		DRM_ERROR("Look up GEM BO %d failed\n", args->exec_bo_handle);
		return -EINVAL;
	}

	zocl_bo = to_zocl_bo(gem_obj);
	if (!zocl_bo_execbuf(zocl_bo) || !zocl_execbuf_to_ert(zocl_bo, filp)) {
		ret = -EINVAL;
		goto out;
	}

	/* check whether dma can be perfomed or not */
	if (!zocl_dma_check(dev, zocl_bo)) {
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
		return -EOPNOTSUPP;
	}

	/* TODO:
	 * When we start validating CUs, make a client_validate function
	 */
	client = filp->driver_priv;
	if (add_gem_bo_cmd(dev, zocl_bo, client)) {
		ret = -EINVAL;
		goto out;
	}

	SCHED_DEBUG("<- %s", __func__);
	return ret;

out:
	if (zdev->domain)
		drm_gem_cma_free_object(&zocl_bo->gem_base);
	else
		drm_gem_cma_free_object(&zocl_bo->cma_base.base);
	return ret;
}

struct ert_packet *
get_next_packet(struct ert_packet *packet, unsigned int size)
{
	char *bytes = (char *)packet;

	return (struct ert_packet *)(bytes+size);
}

void
zocl_cmd_buffer_free(struct sched_cmd *cmd)
{
	SCHED_DEBUG("-> %s", __func__);
	kfree(cmd->buffer);
	SCHED_DEBUG("<- %s", __func__);
}

static unsigned int
get_packet_size(struct ert_packet *packet)
{
	unsigned int payload;

	SCHED_DEBUG("-> %s", __func__);
	switch (packet->opcode) {

	case ERT_CONFIGURE:
		SCHED_DEBUG("configure cmd");
		payload = 5 + packet->count;
		break;

	case ERT_SK_CONFIG:
		SCHED_DEBUG("configure soft kernel cmd");
		payload = packet->count;
		break;

	case ERT_SK_UNCONFIG:
		SCHED_DEBUG("unconfigure soft kernel cmd");
		payload = packet->count;
		break;

	case ERT_SK_START:
		SCHED_DEBUG("start Soft CU/Kernel cmd");
		payload = packet->count;
		break;

	case ERT_START_CU:
		SCHED_DEBUG("start CU/Kernel cmd");
		payload = packet->count;
		break;

	case ERT_EXIT:
	case ERT_ABORT:
		SCHED_DEBUG("abort or stop cmd");

	default:
		payload = 0;
	}

	SCHED_DEBUG("<- %s", __func__);
	return 1 + payload;
}

/*
 * add_ert_cq_cmd() - add a command by ERT command queue
 *
 * @ddev: drm device owning adding the buffer object
 * @buffer: buffer
 * @cq_idx: index of the CQ slot in BRAM
 *
 * Get a free scheduler command and initial it by gem buffer object.
 * After all, add this command to pending list.
 *
 * Return: 0 on success, -errno on failure
 */
static int
add_ert_cq_cmd(struct drm_device *drm, void *buffer, unsigned int cq_idx)
{
	struct sched_cmd *cmd = get_free_sched_cmd();
	struct drm_zocl_dev *zdev = drm->dev_private;
	int ret;

	SCHED_DEBUG("-> %s", __func__);
	cmd->ddev = drm;
	cmd->sched = zdev->exec->scheduler;
	cmd->buffer = buffer;
	cmd->packet = buffer;
	cmd->exec = zdev->exec;
	cmd->cq_slot_idx = cq_idx;
	cmd->free_buffer = zocl_cmd_buffer_free;

	ret = add_cmd(cmd);

	SCHED_DEBUG("<- %s", __func__);
	return ret;
}

/**
 * create_cmd_buffer() - Create cmd command buffer of packet if state is NEW
 *
 * @packet:    Scheduler packet from ERT Command Queue
 * @slot_size: slot_size
 *
 * Return: buffer pointer
 */
static void*
create_cmd_buffer(struct ert_packet *packet, unsigned int slot_size)
{
	void *buffer;
	size_t size;

	if (packet->state != ERT_CMD_STATE_NEW)
		return ERR_PTR(-EAGAIN);

	packet->state = ERT_CMD_STATE_QUEUED;
	SCHED_DEBUG("packet header 0x%8x, packet addr 0x%p slot size %d",
		    packet->header, packet, slot_size);
	buffer = kzalloc(slot_size, GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);
	/**
	 * In 2018.2, CQ BRAM is used. Access PL by AXI lite is expense,
	 * so copy packet to PS DDR. If host can directly submit command
	 * to PS DDR, we don't need this copy.
	 */
	size = get_packet_size(packet) * sizeof(uint32_t);
	memcpy_fromio(buffer, packet, size);
	return buffer;
}

/**
 * iterate_packets() - iterate packets in HW Command Queue
 *
 * @drm:  DRM device
 *
 * Return: errno
 */
static int
iterate_packets(struct drm_device *drm)
{
	struct drm_zocl_dev *zdev = drm->dev_private;
	struct zocl_ert_dev *ert = zdev->ert;
	struct sched_exec_core *exec_core = zdev->exec;
	struct ert_packet *packet;
	unsigned int slot_idx, num_slots, slot_sz;
	void *buffer;
	int ret;

	packet = ert->cq_ioremap;
	num_slots = exec_core->num_slots;
	slot_sz = slot_size(zdev->ddev);

	for (slot_idx = 0; slot_idx < num_slots; slot_idx++) {
		buffer = create_cmd_buffer(packet, slot_sz);
		packet = get_next_packet(packet, slot_sz);
		if (IS_ERR(buffer))
			continue;

		if (add_ert_cq_cmd(zdev->ddev, buffer, slot_idx)) {
			ret = -EINVAL;
			goto err;
		}
	}

	return 0;
err:
	kfree(buffer);
	return ret;
}

/**
 * cq_check() - Check CQ status and submit new command to KDS
 *
 * @data: drm zocl device
 *
 * Iterate CQ BRAM for new command.
 *
 * Return: 0 on success, -errno on failure
 */
static int
cq_check(void *data)
{
	struct drm_zocl_dev *zdev = data;
	struct sched_exec_core *exec_core = zdev->exec;

	SCHED_DEBUG("-> %s", __func__);
	while (!kthread_should_stop() && !exec_core->cq_interrupt) {
		iterate_packets(zdev->ddev);
		schedule();
	}
	SCHED_DEBUG("<- %s", __func__);
	return 0;
}

static irqreturn_t sched_cq_isr(int irq, void *arg)
{
	struct drm_zocl_dev *zdev = arg;
	struct ert_packet *pkg;
	int slot_sz, slot_idx = 0;
	int good_pkg;
	void *buffer;

	SCHED_DEBUG("-> %s", __func__);

	good_pkg = 1;
	slot_sz = slot_size(zdev->ddev);
	pkg = zdev->ert->ops->get_next_cmd(zdev->ert, NULL, &slot_idx);
	while (pkg) {
		/* Usually, if the status of the pkg is not NEW. We think it is
		 * not 'good' at this point.
		 */
		buffer = create_cmd_buffer(pkg, slot_sz);
		if (IS_ERR(buffer))
			good_pkg = 0;

		if (good_pkg)
			if (add_ert_cq_cmd(zdev->ddev, buffer, slot_idx))
				kfree(buffer);

		pkg = zdev->ert->ops->get_next_cmd(zdev->ert, pkg, &slot_idx);
		/* No harm to assume the next pkg is good */
		good_pkg = 1;
	}

	SCHED_DEBUG("<- %s", __func__);
	return IRQ_HANDLED;
}

static inline void init_exec(struct sched_exec_core *exec_core)
{
	unsigned int i;

	exec_core->scheduler = &g_sched0;
	exec_core->num_slots = 16;
	exec_core->num_cus = 0;
	exec_core->cu_base_addr = 0;
	exec_core->cu_shift_offset = 0;
	exec_core->polling_mode = 1;
	exec_core->cq_interrupt = 0;
	exec_core->configured = 0;
	exec_core->cu_isr = 0;
	exec_core->cu_dma = 0;
	exec_core->num_slot_masks = 1;
	exec_core->num_cu_masks = 0;
	exec_core->ops = &penguin_ops;

	for (i = 0; i < MAX_SLOTS; ++i)
		exec_core->submitted_cmds[i] = NULL;

	for (i = 0; i < MAX_U32_SLOT_MASKS; ++i)
		exec_core->slot_status[i] = 0;

	for (i = 0; i < MAX_U32_CU_MASKS; ++i) {
		exec_core->cu_status[i] = 0;
		exec_core->cu_init[i] = 0;
		exec_core->cu_valid[i] = 0; /* default value is invalid(0) */
	}
}

/**
 * sched_init_exec() - Initialize the command execution for device
 *
 * @drm: Device node to initialize
 *
 * Return: 0 on success, -errno otherwise
 */
int
sched_init_exec(struct drm_device *drm)
{
	struct sched_exec_core *exec_core;
	struct drm_zocl_dev *zdev = drm->dev_private;
	unsigned int i;
	char name[256] = "zocl-ert-thread";

	SCHED_DEBUG("-> %s\n", __func__);

	exec_core = devm_kzalloc(drm->dev, sizeof(*exec_core), GFP_KERNEL);
	if (!exec_core)
		return -ENOMEM;

	zdev->exec = exec_core;
	spin_lock_init(&exec_core->ctx_list_lock);
	INIT_LIST_HEAD(&exec_core->ctx_list);
	init_waitqueue_head(&exec_core->poll_wait_queue);

	init_exec(exec_core);
	init_scheduler_thread();

	if (zdev->ert) {
		for (i = 0; i < MAX_U32_CU_MASKS; ++i)
			exec_core->scu_status[i] = 0;

		/* Initialize soft kernel */
		zocl_init_soft_kernel(drm);

		exec_core->cq_thread = kthread_run(cq_check, zdev, name);
	}

	SCHED_DEBUG("<- %s\n", __func__);
	return 0;
}

/*
 * fini_configure() - clean up configure() specific data.
 *    As a matter of fact, the configure() will potentially request_irq for
 *    both KDS and ERT. The other configuration data can be easily reset to 0 or
 *    initial value, but for requested interrupt resources, we should recycle
 *    them by free_irq.
 */
static inline void
fini_configure(struct drm_device *drm)
{
	struct drm_zocl_dev *zdev = drm->dev_private;
	unsigned int i;

	if (!(zdev->ert || zdev->exec->polling_mode)) {
		for (i = 0; i < zdev->exec->num_cus; i++) {
			if (zocl_cu_is_valid(zdev->exec, i))
				free_irq(zdev->irq[i], zdev);
		}
	}

	if (zdev->exec->cq_interrupt)
		free_irq(zdev->ert->irq[ERT_CQ_IRQ], zdev);
}

/**
 * sched_fini_exec() - Finalize the command execution for device
 *
 * @xdev: Device node to finalize
 *
 * Return: 0 on success, -errno otherwise
 */
int sched_fini_exec(struct drm_device *drm)
{
	struct drm_zocl_dev *zdev = drm->dev_private;

	SCHED_DEBUG("-> %s\n", __func__);

	fini_configure(drm);

	if (zdev->exec->cq_thread)
		kthread_stop(zdev->exec->cq_thread);

	fini_scheduler_thread();
	vfree(zdev->exec->zcu);
	zocl_cleanup_cu_timer(zdev);
	SCHED_DEBUG("<- %s\n", __func__);

	return 0;
}

/*
 * Block comment for the resetting theory.
 *
 * Note:
 *   1) Run as a transaction:
 *      Done or rollback to original config.
 *   2) Can retry:
 *      Suppose user understands switching xclbin depends on previous
 *      xclbin is not being used. Some stale cmds might not be drained safely
 *      in XRT, it is safter to check if hardware is hung too. For example:
 *      if hls cu status is 0x1, we don't know if this is going to be
 *      finished. In the future, once hls cu reset is fully supported, we
 *      might consider enabling CU reset to accelate the procedure of
 *      draining running CUs.
 *
 * Reset stages:
 *   Stage 1: stop scheduler, no more new cmds. Wait for existing cmds
 *            to be drained.
 *   Stage 2: flush scheduler (see cmd_update_state),
 *         2.1: any pending cmds should be set to abort.
 *         2.2: any running cmds should be set to:
 *           2.2.1: abort when cu idx is -1.
 *           2.2.2: abort cu status is not ap_start.
 *           2.2.3: retry running cu for certain time.
 *              if still running, return EBUSY.
 *              if completed, continue.
 *         3: fini, init scheduler
 *         4: start scheduler, accept adding cmds
 */
int
sched_reset_exec(struct drm_device *drm)
{
	struct drm_zocl_dev *zdev = drm->dev_private;
	struct sched_exec_core *exec = zdev->exec;
	int wait_ms = 100;
	int retry = 20;
	int outstanding = 0;

	DRM_INFO("%s: stop scheduler", __func__);

	/* Once stopped, keep this status until reset done */
	atomic_set(&exec->exec_status, ZOCL_EXEC_STOP);

	outstanding = atomic_read(&num_pending);
	while (retry-- && outstanding) {
		DRM_INFO("Wait for (%d) pending cmds to finish", outstanding);
		msleep(wait_ms);
		outstanding = atomic_read(&num_pending);
	}

	retry = 20;
	outstanding = atomic_read(&num_running);
	while (retry-- && outstanding) {
		DRM_INFO("Wait for (%d) pending cmds to finish", outstanding);
		msleep(wait_ms);
		outstanding = atomic_read(&num_running);
	}

	/*
	 * Set scheduler to flash status, so that abormal cmds will be
	 * aborted. If there are still outstanding commands, return EBUSY.
	 * User should deal with potential hung or long time running CUs.
	 */
	if (atomic_read(&num_pending) || atomic_read(&num_running)) {
		atomic_set(&exec->exec_status, ZOCL_EXEC_FLUSH);
		msleep(1000); /* wait a second */
	}

	if (atomic_read(&num_pending) || atomic_read(&num_running)) {
		/* set back to normal, user can retry next time */
		DRM_WARN("Still have pending(%d), running(%d) cmds",
		    atomic_read(&num_pending), atomic_read(&num_running));
		atomic_set(&exec->exec_status, ZOCL_EXEC_NORMAL);
		return -EBUSY;
	}

	/*
	 * When scheduler is stopped, reset exec_core.
	 * 1) cleanup additional irqs from configure
	 * 2) reset exec initial value
	 *
	 * Note: no need to check stale commands, becuase we return EBUSY above
	 * for this condition.
	 */
	fini_configure(drm);
	init_exec(exec);
	/* start receiving cmds */
	atomic_set(&exec->exec_status, ZOCL_EXEC_NORMAL);
	return 0;
}

u32
sched_is_busy(struct drm_zocl_dev *zdev)
{
	return (atomic_read(&num_pending) + atomic_read(&num_running));
}

/*
 * Return number of client with open ("live") contexts on CUs.
 * If this number > 0, xclbin is locked down.
 * If plist is non-NULL, the list of PIDs of live clients will also be
 * returned.
 * Note that plist should be freed by caller.
 */
u32
sched_live_clients(struct drm_zocl_dev *zdev, pid_t **plist)
{
	const struct list_head *ptr;
	const struct sched_client_ctx *entry;
	uint count = 0;
	u32 i = 0;
	pid_t *pl = NULL;

	/* Find out number of active client */
	list_for_each(ptr, &zdev->exec->ctx_list) {
		entry = list_entry(ptr, struct sched_client_ctx, link);
		if (CLIENT_NUM_CU_CTX(entry) > 0)
			count++;
	}
	if (count == 0 || plist == NULL)
		goto out;

	/* Collect list of PIDs of active client */
	pl = vmalloc(sizeof(pid_t) * count);
	if (pl == NULL)
		goto out;

	list_for_each(ptr, &zdev->exec->ctx_list) {
		entry = list_entry(ptr, struct sched_client_ctx, link);
		if (CLIENT_NUM_CU_CTX(entry) > 0) {
			pl[i] = pid_nr(entry->pid);
			i++;
		}
	}

	*plist = pl;

out:
	return count;
}


void zocl_track_ctx(struct drm_device *dev, struct sched_client_ctx *fpriv)
{
	unsigned long flags;
	struct drm_zocl_dev *zdev = dev->dev_private;

	spin_lock_irqsave(&zdev->exec->ctx_list_lock, flags);
	list_add_tail(&fpriv->link, &zdev->exec->ctx_list);
	spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, flags);
}

void zocl_untrack_ctx(struct drm_device *dev, struct sched_client_ctx *fpriv)
{
	unsigned long flags;
	struct drm_zocl_dev *zdev = dev->dev_private;

	spin_lock_irqsave(&zdev->exec->ctx_list_lock, flags);
	list_del(&fpriv->link);
	spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, flags);
}
