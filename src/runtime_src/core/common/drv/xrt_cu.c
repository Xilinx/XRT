/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Unify CU Model
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/delay.h>
#include <linux/math64.h>
#include "kds_client.h"
#include "xrt_cu.h"

inline void xrt_cu_circ_produce(struct xrt_cu *xcu, u32 stage, uintptr_t cmd)
{
	unsigned long head = xcu->crc_buf.head;
	unsigned long tail = xcu->crc_buf.tail;
	struct xrt_cu_log *item;

	if (!xcu->debug)
		return;

	/* Will overwrite oldest data if buffer is full */
	item = (struct xrt_cu_log *)&xcu->crc_buf.buf[head];
	item->stage = stage;
	item->cmd_id = cmd;
	item->ts = ktime_to_ns(ktime_get());

	if (CIRC_SPACE(head, tail, CIRC_BUF_SIZE) < sizeof(struct xrt_cu_log)) {
		tail += sizeof(struct xrt_cu_log);
		if (tail >= CIRC_BUF_SIZE)
			tail = 0;
	}

	head += sizeof(struct xrt_cu_log);
	if (head >= CIRC_BUF_SIZE)
		head = 0;

	xcu->crc_buf.head = head;
	xcu->crc_buf.tail = tail;
}

ssize_t xrt_cu_circ_consume_all(struct xrt_cu *xcu, char *buf, size_t size)
{
	unsigned long head = xcu->crc_buf.head;
	unsigned long tail = xcu->crc_buf.tail;
	size_t cnt = CIRC_CNT(head, tail, CIRC_BUF_SIZE);
	/* return min(count to the end of buffer, CIRC_CNT()) */
	size_t cnt_to_end = CIRC_CNT_TO_END(head, tail, CIRC_BUF_SIZE);
	size_t nread = 0;

	if (size < cnt)
		nread = size;
	else
		nread = cnt;

	if (nread <= cnt_to_end) {
		memcpy(buf, xcu->crc_buf.buf + tail, nread);
		xcu->crc_buf.tail += nread;
	} else {
		/* Needs two times of copy */
		memcpy(buf, xcu->crc_buf.buf + tail, cnt_to_end);
		memcpy(buf + cnt_to_end, xcu->crc_buf.buf, nread - cnt_to_end);
		xcu->crc_buf.tail = nread - cnt_to_end;
	}

	return nread;
}

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

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void cu_stats_timer(unsigned long data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
#else
static void cu_stats_timer(struct timer_list *t)
{
	struct xrt_cu *xcu = from_timer(xcu, t, stats.stats_timer);
#endif
	unsigned long   flags;

	if (!xcu->stats.stats_enabled)
		return;

	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	xcu->stats.stats_tick++;
	xrt_cu_incr_sq_count(xcu);
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);
	mod_timer(&xcu->stats.stats_timer, jiffies + CU_STATS_TIMER);
}

static void xrt_cu_switch_to_interrupt(struct xrt_cu *xcu)
{
	xrt_cu_enable_intr(xcu, CU_INTR_DONE | CU_INTR_READY);
	xcu->interrupt_used = 1;
}

static void xrt_cu_switch_to_poll(struct xrt_cu *xcu)
{
	xrt_cu_disable_intr(xcu, CU_INTR_DONE | CU_INTR_READY);
	xcu->interrupt_used = 0;
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
	if (dst_q)
		list_move_tail(&xcmd->list, dst_q);
	++(*dst_len);
}

static bool abort_client(struct kds_command *xcmd, void *cond)
{
	void *client = cond;

	return (xcmd->client == client)? true : false;
}

static bool abort_handle(struct kds_command *xcmd, void *cond)
{
	u32 handle = *(u32 *)cond;

	return (xcmd->exec_bo_handle == handle)? true : false;
}

