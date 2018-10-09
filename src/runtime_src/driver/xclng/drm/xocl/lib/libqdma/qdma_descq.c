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

#include "qdma_descq.h"

#include <linux/kernel.h>
#include <linux/delay.h>

#include "qdma_device.h"
#include "qdma_intr.h"
#include "qdma_regs.h"
#include "qdma_thread.h"
#include "qdma_context.h"
#include "thread.h"
#include "version.h"
#ifdef ERR_DEBUG
#include "qdma_nl.h"
#endif

void intr_cidx_update(struct qdma_descq *descq, unsigned int sw_cidx)
{
	unsigned int cidx = 0;

	cidx |= V_INTR_CIDX_UPD_SW_CIDX(sw_cidx);

	if (descq->conf.c2h)
		cidx |= 1 << S_INTR_CIDX_UPD_DIR_SEL;

	__write_reg(descq->xdev,
		QDMA_REG_INT_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		cidx);

	dma_wmb();
}

/*
 * dma transfer requests
 */
#ifdef DEBUG
static void sgl_dump(struct qdma_sw_sg *sgl, unsigned int sgcnt)
{
	struct qdma_sw_sg *sg = sgl;
	int i;

	pr_info("sgl 0x%p, sgcntt %u.\n", sgl, sgcnt);

	for (i = 0; i < sgcnt; i++, sg++)
		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx.\n",
			i, sg, sg->pg, sg->offset, sg->len, sg->dma_addr);
}
#endif

static int sgl_find_offset(struct qdma_sw_sg *sgl, unsigned int sgcnt,
			unsigned int offset, struct qdma_sw_sg **sg_p,
			unsigned int *sg_offset)
{
	struct qdma_sw_sg *sg = sgl;
	unsigned int len = 0;
	int i;

	for (i = 0;  i < sgcnt; i++, sg++) {
		len += sg->len;

		if (len == offset) {
			*sg_p = sg + 1;
			*sg_offset = 0;
			return ++i;
		} else if (len > offset) {
			*sg_p = sg;
			*sg_offset = len - offset;
			return i;
		}
	}

	return -EINVAL;
}

static inline void req_submitted(struct qdma_descq *descq,
				struct qdma_sgt_req_cb *cb)
{
	cb->req_state = QDMA_REQ_SUBMIT_COMPLETE;
	list_del(&cb->list);
	list_add_tail(&cb->list, &descq->pend_list);
}

static ssize_t descq_mm_proc_request(struct qdma_descq *descq,
				struct qdma_sgt_req_cb *cb)
{
	struct qdma_request *req = (struct qdma_request *)cb;
	struct qdma_sw_sg *sg = req->sgl;
	unsigned int sg_offset = 0;
	unsigned int sg_max = req->sgcnt;
	u64 ep_addr = req->ep_addr + cb->offset;
	struct qdma_mm_desc *desc = (struct qdma_mm_desc *)descq->desc;
	struct qdma_mm_desc *desc_start = NULL;
	struct qdma_mm_desc *desc_end = NULL;
	unsigned int desc_max = descq->avail;
	unsigned int data_cnt = 0;
	unsigned int desc_cnt = 0;
	unsigned int len = 0;
	int i = 0;

	if (!desc_max) {
		pr_debug("descq %s, full, try again.\n", descq->conf.name);
		return 0;
	}

	if (cb->offset) {
		int rv = sgl_find_offset(req->sgl, req->sgcnt, cb->offset, &sg,
					&sg_offset);
		if (rv < 0 || rv >= sg_max) {
			pr_info("descq %s, req 0x%p, OOR %u/%u, %d/%u.\n",
				descq->conf.name, req, cb->offset, req->count,
				rv, sg_max);
			return -EINVAL;
		}
		i = rv;
		pr_debug("%s, req 0x%p, offset %u/%u -> sg %d, 0x%p,%u.\n",
			descq->conf.name, req, cb->offset, req->count, rv, sg,
			sg_offset);
	} else {
		i = 0;
		descq->cur_req_count = req->count;
		descq->cur_req_count_completed = 0;
	}

	desc += descq->pidx;
	desc_start = desc;
	for (; i < sg_max && desc_cnt < desc_max; i++, sg++) {
		unsigned int tlen = sg->len;
		dma_addr_t addr = sg->dma_addr;
		unsigned int pg_off = sg->offset;

		pr_debug("desc %u/%u, sgl %d, len %u,%u, offset %u.\n",
			desc_cnt, desc_max, i, len, tlen, sg_offset);

		if (sg_offset) {
			sg_offset = 0;
			tlen -= sg_offset;
			addr += sg_offset;
			pg_off += sg_offset;
		}

		while (tlen) {
			unsigned int len = min_t(unsigned int, tlen,
						QDMA_DESC_BLEN_MAX);
			desc_end = desc;

			desc->rsvd1 = 0UL;
			desc->rsvd0 = 0U;

			if (descq->conf.c2h) {
				desc->src_addr = ep_addr;
				desc->dst_addr = addr;
			} else {
				desc->dst_addr = ep_addr;
				desc->src_addr = addr;
			}

			desc->flag_len = len;
			desc->flag_len |= (1 << S_DESC_F_DV);

			ep_addr += len;
			data_cnt += len;
			addr += len;
			tlen -= len;
			pg_off += len;

			if (++descq->pidx == descq->conf.rngsz) {
				descq->pidx = 0;
				desc = (struct qdma_mm_desc *)descq->desc;
			} else {
				desc++;
			}

			desc_cnt++;
			descq->cur_req_count_completed += len;
			if (desc_cnt == desc_max)
				break;
		}
	}

