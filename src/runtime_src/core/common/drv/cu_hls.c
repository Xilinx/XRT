/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx HLS CU
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/delay.h>
#include "xrt_cu.h"
#include "xgq_cmd_ert.h"

/* Control register bits and special behavior if any.
 * Bit 0: ap_start(Read/Set). Clear by CU when ap_ready assert.
 * Bit 1: ap_done(Read only). Clear on read.
 * Bit 2: ap_idle(Read only).
 * Bit 3: ap_ready(Read only). Self clear after clear ap_start.
 * Bit 4: ap_continue(Read/Set). Self clear.
 * Bit 5-7: Not support yet
 * Bit 8: ap_sw_reset. Clear when reset is done.
 */
#define CTRL		0x0
/* Global interrupt enable: Set bit 0 to enable. Clear it to disable */
#define GIE		0x4
/* Interrupt Enable Register
 * Bit 0: ap_done. 0 - disable; 1 - enable.
 * Bit 1: ap_ready. 0 - disable; 1 - enable.
 */
#define IER		0x8
/* Interrupt Status Register
 * Bit 0: ap_done(Toggle on set).
 * Bit 1: ap_done(Toggle on set).
 *   Toggle on set - Write 1 would flip the bit. Write 0 has no change.
 */
#define ISR		0xC
#define ARGS		0x10

extern int kds_echo;

struct xrt_cu_hls {
	void __iomem		*vaddr;
	int			 max_credits;
	int			 credits;
	int			 run_cnts;
	bool			 ctrl_chain;
	bool			 sw_reset;
	spinlock_t		 cu_lock;
	u32			 done;
	u32			 ready;
	struct list_head	 submitted;
	struct list_head	 completed;
};

static inline u32 cu_read32(struct xrt_cu_hls *cu, u32 reg)
{
	u32 ret;

	ret = ioread32(cu->vaddr + reg);
	return ret;
}

static inline void cu_write32(struct xrt_cu_hls *cu, u32 reg, u32 val)
{
	iowrite32(val, cu->vaddr + reg);
}

static inline void cu_move_to_complete(struct xrt_cu_hls *cu, int status)
{
	struct kds_command *xcmd = NULL;

	if (unlikely(list_empty(&cu->submitted)))
		return;

	xcmd = list_first_entry(&cu->submitted, struct kds_command, list);
	xcmd->status = status;
	cu->run_cnts--;
	list_move_tail(&xcmd->list, &cu->completed);
}

static int cu_hls_alloc_credit(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	return (cu_hls->credits) ? cu_hls->credits-- : 0;
}

static void cu_hls_free_credit(void *core, u32 count)
{
	struct xrt_cu_hls *cu_hls = core;

	cu_hls->credits += count;
	if (cu_hls->credits > cu_hls->max_credits)
		cu_hls->credits = cu_hls->max_credits;
}

static int cu_hls_peek_credit(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	return cu_hls->credits;
}

static void cu_hls_xgq_start(struct xrt_cu_hls *cu_hls, u32 *data)
{
	struct xgq_cmd_start_cuidx *cmd = (struct xgq_cmd_start_cuidx *)data;
	u32 num_reg = 0;
	u32 i = 0;

	num_reg = (cmd->hdr.count - (sizeof(struct xgq_cmd_start_cuidx)
				     - sizeof(cmd->hdr) - sizeof(cmd->data)))/sizeof(u32);
	for (i = 0; i < num_reg; ++i) {
		cu_write32(cu_hls, ARGS + i * 4, cmd->data[i]);
	}
}

static void cu_hls_xgq_start_kv(struct xrt_cu_hls *cu_hls, u32 *data)
{
	struct xgq_cmd_start_cuidx_kv *cmd = (struct xgq_cmd_start_cuidx_kv *)data;
	u32 num_reg = 0;
	u32 i = 0;

	num_reg = (cmd->hdr.count - (sizeof(struct xgq_cmd_start_cuidx_kv)
				     - sizeof(cmd->hdr) - sizeof(cmd->data)))/sizeof(u32);
	/* data is a {offset : value} pairs list
	 * cmd->data[i] -> offset
	 * cmd->data[i+1] -> value
	 */
	for (i = 0; i < num_reg; i += 2)
		cu_write32(cu_hls, cmd->data[i], cmd->data[i + 1]);
}

