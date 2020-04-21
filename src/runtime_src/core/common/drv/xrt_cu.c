// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xrt_cu.h"

inline void xrt_cu_config(struct xrt_cu *xcu, u32 *data, size_t sz, int type)
{
	xcu->funcs->configure(xcu->core, data, sz, type);
}

inline void xrt_cu_start(struct xrt_cu *xcu)
{
	xcu->funcs->start(xcu->core);
}

/* XRT CU still thought command is finished in order on CU
 * It is possible to make this more flesible. Let's do it later..
 */
inline void xrt_cu_check(struct xrt_cu *xcu)
{
	struct xcu_status status;

	xcu->funcs->check(xcu->core, &status);
	xcu->done_cnt += status.num_done;
	xcu->ready_cnt += status.num_ready;
}

inline void xrt_cu_wait(struct xrt_cu *xcu)
{
	xcu->funcs->wait(xcu->core);
}

inline void xrt_cu_up(struct xrt_cu *xcu)
{
	xcu->funcs->up(xcu->core);
}

inline int xrt_cu_get_credit(struct xrt_cu *xcu)
{
	return xcu->funcs->get_credit(xcu->core);
}

inline void xrt_cu_put_credit(struct xrt_cu *xcu, u32 count)
{
	xcu->funcs->put_credit(xcu->core, count);
}

int xrt_cu_init(struct xrt_cu *xcu)
{
	int err = 0;

	/* Use list for driver space command queue
	 * Should we consider ring buffer?
	 */
	INIT_LIST_HEAD(&xcu->pq);
	spin_lock_init(&xcu->pq_lock);
	INIT_LIST_HEAD(&xcu->rq);
	spin_lock_init(&xcu->rq_lock);
	INIT_LIST_HEAD(&xcu->sq);
	xcu->num_pq = 0;
	xcu->num_rq = 0;
	xcu->num_sq = 0;
	sema_init(&xcu->sem, 0);
	xcu->stop = 0;

	return err;
}

void xrt_cu_fini(struct xrt_cu *xcu)
{
	return;
}
