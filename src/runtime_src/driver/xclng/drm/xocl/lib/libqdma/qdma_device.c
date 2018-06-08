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
	struct qdma_descq *descq;
	struct mbox_msg m;
	struct mbox_msg_hdr *hdr = &m.hdr;
	struct mbox_msg_fmap *fmap = &m.fmap;
	unsigned short qbase;
	int i;
	int rv;

	memset(&m, 0, sizeof(struct mbox_msg));

	hdr->op = MBOX_OP_FMAP;

	fmap->qbase = qdev->qbase;
	fmap->qmax = qdev->qmax;

	rv = qdma_mbox_send_msg(xdev, &m, 1);
	if (rv < 0) {
		pr_info("%s set q range (fmap) failed %d.\n",
			xdev->conf.name, rv);
		return rv;
	}

	xdev->func_id = hdr->dst;
	xdev->func_id_parent = hdr->src;
	qbase = qdev->qbase = fmap->qbase;

	pr_info("%s, func id %u/%u, Q 0x%x + 0x%x.\n",
		xdev->conf.name, xdev->func_id, xdev->func_id_parent,
		qdev->qbase, qdev->qmax);

	for (i = 0, descq = qdev->h2c_descq; i < qdev->qmax; i++, descq++)
		descq->qidx_hw += qbase;
	for (i = 0, descq = qdev->c2h_descq; i < qdev->qmax; i++, descq++)
		descq->qidx_hw += qbase;

	qdev->init_qrange = 1;
	return 0;
}
#else
static int device_set_qrange(struct xlnx_dma_dev *xdev)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	int rv = 0;

	hw_set_fmap(xdev, xdev->func_id, qdev->qbase, qdev->qmax);

	qdev->init_qrange = 1;

	pr_info("%s, func id %u, Q 0x%x + 0x%x.\n",
		xdev->conf.name, xdev->func_id, qdev->qbase, qdev->qmax);

	return rv;
}
#endif /* ifndef __QDMA_VF__ */

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

	rv = intr_ring_setup(xdev, XDEV_INTR_COAL_RING_SIZE);
	if (rv)
		goto done;

	if (xdev->intr_coal_en) {
		rv = qdma_intr_context_setup(xdev);
		if (rv)
			goto done;
	}

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
	xdev->conf.bar_num_user = 1;
	xdev->func_id = xdev->func_id_parent = 0; /* filled later */
#else
	u32 v;

	xdev->func_id = xdev->func_id_parent = 
			__read_reg(xdev, QDMA_REG_FUNC_ID);
	/* find out the user/AXI-LITE master bar */
	v = __read_reg(xdev, 0x10C);
	v = (v >> (6 * xdev->func_id)) & 0x3F;
	for (i = 0; i < 6; i++) {
		if (v & (1 << i)) {
			xdev->conf.bar_num_user = i;
			pr_info("%s User BAR %d.\n", xdev->conf.name, i);
		}
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

#ifdef __QDMA_VF__
	qdev->qbase = 0;
#else
	qdev->qbase = xdev->func_id * qmax;
#endif

	for (i = 0, descq = qdev->h2c_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_init(descq, xdev, i, i);
	for (i = 0, descq = qdev->c2h_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_init(descq, xdev, i, i);

#ifndef __QDMA_VF__
	if (xdev->func_id == 0) {
		hw_set_global_csr(xdev);
		for (i = 0; i < xdev->conf.h2c_channel_max; i++)
			hw_mm_channel_enable(xdev, i, 1);
		for (i = 0; i < xdev->conf.c2h_channel_max; i++)
			hw_mm_channel_enable(xdev, i, 0);
	}
#endif

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

	for (i = 0, descq = qdev->h2c_descq; i < qdev->qmax; i++, descq++) {
		if (descq->enabled) {
			qdma_queue_stop((unsigned long int)xdev, i, NULL, 0);
		}
	}

	for (i = 0, descq = qdev->c2h_descq; i < qdev->qmax; i++, descq++) {
		if (descq->enabled) {
			qdma_queue_stop((unsigned long int)xdev,
					i + qdev->qmax, NULL, 0);
		}
	}

	intr_teardown(xdev);

	if (xdev->intr_coal_en)
	{
		pr_info("dev %s teardown interrupt coalescing ring  \n",
					dev_name(&xdev->conf.pdev->dev));
		intr_ring_teardown(xdev);
	}

#ifndef __QDMA_VF__
	if (xdev->func_id == 0) {
		for (i = 0; i < xdev->conf.h2c_channel_max; i++)
			hw_mm_channel_disable(xdev, i, DMA_TO_DEVICE);
		for (i = 0; i < xdev->conf.c2h_channel_max; i++)
			hw_mm_channel_disable(xdev, i, DMA_FROM_DEVICE);
	}
#endif

	for (i = 0, descq = qdev->h2c_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_cleanup(descq);
	for (i = 0, descq = qdev->c2h_descq; i < qdev->qmax; i++, descq++)
		qdma_descq_cleanup(descq);

	xdev->dev_priv = NULL;
	kfree(qdev);
}

struct qdma_descq* qdma_device_get_descq_by_id(struct xlnx_dma_dev *xdev,
			unsigned long idx, char *buf, int buflen, int init)
{
	struct qdma_dev *qdev;
	struct qdma_descq *descq;

	if (!xdev) {
		pr_info("xdev NULL.\n");
		return NULL;
	}

	qdev = xdev_2_qdev(xdev);
	if (idx >= qdev->qmax) {
		idx -= qdev->qmax;
		if (idx >= qdev->qmax) {
			pr_info("%s, q idx too big 0x%lx > 0x%x.\n",
				xdev->conf.name, idx, qdev->qmax);
			if (buf)  {
				int len = sprintf(buf,
					"%s, q idx too big 0x%lx > 0x%x.\n",
					xdev->conf.name, idx, qdev->qmax);
				buf[len] = '\0';
			}
			return NULL;
		}
		descq = qdev->c2h_descq + idx;
	} else {
		descq = qdev->h2c_descq + idx;
	}

	if (init) {
		lock_descq(descq);
		if (!(descq->enabled)) {
			pr_info("%s, idx 0x%lx, q 0x%p state invalid.\n",
				xdev->conf.name, idx, descq);
			if (buf) {
				int len = sprintf(buf,
				"%s, idx 0x%lx, q 0x%p state invalid.\n",
					xdev->conf.name, idx, descq);
				buf[len] = '\0';
			}
			unlock_descq(descq);
			return NULL;
		}
		unlock_descq(descq);
	}

	return descq;
}
