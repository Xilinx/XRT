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

#include <linux/kernel.h>
#include <linux/pci.h>
#include "qdma_device.h"
#include "qdma_descq.h"
#include "qdma_intr.h"
#include "qdma_regs.h"
#include "qdma_context.h"

/*
 * Q Context: clear, read and set
 */
static int make_intr_context(struct xlnx_dma_dev *xdev, u32 *data, int cnt)
{
	int i, j;

	if (xdev->intr_coal_en) {
		memset(data, 0, cnt * sizeof(u32));
		return 0;
	}

	/* assume data[2 * XDEV_NUM_IRQ_MAX] */
	if (cnt < (xdev->num_vecs << 1)) {
		pr_warn("%s, intr context %d < (%d * 2).\n",
			xdev->conf.name, cnt, xdev->num_vecs);
		return -EINVAL;
	}

	memset(data, 0, cnt * sizeof(u32));
	/* program the coalescing context */
	for (i = 0, j = 0; i < xdev->num_vecs; i++) {
		u64 bus_64;
		u32 v;
		struct intr_coal_conf *entry = (xdev->intr_coal_list + i);

		/* TBD:
		 * Assume that Qid is irrelevant for interrupt context
		 * programming because, interrupt context is done per vector
		 * which for the function and not for each queue
		 */

		bus_64 = (PCI_DMA_H(entry->intr_ring_bus) << 12) |
			 ((PCI_DMA_L(entry->intr_ring_bus)) >> 9);

		v = bus_64 & M_INT_COAL_W0_BADDR_64;

		data[j] = (1 << S_INT_COAL_W0_F_VALID |
			   V_INT_COAL_W0_VEC_ID(i)|
			   V_INT_COAL_W0_BADDR_64(v));

		v = bus_64 & M_INT_COAL_W1_BADDR_64;
		data[++j] = (V_INT_COAL_W1_BADDR_64(v));

		j++;
	}

	return 0;
}

static int make_sw_context(struct qdma_descq *descq, u32 *data, int cnt)
{
	if (cnt < 4) {
		pr_warn("%s, sw context count %d < 4.\n",
			descq->xdev->conf.name, cnt);
		return -EINVAL;
	}
	memset(data, 0, cnt * sizeof(u32));

	/* sw context */
	data[3] = PCI_DMA_H(descq->desc_bus);
	data[2] = PCI_DMA_L(descq->desc_bus);
	data[1] = (1 << S_DESC_CTXT_W1_F_QEN) |
		  (1 << S_DESC_CTXT_W1_F_WB_ACC_EN) |
		  (1 << S_DESC_CTXT_W1_F_WBI_CHK) |
		  (V_DESC_CTXT_W1_FUNC_ID(descq->xdev->func_id)) |
		  (1 << S_DESC_CTXT_W1_F_WBK_EN) |
		  (descq->irq_en << S_DESC_CTXT_W1_F_IRQ_EN);

	if (!descq->conf.st) { /* mm h2c/c2h */
		data[1] |= (V_DESC_CTXT_W1_DSC_SZ(DESC_SZ_32B)) |
			   (descq->channel << S_DESC_CTXT_W1_F_MM_CHN);
	} else if (descq->conf.c2h) {  /* st c2h */
		data[1] |= (1 << S_DESC_CTXT_W1_F_FCRD_EN) |
			   (V_DESC_CTXT_W1_DSC_SZ(DESC_SZ_8B));
	} else { /* st h2c */
		data[1] |= V_DESC_CTXT_W1_DSC_SZ(DESC_SZ_16B);
	}
	data[0] = 0; /* pidx = 0; irq_ack = 0 */

	pr_info("%s, SW 0x%08x, 0x%08x, 0x%08x, 0x%08x.\n",
		descq->conf.name, data[3], data[2], data[1], data[0]);

	return 0;
}

/* ST: prefetch context setup */
static int make_prefetch_context(struct qdma_descq *descq, u32 *data, int cnt)
{
	if (cnt < 2) {
		pr_warn("%s, prefetch context count %d < 4.\n",
			descq->xdev->conf.name, cnt);
		return -EINVAL;
	}
	memset(data, 0, cnt * sizeof(u32));

	/* prefetch context */
	data[1] = 1 << S_PFTCH_W1_F_VALID;
	data[0] = /* TODO buffer size index*/
		  (V_PFTCH_W0_FNC_ID(descq->xdev->func_id)) |
		  /* TODO port id*/
		  (descq->prefetch_en << S_PFTCH_W0_F_EN_PFTCH);

	pr_info("%s, PFTCH 0x%08x, 0x%08x.\n",
		descq->conf.name, data[1], data[0]);

	return 0;
}

