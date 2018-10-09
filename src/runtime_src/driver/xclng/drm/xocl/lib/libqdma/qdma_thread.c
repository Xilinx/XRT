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

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

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
#if 0 /* This logic is not required as wb thread is always waking up wrk_thrd
	when there are any avaialable descriptors and there is any cb in
	wrk_list*/
	struct qdma_descq *descq;
	struct qdma_sgt_req_cb *cb, *tmp;
	int work = 0;

	descq = list_entry(work_item, struct qdma_descq, wrkthp_list);

	lock_descq(descq);
	list_for_each_entry_safe(cb, tmp, &descq->work_list, list) {
		struct qdma_request *req = (struct qdma_request *)cb;

		if (((req->count == 0) ||
			(cb->offset < req->count)) && descq->avail)
			work += 1;
	}
	unlock_descq(descq);
	return work;
#else
	return false;
#endif
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
			qdma_sgt_req_done(descq);
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
	struct qdma_sgt_req_cb *cb, *tmp;
	int pend = 0;

	lock_descq(descq);
	pend = !list_empty(&descq->pend_list);
	if (!pend) {
		list_for_each_entry_safe(cb, tmp, &descq->work_list, list) {
			if (cb->req_state == QDMA_REQ_SUBMIT_PARTIAL) {
				pend = true;
				break;
			}
		}
	}
	unlock_descq(descq);

	return pend;
}

static int qdma_thread_wb_proc(struct list_head *work_item)
{
	struct qdma_descq *descq;

	descq = list_entry(work_item, struct qdma_descq, wbthp_list);
	qdma_descq_service_wb(descq, 0, 1);
	return 0;
}

/* ********************* public function definitions ************************ */

void qdma_thread_remove_work(struct qdma_descq *descq)
{
	struct qdma_kthread *rq_thread, *cmpl_thread;

	lock_descq(descq);
	rq_thread = descq->wrkthp;
	cmpl_thread = descq->wbthp;

	pr_debug("%s removing workload from thread %s, %s\n",
			descq->conf.name,
			rq_thread ? rq_thread->name : "?",
			cmpl_thread ? cmpl_thread->name : "?");

	descq->wbthp = NULL;
	descq->wrkthp = NULL;
	unlock_descq(descq);

	if (rq_thread) {
		lock_thread(rq_thread);
		list_del(&descq->wrkthp_list);
		rq_thread->work_cnt--;
		unlock_thread(rq_thread);
	}

	if (cmpl_thread) {
		lock_thread(cmpl_thread);
		list_del(&descq->wbthp_list);
		cmpl_thread->work_cnt--;
		unlock_thread(cmpl_thread);
	}
}

void qdma_thread_add_work(struct qdma_descq *descq)
{
	struct qdma_kthread *rq_thread = wrk_threads;
	struct qdma_kthread *cmpl_thread = NULL;
	unsigned int v = 0;
	int i, idx = thread_cnt;

	for (i = 0; i < thread_cnt; i++, rq_thread++) {
		lock_thread(rq_thread);
		if (idx == thread_cnt) {
			v = rq_thread->work_cnt;
			idx = i;
		} else if (!rq_thread->work_cnt) {
			idx = i;
			unlock_thread(rq_thread);
			break;
		} else if (rq_thread->work_cnt < v)
			idx = i;
		unlock_thread(rq_thread);
	}

	rq_thread = wrk_threads + idx;
	lock_thread(rq_thread);
	list_add_tail(&descq->wrkthp_list, &rq_thread->work_list);
	rq_thread->work_cnt++;
	unlock_thread(rq_thread);

	if (descq->xdev->conf.poll_mode) {	/* Polled mode only */
		cmpl_thread = wb_threads + (thread_cnt - idx - 1);
		lock_thread(cmpl_thread);
		list_add_tail(&descq->wbthp_list, &cmpl_thread->work_list);
		cmpl_thread->work_cnt++;
		unlock_thread(cmpl_thread);
	}

	pr_debug("%s 0x%p assigned to thread %s,%u, %s,%u.\n",
		descq->conf.name, descq, rq_thread->name, rq_thread->work_cnt,
		cmpl_thread ? cmpl_thread->name : "?",
		cmpl_thread ? cmpl_thread->work_cnt : 0);
	lock_descq(descq);
	descq->wrkthp = rq_thread;
	descq->wbthp = cmpl_thread;
	unlock_descq(descq);
}

int qdma_threads_create(unsigned int num_threads)
{
	struct qdma_kthread *thp;
	int i;
	int rv;

	if (thread_cnt) {
		pr_warn("threads already created!");
		return 0;
	}

	thread_cnt =  (num_threads == 0) ? num_online_cpus() : num_threads;

	wrk_threads = kzalloc(thread_cnt * 2 *
					sizeof(struct qdma_kthread),
					GFP_KERNEL);
	if (!wrk_threads)
		return -ENOMEM;

	wb_threads = wrk_threads + thread_cnt;

	thp = wrk_threads;
	/* N dma request threads */
	for (i = 0; i < thread_cnt; i++, thp++) {
		thp->cpu = i;
		thp->timeout = 0;
		rv = qdma_kthread_start(thp, "qdma_rq_th", i);
		thp->fproc = qdma_thread_wrk_proc;
		thp->fpending = qdma_thread_wrk_pend;
	}

	/* N dma writeback monitoring threads */
	thp = wb_threads;
	for (i = 0; i < thread_cnt; i++, thp++) {
		thp->cpu = i;
		thp->timeout = 0;
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
