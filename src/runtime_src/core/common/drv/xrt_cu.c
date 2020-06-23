// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xrt_cu.h"

/**
 * process_cq() - Process completed queue
 * @xcu: Target XRT CU
 */
static inline void process_cq(struct xrt_cu *xcu)
{
	struct kds_command *xcmd;

	if (!xcu->num_cq)
		return;

	/* Notify host and free command */
	xcmd = list_first_entry(&xcu->cq, struct kds_command, list);
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	list_del(&xcmd->list);
	xcmd->cb.free(xcmd);
	--xcu->num_cq;
}

/**
 * process_sq() - Process submitted queue
 * @xcu: Target XRT CU
 */
static inline void process_sq(struct xrt_cu *xcu)
{
	struct kds_command *xcmd;

	if (!xcu->num_sq)
		return;

	/* If no command is running on the hardware,
	 * this would not really access hardware.
	 */
	xrt_cu_check(xcu);
	/* CU is ready to accept more commands
	 * Return credits to allow submit more commands
	 */
	if (xcu->ready_cnt) {
		xrt_cu_put_credit(xcu, xcu->ready_cnt);
		xcu->ready_cnt = 0;
	}

	/* Move all of the completed commands to completed queue */
	while (xcu->done_cnt) {
		xcmd = list_first_entry(&xcu->sq, struct kds_command, list);
		list_move_tail(&xcmd->list, &xcu->cq);
		--xcu->num_sq;
		++xcu->num_cq;
		--xcu->done_cnt;
	}
}

/**
 * process_rq() - Process run queue
 * @xcu: Target XRT CU
 *
 * Return: return 0 if run queue is empty or no credit
 *	   Otherwise, return 1
 */
static inline int process_rq(struct xrt_cu *xcu)
{
	struct kds_command *xcmd;

	if (!xcu->num_rq)
		return 0;

	xcmd = list_first_entry(&xcu->rq, struct kds_command, list);

	if (!xrt_cu_get_credit(xcu))
		return 0;

	/* if successfully get credit, you must start cu */
	xrt_cu_config(xcu, (u32 *)xcmd->info, xcmd->isize, 0);
	xrt_cu_start(xcu);

	/* Move xcmd to submmited queue */
	list_move_tail(&xcmd->list, &xcu->sq);
	--xcu->num_rq;
	++xcu->num_sq;

	return 1;
}

int xrt_cu_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	unsigned long flags;
	int ret = 0;

	while (!xcu->stop) {
		/* process run queue would return 1 if submit a command to CU
		 * Let's try submit another command
		 */
		if (process_rq(xcu))
			continue;
		/* process completed queue before submitted queue, for
		 * two reasons:
		 * - The last submitted command may be still running
		 * - while handling completed queue, running command might done
		 * - process_sq will check CU status, which is thru slow bus
		 */
		process_cq(xcu);
		process_sq(xcu);

		/* Continue until run queue emtpy */
		if (xcu->num_rq)
			continue;

		if (!xcu->num_sq && !xcu->num_cq)
			if (down_interruptible(&xcu->sem))
				ret = -ERESTARTSYS;

		/* Get pending queue command number without lock.
		 * The idea is to reduce the possibility of conflict on lock.
		 * Need to check pending command number again after lock.
		 */
		if (!xcu->num_pq)
			continue;
		spin_lock_irqsave(&xcu->pq_lock, flags);
		if (xcu->num_pq) {
			list_splice_tail_init(&xcu->pq, &xcu->rq);
			xcu->num_rq = xcu->num_pq;
			xcu->num_pq = 0;
		}
		spin_unlock_irqrestore(&xcu->pq_lock, flags);
	}

	return ret;
}

void xrt_cu_submit(struct xrt_cu *xcu, struct kds_command *xcmd)
{
	unsigned long flags;
	bool first_command = false;

	/* Add command to pending queue
	 * wakeup CU thread if it is the first command
	 */
	spin_lock_irqsave(&xcu->pq_lock, flags);
	list_add_tail(&xcmd->list, &xcu->pq);
	++xcu->num_pq;
	first_command = (xcu->num_pq == 1);
	spin_unlock_irqrestore(&xcu->pq_lock, flags);
	if (first_command)
		up(&xcu->sem);
}

int xrt_cu_init(struct xrt_cu *xcu)
{
	int err = 0;

	/* Use list for driver space command queue
	 * Should we consider ring buffer?
	 */

	/* Initialize pending queue and lock */
	INIT_LIST_HEAD(&xcu->pq);
	spin_lock_init(&xcu->pq_lock);
	/* Initialize run queue */
	INIT_LIST_HEAD(&xcu->rq);
	/* Initialize submitted queue */
	INIT_LIST_HEAD(&xcu->sq);
	/* Initialize completed queue */
	INIT_LIST_HEAD(&xcu->cq);

	sema_init(&xcu->sem, 0);
	xcu->thread = kthread_run(xrt_cu_thread, xcu, "xrt_thread");

	return err;
}

void xrt_cu_fini(struct xrt_cu *xcu)
{
	xcu->stop = 1;
	up(&xcu->sem);
	(void) kthread_stop(xcu->thread);

	return;
}

ssize_t show_cu_stat(struct xrt_cu *xcu, char *buf)
{
	ssize_t sz = 0;

	sz += sprintf(buf+sz, "CU index: %d\n", xcu->info.cu_idx);
	sz += sprintf(buf+sz, "CU protocol code: %d\n", xcu->info.protocol);
	sz += sprintf(buf+sz, "CU interrupt cap: %d\n", xcu->info.intr_enable);
	sz += sprintf(buf+sz, "CU interrupt ID: %d\n", xcu->info.intr_id);

	if (sz)
		buf[sz++] = 0;

	return sz;
}