static int cu_hls_configure(void *core, u32 *data, size_t sz, int type)
{
	struct xrt_cu_hls *cu_hls = core;
	struct xgq_cmd_sq_hdr *hdr;
	size_t num_reg = 0;
	u32 i = 0;

	if (kds_echo)
		return 0;

	num_reg = sz / sizeof(u32);
	switch (type) {
	case REGMAP:
		/* Write register map, starting at base_addr + 0x10 (byte) */
		for (i = 0; i < num_reg; ++i)
			cu_write32(cu_hls, ARGS + i * 4, data[i]);
		break;
	case KEY_VAL:
		/* Use {offset, value} pairs to configure CU
		 * data[i]: register offset
		 * data[i + 1]: value
		 */
		for (i = 0; i < num_reg; i += 2)
			cu_write32(cu_hls, data[i], data[i + 1]);
		break;
	case XGQ_CMD:
		hdr = (struct xgq_cmd_sq_hdr *)data;
		if (hdr->opcode == XGQ_CMD_OP_START_CUIDX)
			cu_hls_xgq_start(cu_hls, data);
		else if (hdr->opcode == XGQ_CMD_OP_START_CUIDX_KV)
			cu_hls_xgq_start_kv(cu_hls, data);
		else
			return -EINVAL;
		break;
	}
	return 0;
}

static void cu_hls_start(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	cu_hls->run_cnts++;

	if (kds_echo)
		return;

	cu_write32(cu_hls, CTRL, CU_AP_START);
}

/*
 * In ap_ctrl_hs protocol, HLS CU can run one task at a time. Once CU is
 * started, software should wait for CU done before configure/start CU again.
 * The done bit is clear on read. So, the software just need to read control
 * register.
 */
static inline void
cu_hls_ctrl_hs_check(struct xrt_cu_hls *cu_hls, struct xcu_status *status, bool force)
{
	u32 ctrl_reg = 0;
	u32 done_reg = 0;
	u32 ready_reg = 0;

	/* Avoid access CU register unless we do have running commands.
	 * This has a huge impact on performance.
	 */
	if (!force && !cu_hls->run_cnts)
		return;

	ctrl_reg = cu_read32(cu_hls, CTRL);
	/* ap_ready and ap_done would assert at the same cycle */
	if (ctrl_reg & CU_AP_DONE) {
		done_reg  = 1;
		ready_reg = 1;
		cu_move_to_complete(cu_hls, KDS_COMPLETED);
	}

	status->num_done  = done_reg;
	status->num_ready = ready_reg;
	status->new_status = ctrl_reg;
}

/*
 * In ap_ctrl_check protocol, HLS CU can setup next task before CU is done.
 * After CU started, the start bit will keep high until CU assert ap_ready.
 * Once a CU is ready, it means CU is ready to be configured.
 * If CU is done, it means the previous task is completed. But the CU would
 * stall until ap_continue bit is set.
 */
static inline void
cu_hls_ctrl_chain_check(struct xrt_cu_hls *cu_hls, struct xcu_status *status, bool force)
{
	u32 ctrl_reg = 0;
	u32 done_reg = 0;
	u32 ready_reg = 0;
	u32 used_credit;
	unsigned long flags;

	used_credit = cu_hls->max_credits - cu_hls->credits;

	/* Access CU when there are unsed credits or running commands
	 * This has a huge impact on performance.
	 */
	if (!force && !used_credit && !cu_hls->run_cnts)
		return;

	/* HLS ap_ctrl_chain reqiured software to set ap_continue before
	 * clear interrupt. Otherwise, the clear would failed.
	 * So, we have to make ap_continue and clear interrupt as atomic.
	 */
	spin_lock_irqsave(&cu_hls->cu_lock, flags);
	done_reg  = cu_hls->done;
	ready_reg = cu_hls->ready;
	cu_hls->done = 0;
	cu_hls->ready = 0;

	ctrl_reg = cu_read32(cu_hls, CTRL);

	/* if there is submitted tasks, then check if ap_start bit is clear
	 * See comments in cu_hls_start().
	 */
	if (!ready_reg && used_credit && !(ctrl_reg & CU_AP_START))
		ready_reg = 1;

	if (ctrl_reg & CU_AP_DONE) {
		done_reg += 1;
		cu_write32(cu_hls, CTRL, CU_AP_CONTINUE);
		cu_move_to_complete(cu_hls, KDS_COMPLETED);
	}
	spin_unlock_irqrestore(&cu_hls->cu_lock, flags);

	status->num_done  = done_reg;
	status->num_ready = ready_reg;
	status->new_status = ctrl_reg;
}

