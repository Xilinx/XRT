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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
#define qdma_rmb() dma_rmb()
#else
#define qdma_rmb() rmb()
#endif

/*
 * HW APIs
 */
static inline void descq_h2c_pidx_update_raw(struct qdma_descq *descq,
							unsigned int reg_val)
{
	__write_reg(descq->xdev,
		QDMA_REG_H2C_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		reg_val);
}

static inline void descq_c2h_pidx_update_raw(struct qdma_descq *descq,
							unsigned int reg_val)
{
	__write_reg(descq->xdev,
		QDMA_REG_C2H_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		reg_val);
}

static void descq_pidx_update(struct qdma_descq *descq, unsigned int pidx)
{
	pidx |= (descq->irq_en << S_WRB_PIDX_UPD_EN_INT);

	if (descq->conf.c2h)
		descq_c2h_pidx_update_raw(descq, pidx);
	else
		descq_h2c_pidx_update_raw(descq, pidx);
}

static inline void descq_wrb_cidx_update(struct qdma_descq *descq,
					unsigned int cidx)
{
	cidx |= (descq->irq_en << S_WRB_CIDX_UPD_EN_INT);
	cidx |= (descq->wrb_stat_desc_en << S_WRB_CIDX_UPD_EN_STAT_DESC);
	cidx |= V_WRB_CIDX_UPD_TRIG_MODE(descq->wrb_trig_mode);
	cidx |= V_WRB_CIDX_UPD_TIMER_IDX(descq->wrb_timer_idx);

	__write_reg(descq->xdev,
		QDMA_REG_WRB_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		cidx);
}

static inline void intr_cidx_update(struct qdma_descq *descq,
						unsigned int sw_cidx)
{
	unsigned int cidx = 0;

	cidx |= V_INTR_CIDX_UPD_SW_CIDX(sw_cidx);

	if (descq->conf.c2h)
		cidx |= S_INTR_CIDX_UPD_DIR_SEL;

	__write_reg(descq->xdev,
		QDMA_REG_INT_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		cidx);

}

/*
 * freelist
 */
static void fl_free(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	struct device *dev = &xdev->conf.pdev->dev;
	struct fl_desc *fl = descq->st_rx_fl;
	int i;

	if (!fl)
		return;

	for (i = 0; i < descq->conf.rngsz; i++, fl++) {
		if (!fl->pg)
			break;
		pr_debug("%s, fl %d, pg 0x%p.\n", descq->conf.name, i, fl->pg);
		dma_unmap_page(dev, fl->dma_addr, PAGE_SIZE, DMA_FROM_DEVICE);
		__free_pages(fl->pg, 0);

		fl->pg = NULL;
		fl->dma_addr = 0UL;
	}

	kfree(descq->st_rx_fl);
	descq->st_rx_fl = NULL;
}

static int fl_refill_entry(struct device *dev, struct fl_desc *fl)
{
	int node = dev_to_node(dev);
	struct page *pg;
	dma_addr_t mapping;

	if (fl->pg) {
		dma_unmap_page(dev, fl->dma_addr, PAGE_SIZE, DMA_FROM_DEVICE);
		fl->pg = NULL;
		fl->dma_addr = 0UL;
	}

	pg = alloc_pages_node(node, GFP_KERNEL | __GFP_COMP, 0);
	if (unlikely(!pg))
		return -ENOMEM;

	mapping = dma_map_page(dev, pg, 0, PAGE_SIZE, PCI_DMA_FROMDEVICE);
	if (unlikely(dma_mapping_error(dev, mapping))) {
		pr_info("page 0x%p mapping error 0x%llx.\n",
			pg, (unsigned long long)mapping);
		__free_pages(pg, 0);
		return -ENOMEM;
	}

	fl->pg = pg;
	fl->dma_addr = mapping;
	return 0;
}

static int fl_fill(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	struct device *dev = &xdev->conf.pdev->dev;
	int node = dev_to_node(dev);
	struct fl_desc *fl;
	int i;
	int rv = 0;

	fl = kzalloc_node(descq->conf.rngsz * sizeof(struct fl_desc),
				GFP_KERNEL, node);
	if (!fl) {
		pr_info("%s OOM, qsz %u.\n",
			descq->conf.name, descq->conf.rngsz);
		return -ENOMEM;
	}

	descq->st_rx_fl = fl;
	for (i = 0; i < descq->conf.rngsz; i++, fl++) {
		fl->pg = NULL;
		rv = fl_refill_entry(&xdev->conf.pdev->dev, fl);
		if (rv < 0) {
			pr_info("%s, %d, fl refill failed.\n",
				descq->conf.name, i);
			goto err_out;
		}
	}

	return 0;

err_out:
	fl_free(descq);
	return -ENOMEM;
}

/* rxq helper functions: calling function should hold the lock */
static inline struct st_rx_data *rxq_dequeue_head(struct st_rx_queue *rxq)
{
	struct st_rx_data *rx = rxq->head;

	if (rx) {
		rxq->head = rx->next;
		rx->next = NULL;
		if (!rxq->head)
		    rxq->tail = NULL;
		rxq->dlen -= (rx->len - rx->offset);
	}

	return rx;
}