	if (!desc_end || !desc_start) {
		pr_info("descq %s, %u, pidx 0x%x, desc 0x%p ~ 0x%p.\n",
			descq->conf.name, descq->qidx_hw, descq->pidx,
			desc_start, desc_end);
		return -EIO;
	}

	if (descq->conf.c2h)
		descq_c2h_pidx_update(descq, descq->pidx);
	else
		descq_h2c_pidx_update(descq, descq->pidx);

	/* set eop */
	desc_end->flag_len |= (1 << S_DESC_F_EOP);
	/* set sop */
	desc_start->flag_len |= (1 << S_DESC_F_SOP);

	descq->avail -= desc_cnt;
	cb->desc_nr += desc_cnt;
	cb->offset += data_cnt;

	pr_debug("descq %s, +%u,%u, avail %u, ep_addr 0x%llx + 0x%x(%u).\n",
		descq->conf.name, desc_cnt, descq->pidx, descq->avail,
		req->ep_addr, data_cnt, data_cnt);

	if (cb->offset == req->count) {
		descq->cur_req_count = 0;
		descq->cur_req_count_completed = 0;
		req_submitted(descq, cb);
	} else
		cb->req_state = QDMA_REQ_SUBMIT_PARTIAL;

	if (descq->wbthp)
		qdma_kthread_wakeup(descq->wbthp);

	return 0;
}

static ssize_t descq_proc_st_h2c_request(struct qdma_descq *descq,
				struct qdma_sgt_req_cb *cb)
{
	struct qdma_request *req = (struct qdma_request *)cb;
	struct qdma_sw_sg *sg = req->sgl;
	unsigned int sg_offset = 0;
	unsigned int sg_max = req->sgcnt;
	struct qdma_h2c_desc *desc =
		(struct qdma_h2c_desc *)descq->desc + descq->pidx;
	unsigned int desc_max = descq->avail;
	unsigned int data_cnt = 0;
	unsigned int desc_cnt = 0;
	int i = 0;

	/* calling function should hold the lock */

	if (!desc_max) {
		pr_debug("descq %s, full, try again.\n", descq->conf.name);
		return 0;
	}

#ifdef DEBUG
	pr_info("%s, req %u.\n", descq->conf.name, req->count);
	sgl_dump(req->sgl, sg_max);
#endif

	if (cb->offset) {
		int rv = sgl_find_offset(req->sgl, sg_max, cb->offset, &sg,
					&sg_offset);

		if (rv < 0 || rv >= sg_max) {
			pr_info("descq %s, req 0x%p, OOR %u/%u, %d/%u.\n",
				descq->conf.name, req, cb->offset, req->count,
				rv, sg_max);
			return -EINVAL;
		}
		i = rv;
		pr_debug("%s, req 0x%p, offset %u/%u -> sg %d, 0x%p,%u.\n",
			descq->conf.name, req, cb->offset, req->count, rv, sg,
			sg_offset);
	} else {
		i = 0;
		descq->cur_req_count = req->count;
		descq->cur_req_count_completed = 0;
		desc->flags |= S_H2C_DESC_F_SOP;

		if (descq->xdev->stm_en) {
			if (sg_max > descq->conf.pipe_gl_max) {
				pr_err("%s configured gl_max %u > given gls %u\n",
				       descq->conf.name,
				       descq->conf.pipe_gl_max, sg_max);
				return -EINVAL;
			}

			if (req->count > descq->xdev->pipe_stm_max_pkt_size) {
				pr_err("%s max stm pkt size %u > given %u\n",
				       descq->conf.name,
				       descq->xdev->pipe_stm_max_pkt_size,
				       req->count);
				return -EINVAL;
			}

			desc->cdh_flags = (1 << S_H2C_DESC_F_ZERO_CDH);
			desc->cdh_flags |= V_H2C_DESC_NUM_GL(sg_max);
			desc->pld_len = req->count;

			desc->cdh_flags |= (req->eot << S_H2C_DESC_F_EOT) |
				(1 << S_H2C_DESC_F_REQ_WRB);
		}
	}

	for (; i < sg_max && desc_cnt < desc_max; i++, sg++) {
		unsigned int tlen = sg->len;
		dma_addr_t addr = sg->dma_addr;

		if (sg_offset) {
			sg_offset = 0;
			tlen -= sg_offset;
			addr += sg_offset;
		}

		do { /* to support zero byte transfer */
			unsigned int len = min_t(unsigned int, tlen, PAGE_SIZE);

			desc->src_addr = addr;
			desc->len = len;
#ifdef ER_DEBUG
			if (descq->induce_err & (1 << len_mismatch)) {
				desc->len = 0xFFFFFFFF;
				pr_info("inducing %d err", len_mismatch);
			}
#endif

			data_cnt += len;
			addr += len;
			tlen -= len;
			descq->cur_req_count_completed += len;

			if ((i == sg_max - 1)) {
				desc->flags |= S_H2C_DESC_F_EOP;
			}

#if 0
			pr_info("desc %d, pidx 0x%x:\n", i, descq->pidx);
			print_hex_dump(KERN_INFO, "desc", DUMP_PREFIX_OFFSET,
					 16, 1, (void *)desc, 16, false);
#endif

			if (++descq->pidx == descq->conf.rngsz) {
				descq->pidx = 0;
				desc = (struct qdma_h2c_desc *)descq->desc;
			} else {
				desc++;
			}

			desc_cnt++;
			if (desc_cnt == desc_max)
				break;
		} while (tlen);
	}

	if (descq->xdev->stm_en) {
		u16 pidx_diff = 0;

		if (desc_cnt < descq->conf.pipe_gl_max)
			pidx_diff = descq->conf.pipe_gl_max - desc_cnt;

		if ((descq->pidx + pidx_diff) >= descq->conf.rngsz) {
			descq->pidx = descq->pidx + pidx_diff -
				descq->conf.rngsz;
			desc = (struct qdma_h2c_desc *)(descq->desc +
							descq->pidx);
		} else {
			descq->pidx += pidx_diff;
			desc += pidx_diff;
		}
		desc_cnt += pidx_diff;
	}

