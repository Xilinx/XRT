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

#include <linux/module.h>
#include <linux/gfp.h>
#include "qdma_device.h"
#include "qdma_context.h"
#include "qdma_descq.h"
#include "qdma_intr.h"
#include "qdma_regs.h"
#include "qdma_mbox.h"

#ifdef __QDMA_VF__
static int device_set_qrange(struct xlnx_dma_dev *xdev)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct mbox_msg *m;
	struct mbox_msg_hdr *hdr;
	struct mbox_msg_fmap *fmap;
	unsigned short qbase;
	int rv = 0;

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return QDMA_ERR_INVALID_QDMA_DEVICE;
	}

	m = qdma_mbox_msg_alloc(xdev, MBOX_OP_FMAP);
	if (!m)
		return -ENOMEM;

	hdr = &m->hdr;
	fmap = &m->fmap;

	fmap->qbase = qdev->qbase;
	fmap->qmax = qdev->qmax;

	rv = qdma_mbox_msg_send(xdev, m, 1, MBOX_OP_FMAP_RESP,
				QDMA_MBOX_MSG_TIMEOUT_MS);
	if (rv < 0) {
		if (rv != -ENODEV)
			pr_info("%s set q range (fmap) failed %d.\n",
				xdev->conf.name, rv);
		goto err_out;
	}

	if (hdr->status) {
		rv = hdr->status;
		goto err_out;
	}

	qbase = qdev->qbase = xdev->conf.qsets_base = fmap->qbase;

	pr_debug("%s, func id %u/%u, Q 0x%x + 0x%x.\n",
		xdev->conf.name, xdev->func_id, xdev->func_id_parent,
		qdev->qbase, qdev->qmax);


	qdev->init_qrange = 1;

err_out:
	qdma_mbox_msg_free(m);
	return rv;
}
#else
static int device_set_qrange(struct xlnx_dma_dev *xdev)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	int rv = 0;

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return QDMA_ERR_INVALID_QDMA_DEVICE;
	}

	hw_set_fmap(xdev, xdev->func_id, qdev->qbase, qdev->qmax);

	qdev->init_qrange = 1;

	pr_debug("%s, func id %u, Q 0x%x + 0x%x.\n",
		xdev->conf.name, xdev->func_id, qdev->qbase, qdev->qmax);

	return rv;
}
#endif /* ifndef __QDMA_VF__ */

#ifdef ERR_DEBUG
static void qdma_err_mon(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work,
						struct delayed_work, work);
	struct xlnx_dma_dev *xdev = container_of(dwork,
					struct xlnx_dma_dev, err_mon);

	if (!xdev) {
		pr_err("Invalid xdev");
		return;
	}
	spin_lock(&xdev->err_lock);

	if (xdev->err_mon_cancel == 0) {
		err_stat_handler(xdev);
		schedule_delayed_work(dwork, msecs_to_jiffies(50)); /* 50msec */
	}
	spin_unlock(&xdev->err_lock);
}
#endif

int qdma_device_prep_q_resource(struct xlnx_dma_dev *xdev)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	int rv = 0;

	spin_lock(&qdev->lock);

	if (qdev->init_qrange)
		goto done;

	rv = device_set_qrange(xdev);
	if (rv < 0)
		goto done;

	rv = intr_ring_setup(xdev);
	if (rv)
		goto done;

	if (xdev->intr_coal_en) {
		rv = qdma_intr_context_setup(xdev);
		if (rv)
			goto done;
	}

	if ((xdev->conf.poll_mode == 0) && xdev->conf.master_pf)
		qdma_err_intr_setup(xdev, 0);
#ifdef ERR_DEBUG
	else {
		spin_lock_init(&xdev->err_lock);
		xdev->err_mon_cancel = 0;
		INIT_DELAYED_WORK(&xdev->err_mon, qdma_err_mon);
		schedule_delayed_work(&xdev->err_mon, msecs_to_jiffies(50));
	}
#endif

done:
	spin_unlock(&qdev->lock);

	return rv;
}