static inline void rxq_enqueue_head(struct st_rx_queue *rxq,
				struct st_rx_data *rx)
{
	rx->next = rxq->head;
	rxq->head = rx;
	if (!rxq->tail)
		rxq->tail = rx;
	rxq->dlen += rx->len - rx->offset;
}

static inline void rxq_enqueue_tail(struct st_rx_queue *rxq,
				struct st_rx_data *rx)
{
	rx->next = NULL;
	if (!rxq->head)
		rxq->head = rx;
	else
		rxq->tail->next = rx;
	rxq->tail = rx;
	rxq->dlen += rx->len - rx->offset;
}

/* user def data helper functions: calling function should hold the lock */
static inline struct st_c2h_wrb_udd *udd_dequeue_head(struct st_rx_queue *rxq)
{
    struct st_c2h_wrb_udd *udd = rxq->udd_head;

    if (udd) {
	    rxq->udd_head = udd->next;
	    udd->next = NULL;
	    if (!rxq->udd_head)
		    rxq->udd_tail = NULL;
	    rxq->dlen -= (udd->udd_len - udd->offset);
    }

    return udd;
}

static inline void udd_enqueue_head(struct st_rx_queue *rxq,
                                             struct st_c2h_wrb_udd *udd)
{
    udd->next = rxq->udd_head;
    rxq->udd_head = udd;
    if (!rxq->udd_tail)
	rxq->udd_tail = udd;
    rxq->dlen += udd->udd_len - udd->offset;
}

static inline void udd_enqueue_tail(struct st_rx_queue *rxq,
                                             struct st_c2h_wrb_udd *udd)
{
    udd->next = NULL;
    if (!rxq->udd_head)
	rxq->udd_head = udd;
    else
	rxq->udd_tail->next = udd;
    rxq->udd_tail = udd;
    rxq->dlen += udd->udd_len - udd->offset;
}

static inline void rxq_free_resource(struct st_rx_queue *rxq)
{
    struct st_rx_data *rx = rxq_dequeue_head(rxq);

    while (rx) {
	    __free_pages(rx->pg, 0);
	    kfree(rx);
	    rx = rxq_dequeue_head(rxq);
    }
}

static int descq_st_c2h_rx_data(unsigned long arg, struct fl_desc *fl,
				int fl_nr, struct st_c2h_wrb_udd *udd_ref)
{
	struct qdma_descq *descq = (struct qdma_descq *)arg;
	struct st_rx_queue *rxq = &descq->rx_queue;
	struct st_rx_data *rx = kmalloc(sizeof(*rx), GFP_KERNEL);

	if (!rx) {
		pr_err("%s, OOM.\n", descq->conf.name);
		return -ENOMEM;
	}

	do {
		rx->pg = fl->pg;
		rx->offset = 0;
		rx->len = fl->len;
		rx->udd_ref = udd_ref;

		spin_lock(&rxq->lock);
		rxq_enqueue_tail(rxq, rx);
		spin_unlock(&rxq->lock);

		fl_nr--;
	} while (fl_nr);

	return 0;
}

/*
 * dma transfer requests
 */
static inline void req_submitted(struct qdma_descq *descq,
				struct qdma_sgt_req_cb *cb)
{
	list_del(&cb->list);
	list_add_tail(&cb->list, &descq->pend_list);
}

static ssize_t descq_mm_proc_request(struct qdma_descq *descq,
				struct qdma_sgt_req_cb *cb)
{
	struct qdma_sg_req *req = (struct qdma_sg_req *)cb;
	struct sg_table *sgt = &req->sgt;
	struct scatterlist *sg = sgt->sgl;
	unsigned int sg_offset = 0;
	unsigned int sg_max = sgt->nents;
	u64 ep_addr = req->ep_addr;
	struct qdma_mm_desc *desc = (struct qdma_mm_desc *)descq->desc;
	struct qdma_mm_desc *desc_start = NULL;
	struct qdma_mm_desc *desc_end = NULL;
	unsigned int desc_max = descq->avail;
	unsigned int data_cnt = 0;
	unsigned int desc_cnt = 0;
	unsigned int len = 0;
	int i = 0;

	if (!desc_max) {
		pr_info("descq %s, full, try again.\n", descq->conf.name);
		return 0;
	}

	if (cb->offset) {
		int rv;
		rv = sgt_find_offset(sgt, cb->offset, &sg, &sg_offset);
		if (rv < 0 || rv >= sg_max) {
			pr_info("descq %s, req 0x%p, OOR %u/%u, %d/%u.\n",
				descq->conf.name, req, cb->offset, req->count,
				rv, sg_max);
			return -EINVAL;
		}
		i = rv;
		pr_info("%s, req 0x%p, offset %u/%u -> sg %d, 0x%p,%u.\n",
			descq->conf.name, req, cb->offset, req->count, rv, sg,
			sg_offset);
	} else
		i = 0;

	desc += descq->pidx;
	desc_start = desc;

