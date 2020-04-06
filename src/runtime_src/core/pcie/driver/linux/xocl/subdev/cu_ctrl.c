// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Controller Sub-device Driver
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include <linux/kthread.h>

#include "xocl_drv.h"
#include "kds_core.h"
#include "xrt_cu.h"

#define XCUC_INFO(xcuc, fmt, arg...) \
	xocl_info(&xcuc->pdev->dev, fmt "\n", ##arg)
#define XCUC_ERR(xcuc, fmt, arg...) \
	xocl_err(&xcuc->pdev->dev, fmt "\n", ##arg)
#define XCUC_DBG(xcuc, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

struct xocl_cu_ctrl {
	struct kds_controller    core;
	struct platform_device	*pdev;
	struct xrt_cu		*xcus[MAX_CUS];
	/* TODO: Maybe rethink if we should use two threads,
	 * one for submit, one for complete
	 */
	struct task_struct     **threads;
	int			 num_cus;

};

#define Test_A_0 0
#define Test_A_1 1
#define Test_A_2 0
#define Test_B 0

#if Test_A_0
static int cu_ctrl_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	struct kds_command *xcmd = NULL;
	unsigned long flags;
	u32 loop_cnt = 0;
	int i;

	/* The CU is not able to interrupt host
	 * this thread has to poll CU status, so
	 * let me do everything in a busy loop...
	 */
	while (!kthread_should_stop()) {
		spin_lock_irqsave(&xcu->pq_lock, flags);
		xcmd = list_first_entry_or_null(&xcu->pq, struct kds_command, list);
		spin_unlock_irqrestore(&xcu->pq_lock, flags);

		if (xcmd) {
			/* submit one command */
			if (xrt_cu_get_credit(xcu)) {
				/* if successfully get credit, you must start cu */
				xrt_cu_config(xcu, (u32 *)xcmd->info, xcmd->isize, 0);
				xrt_cu_start(xcu);
				/* Move pending command to run queue */
				spin_lock_irqsave(&xcu->pq_lock, flags);
				list_move_tail(&xcmd->list, &xcu->rq);
				--xcu->num_pq;
				spin_unlock_irqrestore(&xcu->pq_lock, flags);
			}
		}

		if (list_empty(&xcu->rq)) {
			/* This also impact the IOPS (about 30K) */
			++loop_cnt;
			if (!(loop_cnt & 0x0F))
				schedule();
			continue;
		}

		xrt_cu_check(xcu);
		xrt_cu_put_credit(xcu, xcu->ready_cnt);
		xcu->ready_cnt = 0;

		if (!xcu->done_cnt)
			continue;

		for (i = 0; i < xcu->done_cnt; ++i) {
			xcmd = list_first_entry_or_null(&xcu->rq, struct kds_command, list);
			xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
			list_del(&xcmd->list);
			kds_free_command(xcmd);
		}

		xcu->done_cnt = 0;
	}

	return 0;
}
#endif

#if Test_A_1
static inline void process_sq_once(struct xrt_cu *xcu)
{
	struct list_head *q;
	struct kds_command *done_xcmd;

	/* This is the critical path, as less check as possible..
	 * if rq and sq both are empty, please DO NOT call this function
	 */

	q = list_empty(&xcu->sq) ? &xcu->rq : & xcu->sq;

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

	/* This funtion would not return until rq is empty */
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
		if(!list_empty(&xcu->rq)) {
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
#endif

#if Test_A_2
static int cu_ctrl_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	struct kds_command *done_xcmd;
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

		while (!list_empty(&xcu->rq)) {
			done_xcmd = list_first_entry(&xcu->rq, struct kds_command, list);
			done_xcmd->cb.notify_host(done_xcmd, KDS_COMPLETED);
			list_del(&done_xcmd->list);
			kds_free_command(done_xcmd);
		}

		while (down_timeout(&xcu->sem, 1000) == -ETIME) {
			if (kthread_should_stop())
				return 0;
		}
		/* Something interesting happened */
	}

	return 0;
}
#endif

#if Test_B
static int cu_ctrl_thread(void *data)
{
	struct xrt_cu *xcu = (struct xrt_cu *)data;
	struct kds_command *xcmd;
	unsigned long flags;
	u32 loop_cnt = 0;

	/* The CU is not able to interrupt host
	 * this thread has to poll CU status, so
	 * let me do everything in a busy loop...
	 */
	while (!kthread_should_stop()) {
		spin_lock_irqsave(&xcu->rq_lock, flags);
		xcmd = list_first_entry_or_null(&xcu->rq, struct kds_command, list);
		spin_unlock_irqrestore(&xcu->rq_lock, flags);

		if (xcmd != 0) {
			xrt_cu_check(xcu);
			xrt_cu_put_credit(xcu, xcu->ready_cnt);
			while (xcu->ready_cnt) {
				xrt_cu_up(xcu);
				--xcu->ready_cnt;
			}

			if (!xcu->done_cnt)
				continue;

			/* now we have at least one command finished */
			xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
			spin_lock_irqsave(&xcu->rq_lock, flags);
			list_del(&xcmd->list);
			spin_unlock_irqrestore(&xcu->rq_lock, flags);
			kds_free_command(xcmd);
			--xcu->done_cnt;
		}

		/* This also impact the IOPS a little bit (about 30K) */
		++loop_cnt;
		if (!(loop_cnt & 0x0F))
			schedule();
	}

	return 0;
}
#endif

static int get_cu_by_addr(struct xocl_cu_ctrl *xcuc, u32 addr)
{
	int i;

	/* Do not use this search in critical path */
	for (i = 0; i < xcuc->num_cus; ++i) {
		if (xcuc->xcus[i]->info.addr == addr)
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

static inline void stop_all_threads(struct xocl_cu_ctrl *xcuc)
{
	int i;

	for (i = 0; i < xcuc->num_cus; ++i) {
		if (xcuc->threads[i] != NULL) {
			kthread_stop(xcuc->threads[i]);
			//xcu->xcus[i]->stop = 1;
			xcuc->threads[i] = NULL;
		}
	}
}

static inline int launch_all_threads(struct xocl_cu_ctrl *xcuc)
{
	int i, err = 0;

	//for (i = 0; i < xcuc->num_cus; ++i) {
	/* only launch one thread, it should be one thread per CU */
	for (i = 0; i < 1; ++i) {
		xcuc->threads[i] = kthread_run(cu_ctrl_thread,
					       (void *)xcuc->xcus[i],
					       "xcu_thread");
		if (IS_ERR(xcuc->threads[i])) {
			err = PTR_ERR(xcuc->threads[i]);
			xcuc->threads[i] = NULL;
			goto error;
		}
	}
	return 0;

error:
	stop_all_threads(xcuc);
	return err;
}

static void cu_ctrl_config(struct xocl_cu_ctrl *xcuc, struct kds_command *xcmd)
{
	u32 *cus_addr = (u32 *)xcmd->info;
	size_t num_cus = xcmd->isize / sizeof(u32);
	struct xrt_cu *tmp;
	int i, j;

	/* I don't care if the configure command claim less number of cus */
	if (num_cus > xcuc->num_cus)
		goto error;

	/* Now we need to make CU index right */
	for (i = 0; i < num_cus; i++) {
		j = get_cu_by_addr(xcuc, cus_addr[i]);
		if (j == xcuc->num_cus)
			goto error;

		/* Ordering CU index */
		if (j != i) {
			tmp = xcuc->xcus[i];
			xcuc->xcus[i] = xcuc->xcus[j];
			xcuc->xcus[j] = tmp;
		}
		xcuc->xcus[i]->info.cu_idx = i;
	}

	/* TODO: Only at this time, the CU index was known.
	 * this is why I launch threads in this place.
	 * But really need to rethink it later...
	 */
	if (xcuc->threads) {
		stop_all_threads(xcuc);
		vfree(xcuc->threads);
	}

	xcuc->threads = vzalloc(sizeof(*xcuc->threads) * xcuc->num_cus);
	launch_all_threads(xcuc);

	/* TODO: Does it need a queue for configure commands? */
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	kds_free_command(xcmd);
	return;

error:
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	kds_free_command(xcmd);
}

static void cu_ctrl_dispatch(struct xocl_cu_ctrl *xcuc, struct kds_command *xcmd)
{
	unsigned long flags;
	int cu_idx;

	/* Select CU */
	cu_idx = cu_mask_to_cu_idx(xcmd);

	/* about 850K IOPS, if only do below two lines
	 * The purpose is to show how fast a single user thread
	 * could produce a CU task.
	 */
	//xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	//kds_free_command(xcmd);

#if 0
	/* about 160K IOPS...
	 * This let execbuf becomes a blocking call...
	 * Start CU, wait CU done, return.
	 */
	//xrt_cu_wait(xcuc->xcus[cu_idx]);

	/* start CU */
	xrt_cu_config(xcuc->xcus[cu_idx], (u32 *)xcmd->info, xcmd->isize, 0);
	xrt_cu_start(xcuc->xcus[cu_idx]);

	while(1) {
		xrt_cu_check(xcuc->xcus[cu_idx]);
		if (!xcuc->xcus[cu_idx]->done_cnt)
			continue;

		xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
		kds_free_command(xcmd);
		--xcuc->xcus[cu_idx]->done_cnt;
		--xcuc->xcus[cu_idx]->ready_cnt;
		//xrt_cu_up(xcu);
		break;
	}
#endif

#if Test_A_0
	/* about 500K IOPS with "Test_A_0 echo" */
	/* about 400K IOPS with "Test_A_0" */
	spin_lock_irqsave(&xcuc->xcus[cu_idx]->pq_lock, flags);
	list_add_tail(&xcmd->list, &xcuc->xcus[cu_idx]->pq);
	spin_unlock_irqrestore(&xcuc->xcus[cu_idx]->pq_lock, flags);
#endif

#if Test_A_1 || Test_A_2
	/* about 550K IOPS with "Test_A_1 echo" */
	/* about 500K IOPS with "Test_A_1" */
	/* about 550K IOPS with "Test_A_2" w/wo echo */
	spin_lock_irqsave(&xcuc->xcus[cu_idx]->pq_lock, flags);
	list_add_tail(&xcmd->list, &xcuc->xcus[cu_idx]->pq);
	if (xcuc->xcus[cu_idx]->num_pq == 0)
		up(&xcuc->xcus[cu_idx]->sem);
	++xcuc->xcus[cu_idx]->num_pq;
	spin_unlock_irqrestore(&xcuc->xcus[cu_idx]->pq_lock, flags);
#endif

#if Test_B
	/* This approach is starting CU at this thread,
	 * Then add xcmd to CU's run queue to wait for complete
	 */

	/* about 420K IOPS with "Test_B echo*/
	xrt_cu_wait(xcuc->xcus[cu_idx]);
	xrt_cu_get_credit(xcuc->xcus[cu_idx]);

	/* start CU */
	xrt_cu_config(xcuc->xcus[cu_idx], (u32 *)xcmd->info, xcmd->isize, 0);
	xrt_cu_start(xcuc->xcus[cu_idx]);

	spin_lock_irqsave(&xcuc->xcus[cu_idx]->rq_lock, flags);
	list_add_tail(&xcmd->list, &xcuc->xcus[cu_idx]->rq);
	spin_unlock_irqrestore(&xcuc->xcus[cu_idx]->rq_lock, flags);
#endif
}

static void cu_ctrl_submit(struct kds_controller *ctrl, struct kds_command *xcmd)
{
	struct xocl_cu_ctrl *xcuc = (struct xocl_cu_ctrl *)ctrl;

	/* Priority from hight to low */
	if (xcmd->opcode != OP_CONFIG_CTRL)
		cu_ctrl_dispatch(xcuc, xcmd);
	else
		cu_ctrl_config(xcuc, xcmd);
}

static int cu_ctrl_add_cu(struct platform_device *pdev, struct xrt_cu *xcu)
{
	struct xocl_cu_ctrl *xcuc = platform_get_drvdata(pdev);
	int i;

	if (xcuc->num_cus >= MAX_CUS)
		return -ENOMEM;

	for (i = 0; i < MAX_CUS; i++) {
		if (xcuc->xcus[i] != NULL)
			continue;

		xcuc->xcus[i] = xcu;
		++xcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		XCUC_ERR(xcuc, "Could not find a slot for CU %p", xcu);
		return -ENOSPC;
	}

	/* TODO: maybe we should launch a thread when a CU was added
	 * but at this time, we don't know how many threads will be..
	 */

	return 0;
}

static int cu_ctrl_remove_cu(struct platform_device *pdev, struct xrt_cu *xcu)
{
	struct xocl_cu_ctrl *xcuc = platform_get_drvdata(pdev);
	int i, ret = 0;

	if (xcuc->num_cus == 0)
		return -EINVAL;

	/* The xcus list is not the same as when a CU was added
	 * search the CU..
	 */
	for (i = 0; i < MAX_CUS; i++) {
		if (xcuc->xcus[i] != xcu)
			continue;

		/* Maybe the thread is running */
		if (xcuc->threads[i] != NULL) {
			ret = kthread_stop(xcuc->threads[i]);
			xcuc->threads[i] = NULL;
		}

		xcuc->xcus[i] = NULL;
		--xcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		XCUC_ERR(xcuc, "Could not find CU %p", xcu);
		return -EINVAL;
	}

	return ret;
}

static int cu_ctrl_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu_ctrl *xcuc;

	xcuc = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_cu_ctrl));
	if (!xcuc)
		return -ENOMEM;

	xcuc->pdev = pdev;

	/* TODO: handle irq resource when we support CU interrupt to host */

	platform_set_drvdata(pdev, xcuc);

	xcuc->core.submit = cu_ctrl_submit;

	xocl_kds_setctrl(xdev, KDS_CU, (struct kds_controller *)xcuc);

	return 0;
}

