// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xrt_cu.h"

static inline void process_sq_once(struct xrt_cu *xcu)
{
	struct list_head *q;
	struct kds_command *done_xcmd;

	/* This is the critical path, as less check as possible..
	 * if rq and sq both are empty, please DO NOT call this function
	 */

	q = list_empty(&xcu->sq) ? &xcu->rq : &xcu->sq;

	xrt_cu_check(xcu);
	xrt_cu_put_credit(xcu, xcu->ready_cnt);
	xcu->ready_cnt = 0;
	if (!xcu->done_cnt)
		return;

	done_xcmd = list_first_entry_or_null(q, struct kds_command, list);
	done_xcmd->cb.notify_host(done_xcmd, KDS_COMPLETED);
	list_del(&done_xcmd->list);
	done_xcmd->cb.free(done_xcmd);
	--xcu->done_cnt;
}

static inline void process_rq(struct xrt_cu *xcu)
{
	struct kds_command *xcmd;
	struct kds_command *last_xcmd;

	/* This function would not return until rq is empty */
	xcmd = list_first_entry_or_null(&xcu->rq, struct kds_command, list);
	last_xcmd = list_last_entry(&xcu->rq, struct kds_command, list);

	while (xcmd) {
		if (xrt_cu_get_credit(xcu)) {
			/* if successfully get credit, you must start cu */
			xrt_cu_config(xcu, (u32 *)xcmd->info, xcmd->isize, 0);
			xrt_cu_start(xcu);
			/* xcmd should always point to next waiting
			 * to submit command
			 */
			if (xcmd != last_xcmd)
				xcmd = list_next_entry(xcmd, list);
			else
				xcmd = NULL;
		} else {
			/* Run out of credit and still have xcmd in rq.
			 * In this case, only do wait one more command done.
			 */
			process_sq_once(xcu);
		}
	}

	/* Some commands maybe not completed
	 * or they are completed but haven't beed processed
	 * Do not wait, get pending command first.
	 */
	if (!list_empty(&xcu->rq))
		list_splice_tail_init(&xcu->rq, &xcu->sq);
}

int xrt_cu_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	unsigned long flags;

	while (1) {
		// Check num_pq here
		spin_lock_irqsave(&xcu->pq_lock, flags);
		// double check
		if (xcu->num_pq > 0) {
			list_splice_tail_init(&xcu->pq, &xcu->rq);
			xcu->num_pq = 0;
		}
		spin_unlock_irqrestore(&xcu->pq_lock, flags);

		/* !!!Do not change the priority! */
		if (!list_empty(&xcu->rq)) {
			process_rq(xcu);
		} else if (!list_empty(&xcu->sq)) {
			process_sq_once(xcu);
		} else {
			while (down_timeout(&xcu->sem, 1000) == -ETIME) {
				if (kthread_should_stop())
					return 0;
			}
		}
	}

	return 0;
}

void xrt_cu_submit(struct xrt_cu *xcu, struct kds_command *xcmd)
{
	unsigned long flags;

	spin_lock_irqsave(&xcu->pq_lock, flags);
	list_add_tail(&xcmd->list, &xcu->pq);
	if (xcu->num_pq == 0)
		up(&xcu->sem);
	++xcu->num_pq;
	spin_unlock_irqrestore(&xcu->pq_lock, flags);
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
	INIT_LIST_HEAD(&xcu->sq);
	xcu->num_pq = 0;
	xcu->num_sq = 0;
	sema_init(&xcu->sem, 0);
	xcu->stop = 0;
	xcu->thread = kthread_run(xrt_cu_thread, xcu, "xrt_thread");

	return err;
}

void xrt_cu_fini(struct xrt_cu *xcu)
{
	(void) kthread_stop(xcu->thread);

	return;
}