	for (; i < sg_max && desc_cnt < desc_max; i++, sg = sg_next(sg)) {
		unsigned int tlen = sg_dma_len(sg);
		dma_addr_t addr = sg_dma_address(sg);

		pr_debug("sgl %d, len %u,%u, offset %u.\n",
			i, len, tlen, sg_offset);

		if (sg_offset) {
			sg_offset = 0;
			tlen -= sg_offset;
			addr += sg_offset;
		}

		while (tlen) {
			unsigned int len = min_t(unsigned int, tlen,
						XDMA_DESC_BLEN_MAX);
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

			if (++descq->pidx == descq->conf.rngsz) {
				descq->pidx = 0;
				desc = (struct qdma_mm_desc *)descq->desc;
			} else {
				desc++;
			}

			desc_cnt++;
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

	if (cb->offset == req->count)
		req_submitted(descq, cb);

	descq_pidx_update(descq, descq->pidx);

	if (descq->wbthp)
		qdma_kthread_wakeup(descq->wbthp);

	return 0;
}

static ssize_t descq_proc_st_h2c_request(struct qdma_descq *descq,
				struct qdma_sgt_req_cb *cb)
{
	struct qdma_sg_req *req = (struct qdma_sg_req *)cb;
	struct sg_table *sgt = &req->sgt;
	struct scatterlist *sg = sgt->sgl;
	unsigned int sg_offset = 0;
	unsigned int sg_max = sgt->nents;
	struct qdma_h2c_desc *desc = (struct qdma_h2c_desc *)descq->desc;
	unsigned int desc_max = descq->avail;
	unsigned int data_cnt = 0;
	unsigned int desc_cnt = 0;
	int i = 0;

	/* calling function should hold the lock */

	if (!desc_max) {
		pr_info("descq %s, full, try again.\n", descq->conf.name);
		return 0;
	}

#if 0
	pr_info("%s, req %u.\n", descq->conf.name, req->count);
	sgt_dump(sgt);
#endif

	if (cb->offset) {
		int rv;
		rv = sgt_find_offset(sgt, cb->offset, &sg, &sg_offset);
		if (rv < 0 || rv >= sg_max) {
			pr_info("descq %s, req 0x%p, OOR %u/%u, %d/%u.\n",
				descq->conf.name, req, cb->offset, req->count,
				rv, sg_max);
			return -EINVAL;
		}
		i = rv;
		pr_info("%s, req 0x%p, offset %u/%u -> sg %d, 0x%p,%u.\n",
			descq->conf.name, req, cb->offset, req->count, rv, sg,
			sg_offset);
	} else
		i = 0;

	desc += descq->pidx;

	for (; i < sg_max && desc_cnt < desc_max; i++, sg = sg_next(sg)) {
		unsigned int tlen = sg_dma_len(sg);
		dma_addr_t addr = sg_dma_address(sg);

		if (sg_offset) {
			sg_offset = 0;
			tlen -= sg_offset;
			addr += sg_offset;
		}

		while (tlen) {
			unsigned int len = min_t(unsigned int, tlen, PAGE_SIZE);

			desc->src_addr = addr;

			desc->flag_len = len;
			desc->flag_len |= (1 << S_DESC_F_DV) |
					  (1 << S_DESC_F_SOP) |
					  (1 << S_DESC_F_EOP);

			data_cnt += len;
			addr += len;
			tlen -= len;

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

			descq_pidx_update(descq, descq->pidx);

			desc_cnt++;
			if (desc_cnt == desc_max)
				break;
		}
	}

	descq->avail -= desc_cnt;
	cb->desc_nr += desc_cnt;
	cb->offset += data_cnt;

	pr_debug("descq %s, +%u,%u, avail %u, 0x%x(%u).\n",
		descq->conf.name, desc_cnt, descq->pidx, descq->avail, data_cnt,
		data_cnt);

	if (cb->offset == req->count)
		req_submitted(descq, cb);

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
			qdma_sgt_req_done(cb, 0);
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
	int i, idx = 0, min = xdev->intr_list_cnt[0];

	if (!xdev->num_vecs)
		return;

	/* Pick the MSI-X vector that currently has the fewest queues */
	spin_lock_irqsave(&xdev->lock, flags);
	for (i = 0; i < xdev->num_vecs; i++) {
		if (xdev->intr_list_cnt[i] < min) {
			min = xdev->intr_list_cnt[i];
			idx = i;
		}

		if (!min)
			break;
	}
	xdev->intr_list_cnt[idx]++;
	spin_unlock_irqrestore(&xdev->lock, flags);
	descq->intr_id = idx;
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
	struct xlnx_dma_dev *xdev = descq->xdev;

	pr_debug("descq 0x%p, %s, pidx %u, cidx %u.\n",
		descq, descq->conf.name, descq->pidx, descq->cidx);

	if (descq->pidx == descq->cidx) { /* queue empty? */
		pr_debug("descq %s empty, return.\n", descq->conf.name);
		return 0;
	}

	cidx = descq->cidx;
	wb = (struct qdma_desc_wb *)descq->desc_wb;
	qdma_rmb();

	cidx_hw = wb->cidx;

	if (cidx_hw == cidx) { /* no new writeback? */
		return 0;
	}

#if 0
	print_hex_dump(KERN_INFO, "desc WB ", DUMP_PREFIX_OFFSET,
			 16, 1, (void *)wb, 8, false);
	pr_info("descq %s, wb 0x%08x, cidx 0x%x/0x%x, pidx 0x%04x.\n",
		descq->conf.name, wb->rsvd, wb->cidx, cidx, wb->pidx);
#endif

	cr = descq_wb_credit(descq, cidx_hw);

	/* Worker thread may have only setup a fraction of the transfer (e.g.
	 * there wasn't enough space in desc ring). We now have more space
	 * available again so we can continue programming the
	 * dma transfer by resuming worker thread here.
	 */
	if (!list_empty(&descq->work_list) && descq->avail)
		qdma_kthread_wakeup(descq->wrkthp);

	req_update_pend(descq, cr);

	descq_pidx_update(descq, descq->pidx);

	if(xdev->intr_coal_en)		{
		if(xdev->intr_coal_list->cidx >= xdev->intr_coal_list->intr_ring_size) {
			pr_info("Intr ring wrap around\n");
			xdev->intr_coal_list->cidx = 0;
			xdev->intr_coal_list->pidx = 0;
			xdev->intr_coal_list->color = (xdev->intr_coal_list->color)? 0: 1;
		}
		else {
			xdev->intr_coal_list->cidx++;
			xdev->intr_coal_list->pidx++;
		}
		intr_cidx_update(descq, xdev->intr_coal_list->cidx);
	}

	return 0;
}

static inline void wrb_next(struct qdma_descq *descq)
{
	uint8_t *desc_wrb_cur = (uint8_t *)descq->desc_wrb_cur +
			descq->st_c2h_wrb_entry_len;

	descq->desc_wrb_cur = desc_wrb_cur;
	if (unlikely(++descq->cidx_wrb == descq->rngsz_wrb)) {
		descq->cidx_wrb = 0;
		descq->desc_wrb_cur = descq->desc_wrb;
	}
}

static void inline check_rx_request_completed(struct qdma_descq *descq)
{
	struct st_rx_queue *rxq = &descq->rx_queue;
	struct qdma_sgt_req_cb *cb, *tmp;
	unsigned int dlen;

	spin_lock(&rxq->lock);
	dlen = rxq->dlen;
	spin_unlock(&rxq->lock);

	pr_debug("%s, 0x%p, rx data %u.\n", descq->conf.name, descq, rxq->dlen);

	list_for_each_entry_safe(cb, tmp, &descq->pend_list, list) {
		pr_debug("%s, 0x%p, cb 0x%p, left %u.\n",
			descq->conf.name, descq, cb, cb->offset);

		if (dlen < cb->offset) {
			pr_debug("%s, cb 0x%p pending, left %u > %u.\n",
				descq->conf.name, cb, cb->offset, dlen);
			break;
		}

		pr_debug("%s, cb 0x%p done, left %u <= %u.\n",
			descq->conf.name, cb, cb->offset, dlen);

		dlen -= cb->offset;
		qdma_sgt_req_done(cb, 0);
	}
}

static int descq_st_c2h_wb(struct qdma_descq *descq)
{
	struct device *dev = &descq->xdev->conf.pdev->dev;
	struct qdma_c2h_wrb_wb *wb = (struct qdma_c2h_wrb_wb *)
				descq->desc_wrb_wb;
	unsigned int pidx = descq->pidx;
	struct xlnx_dma_dev *xdev = descq->xdev;
	int budget;
	int proc_cnt;

	qdma_rmb();

	descq->pidx_wrb = wb->pidx;

	if (descq->pidx_wrb > descq->cidx_wrb)
		budget = descq->pidx_wrb - descq->cidx_wrb;
	else if (descq->pidx_wrb < descq->cidx_wrb)
		budget = descq->pidx_wrb +
			(descq->conf.rngsz - descq->cidx_wrb);
	else
		budget = 0;

	proc_cnt = budget;

	while (likely(budget)) {
		u32 len;
		u32 err;
		int fl_nr;
		__be64 *wrb;
		struct st_rx_queue *rxq = &descq->rx_queue;
		struct st_c2h_wrb_udd *udd = NULL;

		qdma_rmb();

		wrb = descq->desc_wrb_cur;

		if (descq->st_c2h_wrb_udd_en) {
			unsigned char *udd_ptr = NULL;

			udd = kmalloc(sizeof(struct st_c2h_wrb_udd), GFP_KERNEL);
			if (!udd) {
				pr_err("%s, OOM.\n", descq->conf.name);
				return -ENOMEM;
			}
			udd->udd_len = descq->st_c2h_wrb_entry_len -
				L_C2H_WB_ENTRY_DMA_INFO + 1; /* +1 for extra nibble*/
			udd->data = kmalloc(udd->udd_len, GFP_KERNEL);
			if (!udd->data) {
				pr_err("%s, OOM.\n", descq->conf.name);
				return -ENOMEM;
			}
			udd_ptr = (unsigned char *)&wrb[0];
			udd->data[0] = udd_ptr[L_C2H_WB_ENTRY_DMA_INFO - 1] & 0xF0;
			memcpy(&udd->data[1],
			       &udd_ptr[L_C2H_WB_ENTRY_DMA_INFO],
			       udd->udd_len - 1);
			udd->offset = 0;
			udd_enqueue_tail(rxq, udd);
		}
		len = (wrb[0] >> S_C2H_WB_ENTRY_LENGTH) &
			M_C2H_WB_ENTRY_LENGTH;
		err = (wrb[0] >> S_C2H_WB_ENTRY_F_DESC_ERR) & 0x1;
		if (unlikely(err)) {
			pr_warn_ratelimited("%s, wb entry error: 0x%x",
					descq->conf.name, err);
			return -EIO;
		}

		fl_nr = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;
		while (fl_nr) {
			struct fl_desc *fl = descq->st_rx_fl;
			struct qdma_c2h_desc *desc = (struct qdma_c2h_desc *)
						descq->desc;
			int rv;

			desc += pidx;
			fl += pidx;
			fl->len = min_t(unsigned int, len, PAGE_SIZE);

			descq->fp_rx_handler(descq->arg, fl, 1, udd);
			udd = NULL;

			rv = fl_refill_entry(dev, fl);
			if (rv < 0) {
				pr_warn_ratelimited("%s, refill fl %d, failed %d.\n",
					descq->conf.name, pidx, rv);
			}

			desc->dst_addr = fl->dma_addr;

			fl_nr--;
			len -= fl->len;

			if (++pidx >= descq->conf.rngsz)
				pidx = 0;
		}

		wrb_next(descq);
		budget--;
	}

	if (proc_cnt) {
		pr_info("%s, 0x%x proc'ed, pidx_wrb 0x%x cidx_wrb 0x%x, pidx 0x%x.\n",
			descq->conf.name, proc_cnt, descq->pidx_wrb,
			descq->cidx_wrb, pidx);

		descq->pidx = pidx;
		descq_pidx_update(descq, descq->pidx ? descq->pidx - 1 :
							descq->conf.rngsz - 1);
		descq_wrb_cidx_update(descq, descq->cidx_wrb);
		if(xdev->intr_coal_en)		{
			if(xdev->intr_coal_list->cidx >= xdev->intr_coal_list->intr_ring_size) {
				pr_info("%s intr ring wrap around\n",
					xdev->conf.name);
				xdev->intr_coal_list->cidx = 0;
				xdev->intr_coal_list->pidx = 0;
				xdev->intr_coal_list->color = (xdev->intr_coal_list->color)? 0: 1;
			}
			else {
				xdev->intr_coal_list->cidx++;
				xdev->intr_coal_list->pidx++;
			}
			intr_cidx_update(descq, xdev->intr_coal_list->cidx);
		}
		check_rx_request_completed(descq);
	}

	return 0;
}

/* ************** public function definitions ******************************* */

void qdma_descq_init(struct qdma_descq *descq, struct xlnx_dma_dev *xdev,
			int idx_hw, int idx_sw)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);