int qdma_device_init(struct xlnx_dma_dev *xdev)
{
	int i;
	int rv = 0;
	int qmax = xdev->conf.qsets_max;
	struct qdma_descq *descq;
	struct qdma_dev *qdev;

#ifdef __QDMA_VF__
	xdev->conf.bar_num_user = -1;
	xdev->func_id = xdev->func_id_parent = 0; /* filled later */
#else
	u32 v;

	xdev->func_id = __read_reg(xdev, QDMA_REG_FUNC_ID);
	/* find out the user/AXI-LITE master bar */
	v = __read_reg(xdev, 0x10C);
	v = (v >> (6 * xdev->func_id)) & 0x3F;
	for (i = 0; i < 6; i++) {
		if (v & (1 << i)) {
			xdev->conf.bar_num_user = i;
			pr_info("%s User BAR %d.\n", xdev->conf.name, i);
		}
	}

	if (xdev->conf.master_pf) {
		pr_info("%s master PF clearing memory.\n", xdev->conf.name);
		rv = hw_init_global_context_memory(xdev);
		if (rv)
			return rv;
	}
#endif

	rv = intr_setup(xdev);
	if (rv)
		return -EINVAL;

	if (!qmax) {
		pr_info("dev %s NO queue config. %d.\n",
			dev_name(&xdev->conf.pdev->dev), qmax);
		return -EINVAL;
	}

	qdev = kzalloc(sizeof(struct qdma_dev) +
			sizeof(struct qdma_descq) * qmax * 2, GFP_KERNEL);
	if (!qdev) {
		pr_info("dev %s qmax %d OOM.\n",
			dev_name(&xdev->conf.pdev->dev), qmax);
		intr_teardown(xdev);
		return -ENOMEM;
	}

	spin_lock_init(&qdev->lock);

	descq = (struct qdma_descq *)(qdev + 1);
	qdev->h2c_descq = descq;
	qdev->c2h_descq = descq + qmax;

	xdev->dev_priv = (void *)qdev;
	qdev->qmax = qmax;
	qdev->init_qrange = 0;

#ifdef __QDMA_VF__
	qdev->qbase = TOTAL_QDMA_QS - TOTAL_VF_QS
		+ (xdev->conf.idx - 1) * QDMA_Q_PER_VF_MAX;
#else
	/*
	 * for the first device make qbase as 0 indicated by <0 value
	 * for others initial configuration, go for QDMA_Q_PER_PF_MAX
	 * for qbase
	 * if modified using sysfs/qmax, take it from calculated qbase
	*/
	if (xdev->conf.cur_cfg_state == CFG_UNCONFIGURED) {
		qdev->qbase = (xdev->conf.idx - 1) * MAX_QS_PER_PF;
		xdev->conf.cur_cfg_state = CFG_INITIAL;
	} else
		qdev->qbase = xdev->conf.qsets_base;
#endif
	xdev->conf.qsets_base = qdev->qbase;

	for (i = 0, descq = qdev->h2c_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_init(descq, xdev, i, i);
	for (i = 0, descq = qdev->c2h_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_init(descq, xdev, i, i);
#ifdef ERR_DEBUG
	if (descq->induce_err & (1 << vf_access_err)) {
		unsigned int wb_acc;

		qdma_csr_read_wbacc(xdev, &wb_acc);
	}
#endif
#ifndef __QDMA_VF__
	if (xdev->conf.master_pf) {
		pr_info("%s master PF.\n", xdev->conf.name);
		hw_set_global_csr(xdev);
		for (i = 0; i < xdev->mm_channel_max; i++) {
			hw_mm_channel_enable(xdev, i, 1);
			hw_mm_channel_enable(xdev, i, 0);
		}
	}
#endif
	/** STM specific init */
	xdev->pipe_stm_max_pkt_size = STM_MAX_PKT_SIZE;

	return 0;
}

void qdma_device_cleanup(struct xlnx_dma_dev *xdev)
{
	int i;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct qdma_descq *descq;

	if  (!qdev) {
		pr_info("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return;
	}

#ifdef ERR_DEBUG
	if ((xdev->conf.master_pf) && (xdev->conf.poll_mode != 0)) {
		pr_info("Cancelling delayed work");
		spin_lock(&xdev->err_lock);
		xdev->err_mon_cancel = 1;
		cancel_delayed_work_sync(&xdev->err_mon);
		spin_unlock(&xdev->err_lock);
	}
#endif

	for (i = 0, descq = qdev->h2c_descq; i < qdev->qmax; i++, descq++) {
		if (descq->q_state != Q_STATE_DISABLED)
			qdma_queue_stop((unsigned long int)xdev, i, NULL, 0);
	}

	for (i = 0, descq = qdev->c2h_descq; i < qdev->qmax; i++, descq++) {
		if (descq->q_state != Q_STATE_DISABLED) {
			qdma_queue_stop((unsigned long int)xdev,
					i + qdev->qmax, NULL, 0);
		}
	}

	intr_teardown(xdev);

	if (xdev->intr_coal_en) {
		pr_info("dev %s teardown interrupt coalescing ring\n",
					dev_name(&xdev->conf.pdev->dev));
		intr_ring_teardown(xdev);
	}

#ifndef __QDMA_VF__
	if (xdev->func_id == 0) {
		for (i = 0; i < xdev->mm_channel_max; i++) {
			hw_mm_channel_disable(xdev, i, DMA_TO_DEVICE);
			hw_mm_channel_disable(xdev, i, DMA_FROM_DEVICE);
		}
	}
#endif

	for (i = 0, descq = qdev->h2c_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_cleanup(descq);
	for (i = 0, descq = qdev->c2h_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_cleanup(descq);

	xdev->dev_priv = NULL;
	kfree(qdev);
}

struct qdma_descq *qdma_device_get_descq_by_id(struct xlnx_dma_dev *xdev,
			unsigned long idx, char *buf, int buflen, int init)
{
	struct qdma_dev *qdev;
	struct qdma_descq *descq;

	if (!xdev) {
		pr_info("xdev NULL.\n");
		return NULL;
	}

	qdev = xdev_2_qdev(xdev);

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return NULL;
	}

	if (idx >= qdev->qmax) {
		idx -= qdev->qmax;
		if (idx >= qdev->qmax) {
			pr_info("%s, q idx too big 0x%lx > 0x%x.\n",
				xdev->conf.name, idx, qdev->qmax);
			if (buf && buflen)  {
				int len = snprintf(buf, buflen,
					"%s, q idx too big 0x%lx > 0x%x.\n",
					xdev->conf.name, idx, qdev->qmax);
				buf[len] = '\0';
				buflen -= len + 1;
			}
			return NULL;
		}
		descq = qdev->c2h_descq + idx;
	} else {
		descq = qdev->h2c_descq + idx;
	}

	if (init) {
		lock_descq(descq);
		if (descq->q_state == Q_STATE_DISABLED) {
			pr_info("%s, idx 0x%lx, q 0x%p state invalid.\n",
				xdev->conf.name, idx, descq);
			if (buf && buflen) {
				int len = snprintf(buf, buflen,
				"%s, idx 0x%lx, q 0x%p state invalid.\n",
					xdev->conf.name, idx, descq);
				buf[len] = '\0';
				buflen -= len + 1;
			}
			unlock_descq(descq);
			return NULL;
		}
		unlock_descq(descq);
	}

	return descq;
}


struct qdma_descq *qdma_device_get_descq_by_hw_qid(struct xlnx_dma_dev *xdev,
			unsigned long qidx_hw, u8 c2h)
{
	struct qdma_dev *qdev;
	struct qdma_descq *descq;
	unsigned long qidx_sw = 0;

	if (!xdev) {
		pr_info("xdev NULL.\n");
		return NULL;
	}

	qdev = xdev_2_qdev(xdev);

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return NULL;
	}


	qidx_sw = qidx_hw - qdev->qbase;
	if (c2h)
		descq = &qdev->c2h_descq[qidx_sw];
	else
		descq = &qdev->h2c_descq[qidx_sw];

	if (!descq)
		return NULL;

	return descq;
}
