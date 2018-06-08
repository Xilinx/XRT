/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include "thread.h"

#include <linux/kernel.h>

/*
 * kernel thread function wrappers
 */
int qdma_kthread_dump(struct qdma_kthread *thp, char *buf, int buflen,
			int detail)
{
	int len = 0;

	if (!buf || !buflen)
		return 0;

	lock_thread(thp);
	len += sprintf(buf + len, "%s, cpu %u, work %u.\n",
			thp->name, thp->cpu, thp->work_cnt);

	if (detail) {
		;
	}
	unlock_thread(thp);

	buf[len] = '\0';
	return len;
}

static inline int xthread_work_pending(struct qdma_kthread *thp)
{
	struct list_head *work_item;

	/* any work items assigned to this thread? */
	if (list_empty(&thp->work_list)) {
		return 0;
	}

	/* any work item has pending work to do? */
	list_for_each(work_item, &thp->work_list) {
		if (thp->fpending && thp->fpending(work_item)) {
			return 1;
		}
	}
	return 0;
}

static inline void xthread_reschedule(struct qdma_kthread *thp) {
	if (thp->timeout) {
		pr_debug("%s rescheduling for %u seconds",
				thp->name, thp->timeout);
		schedule_timeout(thp->timeout * HZ);
	} else {
		pr_debug("%s rescheduling", thp->name);
		schedule();
	}
}

static int xthread_main(void *data)
{
	struct qdma_kthread *thp = (struct qdma_kthread *)data;
	DECLARE_WAITQUEUE(wait, current);

	pr_debug("%s UP.\n", thp->name);

	disallow_signal(SIGPIPE);

	if (thp->finit)
		thp->finit(thp);

	add_wait_queue(&thp->waitq, &wait);

	while (!kthread_should_stop()) {

		struct list_head *work_item;

		__set_current_state(TASK_INTERRUPTIBLE);
		pr_debug("%s interruptible\n", thp->name);

		/* any work to do? */
		lock_thread(thp);
		if (!xthread_work_pending(thp)) {
			unlock_thread(thp);
			xthread_reschedule(thp);
			lock_thread(thp);
		}

		__set_current_state(TASK_RUNNING);
		pr_debug("%s processing %u work items\n",
				thp->name, thp->work_cnt);
		/* do work */
		list_for_each(work_item, &thp->work_list) {
			thp->fproc(work_item);
		}
		unlock_thread(thp);
		schedule(); /* yield */
	}

	pr_debug("%s, work done.\n", thp->name);

	remove_wait_queue(&thp->waitq, &wait);

	if (thp->fdone)
		thp->fdone(thp);

	pr_debug("%s, exit.\n", thp->name);
	return 0;
}

int qdma_kthread_start(struct qdma_kthread *thp, char *name, int id)
{
	int len;

	if (thp->task) {
		pr_info("kthread %s task already running?\n", thp->name);
		return -EINVAL;
	}

#ifdef __QDMA_VF__
	len = sprintf(thp->name, "%s_vf_%d", name, id);
#else
	len = sprintf(thp->name, "%s%d", name, id);
#endif
	thp->name[len] = '\0';
	thp->id = id;

	spin_lock_init(&thp->lock);
	INIT_LIST_HEAD(&thp->work_list);
	init_waitqueue_head(&thp->waitq);		

	thp->task = kthread_create_on_node(xthread_main, (void *)thp,
					cpu_to_node(thp->cpu), "%s", thp->name);
	if (IS_ERR(thp->task)) {
		pr_err("kthread %s, create task failed: 0x%lx\n",
			thp->name, (unsigned long)IS_ERR(thp->task));
		thp->task = NULL;
		return -EFAULT;
	}

	kthread_bind(thp->task, thp->cpu);

	pr_info("kthread 0x%p, %s, cpu %u, 0x%p.\n",
		thp, thp->name, thp->cpu, thp->task);

	wake_up_process(thp->task);
	return 0;
}

int qdma_kthread_stop(struct qdma_kthread *thp)
{
	int rv;

	if (!thp->task) {
		pr_info("kthread %s, already stopped.\n", thp->name);
		return 0;
	}

	rv = kthread_stop(thp->task);
	if (rv < 0) {
		pr_info("kthread %s, stop err %d.\n", thp->name, rv);
		return rv;
	}

	pr_info("kthread %s, 0x%p, stopped.\n", thp->name, thp->task);
	thp->task = NULL;

	return 0;
}