	memset(descq, 0, sizeof(struct qdma_descq));

	spin_lock_init(&descq->lock);
	spin_lock_init(&descq->wrk_lock);
	spin_lock_init(&descq->wb_lock);
	INIT_LIST_HEAD(&descq->work_list);
	INIT_LIST_HEAD(&descq->pend_list);
	INIT_LIST_HEAD(&descq->intr_list);
	INIT_WORK(&descq->work, intr_work);
	descq->xdev = xdev;
	descq->channel = 0;
	descq->qidx_hw = qdev->qbase + idx_hw;
	descq->conf.qidx = idx_sw;

	spin_lock_init(&descq->rx_queue.lock);
}

void qdma_descq_cleanup(struct qdma_descq *descq)
{
	lock_descq(descq);

	if (descq->inited) {
		descq->inited = 0;
		descq->online = 0;
		qdma_descq_context_clear(descq->xdev, descq->qidx_hw,
					descq->conf.st, descq->conf.c2h);
	}

	desc_free_irq(descq);

	qdma_descq_free_resource(descq);

	unlock_descq(descq);
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
		int i;
		struct qdma_c2h_desc *desc = (struct qdma_c2h_desc *)
						descq->desc;
		struct fl_desc *fl;

		rv = fl_fill(descq);
		if (rv < 0)
			goto err_out;

		fl = descq->st_rx_fl;
		for (i = 0; i < descq->conf.rngsz; i++, desc++, fl++)
			desc->dst_addr = fl->dma_addr;

		/* writeback ring */
		descq->desc_wrb = desc_ring_alloc(xdev, descq->rngsz_wrb,
					descq->st_c2h_wrb_entry_len,
					sizeof(struct qdma_c2h_wrb_wb),
					&descq->desc_wrb_bus,
					&descq->desc_wrb_wb);
		if (!descq->desc_wrb) {
			pr_warn("dev %s, descq %s, sz %u, wrb ring OOM.\n",
				xdev->conf.name, descq->conf.name,
				descq->rngsz_wrb);
			goto err_out;
		}
		descq->desc_wrb_cur = descq->desc_wrb;
		descq->fp_rx_handler = descq_st_c2h_rx_data;
		descq->arg = (unsigned long)descq;
		descq->rx_queue.dlen = 0;
	}

	pr_info("%s: %u/%u, rng %u,%u, desc 0x%p, fl 0x%p, wb 0x%p.\n",
		descq->conf.name, descq->conf.qidx, descq->qidx_hw,
		descq->conf.rngsz, descq->rngsz_wrb, descq->desc,
		descq->st_rx_fl, descq->desc_wrb);

	/* interrupt vectors */
	desc_alloc_irq(descq);

	return 0;

