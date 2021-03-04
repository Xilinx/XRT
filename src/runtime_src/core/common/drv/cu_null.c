/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx HLS CU
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "xrt_cu.h"

/* This is NULL CU model that handles unexpected CU protocol or unknown CUs.
 * So that other CUs would still work.
 *
 * Note: Any commands submit to this CU would return immediately.
 */

static int cu_null_alloc_credit(void *core)
{
	return 1;
}

static void cu_null_free_credit(void *core, u32 count)
{
}

static int cu_null_peek_credit(void *core)
{
	return 1;
}

static void cu_null_configure(void *core, u32 *data, size_t sz, int type)
{
}

static void cu_null_start(void *core)
{
}

static void cu_null_check(void *core, struct xcu_status *status)
{
	status->num_done = 1;
	status->num_ready = 1;
	status->new_status = CU_AP_IDLE;
}

static void cu_null_enable_intr(void *core, u32 intr_type)
{
}

static void cu_null_disable_intr(void *core, u32 intr_type)
{
}

static u32 cu_null_clear_intr(void *core)
{
	return 0;
}

static struct xcu_funcs xrt_cu_null_funcs = {
	.alloc_credit	= cu_null_alloc_credit,
	.free_credit	= cu_null_free_credit,
	.peek_credit	= cu_null_peek_credit,
	.configure	= cu_null_configure,
	.start		= cu_null_start,
	.check		= cu_null_check,
	.enable_intr	= cu_null_enable_intr,
	.disable_intr	= cu_null_disable_intr,
	.clear_intr	= cu_null_clear_intr,
};

int xrt_cu_null_init(struct xrt_cu *xcu)
{
	xcu_info(xcu, "CU(%d) is null, command will directly complete",
		 xcu->info.cu_idx);

	xcu->status = CU_AP_IDLE;
	xcu->core = NULL;
	xcu->funcs = &xrt_cu_null_funcs;

	xcu->busy_threshold = -1;
	xcu->interval_min = 2;
	xcu->interval_max = 5;

	return xrt_cu_init(xcu);
}

void xrt_cu_null_fini(struct xrt_cu *xcu)
{
	xrt_cu_fini(xcu);
}