	descq_h2c_pidx_update(descq, descq->pidx);

	descq->avail -= desc_cnt;
	cb->desc_nr += desc_cnt;
	cb->offset += data_cnt;

	pr_debug("descq %s, +%u,%u, avail %u, 0x%x(%u).\n",
		descq->conf.name, desc_cnt, descq->pidx, descq->avail, data_cnt,
		data_cnt);

	if (cb->offset == req->count) {
		descq->cur_req_count = req->count;
		descq->cur_req_count_completed = 0;
		req_submitted(descq, cb);
	}

	if (descq->wbthp)
		qdma_kthread_wakeup(descq->wbthp);

	return 0;
}

static void req_update_pend(struct qdma_descq *descq, unsigned int credit)
{
	struct qdma_sgt_req_cb *cb, *tmp;

	pr_debug("%s, 0x%p, credit %u + %u.\n",
		descq->conf.name, descq, credit, descq->credit);

	credit += descq->credit;

	/* calling routine should hold the lock */
	list_for_each_entry_safe(cb, tmp, &descq->pend_list, list) {
		pr_debug("%s, 0x%p, cb 0x%p, desc_nr %u, credit %u.\n",
			descq->conf.name, descq, cb, cb->desc_nr, credit);
		if (credit >= cb->desc_nr) {
			pr_debug("%s, cb 0x%p done, credit %u > %u.\n",
				descq->conf.name, cb, credit, cb->desc_nr);
			credit -= cb->desc_nr;
			cb->done = 1;
			cb->err_code = 0;
		} else {
			pr_debug("%s, cb 0x%p not done, credit %u < %u.\n",
				descq->conf.name, cb, credit, cb->desc_nr);
			cb->desc_nr -= credit;
			credit = 0;
		}

		if (!credit)
			break;
	}

	descq->credit = credit;
	pr_debug("%s, 0x%p, credit %u.\n",
		descq->conf.name, descq, descq->credit);
}

/*
 * descriptor Queue
 */
static inline int get_desc_size(struct qdma_descq *descq)
{
	if (!descq->conf.st)
		return (int)sizeof(struct qdma_mm_desc);

	if (descq->conf.c2h)
		return (int)sizeof(struct qdma_c2h_desc);

	return (int)sizeof(struct qdma_h2c_desc);
}

static inline int get_desc_wb_size(struct qdma_descq *descq)
{
	return (int)sizeof(struct qdma_desc_wb);
}

static inline void desc_ring_free(struct xlnx_dma_dev *xdev, int ring_sz,
			int desc_sz, int wb_sz, u8 *desc, dma_addr_t desc_bus)
{
	unsigned int len = ring_sz * desc_sz + wb_sz;

	pr_debug("free %u(0x%x)=%d*%u+%d, 0x%p, bus 0x%llx.\n",
		len, len, desc_sz, ring_sz, wb_sz, desc, desc_bus);

	dma_free_coherent(&xdev->conf.pdev->dev, ring_sz * desc_sz + wb_sz,
			desc, desc_bus);
}

static void *desc_ring_alloc(struct xlnx_dma_dev *xdev, int ring_sz,
			int desc_sz, int wb_sz, dma_addr_t *bus, u8 **wb_pp)
{
	unsigned int len = ring_sz * desc_sz + wb_sz;
	u8 *p = dma_alloc_coherent(&xdev->conf.pdev->dev, len, bus, GFP_KERNEL);

	if (!p) {
		pr_info("%s, OOM, sz ring %d, desc %d, wb %d.\n",
			xdev->conf.name, ring_sz, desc_sz, wb_sz);
		return NULL;
	}

	*wb_pp = p + ring_sz * desc_sz;
	memset(p, 0, len);

	pr_debug("alloc %u(0x%x)=%d*%u+%d, 0x%p, bus 0x%llx, wb 0x%p.\n",
		len, len, desc_sz, ring_sz, wb_sz, p, *bus, *wb_pp);

	return p;
}

static void desc_free_irq(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	unsigned long flags;

	if (!xdev->num_vecs)
		return;

	spin_lock_irqsave(&xdev->lock, flags);
	if (xdev->intr_list_cnt[descq->intr_id])
		xdev->intr_list_cnt[descq->intr_id]--;
	spin_unlock_irqrestore(&xdev->lock, flags);
}

static void desc_alloc_irq(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	unsigned long flags;
	int i, idx = 0, min = 0;

	if (!xdev->num_vecs)
		return;

	/** Pick the MSI-X vector that currently has the fewest queues
	  * on PF0, vector#0 is dedicated for Error interrupts and
	  * vector #1 is dedicated for User interrupts
	  * For all other PFs, vector#0 is dedicated for User interrupts
	  */
	min = xdev->intr_list_cnt[xdev->dvec_start_idx];
	idx = xdev->dvec_start_idx;
	if (!xdev->intr_coal_en) {
		spin_lock_irqsave(&xdev->lock, flags);
		for (i = xdev->dvec_start_idx; i < xdev->num_vecs; i++) {
			if (xdev->intr_list_cnt[i] < min) {
				min = xdev->intr_list_cnt[i];
				idx = i;
			}

			if (!min)
				break;
		}
		xdev->intr_list_cnt[idx]++;
		spin_unlock_irqrestore(&xdev->lock, flags);
	}
	descq->intr_id = idx;
	pr_debug("descq->intr_id = %d allocated to qidx = %d\n",
		descq->intr_id, descq->conf.qidx);
}