err_out:
	qdma_descq_free_resource(descq);
	return -ENOMEM;
}

void qdma_descq_free_resource(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;

	pr_debug("%s: desc 0x%p, fl 0x%p, wrb 0x%p.\n",
		descq->conf.name, descq->desc, descq->st_rx_fl,
		descq->desc_wrb);

	if (descq->desc) {
		int desc_sz = get_desc_size(descq);
		int wb_sz = get_desc_wb_size(descq);

		desc_ring_free(xdev, descq->conf.rngsz, desc_sz, wb_sz,
				descq->desc, descq->desc_bus);

		descq->desc_wb = NULL;
		descq->desc = NULL;
		descq->desc_bus = 0UL;
	}

	if (descq->st_rx_fl)
		fl_free(descq);

	if (descq->desc_wrb) {
		desc_ring_free(xdev, descq->rngsz_wrb,
			descq->st_c2h_wrb_entry_len,
			sizeof(struct qdma_c2h_wrb_wb),
			descq->desc_wrb, descq->desc_wrb_bus);

		descq->desc_wrb_wb = NULL;
		descq->desc_wrb = NULL;
		descq->desc_wrb_bus = 0UL;
	}

	if (descq->rx_queue.dlen) {
		struct st_rx_queue *rxq = &descq->rx_queue;

		spin_lock(&rxq->lock);
		rxq_free_resource(rxq);
		spin_unlock(&rxq->lock);
	}
}