static void cu_hls_check(void *core, struct xcu_status *status, bool force)
{
	struct xrt_cu_hls *cu_hls = core;

	if (kds_echo) {
		status->num_done = 1;
		status->num_ready = 1;
		status->new_status = CU_AP_IDLE;
		cu_move_to_complete(cu_hls, KDS_COMPLETED);
		return;
	}

	if (cu_hls->ctrl_chain)
		cu_hls_ctrl_chain_check(cu_hls, status, force);
	else
		cu_hls_ctrl_hs_check(cu_hls, status, force);
}

static void cu_hls_enable_intr(void *core, u32 intr_type)
{
	struct xrt_cu_hls *cu_hls = core;

	cu_write32(cu_hls, GIE, 0x1);
	cu_write32(cu_hls, IER, intr_type);
}

static void cu_hls_disable_intr(void *core, u32 intr_type)
{
	struct xrt_cu_hls *cu_hls = core;
	u32 orig = cu_read32(cu_hls, IER);
	/* if bit 0 of intr_type is set, it means disable ap_done,
	 * same to bit 1 for ap_ready.
	 */
	u32 new = orig & (~intr_type);

	cu_write32(cu_hls, GIE, 0x0);
	cu_write32(cu_hls, IER, new);
}

static u32 cu_hls_clear_intr(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	/* Clear all interrupts of the CU
	 *
	 * HLS style kernel has Interrupt Status Register at offset 0x0C
	 * It has two interrupt bits, bit[0] is ap_done, bit[1] is ap_ready.
	 *
	 * The ap_done interrupt means this CU is complete.
	 * The ap_ready interrupt means all inputs have been read.
	 */
	if (cu_hls->max_credits == 1) {
		u32 isr;
		u32 ctrl_reg = 0;
		unsigned long flags;

		/*
		 * The old HLS adapter.
		 *
		 * The Interrupt Status Register is Toggle On Write
		 * RegData = RegData ^ WriteData
		 *
		 * So, the reliable way to clear this register is read
		 * then write the same value back.
		 *
		 * Do not write 1 to this register.  If, somehow, the
		 * status register is 0, write 1 to this register will
		 * trigger interrupt.
		 */
		isr = cu_read32(cu_hls, ISR);

		/* See comment in cu_hls_ctrl_chain_check() */
		if (cu_hls->ctrl_chain) {
			spin_lock_irqsave(&cu_hls->cu_lock, flags);
			if (isr & CU_INTR_READY)
				cu_hls->ready++;

			if (isr & CU_INTR_DONE) {
				ctrl_reg = cu_read32(cu_hls, CTRL);
				if (ctrl_reg & CU_AP_DONE) {
					cu_hls->done++;
					cu_move_to_complete(cu_hls, KDS_COMPLETED);
					cu_write32(cu_hls, CTRL, CU_AP_CONTINUE);
				}
			}
			spin_unlock_irqrestore(&cu_hls->cu_lock, flags);
		}

		cu_write32(cu_hls, ISR, isr);
		return isr;
	}

	/*
	 * The new HLS adapter with queue.
	 *
	 * The Interrupt Status Register is Clear on Read.
	 *
	 * For debug purpose, This register is toggle on write.
	 * Write 1 to this register will trigger interrupt.
	 */
	return cu_read32(cu_hls, ISR);
}

static void cu_hls_reset(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	cu_write32(cu_hls, CTRL, CU_AP_SW_RESET);
}

static bool cu_hls_reset_done(void *core)
{
	struct xrt_cu_hls *cu_hls = core;
	u32 ctrl_reg;

	ctrl_reg = cu_read32(cu_hls, CTRL);
	if (ctrl_reg & CU_AP_SW_RESET)
		return false;

	return true;
}

static int cu_hls_submit_config(void *core, struct kds_command *xcmd)
{
	struct xrt_cu_hls *cu_hls = core;
	int ret = 0;

	ret = cu_hls_configure(core, (u32 *)xcmd->info, xcmd->isize, xcmd->payload_type);
	if (ret)
		return ret;

	list_move_tail(&xcmd->list, &cu_hls->submitted);
	return ret;
}