/*
 * writeback handling
 */
static inline int descq_wb_credit(struct qdma_descq *q, unsigned int cidx)
{
	unsigned int n;

	if (cidx == q->cidx)
		return 0;

	/* wrapped around */
	n = (cidx < q->cidx) ? (q->conf.rngsz - q->cidx) + cidx :
				cidx - q->cidx;

	pr_debug("descq %s, cidx 0x%x -> 0x%x, avail 0x%x + 0x%x.\n",
		q->conf.name, q->cidx, cidx, q->avail, n);

	q->cidx = cidx;
	q->avail += n;

	return n;
}

static int descq_mm_n_h2c_wb(struct qdma_descq *descq)
{
	unsigned int cidx, cidx_hw;
	unsigned int cr;
	struct qdma_desc_wb *wb;
	unsigned int max_io_block;

	if (descq->pidx == descq->cidx) { /* queue empty? */
		return 0;
	}

	cidx = descq->cidx;
	wb = (struct qdma_desc_wb *)descq->desc_wb;
	dma_rmb();

	cidx_hw = wb->cidx;

	if (cidx_hw == cidx) { /* no new writeback? */
		qdma_notify_cancel(descq);
		return 0;
	}

#if 0
	print_hex_dump(KERN_INFO, "desc WB ", DUMP_PREFIX_OFFSET,
			 16, 1, (void *)wb, 8, false);
	pr_info("descq %s, wb 0x%08x, cidx 0x%x/0x%x, pidx 0x%04x.\n",
		descq->conf.name, wb->rsvd, wb->cidx, cidx, wb->pidx);
#endif

	cr = descq_wb_credit(descq, cidx_hw);

	/* Request thread may have only setup a fraction of the transfer (e.g.
	 * there wasn't enough space in desc ring). We now have more space
	 * available again so we can continue programming the
	 * dma transfer by resuming the thread here.
	 */
	if (!list_empty(&descq->work_list) && descq->avail) {
		/* batch the request thread wakeup for max_io_block */
		max_io_block = descq->io_batch_cnt * PAGE_SIZE;
		if (((descq->cur_req_count - descq->cur_req_count_completed) <
				max_io_block) ||
				(descq->avail  >= descq->io_batch_cnt))
			qdma_kthread_wakeup(descq->wrkthp);
	}

	req_update_pend(descq, cr);

	if (descq->conf.c2h)
		descq_c2h_pidx_update(descq, descq->pidx);
	else
		descq_h2c_pidx_update(descq, descq->pidx);

	qdma_sgt_req_done(descq);

	return 0;
}

/* ************** public function definitions ******************************* */

void qdma_descq_init(struct qdma_descq *descq, struct xlnx_dma_dev *xdev,
			int idx_hw, int idx_sw)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);

	memset(descq, 0, sizeof(struct qdma_descq));

	spin_lock_init(&descq->lock);
	spin_lock_init(&descq->cancel_lock);
	INIT_LIST_HEAD(&descq->work_list);
	INIT_LIST_HEAD(&descq->pend_list);
	INIT_LIST_HEAD(&descq->intr_list);
	INIT_LIST_HEAD(&descq->cancel_list);
	INIT_WORK(&descq->work, intr_work);
	init_completion(&descq->cancel_comp);
	descq->xdev = xdev;
	descq->channel = 0;
	descq->qidx_hw = qdev->qbase + idx_hw;
	descq->conf.qidx = idx_sw;
}

void qdma_descq_cleanup(struct qdma_descq *descq)
{
	lock_descq(descq);

	if (descq->q_state == Q_STATE_ONLINE) {
		descq->q_state = Q_STATE_ENABLED;
		qdma_descq_context_clear(descq->xdev, descq->qidx_hw,
					descq->conf.st, descq->conf.c2h, 0);
	}
	else
		goto end;

	desc_free_irq(descq);

	qdma_descq_free_resource(descq);

end:
	unlock_descq(descq);
	return;
}

int qdma_descq_alloc_resource(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	int rv;

	/* descriptor ring */
	descq->desc = desc_ring_alloc(xdev, descq->conf.rngsz,
				get_desc_size(descq), get_desc_wb_size(descq),
				&descq->desc_bus, &descq->desc_wb);
	if (!descq->desc) {
		pr_info("dev %s, descq %s, sz %u, desc ring OOM.\n",
			xdev->conf.name, descq->conf.name, descq->conf.rngsz);
		goto err_out;
	}

	if (descq->conf.st && descq->conf.c2h) {
		struct qdma_flq *flq = &descq->flq;

		descq->color = 1;
		flq->desc = (struct qdma_c2h_desc *)descq->desc;
		flq->size = descq->conf.rngsz;
		flq->pg_shift = fls(descq->conf.c2h_bufsz) - 1;

		/* These code changes are to accomodate buf_sz of less than 4096*/
		if(flq->pg_shift < PAGE_SHIFT) {
			flq->pg_shift = PAGE_SHIFT;
			flq->pg_order = 0;
		}
		else
		    flq->pg_order = flq->pg_shift - PAGE_SHIFT;

		/* writeback ring */
		descq->desc_wrb = desc_ring_alloc(xdev,
					descq->conf.rngsz_wrb,
					descq->wb_entry_len,
					sizeof(struct qdma_c2h_wrb_wb),
					&descq->desc_wrb_bus,
					&descq->desc_wrb_wb);
		if (!descq->desc_wrb) {
			pr_warn("dev %s, descq %s, sz %u, wrb ring OOM.\n",
				xdev->conf.name, descq->conf.name,
				descq->conf.rngsz_wrb);
			goto err_out;
		}
		descq->desc_wrb_cur = descq->desc_wrb;

		/* freelist / rx buffers */
		rv = descq_flq_alloc_resource(descq);
		if (rv < 0)
			goto err_out;
	}

	pr_debug("%s: %u/%u, rng %u,%u, desc 0x%p, wb 0x%p.\n",
		descq->conf.name, descq->conf.qidx, descq->qidx_hw,
		descq->conf.rngsz, descq->conf.rngsz_wrb, descq->desc,
		descq->desc_wrb);

	/* interrupt vectors */
	desc_alloc_irq(descq);

	return 0;

err_out:
	qdma_descq_free_resource(descq);
	return QDMA_ERR_OUT_OF_MEMORY;
}