int qdma_descq_rxq_read(struct qdma_descq *descq, struct sg_table *sgt,
                unsigned int count)
{
	struct scatterlist *sgl = sgt->sgl;
	struct scatterlist *sg;
	struct st_rx_queue *rxq = &descq->rx_queue;
	struct st_rx_data *rx;
	unsigned int sg_max = sgt->nents;
	unsigned int copied = 0;
	int i = 0;
	struct st_c2h_wrb_udd *udd = NULL;

	spin_lock(&rxq->lock);

	rx = rxq_dequeue_head(rxq);
	if ((rx == NULL) && descq->st_c2h_wrb_udd_en) {
		/* This is the case where there is no actual data */
		udd = udd_dequeue_head(rxq);
		if (udd == NULL) {
			pr_err("Write back entry without udd or actual data\n");
			return copied;
		}
	}
	for_each_sg(sgl, sg, sg_max, i) {
		unsigned int len = sg->length;
		unsigned int off = 0;
		unsigned int udd_state = 0;

		while (len && (rx || udd)) {
			unsigned int copy = len;
			/*
			 * These are the cases to consider corresponding to write back entries:
			 * 1. There is actual data and user defined data.
			 * 2. There is only actual data. No user defined data
			 * 3. There is only user defined data
			 * */
			/* rx->offset=0 indicates transmission of SOP */
			if ((rx && (rx->udd_ref != NULL) &&
				(rx->offset == 0)) || udd) {
				unsigned int udd_cpy;

				udd_state = 1;
				if (rx) {
					udd = udd_dequeue_head(rxq);
					udd_state = 2;
				}
				if (udd) {
					udd_state = 3;
					udd_cpy = min_t(unsigned int,
					                copy,
					                udd->udd_len -
					                udd->offset);

					memcpy(sg_virt(sg) + off,
					       udd->data + rx->offset, udd_cpy);
					len -= udd_cpy;
					off += udd_cpy;
					copied += udd_cpy;
					udd->offset += udd_cpy;

					if (udd->offset == udd->udd_len) {
						/* free already dequeued udd */
						kfree(udd->data);
						kfree(udd);
						if (rx) {
							udd = NULL;
						} else {
							/* when no actual data, it is continous udd */
							udd = udd_dequeue_head(rxq);
						}
					} else
					    udd_enqueue_head(rxq, udd);
					/* No actual data can be copied, so rx has to be enqueued back.
					 *                          * so not returning here */
					pr_info("\ncopied udd at off=%d\n\n",copied-udd_cpy);
				}
			}

			if (rx) {
				copy = min_t(unsigned int, len,
				             rx->len - rx->offset);

				memcpy(sg_virt(sg) + off,
				       page_address(rx->pg) + rx->offset, copy);
			}

			copied += copy;
			len -= copy;
			off += copy;
			rx->offset += copy;

			if (rx->offset == rx->len) {
				__free_pages(rx->pg, 0);
				kfree(rx);
				rx = rxq_dequeue_head(rxq);
			}
		}

		if (!rx && !udd)
		    break;
	}

	if (rx && rx->offset < rx->len) {
		/* put back to the queue */
		rxq_enqueue_head(rxq, rx);
	}

	spin_unlock(&rxq->lock);

	return copied;
}