static int cu_ctrl_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu_ctrl *xcuc;
	void *hdl;

	xcuc = platform_get_drvdata(pdev);
	if (!xcuc) {
		XCUC_ERR(xcuc, "driver data is NULL");
		return -EINVAL;
	}

	if (xcuc->threads) {
		stop_all_threads(xcuc);
		vfree(xcuc->threads);
	}

	xocl_drvinst_release(xcuc, &hdl);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	xocl_kds_setctrl(xdev, KDS_CU, NULL);

	return 0;
}

static struct xocl_kds_ctrl_funcs cu_ctrl_ops = {
	.add_cu		= cu_ctrl_add_cu,
	.remove_cu	= cu_ctrl_remove_cu,
};

static struct xocl_drv_private cu_ctrl_priv = {
	.ops = &cu_ctrl_ops,
};

static struct platform_device_id cu_ctrl_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CU_CTRL), (kernel_ulong_t)&cu_ctrl_priv },
	{ },
};

static struct platform_driver cu_ctrl_driver = {
	.probe		= cu_ctrl_probe,
	.remove		= cu_ctrl_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CU_CTRL),
	},
	.id_table	= cu_ctrl_id_table,
};

int __init xocl_init_cu_ctrl(void)
{
	return platform_driver_register(&cu_ctrl_driver);
}

void xocl_fini_cu_ctrl(void)
{
	platform_driver_unregister(&cu_ctrl_driver);
}
