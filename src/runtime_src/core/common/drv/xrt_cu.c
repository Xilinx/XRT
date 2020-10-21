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

	/* Notify host and free command
	 *
	 * NOTE: Use loop to handle completed commands could improve
	 * performance (Reduce sleep time and increase chance for more
	 * pending commands at one time)
	 */
	while (xcu->num_cq) {
		xcmd = list_first_entry(&xcu->cq, struct kds_command, list);
		xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
		list_del(&xcmd->list);
		xcmd->cb.free(xcmd);
		--xcu->num_cq;
	}
}

/**
 * __process_sq() - Process submitted queue
 * @xcu: Target XRT CU
 */
static inline void __process_sq(struct xrt_cu *xcu)
{
	struct kds_command *xcmd;
	u64 time;

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
 * process_sq() - Process submitted queue
 * @xcu: Target XRT CU
 */
static inline void process_sq(struct xrt_cu *xcu)
{
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

	__process_sq(xcu);
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
 * process_pq() - Process pending queue
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
	if (xcu->max_running < xcu->num_rq)
		xcu->max_running = xcu->num_rq;
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

int xrt_cu_polling_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	int ret = 0;

	xcu_info(xcu, "start");
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

		/* The idea is when CU's credit is less than busy threshold,
		 * sleep a while to wait for CU completion.
		 * The interval is configurable and it should be determin by
		 * kernel execution.
		 * If threshold is -1, then this is a busy loop to check CU
		 * statue.
		 */
		if (xrt_cu_peek_credit(xcu) <= xcu->busy_threshold)
			usleep_range(xcu->interval_min, xcu->interval_max);

		/* Continue until run queue empty */
		if (xcu->num_rq)
			continue;

		if (!xcu->num_sq && !xcu->num_cq) {
			xcu->sleep_cnt++;
			if (down_interruptible(&xcu->sem))
				ret = -ERESTARTSYS;
		}

		process_pq(xcu);
	}

	if (!xcu->bad_state) {
		xcu_info(xcu, "end");
		return ret;
	}

	xcu_info(xcu, "bad state handling");
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

	xcu_info(xcu, "end handling");
	return ret;
}

int xrt_cu_intr_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	int ret = 0;

	xcu_info(xcu, "start");
	xrt_cu_enable_intr(xcu, CU_INTR_DONE | CU_INTR_READY);
	while (!xcu->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
		 */
		if (process_rq(xcu))
			continue;

		if (xcu->num_sq) {
			if (down_interruptible(&xcu->sem_cu))
				ret = -ERESTARTSYS;
			__process_sq(xcu);
		}

		process_cq(xcu);
		process_event(xcu);

		if (xcu->bad_state)
			break;

		/* Continue until run queue empty */
		if (xcu->num_rq)
			continue;

		if (!xcu->num_sq && !xcu->num_cq) {
			xcu->sleep_cnt++;
			if (down_interruptible(&xcu->sem))
				ret = -ERESTARTSYS;
		}

		process_pq(xcu);
	}
	xrt_cu_disable_intr(xcu, CU_INTR_DONE | CU_INTR_READY);

	if (!xcu->bad_state) {
		xcu_info(xcu, "end");
		return ret;
	}

	xcu_info(xcu, "bad state handling");
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

	xcu_info(xcu, "end handling");
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
	up(&xcu->sem_cu);
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

int xrt_cu_cfg_update(struct xrt_cu *xcu, int intr)
{
	int (* cu_thread)(void *data);
	int err = 0;

	/* Check if CU support interrupt in hardware */
	if (!xcu->info.intr_enable)
		return -ENOSYS;

	if (intr)
		cu_thread = xrt_cu_intr_thread;
	else
		cu_thread = xrt_cu_polling_thread;

	/* Stop old thread */
	xcu->stop = 1;
	up(&xcu->sem_cu);
	up(&xcu->sem);
	if (!IS_ERR(xcu->thread))
		(void) kthread_stop(xcu->thread);

	/* launch new thread */
	xcu->stop = 0;
	sema_init(&xcu->sem, 0);
	sema_init(&xcu->sem_cu, 0);
	xcu->thread = kthread_run(cu_thread, xcu, xcu->info.iname);
	if (IS_ERR(xcu->thread)) {
		err = IS_ERR(xcu->thread);
		xcu_err(xcu, "Create CU thread failed, err %d\n", err);
	}

	return err;
}

/* 
 * If KDS has to manage PLRAM resources, we should come up with a better design.
 * Ideally, CU subdevice should request for plram resource instead of KDS assign
 * plram resource to CU.
 * Or, another solution is to let KDS create CU subdevice indtead of
 * icap/zocl_xclbin.
 */
static u32 cu_fa_get_desc_size(struct xrt_cu *xcu)
{
	struct xrt_cu_info *info = &xcu->info;
	u32 size = sizeof(descriptor_t);
	int i;

	for (i = 0; i < info->num_args; i++) {
		/* The "nextDescriptorAddr" argument is a fast adapter argument.
		 * It is not required to construct descriptor
		 */
		if (!strcmp(info->args[i].name, "nextDescriptorAddr") || !info->args[i].size)
			continue;

		size += (sizeof(descEntry_t)+info->args[i].size);
	}

	return round_up_to_next_power2(size);
}