static struct kds_command *cu_hls_get_complete(void *core)
{
	struct xrt_cu_hls *cu_hls = core;
	struct kds_command *xcmd = NULL;

	if (list_empty(&cu_hls->completed))
		return NULL;

	xcmd = list_first_entry(&cu_hls->completed, struct kds_command, list);
	list_del_init(&xcmd->list);

	return xcmd;
}

static int cu_hls_abort(void *core, void *cond,
			bool (*match)(struct kds_command *xcmd, void *cond))
{
	struct xrt_cu_hls *cu_hls = core;
	struct kds_command *xcmd = NULL;
	struct kds_command *next = NULL;
	int ret = -EBUSY;

	if (cu_hls->sw_reset) {
		int time = 10 * 1000 * 1000; /* 10 second */
		cu_hls_reset(core);

		do {
			usleep_range(1000, 1500);
			time -= 1000;
			if (cu_hls_reset_done(core))
				break;
		} while (time > 0);

		/* Reset is done, CU is still functional */
		if (time >= 0) {
			cu_hls->credits = cu_hls->max_credits;
			ret = 0;
		}
	}

	list_for_each_entry_safe(xcmd, next, &cu_hls->submitted, list) {
		if (!match(xcmd, cond))
			continue;

		xcmd->status = KDS_TIMEOUT;
		list_move_tail(&xcmd->list, &cu_hls->completed);
	}

	return ret;
}

static struct xcu_funcs xrt_cu_hls_funcs = {
	.alloc_credit	= cu_hls_alloc_credit,
	.free_credit	= cu_hls_free_credit,
	.peek_credit	= cu_hls_peek_credit,
	.configure	= cu_hls_configure,
	.start		= cu_hls_start,
	.check		= cu_hls_check,
	.enable_intr	= cu_hls_enable_intr,
	.disable_intr	= cu_hls_disable_intr,
	.clear_intr	= cu_hls_clear_intr,
	.reset		= cu_hls_reset,
	.reset_done	= cu_hls_reset_done,
	.submit_config  = cu_hls_submit_config,
	.get_complete   = cu_hls_get_complete,
	.abort		= cu_hls_abort,
};

int xrt_cu_hls_init(struct xrt_cu *xcu)
{
	struct xrt_cu_hls *core;
	struct resource *res;
	size_t size;
	int err = 0;

	core = kzalloc(sizeof(struct xrt_cu_hls), GFP_KERNEL);
	if (!core) {
		err = -ENOMEM;
		goto err;
	}

	/* map CU register */
	/* TODO: add comment why uses res[0] */
	res = xcu->res[0];
	size = res->end - res->start + 1;
	core->vaddr = ioremap_nocache(res->start, size);
	if (!core->vaddr) {
		err = -ENOMEM;
		xcu_err(xcu, "Map CU register failed");
		goto err;
	}

	core->max_credits = 1;
	core->credits = core->max_credits;
	core->run_cnts = 0;
	core->ctrl_chain = (xcu->info.protocol == CTRL_CHAIN)? true : false;
	spin_lock_init(&core->cu_lock);
	core->done = 0;
	core->ready = 0;
	core->sw_reset = xcu->info.sw_reset;
	INIT_LIST_HEAD(&core->submitted);
	INIT_LIST_HEAD(&core->completed);

	xcu->core = core;
	xcu->funcs = &xrt_cu_hls_funcs;

	xcu->busy_threshold = -1;
	xcu->interval_min = 2;
	xcu->interval_max = 5;
	mutex_init(&xcu->read_regs.xcr_lock);

	/* No control and interrupt registers in ap_ctrl_none protocol.
	 * In this case, return here for creating CU sub-dev. No need to setup
	 * CU thread and queues.
	 */
	if (xcu->info.protocol == CTRL_NONE)
		return  0;

	xcu->status = cu_read32(core, CTRL);
	err = xrt_cu_init(xcu);
	if (err)
		return err;

	return 0;

err:
	kfree(core);
	return err;
}

void xrt_cu_hls_fini(struct xrt_cu *xcu)
{
	struct xrt_cu_hls *core = xcu->core;

	if (xcu->info.protocol != CTRL_NONE)
		xrt_cu_fini(xcu);

	if (xcu->core) {
		if (core->vaddr)
			iounmap(core->vaddr);
		kfree(xcu->core);
	}
}