/* ST C2H : writeback context setup */
static int make_wrb_context(struct qdma_descq *descq, u32 *data, int cnt)
{
	u64 bus_64;
	u32 v;

	if (cnt < 4) {
		pr_warn("%s, wrb context count %d < 4.\n",
			descq->xdev->conf.name, cnt);
		return -EINVAL;
	}
	memset(data, 0, cnt * sizeof(u32));

	/* writeback context */
	bus_64 = (PCI_DMA_H(descq->desc_wrb_bus) << 26) |
		 ((PCI_DMA_L(descq->desc_wrb_bus)) >> 6);

	v = bus_64 & M_WRB_CTXT_W0_BADDR_64;

	data[0] = (descq->wrb_stat_desc_en << S_WRB_CTXT_W0_F_EN_STAT_DESC) |
		  (descq->irq_en << S_WRB_CTXT_W0_F_EN_INT) |
		  (V_WRB_CTXT_W0_TRIG_MODE(descq->wrb_trig_mode)) |
		  (V_WRB_CTXT_W0_FNC_ID(descq->xdev->func_id)) |
		  /* TODO timer index */
		  /* TODO counter index */
		  (1 << S_WRB_CTXT_W0_F_COLOR) |
		  /* TODO ring size index (S_WRB_CTXT_W0_RNG_SZ(0)) | */
		  (V_WRB_CTXT_W0_BADDR_64(v));

	data[1] = (bus_64 >> L_WRB_CTXT_W0_BADDR_64) & 0xFFFFFFFF;

	v = (bus_64 >> (L_WRB_CTXT_W0_BADDR_64 + 32)) & M_WRB_CTXT_W2_BADDR_64;
	data[2] = (V_WRB_CTXT_W2_BADDR_64(v)) |
		  (V_WRB_CTXT_W2_DESC_SIZE(descq->st_c2h_wrb_desc_size));

	data[3] = (1 << S_WRB_CTXT_W3_F_VALID);

	pr_info("%s, WRB 0x%08x, 0x%08x, 0x%08x, 0x%08x.\n",
		descq->conf.name, data[3], data[2], data[1], data[0]);

	return 0;
}


#ifdef __QDMA_VF__
int qdma_intr_context_setup(struct xlnx_dma_dev *xdev)
{
	u32 data[XDEV_NUM_IRQ_MAX << 1];
	int i = 0;
	int rv;

	if (!xdev->intr_coal_en)
		return 0;

	rv = make_intr_context(xdev, data, XDEV_NUM_IRQ_MAX << 1);
	if (rv < 0)
		return rv;	
	do {
		struct mbox_msg m;
		struct mbox_msg_hdr *hdr = &m.hdr;
		struct mbox_msg_intr_ctxt *ictxt = &m.intr_ctxt;
		u8 copy = xdev->num_vecs  - i;

		if (copy > MBOX_INTR_CTXT_VEC_MAX)
			copy = MBOX_INTR_CTXT_VEC_MAX;

		memset(&m, 0, sizeof(struct mbox_msg));

		hdr->src = xdev->func_id;
		hdr->dst = xdev->func_id_parent;
		hdr->op = MBOX_OP_INTR_CTXT;

		ictxt->clear = 1;
		ictxt->vec_base = i;
		ictxt->vec_cnt = copy;

		memcpy(ictxt->w, data + 2 * i, copy * sizeof(u32));	

		rv = qdma_mbox_send_msg(xdev, &m, 1);
		if (rv < 0) {
			pr_info("%s, vec %d, +%d mbox failed %d.\n",
				xdev->conf.name, i, copy, rv);
			return rv;
		}
		if (hdr->ack) {
			if (hdr->status)
				return hdr->status;
		} else {
pr_info("%s, vec %d, +%d mbox, rcv status %d.\n", xdev->conf.name, i, copy, hdr->status);
			return -EINVAL;
		}

		i += copy;

	} while (i < xdev->num_vecs);
	
	return 0;
}

