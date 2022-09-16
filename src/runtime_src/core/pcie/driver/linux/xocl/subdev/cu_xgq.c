// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Sub-device Driver
 *
 * Copyright (C) 2021-2022 Xilinx, Inc.
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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
	int			 cu_idx;
	int			 cu_domain;
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
	return 0;
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
	while (!xocl_xgq_check_response(cu_xgq->xgq, cu_xgq->xgq_client_id));
	status->new_status = 0x4;
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

static int cu_xgq_submit_config(void *core, struct kds_command *xcmd)
{
	struct xrt_cu_xgq *cu_xgq = core;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;

	hdr = (struct xgq_cmd_sq_hdr *)xcmd->info;
	hdr->cu_idx = cu_xgq->cu_idx;
	hdr->cu_domain = cu_xgq->cu_domain;
	ret = xocl_xgq_set_command(cu_xgq->xgq, cu_xgq->xgq_client_id, xcmd);
	return ret;
}

static struct kds_command *cu_xgq_get_complete(void *core)
{
	struct xrt_cu_xgq *cu_xgq = core;

	return xocl_xgq_get_command(cu_xgq->xgq, cu_xgq->xgq_client_id);
}

static int cu_xgq_abort(void *core, void *cond,
			bool (*match)(struct kds_command *xcmd, void *cond))
{
	struct xrt_cu_xgq *cu_xgq = core;

	return xocl_xgq_abort(cu_xgq->xgq, cu_xgq->xgq_client_id, cond, match);
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
	.submit_config  = cu_xgq_submit_config,
	.get_complete   = cu_xgq_get_complete,
	.abort		= cu_xgq_abort,
};

int xrt_cu_xgq_init(struct xrt_cu *xcu, int slow_path)
{
	struct xrt_cu_xgq *core = NULL;
	u32 prot = 0;
	int err = 0;

	core = kzalloc(sizeof(struct xrt_cu_xgq), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	if (slow_path)
		prot = XGQ_PROT_NEED_RESP;

	core->xgq = xcu->info.xgq;
	core->max_credits = 1;
	core->credits = 1;
	core->cu_idx = xcu->info.cu_idx;
	core->cu_domain = xcu->info.cu_domain;

	xcu->core = core;
	xcu->funcs = &xrt_cu_xgq_funcs;

	xcu->busy_threshold = 2;
	xcu->interval_min = 2;
	xcu->interval_max = 5;
	mutex_init(&xcu->read_regs.xcr_lock);

	xcu->status = 0x4;
	err = xrt_cu_init(xcu);
	if (err)
		goto error_out;

	err = xocl_xgq_attach(core->xgq, (void *)core, &xcu->sem_cu,  prot, &core->xgq_client_id);
	if (err)
		goto error_out1;

	return 0;

error_out1:
	xrt_cu_fini(xcu);
error_out:
	return err;
}

void xrt_cu_xgq_fini(struct xrt_cu *xcu)
{
	struct xrt_cu_xgq *core = xcu->core;

	xrt_cu_fini(xcu);

	if (core->vaddr)
		iounmap(core->vaddr);
	kfree(xcu->core);
}

