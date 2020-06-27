// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include <linux/delay.h>
#include "xrt_cu.h"

/* Use for flush queue */
static inline void
flush_queue(struct list_head *q, u32 *len, int status, void *client)
{
	struct kds_command *xcmd;
	struct kds_command *next;

	if (*len == 0)
		return;

	list_for_each_entry_safe(xcmd, next, q, list) {
		if (client && client != xcmd->client)
			continue;
		xcmd->cb.notify_host(xcmd, status);
		list_del(&xcmd->list);
		xcmd->cb.free(xcmd);
		--(*len);
	}
}

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
	u64 time;

	/* A submitted command might be done but the CU is still
	 * not ready for next command. In this case, check CU status
	 * to get ready count.
	 */
	if (!xcu->num_sq && !is_zero_credit(xcu))
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

	if (xcu->run_timeout && !xcu->done_cnt && xcu->num_sq) {
		xcmd = list_first_entry(&xcu->sq, struct kds_command, list);
		if (!xcu->old_cmd || xcu->old_cmd != xcmd) {
			xcmd->start = ktime_get_raw_fast_ns();
			xcu->old_cmd = xcmd;
			return;
		}

		time = ktime_get_raw_fast_ns();
		if (time - xcmd->start > xcu->run_timeout)
			xcu->bad_state = 1;
		return;
	}

	/* Move all of the completed commands to completed queue */
	while (xcu->done_cnt && xcu->num_sq) {
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

	/* ktime_get_* is still heavy. This impact ~20% of IOPS on echo mode.
	 * For some sort of CU, which usually has a relative long execute time,
	 * we could hide this overhead and provide timestamp for each command.
	 * But this implementation is used for general purpose. Please create CU
	 * specific thread if needed.
	 */
	//xcmd->start = ktime_get_raw_fast_ns();
	/* Move xcmd to submmited queue */
	list_move_tail(&xcmd->list, &xcu->sq);
	--xcu->num_rq;
	++xcu->num_sq;

	return 1;
}

/**
 * process_rq() - Process pending queue
 * @xcu: Target XRT CU
 *
 * Move all of the pending queue commands to the tail of run queue
 * and re-initialized pending queue
 */
static inline void process_pq(struct xrt_cu *xcu)
{
	unsigned long flags;

	/* Get pending queue command number without lock.
	 * The idea is to reduce the possibility of conflict on lock.
	 * Need to check pending command number again after lock.
	 */
	if (!xcu->num_pq)
		return;
	spin_lock_irqsave(&xcu->pq_lock, flags);
	if (xcu->num_pq) {
		list_splice_tail_init(&xcu->pq, &xcu->rq);
		xcu->num_rq += xcu->num_pq;
		xcu->num_pq = 0;
	}
	spin_unlock_irqrestore(&xcu->pq_lock, flags);
}

/**
 * process_event() - Process event
 * @xcu: Target XRT CU
 *
 * This is used to process low frequency events.
 * For example, client abort event would happen when closing client.
 * Before the client close, make sure all of the client commands have
 * been handle properly.
 */
static inline void process_event(struct xrt_cu *xcu)
{
	void *client = NULL;
	struct kds_command *xcmd;

	mutex_lock(&xcu->ev.lock);
	if (!xcu->ev.client)
		goto done;

	client = xcu->ev.client;

	flush_queue(&xcu->rq, &xcu->num_rq, KDS_ABORT, client);

	/* Let's check submitted commands one more time */
	process_sq(xcu);
	if (xcu->num_sq) {
		/* This rarely happens */
		list_for_each_entry(xcmd, &xcu->sq, list) {
			if (xcmd->client == client) {
				/* It is possible to print 'hang' command
				 * details for debug. Should we?
				 */
				xcu->ev.state = CU_STATE_BAD;
				break;
			}
		}
		flush_queue(&xcu->sq, &xcu->num_sq, KDS_ABORT, client);
	}

	while (xcu->num_cq)
		process_cq(xcu);

	/* Maybe pending queue has commands of this client */
	process_pq(xcu);
	flush_queue(&xcu->rq, &xcu->num_rq, KDS_ABORT, client);

	if (!xcu->ev.state)
		xcu->ev.state = CU_STATE_GOOD;
done:
	mutex_unlock(&xcu->ev.lock);
}

int xrt_cu_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	int ret = 0;

	while (!xcu->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
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
		process_event(xcu);

		if (xcu->bad_state)
			break;

		/* Continue until run queue empty */
		if (xcu->num_rq)
			continue;

		if (!xcu->num_sq && !xcu->num_cq)
			if (down_interruptible(&xcu->sem))
				ret = -ERESTARTSYS;

		process_pq(xcu);
	}

	if (!xcu->bad_state)
		return ret;

	/* CU in bad state mode, abort all new commands */
	flush_queue(&xcu->sq, &xcu->num_sq, KDS_ABORT, NULL);
	flush_queue(&xcu->cq, &xcu->num_cq, KDS_ABORT, NULL);
	while (!xcu->stop) {
		flush_queue(&xcu->rq, &xcu->num_rq, KDS_ABORT, NULL);
		process_event(xcu);

		if (down_interruptible(&xcu->sem))
			ret = -ERESTARTSYS;

		process_pq(xcu);
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

/**
 * xrt_cu_abort() - Sent an abort event to CU thread
 * @xcu: Target XRT CU
 * @client: The client tries to abort commands
 *
 * This is used to ask CU thread to abort all commands from the client.
 */
int xrt_cu_abort(struct xrt_cu *xcu, void *client)
{
	int ret = 0;

	mutex_lock(&xcu->ev.lock);
	if (xcu->ev.client) {
		ret = -EAGAIN;
		goto done;
	}

	xcu->ev.client = client;
	xcu->ev.state = 0;

done:
	mutex_unlock(&xcu->ev.lock);
	up(&xcu->sem);
	return ret;
}

/**
 * xrt_cu_abort() - Get done flag of abort
 * @xcu: Target XRT CU
 *
 * Use this to wait for abort event done
 */
int xrt_cu_abort_done(struct xrt_cu *xcu)
{
	int state = 0;

	mutex_lock(&xcu->ev.lock);
	if (xcu->ev.state) {
		xcu->ev.client = NULL;
		state = xcu->ev.state;
	}
	mutex_unlock(&xcu->ev.lock);

	return state;
}

void xrt_cu_set_bad_state(struct xrt_cu *xcu)
{
	xcu->bad_state = 1;
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

	mutex_init(&xcu->ev.lock);
	xcu->ev.client = NULL;
	/* default timeout, 0 means infinity */
	xcu->run_timeout = 0;
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
	sz += sprintf(buf+sz, "  protocol code: %d\n", xcu->info.protocol);
	sz += sprintf(buf+sz, "  interrupt cap: %d\n", xcu->info.intr_enable);
	sz += sprintf(buf+sz, "  interrupt ID:  %d\n", xcu->info.intr_id);
	sz += sprintf(buf+sz, "  bad state:     %d\n", xcu->bad_state);

	if (sz)
		buf[sz++] = 0;

	return sz;
}
