/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/delay.h>
#include "kds_client.h"
#include "xrt_cu.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void cu_timer(unsigned long data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
#else
static void cu_timer(struct timer_list *t)
{
	struct xrt_cu *xcu = from_timer(xcu, t, timer);
#endif

	xcu_dbg(xcu, "%s tick\n", xcu->info.iname);

	atomic_inc(&xcu->tick);

	mod_timer(&xcu->timer, jiffies + CU_TIMER);
}

static inline struct kds_client *
first_event_client_or_null(struct xrt_cu *xcu)
{
	struct kds_client *curr = NULL;

	if (list_empty(&xcu->events))
		return NULL;

	mutex_lock(&xcu->ev_lock);
	if (list_empty(&xcu->events))
		goto done;

	curr = list_first_entry(&xcu->events, struct kds_client, ev_entry);

done:
	mutex_unlock(&xcu->ev_lock);
	return curr;
}

static inline void
move_to_queue(struct kds_command *xcmd, struct list_head *dst_q, u32 *dst_len)
{
	list_move_tail(&xcmd->list, dst_q);
	++(*dst_len);
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
		xcmd->cb.notify_host(xcmd, xcmd->status);
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
	struct kds_command *next;
	struct kds_client *ev_client = NULL;
	unsigned int tick;
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

	/* Sometimes a CU done but it doesn't ready for new command.
	 * In this case, sq could be empty.
	 */
	if (!xcu->num_sq)
		return;

	BUG_ON(xcu->num_sq < xcu->done_cnt);

	ev_client = first_event_client_or_null(xcu);
	list_for_each_entry_safe(xcmd, next, &xcu->sq, list) {
		if (xcu->done_cnt) {
			/* Done command has priority */
			xcmd->status = KDS_COMPLETED;
			--xcu->done_cnt;
		} else if (unlikely(ev_client)) {
			/* Client event happens rarely */
			if (xcmd->client != ev_client)
				continue;

			tick = atomic_read(&xcu->tick);
			/* Record CU tick to start timeout counting */
			if (!xcmd->tick) {
				xcmd->tick = tick;
				continue;
			}

			/* If xcmd haven't timeout */
			if (tick - xcmd->tick < CU_EXEC_DEFAULT_TTL)
				continue;

			xcmd->status = KDS_TIMEOUT;
			/* Mark this CU as bad state */
			xcu->bad_state = true;
		} else
			break;

		move_to_queue(xcmd, &xcu->cq, &xcu->num_cq);
		--xcu->num_sq;
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
	struct kds_client *ev_client;
	struct list_head *dst_q;
	int *dst_len;

	if (!xcu->num_rq)
		return 0;

	xcmd = list_first_entry(&xcu->rq, struct kds_command, list);

	ev_client = first_event_client_or_null(xcu);
	if (unlikely(xcu->bad_state || (ev_client == xcmd->client))) {
		xcmd->status = KDS_ABORT;
		dst_q = &xcu->cq;
		dst_len = &xcu->num_cq;
		goto move_cmd;
	}

	if (!xrt_cu_get_credit(xcu))
		return 0;

	/* if successfully get credit, you must start cu */
	xrt_cu_config(xcu, (u32 *)xcmd->info, xcmd->isize, xcmd->payload_type);
	xrt_cu_start(xcu);

	dst_q = &xcu->sq;
	dst_len = &xcu->num_sq;
	/* ktime_get_* is still heavy. This impact ~20% of IOPS on echo mode.
	 * For some sort of CU, which usually has a relative long execute time,
	 * we could hide this overhead and provide timestamp for each command.
	 * But this implementation is used for general purpose. Please create CU
	 * specific thread if needed.
	 */
	//xcmd->start = ktime_get_raw_fast_ns();
move_cmd:
	move_to_queue(xcmd, dst_q, dst_len);
	--xcu->num_rq;

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

int xrt_cu_polling_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	int ret = 0;
	int loop_cnt = 0;

	xcu_info(xcu, "CU[%d] start", xcu->info.cu_idx);
	mod_timer(&xcu->timer, jiffies + CU_TIMER);
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

		/* The idea is when CU's credit is less than busy threshold,
		 * sleep a while to wait for CU completion.
		 * The interval is configurable and it should be determin by
		 * kernel execution.
		 * If threshold is -1, then this is a busy loop to check CU
		 * status.
		 */
		if (xrt_cu_peek_credit(xcu) <= xcu->busy_threshold)
			usleep_range(xcu->interval_min, xcu->interval_max);

		/* TODO: Without schedule(), we see unexpected stuck when a host
		 * application is killed or randomly call exit. This can be
		 * reproduced with a slow kernel, which take more than 1 second
		 * to execute one command. Phenomenon observed so far:
		 * 1. Let host code random exit when commands are still running.
		 * 2. strace shows exit_group systemcall on the screen
		 * 3. stuck for a while
		 * 4. dmesg shows drm_release is called
		 *
		 * Need more efforts to re-compile linux kernel to debug.
		 * Will check this later.
		 */
		/* Avoid large num_rq leads to more 120 sec blocking */
		if (++loop_cnt == 8) {
			loop_cnt = 0;
			schedule();
		}

		/* Continue until run queue empty */
		if (xcu->num_rq)
			continue;

		if (!xcu->num_sq && !xcu->num_cq) {
			loop_cnt = 0;
			xcu->sleep_cnt++;
			if (down_interruptible(&xcu->sem))
				ret = -ERESTARTSYS;
		}

		process_pq(xcu);
	}
	del_timer_sync(&xcu->timer);

	if (xcu->bad_state)
		ret = -EBUSY;

	xcu_info(xcu, "CU[%d] thread end, bad state %d\n",
		 xcu->info.cu_idx, xcu->bad_state);
	return ret;
}

