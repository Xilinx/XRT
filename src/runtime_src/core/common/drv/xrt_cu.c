/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Min Ma <min.ma@xilinx.com>
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
	INIT_LIST_HEAD(&xcu->rq);
	spin_lock_init(&xcu->rq_lock);
	INIT_LIST_HEAD(&xcu->pq);
	//mutex_init(&xcu->pq_lock);
	spin_lock_init(&xcu->pq_lock);

	switch (xcu->info.model) {
	case MODEL_PLRAM:
		err = xrt_cu_plram_init(xcu);
		break;
	default:
		xcu_err(xcu, "Unknown CU execution model");
		err = -EINVAL;
	}

	return err;
}

void xrt_cu_fini(struct xrt_cu *xcu)
{
	switch (xcu->info.model) {
	case MODEL_PLRAM:
		xrt_cu_plram_fini(xcu);
		break;
	default:
		/* It should never go here */
		xcu_err(xcu, "Unknown CU execution model");
	}
}