int xrt_is_fa(struct xrt_cu *xcu, u32 *size)
{
	struct xrt_cu_info *info = &xcu->info;
	int ret;

	ret = (info->model == XCU_FA)? 1 : 0;

	if (ret && size)
		*size = cu_fa_get_desc_size(xcu);

	return ret;
}

int xrt_fa_cfg_update(struct xrt_cu *xcu, u64 bar, u64 dev, void __iomem *vaddr, u32 num_slots)
{
	struct xrt_cu_fa *cu_fa = xcu->core;
	u32 slot_size;

	if (bar == 0) {
		cu_fa->plram = NULL;
		cu_fa->paddr = 0;
		cu_fa->slot_sz = 0;
		cu_fa->num_slots = 0;
		return 0;
	}

	slot_size = cu_fa_get_desc_size(xcu);

	cu_fa->plram = vaddr;
	cu_fa->paddr = dev;
	cu_fa->slot_sz = slot_size;
	cu_fa->num_slots = num_slots;

	cu_fa->credits = (cu_fa->num_slots < cu_fa->max_credits)?
			 cu_fa->num_slots : cu_fa->max_credits;

	return 0;
}

void xrt_cu_set_bad_state(struct xrt_cu *xcu)
{
	xcu->bad_state = 1;
}

int xrt_cu_init(struct xrt_cu *xcu)
{
	int err = 0;
	char *name = xcu->info.iname;

	/* TODO A workaround to avoid m2m subdev launch thread */
	if (!strlen(name))
		return 0;

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
	sema_init(&xcu->sem_cu, 0);
	/* A CU maybe doesn't support interrupt, polling */
	xcu->thread = kthread_run(xrt_cu_polling_thread, xcu, name);
	if (IS_ERR(xcu->thread)) {
		err = IS_ERR(xcu->thread);
		xcu_err(xcu, "Create CU thread failed, err %d\n", err);
	}

	return err;
}

void xrt_cu_fini(struct xrt_cu *xcu)
{
	/* TODO A workaround for m2m subdev */
	if (!strlen(xcu->info.iname))
		return;

	xcu->stop = 1;
	up(&xcu->sem_cu);
	up(&xcu->sem);
	if (!IS_ERR(xcu->thread))
		(void) kthread_stop(xcu->thread);

	return;
}

ssize_t show_cu_stat(struct xrt_cu *xcu, char *buf)
{
	ssize_t sz = 0;

	/* Add CU dynamic statistic information in below */
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Pending queue:    %d\n",
			xcu->num_pq);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Running queue:    %d\n",
			xcu->num_rq);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Submitted queue:  %d\n",
			xcu->num_sq);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Completed queue:  %d\n",
			xcu->num_cq);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Bad state:        %d\n",
			xcu->bad_state);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Current credit:   %d\n",
			xcu->funcs->peek_credit(xcu->core));
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "CU status:        0x%x\n",
			xcu->status);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "sleep cnt:        %d\n",
			xcu->sleep_cnt);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "max running:      %d\n",
			xcu->max_running);

	if (xcu->info.model == XCU_FA) {
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, "-- FA CU specific --\n");
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Check count: %lld\n",
				to_cu_fa(xcu->core)->check_count);
	}

	if (sz < PAGE_SIZE - 1)
		buf[sz++] = 0;
	else
		buf[PAGE_SIZE - 1] = 0;

	return sz;
}

ssize_t show_cu_info(struct xrt_cu *xcu, char *buf)
{
	struct xrt_cu_info *info = &xcu->info;
	ssize_t sz = 0;
	char dir[10];
	int i;

	/* Add any CU static information in below */
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Kernel name: %s\n",
			info->kname);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Instance(CU) name: %s\n",
			info->iname);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "CU address: 0x%llx\n",
			info->addr);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "CU index: %d\n",
			info->cu_idx);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Protocol: %s\n",
			prot2str(info->protocol));
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Interrupt cap: %d\n",
			info->intr_enable);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Interrupt ID:  %d\n",
			info->intr_id);

	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "--- Arguments ---\n");
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "Number of arguments: %d\n",
			info->num_args);
	for (i = 0; i < info->num_args; i++) {
		if (info->args[i].dir == DIR_INPUT)
			strcpy(dir, "input");
		else if (info->args[i].dir == DIR_OUTPUT)
			strcpy(dir, "output");
		else
			strcpy(dir, "unknown");

		sz += scnprintf(buf+sz, PAGE_SIZE - sz, "arg name: %s\n",
				info->args[i].name);
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, "  size: %d\n",
				info->args[i].size);
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, "  offset: 0x%x\n",
				info->args[i].offset);
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, "  direction: %s\n",
				dir);
	}

	if (sz < PAGE_SIZE - 1)
		buf[sz++] = 0;
	else
		buf[PAGE_SIZE - 1] = 0;


	return sz;
}
