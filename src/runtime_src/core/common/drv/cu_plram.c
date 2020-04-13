// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx PLRAM Queue Based CU
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xrt_cu.h"

#define ECHO 0

static void cu_plram_wait(void *core)
{
	struct xrt_cu_plram *cu_plram = core;

	down_interruptible(&cu_plram->sem);
}

static void cu_plram_up(void *core)
{
	struct xrt_cu_plram *cu_plram = core;

	up(&cu_plram->sem);
}

static int cu_plram_get_credit(void *core)
{
	struct xrt_cu_plram *cu_plram = core;

	return (cu_plram->credits) ? cu_plram->credits-- : 0;
}

static void cu_plram_put_credit(void *core, u32 count)
{
	struct xrt_cu_plram *cu_plram = core;

	cu_plram->credits += count;
	if (cu_plram->credits > cu_plram->max_credits)
		cu_plram->credits = cu_plram->max_credits;
}

static void cu_plram_configure(void *core, u32 *data, size_t sz, int type)
{
#if ECHO
	return;
#else
	struct xrt_cu_plram *cu_plram = core;

	/* TODO: Configure in specific slot */
	memcpy(cu_plram->plram, data, sz);
#endif
}

static void cu_plram_start(void *core)
{
#if ECHO
	return;
#else
	struct xrt_cu_plram *cu_plram = core;

	/* TODO: Start CU in specific slot */
	iowrite32(0x0, cu_plram->vaddr + 0x10);
#endif
}

static void cu_plram_check(void *core, struct xcu_status *status)
{
	struct xrt_cu_plram *cu_plram = core;
	u32 done_reg = 0;

#if ECHO
	done_reg = 1;
#else
	/* There is only one done commands counter in the plram CU
	 * It tells how many commands are done and how many FIFO slots
	 * are ready for more commands.
	 *
	 * ioread32 is expensive! If no credits are taken(CU is not running),
	 * DO NOT read done register.
	 */
	if (cu_plram->credits != cu_plram->max_credits)
		done_reg = ioread32(cu_plram->vaddr + 0x1C);
#endif
	status->num_done = done_reg;
	status->num_ready = done_reg;
}

static struct xcu_funcs xrt_cu_plram_funcs = {
	.get_credit	= cu_plram_get_credit,
	.put_credit	= cu_plram_put_credit,
	.configure	= cu_plram_configure,
	.start		= cu_plram_start,
	.check		= cu_plram_check,
	.wait		= cu_plram_wait,
	.up		= cu_plram_up,
};

int xrt_cu_plram_init(struct xrt_cu *xcu)
{
	struct xrt_cu_plram *core;
	struct xrt_cu_info *info = &xcu->info;
	struct resource *res;
	size_t size;
	u32 val;
	int err = 0;

	if (info->num_res != 2) {
		xcu_err(xcu, "2 resources are required");
		return -EINVAL;
	}

	core = kzalloc(sizeof(struct xrt_cu_plram), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	/* map CU register */
	res = xcu->res[0];
	size = res->end - res->start + 1;
	core->vaddr = ioremap_nocache(res->start, size);
	if (!core->vaddr) {
		err = -ENOMEM;
		xcu_err(xcu, "Map CU register failed");
		goto err;
	}

	/* NOTE: Dummy read needed on kernel 0x18 and 0x1c register because
	 * of some bug in kernel, first read will give some garbage data
	 */
	val = ioread32(core->vaddr + 0x18);
	val = ioread32(core->vaddr + 0x1C);

	val = ioread32(core->vaddr + 0x18);
	val = ioread32(core->vaddr + 0x1C);
	xcu_info(xcu, "FIFO depth 0x%x", val);
	core->max_credits = val;
	core->credits = core->max_credits;
	sema_init(&core->sem, core->max_credits);

	/* Set plram base address, this is hard coding */
	iowrite32(0x00, core->vaddr + 0x20);
	iowrite32(0x41, core->vaddr + 0x24);

	/* map plram */
	res = xcu->res[1];
	size = res->end - res->start + 1;
	/* use wc would make huge different on IOPS */
	//core->plram = ioremap_nocache(res->start, size);
	core->plram = ioremap_wc(res->start, size);
	if (!core->plram) {
		err = -ENOMEM;
		xcu_err(xcu, "Map CU arguments RAM failed");
		goto err1;
	}

	xcu->core = core;
	xcu->funcs = &xrt_cu_plram_funcs;

	return 0;
err1:
	iounmap(core->vaddr);
err:
	kfree(core);
	return err;
}

void xrt_cu_plram_fini(struct xrt_cu *xcu)
{
	struct xrt_cu_plram *core = xcu->core;

	if (xcu->core) {
		if (core->vaddr)
			iounmap(core->vaddr);
		if (core->plram)
			iounmap(core->plram);
		kfree(xcu->core);
	}
}