int qdma_descq_context_clear(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h)
{
	struct mbox_msg m;
	struct mbox_msg_hdr *hdr = &m.hdr;
	struct mbox_msg_qctxt *qctxt = &m.qctxt;
	int rv;

	memset(&m, 0, sizeof(m));

	hdr->src = xdev->func_id;
	hdr->dst = xdev->func_id_parent;
	hdr->op = MBOX_OP_QCTXT_CLR;

	qctxt->qid = qid_hw;
	qctxt->st = st;
	qctxt->c2h = c2h;

	rv = qdma_mbox_send_msg(xdev, &m, 1);
	if (rv < 0) {
		pr_info("%s, qid_hw 0x%x mbox failed %d.\n",
			xdev->conf.name, qid_hw, rv);
		return rv;
	}
	pr_info("%s, mbox rcv ack:%d, status 0x%x.\n",
		xdev->conf.name, hdr->ack, hdr->status);
	if (hdr->ack) 
		return hdr->status;
	else { 
		return -EINVAL;
	}

}

int qdma_descq_context_read(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *context)
{
	struct mbox_msg m;
	struct mbox_msg_hdr *hdr = &m.hdr;
	struct mbox_msg_qctxt *qctxt = &m.qctxt;
	int rv;
	
	memset(&m, 0, sizeof(m));

	hdr->op = MBOX_OP_QCTXT_RD;
	hdr->dst = xdev->func_id_parent;
	hdr->src = xdev->func_id;	

	qctxt->qid = qid_hw;
	qctxt->st = st;
	qctxt->c2h = c2h;

	pr_info("%s, mbox send to PF, QCTXT RD 0x%x.\n",
		xdev->conf.name, qid_hw);

	rv = qdma_mbox_send_msg(xdev, &m, 1);
	if (rv < 0) {
		pr_info("%s, qid_hw 0x%x mbox failed %d.\n",
			xdev->conf.name, qid_hw, rv);
		return rv;
	}
	pr_info("%s, mbox rcv ack:%d, status 0x%x.\n",
		xdev->conf.name, hdr->ack, hdr->status);
	if (hdr->ack) {
		if (hdr->status) 
			return hdr->status;
	} else 
		return -EINVAL;

	memcpy(context, &qctxt->context, sizeof(struct hw_descq_context));
	return 0;
}

int qdma_descq_context_setup(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	struct mbox_msg m;
	struct mbox_msg_hdr *hdr = &m.hdr;
	struct mbox_msg_qctxt *qctxt = &m.qctxt;
	struct hw_descq_context *context = &qctxt->context;
	int rv;

	memset(&m, 0, sizeof(m));

	make_sw_context(descq, context->sw, 4);
	if (descq->conf.st && descq->conf.c2h) {
		make_prefetch_context(descq, context->prefetch, 2);
		make_wrb_context(descq, context->wrb, 4);
	}

	hdr->op = MBOX_OP_QCTXT_WRT;
	hdr->src = xdev->func_id;	
	hdr->dst = xdev->func_id_parent;

	qctxt->clear = 1;
	qctxt->verify = 1;
	qctxt->st = descq->conf.st;
	qctxt->c2h = descq->conf.c2h;

	qctxt->qid = descq->qidx_hw;

	rv = qdma_mbox_send_msg(xdev, &m, 1);
	if (rv < 0) {
		pr_info("%s, qid_hw 0x%x, %s mbox failed %d.\n",
			xdev->conf.name, descq->qidx_hw, descq->conf.name, rv);
		return rv;
	}
	pr_info("%s, mbox rcv ack:%d, status 0x%x.\n",
		xdev->conf.name, hdr->ack, hdr->status);
	if (hdr->ack)
		return hdr->status;
	else 
		return -EINVAL;
}

#else /* PF only */

int qdma_intr_context_setup(struct xlnx_dma_dev *xdev)
{
	u32 data[XDEV_NUM_IRQ_MAX << 1];
	int i = 0;
	int rv;

	if (!xdev->intr_coal_en)
		return 0;

	rv = make_intr_context(xdev, data, XDEV_NUM_IRQ_MAX << 1);
	if (rv < 0)
		return rv;	

	for (i = 0; i < xdev->num_vecs; i++) {
		/*
		 * TBD: Assume that Qid is irrelevant for interrupt context
		 * programming because, interrupt context is done per vector
		 * which for the function and not for each queue
		 */
		/* INTR COALESCING */
		rv = hw_indirect_ctext_prog(xdev, 0, QDMA_CTXT_CMD_CLR,
					QDMA_CTXT_SEL_COAL, NULL, 4, 0);
		if (rv < 0)
			return rv;

		rv = hw_indirect_ctext_prog(xdev, 0, QDMA_CTXT_CMD_WR,
					QDMA_CTXT_SEL_COAL, data + 2*i, 2, 1);
		if (rv < 0)
			return rv;
	}
	
	return 0;
}

