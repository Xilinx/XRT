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

#include "qdma_thread.h"

#include <linux/kernel.h>

#include "qdma_descq.h"
#include "thread.h"
#include "xdev.h"

/* ********************* global variables *********************************** */

static unsigned int thread_cnt;
static struct qdma_kthread *wrk_threads;
static struct qdma_kthread *wb_threads;

/* ********************* static function declarations *********************** */

static int qdma_thread_wrk_pend(struct list_head *work_item);
static int qdma_thread_wrk_proc(struct list_head *work_item);
static int qdma_thread_wb_pend(struct list_head *work_item);
static int qdma_thread_wb_proc(struct list_head *work_item);

/* ********************* static function definitions ************************ */

static int qdma_thread_wrk_pend(struct list_head *work_item)
{
	struct qdma_descq *descq;
	struct qdma_sgt_req_cb *cb, *tmp;
	int work = 0;

	descq = list_entry(work_item, struct qdma_descq, wrkthp_list);

	lock_descq(descq);
	list_for_each_entry_safe(cb, tmp, &descq->work_list, list) {
		struct qdma_sg_req *req = (struct qdma_sg_req *)cb;

		if (cb->offset < req->count && descq->avail) {
			work += 1;
		}
	}
	unlock_descq(descq);
	return work;
}

static int qdma_thread_wrk_proc(struct list_head *work_item)
{
	struct qdma_descq *descq;
	struct qdma_sgt_req_cb *cb, *tmp;
	int rv;

	descq = list_entry(work_item, struct qdma_descq, wrkthp_list);

	lock_descq(descq);
	list_for_each_entry_safe(cb, tmp, &descq->work_list, list) {
		pr_debug("descq %s, wrk 0x%p.\n", descq->conf.name, cb);
		rv = qdma_descq_proc_sgt_request(descq, cb);
		if (rv < 0) { /* failed, return */
			qdma_sgt_req_done(cb, rv);
		}
		if (!descq->avail)
			break;
	}
	unlock_descq(descq);

	return 0;
}

static int qdma_thread_wb_pend(struct list_head *work_item)
{
	struct qdma_descq *descq = list_entry(work_item, struct qdma_descq,
						wbthp_list);
	int pend = 0;

	lock_descq(descq);
	pend = !list_empty(&descq->pend_list);
	unlock_descq(descq);

	return pend;
}

static int qdma_thread_wb_proc(struct list_head *work_item)
{
	struct qdma_descq *descq;

	descq = list_entry(work_item, struct qdma_descq, wbthp_list);
	qdma_descq_service_wb(descq);
	return 0;
}

/* ********************* public function definitions ************************ */

void qdma_thread_remove_work(struct qdma_descq *descq)
{
	struct qdma_kthread *thp, *thp_wb;

	lock_descq(descq);
	thp = descq->wrkthp;
	thp_wb = descq->wbthp;

	pr_debug("%s 0x%p, thread %s, %s.\n", descq->conf.name, descq,
		thp ? thp->name : "?", thp_wb ? thp_wb->name : "?");

	descq->wbthp = NULL;
	descq->wrkthp = NULL;
	unlock_descq(descq);

	if (thp) {
		lock_thread(thp);
		list_del(&descq->wrkthp_list);
		thp->work_cnt--;
		unlock_thread(thp);
	}

	if (thp_wb) {
		/* Polled mode */
		lock_thread(thp_wb);
		list_del(&descq->wbthp_list);
		thp_wb->work_cnt--;
		unlock_thread(thp_wb);
	} else {
		/* Interrupt mode */
		unsigned long flags;

		spin_lock_irqsave(&descq->xdev->lock, flags);
		list_del(&descq->intr_list);
		spin_unlock_irqrestore(&descq->xdev->lock, flags);
	}
}

