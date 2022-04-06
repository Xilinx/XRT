/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx CU with fast adapter.
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "xrt_cu.h"

/* Fast Adapter register layout
 * nextDescriptorAddr_MSW: Most significant word of paddr of descriptor
 * nextDescriptorAddr_LSW: Least significant word of paddr of descriptor
 * interruptStatus: Interrupt status register
 * Status: Flagging error conditions, e.g. FIFO overrun
 * taskCount: Task Counter that tracks how many tasks have been processed
 * currentDescriptorAddr: Address of descriptor currently being processed by CU
 * fifoDepth: Current adapter FIFO depth
 */
#define MSWR		0x0
#define LSWR		0x4
#define ISR		0x8
#define IER		0xC
#define SR		0x10
#define TCR		0x14
#define CDAR		0x18
#define FDR		0x1C

#define ENABLE  1
#define DISABLE 0

extern int kds_echo;

static inline u32 cu_read32(struct xrt_cu_fa *cu, u32 reg)
{
	u32 ret;

	ret = ioread32(cu->vaddr + reg);
	return ret;
}

static inline void cu_write32(struct xrt_cu_fa *cu, u32 reg, u32 val)
{
	iowrite32(val, cu->vaddr + reg);
}

static inline void cu_move_to_complete(struct xrt_cu_fa *cu, int status)
{
	struct kds_command *xcmd = NULL;

	if (unlikely(list_empty(&cu->submitted)))
		return;

	xcmd = list_first_entry(&cu->submitted, struct kds_command, list);
	xcmd->status = status;

	list_move_tail(&xcmd->list, &cu->completed);
}

static int cu_fa_alloc_credit(void *core)
{
	struct xrt_cu_fa *cu_fa = core;

	return (cu_fa->credits) ? cu_fa->credits-- : 0;
}

static void cu_fa_free_credit(void *core, u32 count)
{
	struct xrt_cu_fa *cu_fa = core;
	int max_credits;

	cu_fa->credits += count;

	max_credits = (cu_fa->num_slots < cu_fa->max_credits)?
		       cu_fa->num_slots : cu_fa->max_credits;

	if (cu_fa->credits > max_credits)
		cu_fa->credits = max_credits;
}

static int cu_fa_peek_credit(void *core)
{
	struct xrt_cu_fa *cu_fa = core;

	return cu_fa->credits;
}

static int cu_fa_configure(void *core, u32 *data, size_t sz, int type)
{
	struct xrt_cu_fa *cu_fa = core;
	u32 *slot = cu_fa->cmdmem + cu_fa->head_slot;

	/* in case cmdmem is not update properly */
	WARN_ON(!cu_fa->cmdmem);

	if (kds_echo || !cu_fa->cmdmem)
		return 0;

	/* move commands to device quickly is the key of performance */
	memcpy(&slot[1], &data[1], sz - 4);

	/* Update status of descriptor */
	wmb();
	slot[0] = data[0];
	return 0;
}

static void cu_fa_start(void *core)
{
	struct xrt_cu_fa *cu_fa = core;
	u32 desc_msw = cu_fa->paddr >> 32;
	u32 desc_lsw = (u32)cu_fa->paddr;

	cu_fa->run_cnts++;

	if (kds_echo || !cu_fa->cmdmem)
		return;

	/* The MSW of descriptor is fixed */
	if (desc_msw != cu_fa->desc_msw) {
		cu_write32(cu_fa, MSWR, desc_msw);
		cu_fa->desc_msw = desc_msw;
	}

	/* Write LSW would kick off CU */
	cu_write32(cu_fa, LSWR, desc_lsw + cu_fa->head_slot);

	/* move to next descriptor slot */
	cu_fa->head_slot += cu_fa->slot_sz;
	if (cu_fa->head_slot == cu_fa->slot_sz * cu_fa->num_slots)
		cu_fa->head_slot = 0;
}

static void cu_fa_check(void *core, struct xcu_status *status, bool force)
{
	struct xrt_cu_fa *cu_fa = core;
	u32 task_count = 0;
	u32 done = 0;
	int i = 0;

	if (kds_echo || !cu_fa->cmdmem) {
		cu_fa->run_cnts--;
		status->num_done = 1;
		status->num_ready = 1;
		return;
	}

	/* Avoid access CU register unless we do have running commands.
	 * This has a huge impact on performance.
	 */
	if (!force && !cu_fa->run_cnts)
		return;

	cu_fa->check_count++;
	task_count = cu_read32(cu_fa, TCR);
	/* If taskCount register overflow once, below calculate is still correct.
	 * Only when the register overflow more than once, this is wrong.
	 * But this should never be the case.
	 */
	done = task_count - cu_fa->task_cnt;
	cu_fa->task_cnt = task_count;

	cu_fa->run_cnts -= done;
	for (i = 0; i < done; i++)
		cu_move_to_complete(cu_fa, KDS_COMPLETED);

	status->num_done  = done;
	status->num_ready = done;
}

static void cu_fa_enable_intr(void *core, u32 intr_type)
{
	struct xrt_cu_fa *cu_fa = core;

	cu_write32(cu_fa, IER, ENABLE);
	return;
}