void qdma_descq_cancel_all(struct qdma_descq *descq)
{
	struct qdma_sgt_req_cb *cb, *tmp;
	struct qdma_request *req;

	list_for_each_entry_safe(cb, tmp, &descq->pend_list, list) {
		req = (struct qdma_request *)cb;
		list_del(&cb->list);
		descq_cancel_req(descq, req);
	}
	schedule_work(&descq->work);
}

void qdma_descq_free_resource(struct qdma_descq *descq)
{
	if (!descq)
		return;

	pr_debug("%s: desc 0x%p, wrb 0x%p.\n",
		descq->conf.name, descq->desc, descq->desc_wrb);

	/* free all pending requests */
	if (!list_empty(&descq->pend_list)) {
		qdma_descq_cancel_all(descq);
		reinit_completion(&descq->cancel_comp);
		unlock_descq(descq);
		wait_for_completion(&descq->cancel_comp);
		lock_descq(descq);
	}

	if (descq->desc) {

		int desc_sz = get_desc_size(descq);
		int wb_sz = get_desc_wb_size(descq);

		pr_debug("%s: desc 0x%p, wrb 0x%p.\n",
			descq->conf.name, descq->desc, descq->desc_wrb);

		desc_ring_free(descq->xdev, descq->conf.rngsz, desc_sz, wb_sz,
				descq->desc, descq->desc_bus);

		descq->desc_wb = NULL;
		descq->desc = NULL;
		descq->desc_bus = 0UL;
	}

	if (descq->desc_wrb) {
		descq_flq_free_resource(descq);
		desc_ring_free(descq->xdev, descq->conf.rngsz_wrb,
			descq->wb_entry_len,
			sizeof(struct qdma_c2h_wrb_wb),
			descq->desc_wrb, descq->desc_wrb_bus);

		descq->desc_wrb_wb = NULL;
		descq->desc_wrb = NULL;
		descq->desc_wrb_bus = 0UL;
	}
}

void qdma_descq_config(struct qdma_descq *descq, struct qdma_queue_conf *qconf,
		 int reconfig)
{
	if (!reconfig) {
		int len;

		memcpy(&descq->conf, qconf, sizeof(struct qdma_queue_conf));
		/* descq->conf.st = qconf->st;
		 * descq->conf.c2h = qconf->c2h;
		 */

		/* qdma[vf]<255>-MM/ST-H2C/C2H-Q[2048] */
#ifdef __QDMA_VF__
		len = sprintf(descq->conf.name, "qdmavf");
#else
		len = sprintf(descq->conf.name, "qdma");
#endif
		len += sprintf(descq->conf.name + len, "%05x-%s-%u",
			descq->xdev->conf.bdf, descq->conf.st ? "ST" : "MM",
			descq->conf.qidx);
		descq->conf.name[len] = '\0';

		descq->conf.st = qconf->st;
		descq->conf.c2h = qconf->c2h;

	} else {
		descq->conf.desc_rng_sz_idx = qconf->desc_rng_sz_idx;
		descq->conf.cmpl_rng_sz_idx = qconf->cmpl_rng_sz_idx;
		descq->conf.c2h_buf_sz_idx = qconf->c2h_buf_sz_idx;

		descq->conf.irq_en = (descq->xdev->num_vecs) ? 1 : 0;
		descq->conf.wbk_en = qconf->wbk_en;
		descq->conf.wbk_acc_en = qconf->wbk_acc_en;
		descq->conf.wbk_pend_chk = qconf->wbk_pend_chk;
		descq->conf.cmpl_stat_en = qconf->cmpl_stat_en;
		descq->conf.cmpl_trig_mode = qconf->cmpl_trig_mode;
		descq->conf.cmpl_timer_idx = qconf->cmpl_timer_idx;
		descq->conf.fetch_credit = qconf->fetch_credit;
		descq->conf.cmpl_cnt_th_idx = qconf->cmpl_cnt_th_idx;

		descq->conf.bypass = qconf->bypass;
		descq->conf.pfetch_en = qconf->pfetch_en;
		descq->conf.cmpl_udd_en = qconf->cmpl_udd_en;
		descq->conf.cmpl_desc_sz = qconf->cmpl_desc_sz;
		descq->conf.pipe_gl_max = qconf->pipe_gl_max;
		descq->conf.pipe_flow_id = qconf->pipe_flow_id;
		descq->conf.pipe_slr_id = qconf->pipe_slr_id;
		descq->conf.pipe_tdest = qconf->pipe_tdest;
	}
}