static inline void xrt_cu_init_ecmd_and_sq_count(struct xrt_cu *xcu)
{
	unsigned long flags;

	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	xcu->stats.sq_total = 0;
	xcu->stats.sq_count = 0;
	xcu->stats.usage_curr = 0;
	xcu->stats.usage_prev = 0;
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);
}

static inline void xrt_cu_incr_ecmd_count(struct xrt_cu *xcu)
{
	unsigned long flags;

	if (!xcu->stats.stats_enabled)
		return;

	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	xcu->stats.usage_curr += 1;
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);
}

static inline void xrt_cu_reset_sq_count(struct xrt_cu *xcu)
{
	unsigned long flags;

	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	xcu->stats.sq_total = 0;
	xcu->stats.sq_count = 0;
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);
}

void xrt_cu_incr_sq_count(struct xrt_cu *xcu)
{
	if (!xcu->stats.stats_enabled)
		return;

	xcu->stats.sq_total += xcu->num_sq;
	xcu->stats.sq_count += 1;
}

static inline void xrt_cu_get_time(u64 *time)
{
	*time = ktime_to_ns(ktime_get());
}

static inline void xrt_cu_idle_start(struct xrt_cu *xcu)
{
	unsigned long flags;

	if (!xcu->stats.stats_enabled)
		return;

	if (xcu->num_sq > 0 || xcu->num_rq > 0 || xcu->num_pq > 0)
		return;
	
	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	xrt_cu_get_time(&xcu->stats.idle_start);
	xcu->stats.idle = 1;
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);
}

static inline void xrt_cu_idle_end(struct xrt_cu *xcu)
{
	unsigned long flags;

	if (!xcu->stats.stats_enabled)
		return;

	if (xcu->num_pq == 0 && xcu->num_sq == 0 && xcu->num_rq == 0)
		return;

	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	if (xcu->stats.idle) {
		xrt_cu_get_time(&xcu->stats.idle_end);
		xcu->stats.idle_total += xcu->stats.idle_end - xcu->stats.idle_start;
		xcu->stats.idle = 0;
	}
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);
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
		set_xcmd_timestamp(xcmd, xcmd->status);
		xrt_cu_circ_produce(xcu, CU_LOG_STAGE_CQ, (uintptr_t)xcmd);
		xcmd->cb.notify_host(xcmd, xcmd->status);
		xrt_cu_incr_ecmd_count(xcu);
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
	struct kds_client *ev_client = NULL;
	unsigned int tick;

	/* CU is ready to accept more commands
	 * Return credits to allow submit more commands
	 */
	if (xcu->ready_cnt) {
		xrt_cu_put_credit(xcu, xcu->ready_cnt);
		xcu->ready_cnt = 0;
	}

	/* Sometimes a CU done but it doesn't ready for new command.
	 * In this case, sq could be empty.
	 */
	if (!xcu->num_sq)
		return;

	ev_client = first_event_client_or_null(xcu);
	if (unlikely(ev_client)) {
		tick = atomic_read(&xcu->tick);
		if (xcu->start_tick == 0) {
			xcu->start_tick = tick;
			goto get_complete_and_out;
		}

		if (tick - xcu->start_tick < CU_EXEC_DEFAULT_TTL)
			goto get_complete_and_out;

		if (xrt_cu_cmd_abort(xcu, ev_client, abort_client))
			xcu->bad_state = true;

		xcu->start_tick = 0;
	}