static void cu_fa_disable_intr(void *core, u32 intr_type)
{
	struct xrt_cu_fa *cu_fa = core;

	cu_write32(cu_fa, IER, DISABLE);
	return;
}

static u32 cu_fa_clear_intr(void *core)
{
	struct xrt_cu_fa *cu_fa = core;

	return cu_read32(cu_fa, ISR);
}

static void cu_fa_reset(void *core)
{
	/* Fast adapter doesn't define software reset */
	return;
}

static bool cu_fa_reset_done(void *core)
{
	return true;
}

static int cu_fa_submit_config(void *core, struct kds_command *xcmd)
{
	struct xrt_cu_fa *cu_fa = core;
	int ret = 0;

	ret = cu_fa_configure(core, (u32 *)xcmd->info, xcmd->isize, xcmd->payload_type);
	if (ret)
		return ret;

	list_move_tail(&xcmd->list, &cu_fa->submitted);
	return ret;
}

static struct kds_command *cu_fa_get_complete(void *core)
{
	struct xrt_cu_fa *cu_fa = core;
	struct kds_command *xcmd = NULL;

	if (list_empty(&cu_fa->completed))
		return NULL;

	xcmd = list_first_entry(&cu_fa->completed, struct kds_command, list);
	list_del_init(&xcmd->list);

	return xcmd;
}

static int cu_fa_abort(void *core, void *cond,
		       bool (*match)(struct kds_command *xcmd, void *cond))
{
	struct xrt_cu_fa *cu_fa = core;
	struct kds_command *xcmd = NULL;
	struct kds_command *next = NULL;
	int ret = -EBUSY;

	list_for_each_entry_safe(xcmd, next, &cu_fa->submitted, list) {
		if (!match(xcmd, cond))
			continue;

		xcmd->status = KDS_TIMEOUT;
		list_move_tail(&xcmd->list, &cu_fa->completed);
	}

	return ret;
}

static struct xcu_funcs xrt_cu_fa_funcs = {
	.alloc_credit	= cu_fa_alloc_credit,
	.free_credit	= cu_fa_free_credit,
	.peek_credit	= cu_fa_peek_credit,
	.configure	= cu_fa_configure,
	.start		= cu_fa_start,
	.check		= cu_fa_check,
	.enable_intr	= cu_fa_enable_intr,
	.disable_intr	= cu_fa_disable_intr,
	.clear_intr	= cu_fa_clear_intr,
	.reset		= cu_fa_reset,
	.reset_done	= cu_fa_reset_done,
	.submit_config  = cu_fa_submit_config,
	.get_complete   = cu_fa_get_complete,
	.abort		= cu_fa_abort,
};

int xrt_cu_fa_init(struct xrt_cu *xcu)
{
	struct xrt_cu_fa *core;
	struct resource *res;
	size_t size;
	int err = 0;

	core = kzalloc(sizeof(struct xrt_cu_fa), GFP_KERNEL);
	if (!core) {
		err = -ENOMEM;
		goto err;
	}

	/* map CU register */
	res = xcu->res[0];
	size = res->end - res->start + 1;
	core->vaddr = ioremap_nocache(res->start, size);
	if (!core->vaddr) {
		err = -ENOMEM;
		xcu_err(xcu, "Map CU register failed");
		goto err;
	}

	/* TODO:
	 * Looks like it would read wrong value from a just downloaded
	 * fast adapter. This is a hardware bug.
	 * We could write 0x0 to Status Register (read only) to workaround
	 * this issue.
	 * But, read any register doesn't always work.
	 * Please do not remove below line until it is fixed on hardware.
	 */
	cu_write32(core, SR, 0x0);
	core->max_credits = cu_read32(core, FDR);
	core->task_cnt = cu_read32(core, TCR);
	core->desc_msw = cu_read32(core, MSWR);
	xcu_info(xcu, "Fast adapter FIFO depth %d", core->max_credits);
	xcu_info(xcu, "Fast adapter init taskCount 0x%x", core->task_cnt);
	core->credits = core->max_credits;
	core->run_cnts = 0;
	INIT_LIST_HEAD(&core->submitted);
	INIT_LIST_HEAD(&core->completed);

	/* TODO:
	 * Maybe in the future, we could initial all of the CU resource at this
	 * place. Now, this is just to note that below variables will initial
	 * when xclbin download finished and kds get update.
	 *   core->cmdmem
	 *   core->paddr
	 *   core->head_slot
	 *   core->num_slots
	 */

	xcu->core = core;
	xcu->funcs = &xrt_cu_fa_funcs;
	/* Not sure what is the best initial value.
	 * But below parameters are configurable to user. No worry.
	 */
	xcu->busy_threshold = core->max_credits / 2;
	xcu->interval_min = 2;
	xcu->interval_max = 5;

	err = xrt_cu_init(xcu);
	if (err)
		return err;

	return 0;

err:
	kfree(core);
	return err;
}

void xrt_cu_fa_fini(struct xrt_cu *xcu)
{
	struct xrt_cu_fa *core = xcu->core;

	xrt_cu_fini(xcu);

	if (xcu->core) {
		if (core->vaddr)
			iounmap(core->vaddr);
		kfree(xcu->core);
	}
}
