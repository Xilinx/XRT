// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx HLS CU
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xrt_cu.h"

extern int kds_echo;

static int cu_hls_get_credit(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	return (cu_hls->credits) ? cu_hls->credits-- : 0;
}

static void cu_hls_put_credit(void *core, u32 count)
{
	struct xrt_cu_hls *cu_hls = core;

	cu_hls->credits += count;
	if (cu_hls->credits > cu_hls->max_credits)
		cu_hls->credits = cu_hls->max_credits;
}

static int cu_hls_is_zero_credit(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	return (cu_hls->credits)? 0 : 1;
}

static void cu_hls_configure(void *core, u32 *data, size_t sz, int type)
{
	struct xrt_cu_hls *cu_hls = core;
	u32 *base_addr = cu_hls->vaddr;
	size_t num_reg;
	u32 i;

	if (kds_echo)
		return;

	num_reg = sz / sizeof(u32);
	/* Write register map, starting at base_addr + 0x10 (byte)
	 * This based on the fact that kernel used
	 *	0x00 -- Control Register
	 *	0x04 and 0x08 -- Interrupt Enable Registers
	 *	0x0C -- Interrupt Status Register
	 * Skip the first 4 words in user regmap.
	 */
	for (i = 0; i < num_reg; ++i)
		iowrite32(data[i], base_addr + 4 + i);
}

static void cu_hls_start(void *core)
{
	struct xrt_cu_hls *cu_hls = core;

	cu_hls->run_cnts++;

	if (kds_echo)
		return;

	/* Bit 0 -- The CU start control bit.
	 * Write 0 to this bit will be ignored.
	 * Until the CU is ready to take next task, this bit will reamin 1.
	 * Once ths CU is ready, it will clear this bit.
	 * So, if this bit is 1, it means the CU is running.
	 */
	iowrite32(CU_AP_START, cu_hls->vaddr);
}

static void cu_hls_check(void *core, struct xcu_status *status)
{
	struct xrt_cu_hls *cu_hls = core;
	u32 ctrl_reg;
	u32 done_reg = 0;
	u32 ready_reg = 0;
	u32 submitted;

	if (kds_echo) {
		done_reg = 1;
		ready_reg = 1;
		cu_hls->run_cnts--;
		goto out;
	}

	submitted = cu_hls->max_credits - cu_hls->credits;

	/* ioread32/iowrite32 is expensive! */
	if (!submitted && !cu_hls->run_cnts)
		goto out;

	/* done is indicated by AP_DONE(2) alone or by AP_DONE(2) | AP_IDLE(4)
	 * but not by AP_IDLE itself.  Since b10 | (b10 | b100) = b110
	 * checking for b10 is sufficient.
	 */
	ctrl_reg  = ioread32(cu_hls->vaddr);

	/* if there is submitted tasks, then check if ap_start bit is clear
	 * See comments in cu_hls_start().
	 */
	if (submitted && !(ctrl_reg & CU_AP_START))
		ready_reg = 1;

	/* For AP_CTRL_HS, ap_ready and ap_done would assert at the same cycle.
	 * For AP_CTRL_CHAIN, ap_done and ap_ready have NO dependency.
	 */
	if (ctrl_reg & CU_AP_DONE) {
		done_reg = 1;
		cu_hls->run_cnts--;
		if (cu_hls->ctrl_chain)
			iowrite32(CU_AP_CONTINUE, cu_hls->vaddr);
	}

out:
	status->num_done = done_reg;
	status->num_ready = ready_reg;
}

static struct xcu_funcs xrt_cu_hls_funcs = {
	.get_credit	= cu_hls_get_credit,
	.put_credit	= cu_hls_put_credit,
	.is_zero_credit	= cu_hls_is_zero_credit,
	.configure	= cu_hls_configure,
	.start		= cu_hls_start,
	.check		= cu_hls_check,
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

	xcu->core = core;
	xcu->funcs = &xrt_cu_hls_funcs;

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

	if (xcu->core) {
		if (core->vaddr)
			iounmap(core->vaddr);
		kfree(xcu->core);
	}

	xrt_cu_fini(xcu);
}