int qdma_descq_config_complete(struct qdma_descq *descq)
{
	struct qdma_csr_info csr_info;
	struct qdma_queue_conf *qconf = &descq->conf;
	int rv;

	memset(&csr_info, 0, sizeof(csr_info));
	csr_info.type = QDMA_CSR_TYPE_RNGSZ;
	csr_info.idx_rngsz = qconf->desc_rng_sz_idx;
	csr_info.idx_bufsz = qconf->c2h_buf_sz_idx;
	csr_info.idx_timer_cnt = qconf->cmpl_timer_idx;
	csr_info.idx_cnt_th = qconf->cmpl_cnt_th_idx;

	rv = qdma_csr_read(descq->xdev, &csr_info, QDMA_MBOX_MSG_TIMEOUT_MS);
	if (rv < 0)
		return rv;

	qconf->rngsz = csr_info.array[qconf->desc_rng_sz_idx] - 1;
	
	/* <= 2018.2 IP
	 * make the cmpl ring size bigger if possible to avoid run out of
	 * cmpl entry while desc. ring still have free entries
	 */
	if (qconf->st && qconf->c2h) {
		int i;
		unsigned int v = csr_info.array[qconf->cmpl_rng_sz_idx];

		for (i = 0; i < QDMA_GLOBAL_CSR_ARRAY_SZ; i++)
			if (csr_info.array[i] > v)
				break;
		
		if (i < QDMA_GLOBAL_CSR_ARRAY_SZ)
			qconf->cmpl_rng_sz_idx = i;

		
		qconf->rngsz_wrb = csr_info.array[qconf->cmpl_rng_sz_idx] - 1;
		qconf->c2h_bufsz = csr_info.bufsz;
	}

	/* we can never use the full ring because then cidx would equal pidx
	 * and thus the ring would be interpreted as empty. Thus max number of
	 * usable entries is ring_size - 1
	 */
	descq->avail = descq->conf.rngsz - 1;

	descq->pidx = 0;
	descq->cidx = 0;
	descq->cidx_wrb = 0;
	descq->pidx_wrb = 0;
	descq->credit = 0;
	descq->io_batch_cnt = descq->conf.rngsz >> 1;

	/* ST C2H only */
	if (qconf->c2h && qconf->st) {
		if (qconf->cmpl_desc_sz == DESC_SZ_RSV)
			qconf->cmpl_desc_sz = DESC_SZ_8B;
		descq->wb_entry_len = 8 << qconf->cmpl_desc_sz;

		if (descq->wb_entry_len > 8)
			qconf->cmpl_udd_en = 1;

		pr_debug("%s: cmpl sz %u(%d), udd_en %d.\n",
			descq->conf.name, descq->wb_entry_len,
			descq->conf.cmpl_desc_sz, descq->conf.cmpl_udd_en);
	}

	if (qconf->fp_descq_isr_top)
		descq->xdev->conf.isr_top_q_en = 1;

	return 0;
}

int qdma_descq_prog_hw(struct qdma_descq *descq)
{
	int rv = qdma_descq_context_setup(descq);

	if (rv < 0) {
		pr_warn("%s failed to program contexts", descq->conf.name);
		return rv;
	}

	/* update pidx/cidx */
	if (descq->conf.st && descq->conf.c2h) {
		descq_wrb_cidx_update(descq, 0);
		descq_c2h_pidx_update(descq, descq->conf.rngsz - 1);
	}

	return rv;
}

/* STM */
#ifndef __QDMA_VF__
int qdma_descq_prog_stm(struct qdma_descq *descq, bool clear)
{
	int rv;

	if (!descq->conf.st) {
		pr_err("%s: STM programming called for MM-mode\n",
		       descq->conf.name);
		return -EINVAL;
	}

	if (descq->qidx_hw > STM_MAX_SUPPORTED_QID) {
		pr_err("%s: QID for STM cannot be > %d\n",
			descq->conf.name, STM_MAX_SUPPORTED_QID);
		return -EINVAL;
	}

	if (!descq->conf.c2h && !descq->conf.bypass) {
		pr_err("%s: H2C queue needs to be in bypass with STM\n",
		       descq->conf.name);
		return -EINVAL;
	}

	if (clear)
		rv = qdma_descq_stm_clear(descq);
	else
		rv = qdma_descq_stm_setup(descq);
	if (rv < 0) {
		pr_warn("%s: failed to program stm", descq->conf.name);
		return rv;
	}
	return rv;
}
#endif

void qdma_descq_service_wb(struct qdma_descq *descq, int budget,
				bool c2h_upd_cmpl)
{
	lock_descq(descq);
	if (descq->q_state != Q_STATE_ONLINE) {
		qdma_notify_cancel(descq);
		complete(&descq->cancel_comp);
	} else if (descq->conf.st && descq->conf.c2h)
		descq_process_completion_st_c2h(descq, budget, c2h_upd_cmpl);
	else
		descq_mm_n_h2c_wb(descq);
	unlock_descq(descq);
}

ssize_t qdma_descq_proc_sgt_request(struct qdma_descq *descq,
					struct qdma_sgt_req_cb *cb)
{
	if (!descq->conf.st) /* MM H2C/C2H */
		return descq_mm_proc_request(descq, cb);
	else if (descq->conf.st && !descq->conf.c2h) /* ST H2C */
		return descq_proc_st_h2c_request(descq, cb);
	else	/* ST C2H - should not happen - handled separately */
		return -1;
}

void qdma_notify_cancel(struct qdma_descq *descq)
{
	struct qdma_sgt_req_cb *cb, *tmp;
	struct qdma_request *req = NULL;
	unsigned long       flags;

	spin_lock_irqsave(&descq->cancel_lock, flags);
        /* calling routine should hold the lock */
        list_for_each_entry_safe(cb, tmp, &descq->cancel_list, list_cancel) {
		list_del(&cb->list_cancel);
		spin_unlock_irqrestore(&descq->cancel_lock, flags);
		req = (struct qdma_request *)cb;
		if (req->fp_cancel) {
			req->fp_cancel(req);
		} else {
			cb->done = 1;
			qdma_waitq_wakeup(&cb->wq);
		}
		spin_lock_irqsave(&descq->cancel_lock, flags);
	}
	spin_unlock_irqrestore(&descq->cancel_lock, flags);
}

