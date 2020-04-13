/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/kthread.h>

#include "zocl_drv.h"
#include "kds_core.h"
#include "xrt_cu.h"

struct zocl_cu_ctrl {
	struct kds_controller    core;
	struct drm_zocl_dev	*zdev;
	struct xrt_cu		*xcus[MAX_CUS];
	/* TODO: Maybe rethink if we should use two threads,
	 * one for submit, one for complete
	 */
	struct task_struct     **threads;
	int			 num_cus;

};

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
	kds_free_command(done_xcmd);
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

static int cu_ctrl_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	unsigned long flags;

	/* The CU is not able to interrupt host
	 * this thread has to poll CU status, so
	 * let me do everything in a busy loop...
	 */
	while (1) {
		spin_lock_irqsave(&xcu->pq_lock, flags);
		if (xcu->num_pq > 0) {
			list_splice_tail_init(&xcu->pq, &xcu->rq);
			xcu->num_pq = 0;
		}
		spin_unlock_irqrestore(&xcu->pq_lock, flags);

		/* Do not change the priority! */
		if (!list_empty(&xcu->rq)) {
			/* No matter if sq is emptey or not */
			process_rq(xcu);
		} else if (!list_empty(&xcu->sq)) {
			process_sq_once(xcu);
		} else {
			/* TODO: looks like the timeout would impact IOPS
			 * maybe it depends on the system?
			 */
			while (down_timeout(&xcu->sem, 1000) == -ETIME) {
				if (kthread_should_stop())
					return 0;
			}
			/* Something interesting happened */
		}
	}

	return 0;
}

static int get_cu_by_addr(struct zocl_cu_ctrl *zcuc, u32 addr)
{
	int i;

	/* Do not use this search in critical path */
	for (i = 0; i < zcuc->num_cus; ++i) {
		if (zcuc->xcus[i]->info.addr == addr)
			break;
	}

	return i;
}

static inline int cu_mask_to_cu_idx(struct kds_command *xcmd)
{
	/* TODO: balance the CU usage if multiple bits are set */

	/* assume there is alwasy one CU */
	return 0;
}

static inline void stop_all_threads(struct zocl_cu_ctrl *zcuc)
{
	int i;

	for (i = 0; i < zcuc->num_cus; ++i) {
		if (zcuc->threads[i] != NULL) {
			kthread_stop(zcuc->threads[i]);
			zcuc->threads[i] = NULL;
		}
	}
}

static inline int launch_all_threads(struct zocl_cu_ctrl *zcuc)
{
	int i, err = 0;

	/* only launch one thread, it should be one thread per CU */
	for (i = 0; i < 1; ++i) {
		zcuc->threads[i] = kthread_run(cu_ctrl_thread,
					       (void *)zcuc->xcus[i],
					       "xcu_thread");
		if (IS_ERR(zcuc->threads[i])) {
			err = PTR_ERR(zcuc->threads[i]);
			zcuc->threads[i] = NULL;
			goto error;
		}
	}
	return 0;

error:
	stop_all_threads(zcuc);
	return err;
}

static void
cu_ctrl_config(struct zocl_cu_ctrl *zcuc, struct kds_command *xcmd)
{
	u32 *cus_addr = (u32 *)xcmd->info;
	size_t num_cus = xcmd->isize / sizeof(u32);
	struct xrt_cu *tmp;
	int i, j;
	int apt_idx;

	/* I don't care if the configure command claim less number of cus */
	if (num_cus > zcuc->num_cus)
		goto error;

	/* Now we need to make CU index right */
	for (i = 0; i < num_cus; i++) {
		j = get_cu_by_addr(zcuc, cus_addr[i]);
		if (j == zcuc->num_cus)
			goto error;

		/* Ordering CU index */
		if (j != i) {
			tmp = zcuc->xcus[i];
			zcuc->xcus[i] = zcuc->xcus[j];
			zcuc->xcus[j] = tmp;
		}
		zcuc->xcus[i]->info.cu_idx = i;

		/* To let aperture work. This should be remove later..
		 * TODO: replace aperture list
		 */
		apt_idx = get_apt_index_by_addr(zcuc->zdev, cus_addr[i]);
		if (apt_idx < 0) {
			DRM_ERROR("CU address %x is not found in XCLBIN\n",
				  cus_addr[i]);
			goto error;
		}
		update_cu_idx_in_apt(zcuc->zdev, apt_idx, i);
	}

	/* TODO: Only at this time, the CU index was known.
	 * this is why I launch threads in this place.
	 * But really need to rethink it later...
	 */
	if (zcuc->threads) {
		stop_all_threads(zcuc);
		vfree(zcuc->threads);
	}

	zcuc->threads = vzalloc(sizeof(*zcuc->threads) * zcuc->num_cus);
	launch_all_threads(zcuc);

	/* TODO: Does it need a queue for configure commands? */
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	kds_free_command(xcmd);
	return;

error:
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	kds_free_command(xcmd);
}