int qdma_descq_config(struct qdma_descq *descq, struct qdma_queue_conf *qconf,
		 int reconfig)
{
	descq->conf.rngsz = RNG_SZ_DFLT;
	descq->rngsz_wrb = WRB_RNG_SZ_DFLT;

	/* we can never use the full ring because then cidx would equal pidx
	 * and thus the ring would be interpreted as empty. Thus max number of
	 * usable entries is ring_size - 1
	 */
	descq->avail = RNG_SZ_DFLT - 1;

	descq->pidx = 0;
	descq->cidx = 0;
	descq->cidx_wrb = 0;
	descq->pidx_wrb = 0;
	descq->credit = 0;
	descq->irq_en = (descq->xdev->num_vecs) ? 1 : 0;
	descq->wrb_stat_desc_en = 1;
	descq->wrb_trig_mode = TRIG_MODE_ANY;
	descq->wrb_timer_idx = 0;
	descq->prefetch_en = descq->xdev->conf.pftch_en;
	if (qconf->c2h && qconf->st) {
	    descq->st_c2h_wrb_desc_size =
			    (enum ctxt_desc_sz_sel)qconf->st_c2h_wrb_desc_size;
	    descq->st_c2h_wrb_udd_en = true;
	} else {
	    descq->st_c2h_wrb_desc_size = DESC_SZ_RSV;
	    descq->st_c2h_wrb_udd_en = false;
	}

	switch (descq->st_c2h_wrb_desc_size) {
	default:
	case DESC_SZ_RSV:
	    descq->st_c2h_wrb_udd_en = false;
	case DESC_SZ_8B:
	    descq->st_c2h_wrb_entry_len = 8;
	    break;
	case DESC_SZ_16B:
	    descq->st_c2h_wrb_entry_len = 16;
	    break;
	case DESC_SZ_32B:
	    descq->st_c2h_wrb_entry_len = 32;
	    break;
	}

	if (!reconfig) {
		int len;

		descq->conf.st = qconf->st;
		descq->conf.c2h = qconf->c2h;

		/* qdma[vf]<255>-MM/ST-H2C/C2H-Q[2048] */
#ifdef __QDMA_VF__
		len = sprintf(descq->conf.name, "qdmavf");
#else
		len = sprintf(descq->conf.name, "qdma");
#endif
		len += sprintf(descq->conf.name + len, "%d-%s-%s-%u",
			descq->xdev->conf.idx, descq->conf.st ? "ST" : "MM",
			descq->conf.c2h ? "C2H" : "H2C", descq->conf.qidx);
		descq->conf.name[len] = '\0';
	}

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
		descq_pidx_update(descq, descq->conf.rngsz - 1);
	}
	intr_cidx_update(descq, 0);

	return rv;
}

void qdma_descq_service_wb(struct qdma_descq *descq)
{
	lock_descq(descq);
	if (descq->conf.st && descq->conf.c2h)
		descq_st_c2h_wb(descq);
	else
		descq_mm_n_h2c_wb(descq);
	unlock_descq(descq);
}

ssize_t qdma_descq_proc_sgt_request(struct qdma_descq *descq,
					struct qdma_sgt_req_cb *cb)
{
	if(!descq->conf.st) /* MM H2C/C2H */
		return descq_mm_proc_request(descq, cb);
	else if (descq->conf.st && !descq->conf.c2h) /* ST H2C */
		return descq_proc_st_h2c_request(descq, cb);
	else	/* ST C2H - should not happen - handled separately */
		return -1;
}

void qdma_sgt_req_done(struct qdma_sgt_req_cb *cb, int error)
{
	struct qdma_sg_req *req = (struct qdma_sg_req *)cb;

	if (error)
		pr_info("req 0x%p, cb 0x%p, fp_done 0x%p done, err %d.\n",
			req, cb, req->fp_done, error);

	list_del(&cb->list);
	if (req->fp_done) {
		if (cb->offset != req->count) {
			pr_info("req not completed %u != %u.\n",
				cb->offset, req->count);
			error = -EINVAL;
		}
		cb->status = error;
		cb->done = 1;
		req->fp_done(req, cb->offset, error);
	} else {
		pr_debug("req 0x%p, cb 0x%p, wake up.\n", req, cb);
		cb->status = error;
		cb->done = 1;
		wake_up_interruptible(&cb->wq);
	}
}