void qdma_sgt_req_done(struct qdma_descq *descq)
{
	struct qdma_request *req;
	struct qdma_sgt_req_cb *cb, *tmp;

        /* calling routine should hold the lock */
        list_for_each_entry_safe(cb, tmp, &descq->pend_list, list) {
		if (!cb->done)
			break;

		req = (struct qdma_request *)cb;
		list_del(&cb->list);
		if (cb->unmap_needed) {
			sgl_unmap(descq->xdev->conf.pdev, req->sgl, req->sgcnt,
				descq->conf.c2h ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
			cb->unmap_needed = 0;
		}

		if (req->fp_done) {
			if (!cb->canceled) {
				unlock_descq(descq);
				req->fp_done(req, cb->offset, cb->err_code);
				lock_descq(descq);
			}
		} else {
			pr_debug("req 0x%p, cb 0x%p, wake up.\n", req, cb);
			qdma_waitq_wakeup(&cb->wq);
		}
	}
}

int qdma_descq_dump_desc(struct qdma_descq *descq, int start,
			int end, char *buf, int buflen)
{
	int desc_sz = get_desc_size(descq);
	u8 *p = descq->desc + start * desc_sz;
	struct qdma_sw_sg *fl = (descq->conf.st && descq->conf.c2h) ?
				descq->flq.sdesc + start : NULL;
	int i = start;
	int len = strlen(buf);

	if (!descq->desc)
		return 0;

	for (; i < end && i < descq->conf.rngsz; i++, p += desc_sz) {
		len += sprintf(buf + len, "%d: 0x%p ", i, p);
		hex_dump_to_buffer(p, desc_sz, (desc_sz < 16) ? 16 : 32,
				4, buf + len, buflen - len, 0);
		len = strlen(buf);
		if (fl) {
			len += sprintf(buf + len, " fl pg 0x%p, 0x%llx.\n",
				fl->pg, fl->dma_addr);
			fl++;
		} else
			buf[len++] = '\n';
	}

	p = descq->desc_wb;

	dma_rmb();

	len += sprintf(buf + len, "WB: 0x%p ", p);
	hex_dump_to_buffer(p, get_desc_wb_size(descq), 16, 4,
			buf + len, buflen - len, 0);
	len = strlen(buf);
	buf[len++] = '\n';

	if (descq->conf.st && descq->conf.c2h) {
		p = page_address(fl->pg);
		len += sprintf(buf + len, "data 0: 0x%p ", p);
		hex_dump_to_buffer(p, descq->wb_entry_len,
				(descq->wb_entry_len < 16) ? 16 : 32,
				4, buf + len,
				buflen - len, 0);
		len = strlen(buf);
		buf[len++] = '\n';
	}

	return len;
}

int qdma_descq_dump_wrb(struct qdma_descq *descq, int start,
			int end, char *buf, int buflen)
{
	uint8_t *wrb = descq->desc_wrb;
	u8 *p;
	int i = start;
	int len = strlen(buf);
	int stride = descq->wb_entry_len;

	if (!descq->desc_wrb)
		return 0;

	for (wrb += (start * stride);
			i < end && i < descq->conf.rngsz_wrb; i++,
			wrb += stride) {
		len += sprintf(buf + len, "%d: 0x%p ", i, wrb);
		hex_dump_to_buffer(wrb, descq->wb_entry_len,
				32, 4, buf + len, buflen - len, 0);
		len = strlen(buf);
		buf[len++] = '\n';
	}

	len += sprintf(buf + len, "WB: 0x%p ", descq->desc_wrb_wb);

	p = descq->desc_wrb_wb;
	dma_rmb();
	hex_dump_to_buffer(p, sizeof(struct qdma_c2h_wrb_wb),
			16, 4, buf + len, buflen - len, 0);
	len = strlen(buf);
	buf[len++] = '\n';

	return len;
}

int qdma_descq_dump_state(struct qdma_descq *descq, char *buf, int buflen)
{
	char *cur = buf;
	char *const end = buf + buflen;

	if (!buf || !buflen) {
		pr_warn("incorrect arguments buf=%p buflen=%d", buf, buflen);
		return 0;
	}

	cur += snprintf(cur, end - cur, "%s %s ",
			descq->conf.name, descq->conf.c2h ? "C2H" : "H2C");
	if (cur >= end)
		goto handle_truncation;

	if (descq->err)
		cur += snprintf(cur, end - cur, "ERR\n");
	else if (descq->q_state == Q_STATE_ONLINE)
		cur += snprintf(cur, end - cur, "online\n");
	else if (descq->q_state == Q_STATE_ENABLED)
		cur += snprintf(cur, end - cur, "cfg'ed\n");
	else
		cur += snprintf(cur, end - cur, "un-initialized\n");
	if (cur >= end)
		goto handle_truncation;

	return cur - buf;

handle_truncation:
	*buf = '\0';
	return cur - buf;
}

int qdma_descq_dump(struct qdma_descq *descq, char *buf, int buflen, int detail)
{
	char *cur = buf;
	char *const end = buf + buflen;

	if (!buf || !buflen) {
		pr_info("%s:%s 0x%x/0x%x, desc sz %u/%u, pidx %u, cidx %u\n",
			descq->conf.name, descq->err ? "ERR" : "",
			descq->conf.qidx, descq->qidx_hw, descq->conf.rngsz,
			descq->avail, descq->pidx, descq->cidx);
		return 0;
	}

	cur += qdma_descq_dump_state(descq, cur, end - cur);
	if (cur >= end)
		goto handle_truncation;

	if (descq->q_state == Q_STATE_DISABLED)
		return cur - buf;

	cur += snprintf(cur, end - cur,
		"\thw_ID %d, thp %s, %s, desc 0x%p/0x%llx, %u\n",
		descq->qidx_hw,
		descq->wrkthp ? descq->wrkthp->name : "?",
		descq->wbthp ? descq->wbthp->name : "?",
		descq->desc, descq->desc_bus, descq->conf.rngsz);
	if (cur >= end)
		goto handle_truncation;

	if (descq->conf.st && descq->conf.c2h) {
		cur += snprintf(cur, end - cur,
			"\twrb desc 0x%p/0x%llx, %u",
			descq->desc_wrb, descq->desc_wrb_bus,
			descq->conf.rngsz_wrb);
		if (cur >= end)
			goto handle_truncation;
	}

	if (!detail)
		return cur - buf;

	if (descq->desc_wb) {
		u8 *wb = descq->desc_wb;

		cur += snprintf(cur, end - cur, "\n\tWB: 0x%p, ", wb);
		if (cur >= end)
			goto handle_truncation;

		dma_rmb();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
		cur += hex_dump_to_buffer(wb, sizeof(struct qdma_desc_wb),
					  16, 4, cur, end - cur, 0);
#else
		hex_dump_to_buffer(wb, sizeof(struct qdma_desc_wb),
					  16, 4, cur, end - cur, 0);
		cur += strlen(cur);
#endif
		if (cur >= end)
			goto handle_truncation;

		cur += snprintf(cur, end - cur, "\n");
		if (cur >= end)
			goto handle_truncation;
	}
	if (descq->desc_wrb_wb) {
		u8 *wb = descq->desc_wrb_wb;

		cur += snprintf(cur, end - cur, "\tWRB WB: 0x%p, ", wb);
		if (cur >= end)
			goto handle_truncation;

		dma_rmb();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
		cur += hex_dump_to_buffer(wb, sizeof(struct qdma_c2h_wrb_wb),
					  16, 4, cur, end - cur, 0);
#else
		hex_dump_to_buffer(wb, sizeof(struct qdma_c2h_wrb_wb),
					  16, 4, cur, end - cur, 0);
		cur += strlen(cur);
#endif
		if (cur >= end)
			goto handle_truncation;

		cur += snprintf(cur, end - cur, "\n");
		if (cur >= end)
			goto handle_truncation;
	}

	return cur - buf;

handle_truncation:
	*buf = '\0';
	return cur - buf;
}

int qdma_queue_avail_desc(unsigned long dev_hndl, unsigned long id)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, NULL, 0, 1);
	int avail;

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	lock_descq(descq);
	avail = descq->avail;
	unlock_descq(descq);

	return avail;
}

