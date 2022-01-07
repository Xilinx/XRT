// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Sub-device Driver
 *
 * Copyright (C) 2021 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "cu_xgq.h"
#include "xocl_xgq.h"
#include "../xgq_xocl_plat.h"

struct xrt_cu_xgq {
	void __iomem		*vaddr;
	int			 max_credits;
	int			 credits;
	int			 run_cnts;
	u32			 done;
	u32			 ready;
	void			*xgq;
	int			 xgq_client_id;
};

static int cu_xgq_alloc_credit(void *core)
{
	return 1;
}

static void cu_xgq_free_credit(void *core, u32 count)
{
	return;
}

static int cu_xgq_peek_credit(void *core)
{
	return 1;
}

static int cu_xgq_configure(void *core, u32 *data, size_t sz, int type)
{
	struct xrt_cu_xgq *cu_xgq = core;
	int ret = 0;

	ret = xocl_xgq_set_command(cu_xgq->xgq, cu_xgq->xgq_client_id, data, sz);
	return ret;
}

static void cu_xgq_start(void *core)
{
	struct xrt_cu_xgq *cu_xgq = core;

	xocl_xgq_notify(cu_xgq->xgq);
}

static void cu_xgq_check(void *core, struct xcu_status *status, bool force)
{
	struct xrt_cu_xgq *cu_xgq = core;

	status->num_ready = 1;
	status->num_done = 0;
	while (!xocl_xgq_get_response(cu_xgq->xgq, cu_xgq->xgq_client_id)) {
		status->num_done += 1;
	}
}

static void cu_xgq_enable_intr(void *core, u32 intr_type)
{
	return;
}

static void cu_xgq_disable_intr(void *core, u32 intr_type)
{
	return;
}

static u32 cu_xgq_clear_intr(void *core)
{
	return 0;
}

static void cu_xgq_reset(void *core)
{
	return;
}

static bool cu_xgq_reset_done(void *core)
{
	return true;
}

static struct xcu_funcs xrt_cu_xgq_funcs = {
	.alloc_credit	= cu_xgq_alloc_credit,
	.free_credit	= cu_xgq_free_credit,
	.peek_credit	= cu_xgq_peek_credit,
	.configure	= cu_xgq_configure,
	.start		= cu_xgq_start,
	.check		= cu_xgq_check,
	.enable_intr	= cu_xgq_enable_intr,
	.disable_intr	= cu_xgq_disable_intr,
	.clear_intr	= cu_xgq_clear_intr,
	.reset		= cu_xgq_reset,
	.reset_done	= cu_xgq_reset_done,
};

int xrt_cu_xgq_init(struct xrt_cu *xcu)
{
	struct xrt_cu_xgq *core = NULL;
	int err = 0;

	core = kzalloc(sizeof(struct xrt_cu_xgq), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	core->xgq = xcu->info.xgq;
	err = xocl_xgq_attach(core->xgq, (void *)core, &core->xgq_client_id);
	if (err)
		return err;

	core->max_credits = 1;
	core->credits = 1;

	xcu->core = core;
	xcu->funcs = &xrt_cu_xgq_funcs;

	xcu->busy_threshold = 2;
	xcu->interval_min = 2;
	xcu->interval_max = 5;

	err = xrt_cu_init(xcu);
	if (err)
		return err;

	return 0;
}

void xrt_cu_xgq_fini(struct xrt_cu *xcu)
{
	struct xrt_cu_xgq *core = xcu->core;

	xrt_cu_fini(xcu);

	if (core->vaddr)
		iounmap(core->vaddr);
	kfree(xcu->core);
}