static void
cu_ctrl_dispatch(struct zocl_cu_ctrl *zcuc, struct kds_command *xcmd)
{
	unsigned long flags;
	int cu_idx;

	/* Select CU */
	cu_idx = cu_mask_to_cu_idx(xcmd);

	spin_lock_irqsave(&zcuc->xcus[cu_idx]->pq_lock, flags);
	list_add_tail(&xcmd->list, &zcuc->xcus[cu_idx]->pq);
	if (zcuc->xcus[cu_idx]->num_pq == 0)
		up(&zcuc->xcus[cu_idx]->sem);
	++zcuc->xcus[cu_idx]->num_pq;
	spin_unlock_irqrestore(&zcuc->xcus[cu_idx]->pq_lock, flags);
}

static void
cu_ctrl_submit(struct kds_controller *ctrl, struct kds_command *xcmd)
{
	struct zocl_cu_ctrl *zcuc = (struct zocl_cu_ctrl *)ctrl;

	/* Priority from hight to low */
	if (xcmd->opcode != OP_CONFIG_CTRL)
		cu_ctrl_dispatch(zcuc, xcmd);
	else
		cu_ctrl_config(zcuc, xcmd);
}

int cu_ctrl_add_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	struct zocl_cu_ctrl *zcuc;
	int i;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	if (!zcuc)
		return -EINVAL;

	if (zcuc->num_cus >= MAX_CUS)
		return -ENOMEM;

	for (i = 0; i < MAX_CUS; i++) {
		if (zcuc->xcus[i] != NULL)
			continue;

		zcuc->xcus[i] = xcu;
		++zcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		DRM_ERROR("Could not find a slot for CU %p\n", xcu);
		return -ENOSPC;
	}

	return 0;
}

int cu_ctrl_remove_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	struct zocl_cu_ctrl *zcuc;
	int i;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	if (!zcuc)
		return -EINVAL;

	if (zcuc->num_cus == 0)
		return -EINVAL;

	for (i = 0; i < MAX_CUS; i++) {
		if (zcuc->xcus[i] != xcu)
			continue;

		/* Maybe the thread is running */
		if (zcuc->threads && zcuc->threads[i]) {
			kthread_stop(zcuc->threads[i]);
			zcuc->threads[i] = NULL;
		}

		zcuc->xcus[i] = NULL;
		--zcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		DRM_ERROR("Could not find CU %p\n", xcu);
		return -EINVAL;
	}

	return 0;
}

int cu_ctrl_init(struct drm_zocl_dev *zdev)
{
	struct zocl_cu_ctrl *zcuc;

	zcuc = kzalloc(sizeof(*zcuc), GFP_KERNEL);
	if (!zcuc)
		return -ENOMEM;

	zcuc->zdev = zdev;

	zcuc->core.submit = cu_ctrl_submit;

	zocl_kds_setctrl(zdev, KDS_CU, (struct kds_controller *)zcuc);

	return 0;
}

void cu_ctrl_fini(struct drm_zocl_dev *zdev)
{
	struct zocl_cu_ctrl *zcuc;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	if (!zcuc)
		return;

	if (zcuc->threads) {
		stop_all_threads(zcuc);
		vfree(zcuc->threads);
	}

	kfree(zcuc);

	zocl_kds_setctrl(zdev, KDS_CU, NULL);
}