get_complete_and_out:
	xcmd = xrt_cu_get_complete(xcu);
	while (xcmd) {
		xrt_cu_circ_produce(xcu, CU_LOG_STAGE_SQ, (uintptr_t)xcmd);
		move_to_queue(xcmd, &xcu->cq, &xcu->num_cq);
		--xcu->num_sq;
		xcmd = xrt_cu_get_complete(xcu);
	}
	xrt_cu_idle_start(xcu);
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
	if (xrt_cu_submit_config(xcu, xcmd)) {
		xrt_cu_put_credit(xcu, 1);
		return 0;
	}
	xrt_cu_start(xcu);
	if (xcu->thread) {
		xcu->poll_count = 0;
		if (xcu->interrupt_used)
			xrt_cu_switch_to_poll(xcu);
	}
	set_xcmd_timestamp(xcmd, KDS_RUNNING);
	xrt_cu_circ_produce(xcu, CU_LOG_STAGE_RQ, (uintptr_t)xcmd);

	dst_q = NULL;
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
	if (xcu->stats.max_sq_length < xcu->num_sq)
		xcu->stats.max_sq_length = xcu->num_sq;
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

	xrt_cu_idle_end(xcu);
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

static inline void
try_abort_cmd(struct xrt_cu *xcu, struct kds_command *abort_cmd)
{
	struct kds_command *xcmd;
	struct kds_command *tmp;
	u32 handle;

	/* Never call this function on the performance critical path */
	handle = *(u32 *)abort_cmd->info;
	list_for_each_entry_safe(xcmd, tmp, &xcu->rq, list) {
		if (xcmd->exec_bo_handle != handle)
			continue;

		/* Found the xcmd to abort! */
		abort_cmd->status = KDS_COMPLETED;

		xcu_info(xcu, "Abort command(%d) on running queue", handle);
		xcmd->status = KDS_ABORT;
		move_to_queue(xcmd, &xcu->cq, &xcu->num_cq);
		--xcu->num_rq;
		return;
	}

	if (!xcu->num_sq)
		return;

	if (xrt_cu_cmd_abort(xcu, &handle, abort_handle)) {
		xcu->bad_state = true;
		abort_cmd->status = KDS_TIMEOUT;
	} else {
		abort_cmd->status = KDS_COMPLETED;
	}

	xcmd = xrt_cu_get_complete(xcu);
	while (xcmd) {
		move_to_queue(xcmd, &xcu->cq, &xcu->num_cq);
		--xcu->num_sq;
		xcmd = xrt_cu_get_complete(xcu);
	}
}

/**
 * process_hpq() - Process high priority queue
 * @xcu: Target XRT CU
 *
 */
static inline void process_hpq(struct xrt_cu *xcu)
{
	struct kds_command *xcmd;
	struct kds_command *tmp;
	unsigned long flags;

	if (!xcu->num_hpq)
		return;

	/* slowpath */
	spin_lock_irqsave(&xcu->hpq_lock, flags);
	if (!xcu->num_hpq)
		goto done;

	list_for_each_entry_safe(xcmd, tmp, &xcu->hpq, list) {
		if (xcmd->opcode == OP_ABORT)
			try_abort_cmd(xcu, xcmd);

		list_del(&xcmd->list);
		--xcu->num_hpq;
	}

done:
	spin_unlock_irqrestore(&xcu->hpq_lock, flags);
	complete(&xcu->comp);
}

int xrt_cu_process_queues(struct xrt_cu *xcu)
{
	/* Move pending commands to running queue */
	process_pq(xcu);

	/* Make sure to submit as many commands as possible */
	while (process_rq(xcu));

	/* process completed queue before submitted queue, for
	 * two reasons:
	 * - The last submitted command may be still running
	 * - while handling completed queue, running command might done
	 * - process_sq will check CU status, which is thru slow bus
	 */
	process_cq(xcu);
	process_sq(xcu);

	/* process rare commands with very high priority. For example
	 * abort command.
	 * If this kind of command don't not exist, this would be a very
	 * low overhead and should not impact performance.
	 * But if this kind of command exist, this would be a slow path.
	 */
	process_hpq(xcu);

	if (xcu->num_rq || xcu->num_sq || xcu->num_cq)
		return XCU_BUSY;

	return XCU_IDLE;
}