void qdma_thread_add_work(struct qdma_descq *descq)
{
	struct qdma_kthread *thp = wrk_threads;
	struct qdma_kthread *thp_wb = NULL;
	unsigned int v = 0;
	int i, idx = thread_cnt;

	for (i = 0; i < thread_cnt; i++, thp++) {
		lock_thread(thp);
		if (idx == thread_cnt) {
			v = thp->work_cnt;
			idx = i;
		} else if (!thp->work_cnt) {
			idx = i;
			unlock_thread(thp);
			break;
		} else if (thp->work_cnt < v)
			idx = i;
		unlock_thread(thp);
	}

	thp = wrk_threads + idx;
	lock_thread(thp);
	list_add_tail(&descq->wrkthp_list, &thp->work_list);
	thp->work_cnt++;
	unlock_thread(thp);

	if (descq->xdev->num_vecs) {
		/* Interrupt mode */
		unsigned long flags;

		spin_lock_irqsave(&descq->xdev->lock, flags);
		list_add_tail(&descq->intr_list,
				&descq->xdev->intr_list[descq->intr_id]);
		spin_unlock_irqrestore(&descq->xdev->lock, flags);
	} else {
		/* Polled mode */
		thp_wb = wb_threads + (thread_cnt - idx - 1);
		lock_thread(thp_wb);
		list_add_tail(&descq->wbthp_list, &thp_wb->work_list);
		thp_wb->work_cnt++;
		unlock_thread(thp_wb);
	}

	lock_descq(descq);
	pr_info("%s 0x%p assigned to thread %s,%u, %s,%u.\n",
		descq->conf.name, descq, thp->name, thp->work_cnt,
		thp_wb ? thp_wb->name : "?",
		thp_wb ? thp_wb->work_cnt : 0);
	descq->wrkthp = thp;
	descq->wbthp = thp_wb;
	unlock_descq(descq);
}

int qdma_threads_create(void)
{
	struct qdma_kthread *thp;
	int i;
	int rv;

	if (thread_cnt)
		return 0;

	thread_cnt =  num_online_cpus();
	pr_info("online cpu %u.\n", thread_cnt);

	wrk_threads = kzalloc(thread_cnt * 2 *
					sizeof(struct qdma_kthread),
					GFP_KERNEL);
	if (!wrk_threads)
		return -ENOMEM;

	wb_threads = wrk_threads + thread_cnt;

	thp = wrk_threads;
	/* N dma submission threads */
	for (i = 0; i < thread_cnt; i++, thp++) {
		thp->cpu = i;
		thp->timeout = 0;
		rv = qdma_kthread_start(thp, "qdma_wrk_th", i);
		thp->fproc = qdma_thread_wrk_proc;
		thp->fpending = qdma_thread_wrk_pend;
	}

	/* N dma writeback monitoring threads */
	thp = wb_threads;
	for (i = 0; i < thread_cnt; i++, thp++) {
		thp->cpu = i;
		thp->timeout = 5;
		rv = qdma_kthread_start(thp, "qdma_wb_th", i);
		if (rv < 0)
			goto cleanup_wrk_threads;
		thp->fproc = qdma_thread_wb_proc;
		thp->fpending = qdma_thread_wb_pend;
	}

	return 0;

cleanup_wrk_threads:
	if (wrk_threads) {
		kfree(wrk_threads);
		wrk_threads = NULL;
		wb_threads = NULL;
	}
	thread_cnt = 0;
	return rv;
}

void qdma_threads_destroy(void)
{
	int i;
	struct qdma_kthread *thp;

	if (!thread_cnt)
		return;

	thp = wrk_threads;
	/* N dma submission threads */
	for (i = 0; i < thread_cnt; i++, thp++)
		qdma_kthread_stop(thp);
	/* N dma writeback monitoring threads */
	thp = wb_threads;
	for (i = 0; i < thread_cnt; i++, thp++)
		qdma_kthread_stop(thp);

	kfree(wrk_threads);
	wrk_threads = NULL;
	wb_threads = NULL;
	thread_cnt = 0;
}