int qdma_descq_dump_desc(struct qdma_descq *descq, int start, int end, char *buf,
			int buflen)
{
	int desc_sz = get_desc_size(descq);
	u8 *p = descq->desc + start * desc_sz;
	struct fl_desc *fl = descq->st_rx_fl;
	int i = start;
	int len = strlen(buf);

	if (!descq->desc)
		return 0;

	for (; i < end && i < descq->conf.rngsz; i++, p += desc_sz) {
		len += sprintf(buf + len, "%d: 0x%p ", i, p);
		hex_dump_to_buffer(p, desc_sz, 16, 4, buf + len,
				buflen - len, 0);
		len = strlen(buf);
		if (fl) {
			len += sprintf(buf + len, " fl pg 0x%p, 0x%llx.\n",
				fl->pg, fl->dma_addr);
			fl++;
		} else
			buf[len++] = '\n';
	}

	p = descq->desc_wb;

	qdma_rmb();

	len += sprintf(buf + len, "WB: 0x%p ", p);
	hex_dump_to_buffer(p, get_desc_wb_size(descq), 16, 4,
			buf + len, buflen - len, 0);
	len = strlen(buf);
	buf[len++] = '\n';

	fl = descq->st_rx_fl;
	p = page_address(fl->pg);
	len += sprintf(buf + len, "data 0: 0x%p ", p);
	hex_dump_to_buffer(p, 128, 16, 4, buf + len,
			buflen - len, 0);
	len = strlen(buf);
	buf[len++] = '\n';

	return len;
}

int qdma_descq_dump_wrb(struct qdma_descq *descq, int start, int end, char *buf,
			int buflen)
{
	uint8_t *wrb = descq->desc_wrb;
	u8 *p;
	int i = start;
	int len = strlen(buf);

	if (!descq->desc_wrb)
		return 0;

	for (wrb += (start * descq->st_c2h_wrb_entry_len);
			i < end && i < descq->rngsz_wrb; i++, wrb_next(descq)) {
		len += sprintf(buf + len, "%d: 0x%p ", i, wrb);
		hex_dump_to_buffer(wrb, descq->st_c2h_wrb_entry_len,
				16, 4, buf + len, buflen - len, 0);
		len = strlen(buf);
		buf[len++] = '\n';
	}

	len += sprintf(buf + len, "WB: 0x%p ", descq->desc_wrb_wb);

	p = descq->desc_wrb_wb;
	qdma_rmb();
	hex_dump_to_buffer(p, sizeof(struct qdma_c2h_wrb_wb),
			16, 4, buf + len, buflen - len, 0);
	len = strlen(buf);
	buf[len++] = '\n';

	return len;
}

int qdma_descq_dump_state(struct qdma_descq *descq, char *buf)
{
	int len;

	if (!buf)
		return 0;

	len = sprintf(buf, "ID 0x%x,0x%x %s: ",
			descq->conf.qidx, descq->qidx_hw, descq->conf.name);

	if (descq->online)
		len += sprintf(buf + len, "online");
	else if (descq->inited)
		len += sprintf(buf + len, "up");
	else if (descq->enabled)
		len += sprintf(buf + len, "cfg'ed");
	else {
		len += sprintf(buf + len, "un-initialized\n");
		return len;
	}

	buf[len++] = '\n';
	return len;
}

int qdma_descq_dump(struct qdma_descq *descq, char *buf, int buflen, int detail)
{
	int len = 0;

	if (!buf) {
		pr_info("%s: 0x%x/0x%x, desc sz %u+%u/%u, pidx %u, cidx %u\n",
			descq->conf.name, descq->conf.qidx, descq->qidx_hw,
			descq->conf.rngsz, descq->avail, descq->pend,
			descq->pidx, descq->cidx);
		return 0;
	}

	len = qdma_descq_dump_state(descq, buf);
	if (!descq->enabled)
		goto buf_done;

	len += sprintf(buf + len,
		"\tthp %s, %s, desc 0x%p/0x%llx, %u\n",
		descq->wrkthp ? descq->wrkthp->name : "?",
		descq->wbthp ? descq->wbthp->name : "?",
		descq->desc, descq->desc_bus, descq->conf.rngsz);
	if (descq->conf.st && descq->conf.c2h) {
		len += sprintf(buf + len,
			"\twrb desc 0x%p/0x%llx, %u, rxq 0x%x",
			descq->desc_wrb, descq->desc_wrb_bus, descq->rngsz_wrb,
			descq->rx_queue.dlen);
	}

	if (!detail)
		goto buf_done;

	if (descq->desc_wb) {
		u8 *wb;

		len += sprintf(buf + len, "\n\tWB: 0x%p, ", descq->desc_wb);

		wb = descq->desc_wb;
		qdma_rmb();
		hex_dump_to_buffer(wb,
				sizeof(struct qdma_desc_wb), 16, 4, buf + len,
				buflen - len, 0);
		len = strlen(buf);
		buf[len++] = '\n';
	}
	if (descq->desc_wrb_wb) {
		u8 *wb;

		len += sprintf(buf + len, "\tWRB WB: 0x%p, ",
				descq->desc_wrb_wb);

		wb = descq->desc_wrb_wb;
		qdma_rmb();
		hex_dump_to_buffer(wb,
			sizeof(struct qdma_c2h_wrb_wb), 16, 4,
			buf + len, buflen - len, 0);
		len = strlen(buf);
		buf[len++] = '\n';
	}

buf_done:
	buf[len] = '\0';
	return len;

	return 0;
}