int xrt_cu_intr_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	int ret = 0;
	int loop_cnt = 0;

	xcu->interrupt_used = 0;
	xcu_info(xcu, "CU[%d] start", xcu->info.cu_idx);
	mod_timer(&xcu->timer, jiffies + CU_TIMER);
	while (!xcu->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
		 */
		if (process_rq(xcu))
			continue;

		process_cq(xcu);
		if (xcu->num_sq || is_zero_credit(xcu)) {
			if (!xcu->interrupt_used) {
				/* Use this special code to measure time of a loop.
				 * On APU, it takes about 2us on each loop.
				 *   xrt_cu_circ_produce(xcu, 5, 0);
				 */
				process_sq(xcu);
				xcu->poll_count++;
				/* If poll_count reach threshold, switch to
				 * interrupt mode.
				 */
				if (xcu->poll_count >= xcu->poll_threshold)
					xrt_cu_switch_to_interrupt(xcu);
			} else {
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
		}

		process_hpq(xcu);

		/* Avoid large num_rq leads to more 120 sec blocking */
		if (++loop_cnt == MAX_CU_LOOP) {
			loop_cnt = 0;
			schedule();
		}

		/* Continue until run queue empty */
		if (xcu->num_rq)
			continue;

		if (!xcu->num_sq && !xcu->num_cq) {
			loop_cnt = 0;
			/* Record CU status before sleep */
			if (!xcu->num_pq)
				xrt_cu_check_force(xcu);
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

void xrt_cu_hpq_submit(struct xrt_cu *xcu, struct kds_command *xcmd)
{
	unsigned long flags;

	/* This high priority queue is designed to handle those hight priority
	 * but low frequency commands. Like abort command.
	 */
	spin_lock_irqsave(&xcu->hpq_lock, flags);
	list_add_tail(&xcmd->list, &xcu->hpq);
	++xcu->num_hpq;
	spin_unlock_irqrestore(&xcu->hpq_lock, flags);
	up(&xcu->sem);

	wait_for_completion(&xcu->comp);
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
	if (!xrt_cu_intr_supported(xcu))
		return -ENOSYS;

	xrt_cu_stop_thread(xcu);

	if (!intr)
		return 0;

	return xrt_cu_start_thread(xcu);
}

bool xrt_cu_intr_supported(struct xrt_cu *xcu)
{
	/* Let's say CU on XGQ always support interrupt */
	if (xcu->info.model == XCU_XGQ)
		return true;

	/* Check if CU support interrupt in hardware */
	if (!xcu->info.intr_enable)
		return false;

	if (xrt_cu_get_protocol(xcu) == CTRL_NONE) {
		xcu_warn(xcu, "Interrupt enabled value should be false for ap_ctrl_none cu\n");
		return false;
	}

    return true;
}

int xrt_cu_start_thread(struct xrt_cu *xcu)
{
	int err = 0;

	if (xcu->thread) {
		xcu_warn(xcu, "CU thread started. Start again?\n");
		return 0;
	}

	/* launch new thread */
	xcu->stop = 0;
	sema_init(&xcu->sem, 0);
	sema_init(&xcu->sem_cu, 0);
	atomic_set(&xcu->tick, 0);
	xcu->thread = kthread_run(xrt_cu_intr_thread, xcu, xcu->info.iname);
	if (IS_ERR(xcu->thread)) {
		err = IS_ERR(xcu->thread);
		xcu->thread = NULL;
		xcu_err(xcu, "Create CU thread failed, err %d\n", err);
	}

	return err;
}

void xrt_cu_stop_thread(struct xrt_cu *xcu)
{
	if (!xcu->thread)
        return;

    xcu->stop = 1;
    up(&xcu->sem_cu);
    up(&xcu->sem);
    if (!IS_ERR(xcu->thread))
        (void) kthread_stop(xcu->thread);

    xcu->thread = NULL;
    sema_init(&xcu->sem, 0);
    sema_init(&xcu->sem_cu, 0);
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

int xrt_cu_get_protocol(struct xrt_cu *xcu)
{
	return xcu->info.protocol;
}

u32 xrt_cu_get_status(struct xrt_cu *xcu)
{
	return xcu->status;
}

int xrt_cu_regmap_size(struct xrt_cu *xcu)
{
	int max_off_idx = 0;
	int max_off = 0;
	int i;

	if (!xcu->info.num_args)
		return xcu->info.size;

	/* Max regmap size is max offset + register size */
	for (i = 0; i < xcu->info.num_args; i++) {
		if (max_off < xcu->info.args[i].offset) {
			max_off = xcu->info.args[i].offset;
			max_off_idx = i;
		}
	}

	return max_off + xcu->info.args[max_off_idx].size;
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
	/* Initialize completed queue */
	INIT_LIST_HEAD(&xcu->cq);

	mutex_init(&xcu->ev_lock);
	INIT_LIST_HEAD(&xcu->events);
	sema_init(&xcu->sem, 0);
	sema_init(&xcu->sem_cu, 0);
	spin_lock_init(&xcu->stats.xcs_lock);

	INIT_LIST_HEAD(&xcu->hpq);
	spin_lock_init(&xcu->hpq_lock);
	init_completion(&xcu->comp);

	xcu->crc_buf.head = 0;
	xcu->crc_buf.tail = 0;
	xcu->crc_buf.buf = xcu->log_buf;
	xrt_cu_init_ecmd_and_sq_count(xcu);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	setup_timer(&xcu->timer, cu_timer, (unsigned long)xcu);
	setup_timer(&xcu->stats.stats_timer, cu_stats_timer, (unsigned long)xcu);
#else
	timer_setup(&xcu->timer, cu_timer, 0);
	timer_setup(&xcu->stats.stats_timer, cu_stats_timer, 0);
#endif
	atomic_set(&xcu->tick, 0);
	xcu->start_tick = 0;
	xcu->thread = NULL;
	xcu->poll_threshold = CU_DEFAULT_POLL_THRESHOLD;
	xcu->interrupt_used = 0;

	mod_timer(&xcu->timer, jiffies + CU_TIMER);
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
	if (xcu->thread && !IS_ERR(xcu->thread))
		(void) kthread_stop(xcu->thread);

	del_timer_sync(&xcu->timer);
	del_timer_sync(&xcu->stats.stats_timer);
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
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "SW Resettable: %d\n",
			info->sw_reset);

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

ssize_t show_formatted_cu_stat(struct xrt_cu *xcu, char *buf)
{
	ssize_t 	   sz = 0;
	u32 		   in_flight;
	u32 		   max_running;
	u32 		   average_sq_len;
	u32                idle;
	u64 		   usage_curr;
	u64 		   cu_idle;
	u64 		   iops;
	u64 		   last_timestamp;
	u64                new_ts;
	u64                incre_ecmds;
	/* parameters for average sq length */
	u32 		   sq_total;
	u32 		   sq_count;
	u32                max_sq_length;
	/* parameters for idle percentage*/
	u64                idle_start;
	u64                last_read_idle_start;
	u64		   delta_idle_time;

	unsigned long      flags;
	char 		   *fmt = "%llu %llu %u %u %u %u %llu %llu\n";

	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	in_flight = xcu->num_sq;
	max_running = xcu->max_running;
	usage_curr = xcu->stats.usage_curr;
	sq_total = xcu->stats.sq_total;
	sq_count = xcu->stats.sq_count;
	max_sq_length = xcu->stats.max_sq_length;
	incre_ecmds = xcu->stats.usage_curr - xcu->stats.usage_prev;
	xcu->stats.usage_prev = xcu->stats.usage_curr;
	/* for idle percentage compute */
	idle_start = xcu->stats.idle_start;
	last_read_idle_start = xcu->stats.last_read_idle_start;
	delta_idle_time = xcu->stats.idle_total - xcu->stats.last_idle_total;
	idle = xcu->stats.idle;
	xrt_cu_get_time(&new_ts);
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);

	last_timestamp = xcu->stats.last_timestamp;
	average_sq_len = xrt_cu_get_average_sq(xcu, sq_total, sq_count);
	iops = xrt_cu_get_iops(xcu, last_timestamp, incre_ecmds, new_ts);
	cu_idle = xrt_cu_get_idle(xcu, last_timestamp, idle_start, last_read_idle_start, delta_idle_time, idle, new_ts);
	xcu->stats.last_timestamp = new_ts;

	sz += scnprintf(buf+sz, PAGE_SIZE - sz, fmt, usage_curr, incre_ecmds, 
			in_flight, average_sq_len, max_sq_length, max_running, iops, cu_idle);
	
	return sz;
}

ssize_t show_stats_begin(struct xrt_cu *xcu, char *buf)
{
	ssize_t		sz = 0;
	char 		*fmt = "stats_begin \n";
	unsigned long   flags;

	xcu->stats.stats_enabled = 1;
	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	xcu->stats.stats_tick = 0;
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);
	mod_timer(&xcu->stats.stats_timer, jiffies + CU_STATS_TIMER);

	sz += scnprintf(buf+sz, PAGE_SIZE - sz, fmt);

	return sz;
}

ssize_t show_stats_end(struct xrt_cu *xcu, char *buf)
{
	ssize_t sz = 0;
	char *fmt = "stats_end \n";

	xcu->stats.stats_enabled = 0;
	sz += scnprintf(buf+sz, PAGE_SIZE - sz, fmt);

	return sz;
}

u64 xrt_cu_get_idle(struct xrt_cu *xcu, u64 last_timestamp, u64 idle_start, u64 last_read_idle_start, u64 delta_idle_time, u32 idle, u64 new_ts)
{
	u64       	delta_xcu_time;	
	u64             cu_idle;
	u32             ts_status;
	unsigned long   flags;

	delta_xcu_time = new_ts - last_timestamp;

	if(idle == 1) {
		if (xcu->stats.last_ts_status) {
			cu_idle = delta_idle_time + new_ts - idle_start;
		} else {
			if (delta_idle_time == 0) {
				cu_idle = delta_xcu_time;
			} else {
				cu_idle = delta_idle_time - (last_timestamp - last_read_idle_start) + (new_ts - idle_start); 
			}
		}
		ts_status = 0;
	} else {
		if (xcu->stats.last_ts_status) {
			cu_idle = delta_idle_time;
		} else {
			cu_idle = delta_idle_time - (last_timestamp - last_read_idle_start);
		}
		ts_status = 1;
	}

	cu_idle = div64_u64(cu_idle * 100, delta_xcu_time);

	spin_lock_irqsave(&xcu->stats.xcs_lock, flags);
	xcu->stats.last_read_idle_start = idle_start;
	xcu->stats.last_idle_total = xcu->stats.idle_total;
	xcu->stats.last_ts_status = ts_status;
	spin_unlock_irqrestore(&xcu->stats.xcs_lock, flags);

	return cu_idle;
}

u64 xrt_cu_get_iops(struct xrt_cu *xcu, u64 last_timestamp, u64 incre_ecmds, u64 new_ts)
{
	u64 		iops = 0;

	if (new_ts - last_timestamp > 0 && incre_ecmds != 0)
		iops = div64_u64(incre_ecmds * 1000000000, (new_ts - last_timestamp));
	else
		iops = 0;

	return iops;
}

u64 xrt_cu_get_average_sq(struct xrt_cu *xcu, u32 sq_total, u32 sq_count)
{
	u32 		average_sq_len = 0;
	
	if (sq_total == 0 || sq_count == 0)
		return average_sq_len;

	average_sq_len = sq_total / sq_count;
	xrt_cu_reset_sq_count(xcu);
	
	return average_sq_len;
}