int qdma_descq_context_clear(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h)
{
	u8 sel;
	int rv = 0;

	sel = c2h ? QDMA_CTXT_SEL_SW_C2H : QDMA_CTXT_SEL_SW_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_CLR, sel,
					 NULL, 0, 0);
	if (rv < 0)
		return rv;

	sel = c2h ? QDMA_CTXT_SEL_HW_C2H : QDMA_CTXT_SEL_HW_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_CLR, sel,
				 NULL, 0, 0);
	if (rv < 0)
		return rv;

	sel = c2h ? QDMA_CTXT_SEL_CR_C2H : QDMA_CTXT_SEL_CR_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_CLR, sel,
		NULL, 0, 0);
	if (rv < 0)
		return rv;

	/* Only clear prefetch and writeback contexts if this queue is ST C2H */
	if (st && c2h) {
#if 1
		/* FIXME - workaround for issues relating to prefetch clearing
		 * Dont clear prefetch context via CLEAR command, rather use
		 * WRITE command with all data set to zero */

		u32 data[4] = {0};

		rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_WR,
					QDMA_CTXT_SEL_PFTCH, data, 4, 1);
#else
		rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_CLR,
					QDMA_CTXT_SEL_PFTCH, NULL, 0, 0);
#endif
		if (rv < 0)
			return rv;
		rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_CLR,
					QDMA_CTXT_SEL_WRB, NULL, 0, 0);
		if (rv < 0)
			return rv;
	}

	/* TODO interrupt context (0x8) */

	/* TODO pasid context (0x9) */

	return 0;
}

int qdma_descq_context_setup(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	struct hw_descq_context context;
	int rv;

	if (descq->irq_en)
		hw_prog_qid2vec(xdev, descq->qidx_hw, descq->conf.c2h,
				descq->intr_id, xdev->intr_coal_en);	

	rv = qdma_descq_context_clear(xdev, descq->qidx_hw, descq->conf.st,
				descq->conf.c2h);
	if (rv < 0)
		return rv;

	memset(&context, 0, sizeof(context));

	make_sw_context(descq, context.sw, 4);
        if (descq->conf.st && descq->conf.c2h) {
                make_prefetch_context(descq, context.prefetch, 2);
                make_wrb_context(descq, context.wrb, 4);
        }

	return qdma_descq_context_program(descq->xdev, descq->qidx_hw,
				descq->conf.st, descq->conf.c2h, &context);
}

int qdma_descq_context_read(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *context)
{
	u8 sel;
	int rv = 0;

	memset(context, 0, sizeof(struct hw_descq_context));

	sel = c2h ? QDMA_CTXT_SEL_SW_C2H : QDMA_CTXT_SEL_SW_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_RD, sel,
					context->sw, 4, 0);
	if (rv < 0)
		return rv;

	sel = c2h ? QDMA_CTXT_SEL_HW_C2H : QDMA_CTXT_SEL_HW_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_RD, sel,
					context->hw, 2, 0);
	if (rv < 0)
		return rv;

	sel = c2h ? QDMA_CTXT_SEL_CR_C2H : QDMA_CTXT_SEL_CR_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_RD, sel,
					context->cr, 1, 0);
	if (rv < 0)
		return rv;

	if (st && c2h) {
		rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_RD,
					QDMA_CTXT_SEL_WRB, context->wrb, 4, 0);
		if (rv < 0)
			return rv;
		rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_RD,
					QDMA_CTXT_SEL_PFTCH, context->prefetch,
					2, 0);
		if (rv < 0)
			return rv;
	}

	return 0;
}

int qdma_descq_context_program(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
                                bool st, bool c2h,
				struct hw_descq_context *context)

{
	u8 sel;
	int rv;

	/* always clear first */
	rv = qdma_descq_context_clear(xdev, qid_hw, st, c2h);
	if (rv < 0)
		return rv;


	sel = c2h ?  QDMA_CTXT_SEL_SW_C2H : QDMA_CTXT_SEL_SW_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_WR, sel,
				context->sw, 4, 1);
	if (rv < 0)
		return rv;

	/* Only c2h st specific setup done below*/
	if (!st || !c2h)
		return 0;

	/* prefetch context */
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_WR,
				QDMA_CTXT_SEL_PFTCH, context->prefetch, 2, 1);
	if (rv < 0)
		return rv;

	/* writeback context */
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_WR,
				QDMA_CTXT_SEL_WRB, context->wrb, 4, 1);
	if (rv < 0)
		return rv;

	return 0;
}

#endif