int xrt_cu_intr_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	int ret = 0;
	int loop_cnt = 0;

	xcu_info(xcu, "CU[%d] start", xcu->info.cu_idx);
	mod_timer(&xcu->timer, jiffies + CU_TIMER);
	xrt_cu_enable_intr(xcu, CU_INTR_DONE | CU_INTR_READY);
	while (!xcu->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
		 */
		if (process_rq(xcu))
			continue;

		if (xcu->num_sq || is_zero_credit(xcu)) {
			xrt_cu_check(xcu);
			if (!xcu->done_cnt || !xcu->ready_cnt) {
				xcu->sleep_cnt++;
				/* Don't use down_interruptible() here.
				 * If CU hang, this thread would keep waiting.
				 * Host application is not able to exit since
				 * there are outstading commands.
				 *
				 * CU_TIMER is runing at low frequence. For
				 * normal CU, it will unlikely timeout.
				 */
				if (down_timeout(&xcu->sem_cu, CU_TIMER))
					ret = -ERESTARTSYS;
				xrt_cu_check(xcu);
			}
			__process_sq(xcu);
		}

		process_cq(xcu);

		/* Avoid large num_rq leads to more 120 sec blocking */
		if (++loop_cnt == 8) {
			loop_cnt = 0;
			schedule();
		}

		/* Continue until run queue empty */
		if (xcu->num_rq)
			continue;

		if (!xcu->num_sq && !xcu->num_cq) {
			loop_cnt = 0;
			if (down_interruptible(&xcu->sem))
				ret = -ERESTARTSYS;
		}

		process_pq(xcu);
	}
	xrt_cu_disable_intr(xcu, CU_INTR_DONE | CU_INTR_READY);
	del_timer_sync(&xcu->timer);

	if (xcu->bad_state)
		ret = -EBUSY;

	xcu_info(xcu, "CU[%d] thread end, bad state %d\n",
		 xcu->info.cu_idx, xcu->bad_state);
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
 * xrt_cu_abort() - Set an abort event in the CU for client
 * @xcu: Target XRT CU
 * @client: The client tries to abort commands
 *
 * This is used to ask CU thread to abort all commands from the client.
 */
void xrt_cu_abort(struct xrt_cu *xcu, struct kds_client *client)
{
	struct kds_client *curr;

	mutex_lock(&xcu->ev_lock);
	if (list_empty(&xcu->events))
		goto add_event;

	/* avoid re-add the same client */
	list_for_each_entry(curr, &xcu->events, ev_entry) {
		if (client == curr)
			goto done;
	}

add_event:
	client->ev_type = EV_ABORT;
	list_add_tail(&client->ev_entry, &xcu->events);
done:
	mutex_unlock(&xcu->ev_lock);
}

/**
 * xrt_cu_abort_done() - Let CU know client asked abort is done
 * @xcu: Target XRT CU
 * @client: The client tries to abort commands
 *
 */
bool xrt_cu_abort_done(struct xrt_cu *xcu, struct kds_client *client)
{
	struct kds_client *curr;
	struct kds_client *next;

	mutex_lock(&xcu->ev_lock);
	if (list_empty(&xcu->events))
		goto done;

	list_for_each_entry_safe(curr, next, &xcu->events, ev_entry) {
		if (client != curr)
			continue;

		list_del(&curr->ev_entry);
		break;
	}

done:
	mutex_unlock(&xcu->ev_lock);

	return xcu->bad_state;
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
	atomic_set(&xcu->tick, 0);
	xcu->thread = kthread_run(cu_thread, xcu, xcu->info.iname);
	if (IS_ERR(xcu->thread)) {
		err = IS_ERR(xcu->thread);
		xcu_err(xcu, "Create CU thread failed, err %d\n", err);
	}

	return err;
}

/*
 * If KDS has to manage PLRAM resources, we should come up with a better design.
 * Ideally, CU subdevice should request for cmdmem resource instead of KDS assign
 * cmdmem resource to CU.
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
		cu_fa->cmdmem = NULL;
		cu_fa->paddr = 0;
		cu_fa->slot_sz = 0;
		cu_fa->num_slots = 0;
		return 0;
	}

	slot_size = cu_fa_get_desc_size(xcu);

	cu_fa->cmdmem = vaddr;
	cu_fa->paddr = dev;
	cu_fa->slot_sz = slot_size;
	cu_fa->num_slots = num_slots;

	cu_fa->credits = (cu_fa->num_slots < cu_fa->max_credits)?
			 cu_fa->num_slots : cu_fa->max_credits;

	return 0;
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

	mutex_init(&xcu->ev_lock);
	INIT_LIST_HEAD(&xcu->events);
	/* default timeout, 0 means infinity */
	xcu->run_timeout = 0;
	sema_init(&xcu->sem, 0);
	sema_init(&xcu->sem_cu, 0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	setup_timer(&xcu->timer, cu_timer, (unsigned long)xcu);
#else
	timer_setup(&xcu->timer, cu_timer, 0);
#endif
	atomic_set(&xcu->tick, 0);
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
			xrt_cu_peek_credit(xcu));
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