#ifdef ERR_DEBUG
int qdma_queue_set_err_injection(unsigned long dev_hndl, unsigned long id,
			u64 err_sel, u64 err_mask, char *buf, int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, buf, buflen, 1);
	const char *dummy; /* to avoid compiler warnings */

	dummy = xnl_attr_str[0];
	dummy = xnl_op_str[0];
	if (!descq)
		return QDMA_ERR_INVALID_QIDX;
	descq->induce_err &= ~err_mask;
	descq->induce_err |= err_sel;
	pr_info("Errs enabled = [1]: 0x%08x [0]: 0x%08x",
		(u32)(descq->induce_err >> 32),
		(u32)descq->induce_err);

	return 0;
}
#endif

int qdma_queue_packet_write(unsigned long dev_hndl, unsigned long id,
				struct qdma_request *req)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, NULL, 0, 1);
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	int rv;

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	if (!descq->conf.st || descq->conf.c2h) {
		pr_info("%s: st %d, c2h %d.\n",
			descq->conf.name, descq->conf.st, descq->conf.c2h);
		return -EINVAL;
	}

	memset(cb, 0, QDMA_REQ_OPAQUE_SIZE);
	qdma_waitq_init(&cb->wq);

	if (!req->dma_mapped) {
		rv = sgl_map(descq->xdev->conf.pdev, req->sgl, req->sgcnt,
				DMA_TO_DEVICE);
		if (rv < 0) {
			pr_info("%s map sgl %u failed, %u.\n",
				descq->conf.name, req->sgcnt, req->count);
			goto unmap_sgl;
		}
		cb->unmap_needed = 1;
	}

	lock_descq(descq);
	if (descq->q_state != Q_STATE_ONLINE) {
		unlock_descq(descq);
		pr_info("%s descq %s NOT online.\n",
			descq->xdev->conf.name, descq->conf.name);
		rv = -EINVAL;
		goto unmap_sgl;
	}

	list_add_tail(&cb->list, &descq->work_list);
	unlock_descq(descq);

	pr_debug("%s: cb 0x%p submitted.\n", descq->conf.name, cb);

	qdma_kthread_wakeup(descq->wrkthp);

	return req->count;

unmap_sgl:
	if (cb->unmap_needed)
		sgl_unmap(descq->xdev->conf.pdev, req->sgl, req->sgcnt,
			DMA_TO_DEVICE);
	return rv;
}

int qdma_descq_get_wrb_udd(unsigned long dev_hndl, unsigned long id, char *buf, int buflen)
{
	uint8_t *wrb;
	uint8_t i = 0;
	int len = 0;
	int print_len = 0;
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, buf, buflen, 1);
	struct qdma_c2h_wrb_wb *wb;

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	wb = (struct qdma_c2h_wrb_wb *)
						descq->desc_wrb_wb;

	wrb = descq->desc_wrb + ((wb->pidx - 1) * descq->wb_entry_len);

	for(i = 2; i < descq->wb_entry_len; i++) {
		if (buf && buflen) {
			if(i == 2)
				print_len = sprintf(buf + len,"%02x", (wrb[i] & 0xF0));
			else
				print_len = sprintf(buf + len,"%02x", wrb[i]);
		}
		buflen -= print_len;
		len += print_len;
	}
	buf[len] = '\0';

	return 0;
}
