/*******************************************************************************
 *
 * Xilinx XDMA IP Core Linux Driver
 * Copyright(c) 2015 - 2017 Xilinx, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "LICENSE".
 *
 * Karen Xie <karen.xie@xilinx.com>
 *
 ******************************************************************************/

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

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

	if (!xdev->intr_coal_en) {
		memset(data, 0, cnt * sizeof(u32));
		return 0;
	}

	/* assume data[2 * XDEV_NUM_IRQ_MAX] */
	if (cnt < (QDMA_DATA_VEC_PER_PF_MAX << 1)) {
		pr_warn("%s, intr context %d < (%d * 2).\n",
			xdev->conf.name, cnt, QDMA_DATA_VEC_PER_PF_MAX);
		return -EINVAL;
	}

	memset(data, 0, cnt * sizeof(u32));
	/* program the coalescing context */
	for (i = 0, j = 0; i < QDMA_DATA_VEC_PER_PF_MAX; i++) {
		u64 bus_64;
		u32 v;
		struct intr_coal_conf *entry = (xdev->intr_coal_list + i );

		/* TBD:
		 * Assume that Qid is irrelevant for interrupt context
		 * programming because, interrupt context is done per vector
		 * which for the function and not for each queue
		 */

		bus_64 = (PCI_DMA_H(entry->intr_ring_bus) << 20) |
			 ((PCI_DMA_L(entry->intr_ring_bus)) >> 12);

		v = bus_64 & M_INT_COAL_W0_BADDR_64;

		data[j] = (1 << S_INT_COAL_W0_F_VALID |
			   V_INT_COAL_W0_VEC_ID(entry->vec_id)|
			   V_INT_COAL_W0_BADDR_64(v) |
			   1 << S_INT_COAL_W0_F_COLOR);


		v = (bus_64 >> L_INT_COAL_W0_BADDR_64) & M_INT_COAL_W1_BADDR_64;
		data[++j] = ((V_INT_COAL_W1_BADDR_64(v)) |
					  V_INT_COAL_W1_VEC_SIZE(xdev->conf.intr_rngsz));

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
			V_DESC_CTXT_W1_RNG_SZ(descq->conf.desc_rng_sz_idx) |
			(descq->conf.wbk_acc_en << S_DESC_CTXT_W1_F_WB_ACC_EN) |
			(descq->conf.wbk_pend_chk << S_DESC_CTXT_W1_F_WBI_CHK) |
			(V_DESC_CTXT_W1_FUNC_ID(descq->xdev->func_id)) |
			(descq->conf.bypass << S_DESC_CTXT_W1_F_BYP) |
			(descq->conf.wbk_en << S_DESC_CTXT_W1_F_WBK_EN) |
			(descq->conf.irq_en << S_DESC_CTXT_W1_F_IRQ_EN);
#ifdef ERR_DEBUG
	if (descq->induce_err & (1 << param)) {
		data[1] |= (0xFF << S_DESC_CTXT_W1_FUNC_ID);
		pr_info("induced error %d", ind_ctxt_cmd_err);
	}
#endif

	if (!descq->conf.st) { /* mm h2c/c2h */
		data[1] |= (V_DESC_CTXT_W1_DSC_SZ(DESC_SZ_32B)) |
			   (descq->channel << S_DESC_CTXT_W1_F_MM_CHN);
	} else if (descq->conf.c2h) {  /* st c2h */
		data[1] |= (descq->conf.fetch_credit << S_DESC_CTXT_W1_F_FCRD_EN) |
			   (V_DESC_CTXT_W1_DSC_SZ(DESC_SZ_8B));
	} else { /* st h2c */
		data[1] |= V_DESC_CTXT_W1_DSC_SZ(DESC_SZ_16B);
	}
	data[0] = 0; /* pidx = 0; irq_ack = 0 */

	pr_debug("%s, SW 0x%08x, 0x%08x, 0x%08x, 0x%08x.\n",
		descq->conf.name, data[3], data[2], data[1], data[0]);

	return 0;
}

/* ST: prefetch context setup */
static int make_prefetch_context(struct qdma_descq *descq, u32 *data, int cnt)
{
	BUG_ON(!descq);
	BUG_ON(!data);

	if (cnt < 2) {
		pr_warn("%s, prefetch context count %d < 2.\n",
			descq->conf.name, cnt);
		return -EINVAL;
	}
	memset(data, 0, cnt * sizeof(u32));

	/* prefetch context */
	data[1] = 1 << S_PFTCH_W1_F_VALID;
	if (descq->conf.pfetch_en) {
		data[0] = (descq->conf.bypass << S_PFTCH_W0_F_BYPASS) |
			  (descq->conf.c2h_buf_sz_idx <<
					  S_PFTCH_W0_BUF_SIZE_IDX) |
			  /* TODO port id*/
			  (descq->conf.pfetch_en << S_PFTCH_W0_F_EN_PFTCH);
	}

	pr_debug("%s, PFTCH 0x%08x 0x%08x\n",
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

	data[0] = (descq->conf.cmpl_stat_en << S_WRB_CTXT_W0_F_EN_STAT_DESC) |
		  (descq->conf.irq_en << S_WRB_CTXT_W0_F_EN_INT) |
		  (V_WRB_CTXT_W0_TRIG_MODE(descq->conf.cmpl_trig_mode)) |
		  (V_WRB_CTXT_W0_FNC_ID(descq->xdev->func_id)) |
		  (descq->conf.cmpl_timer_idx << S_WRB_CTXT_W0_TIMER_IDX) |
		  (descq->conf.cmpl_cnt_th_idx << S_WRB_CTXT_W0_COUNTER_IDX) |
		  (1 << S_WRB_CTXT_W0_F_COLOR) |
		  (descq->conf.cmpl_rng_sz_idx << S_WRB_CTXT_W0_RNG_SZ) |
		  (V_WRB_CTXT_W0_BADDR_64(v));

	data[1] = (bus_64 >> L_WRB_CTXT_W0_BADDR_64) & 0xFFFFFFFF;

	v = (bus_64 >> (L_WRB_CTXT_W0_BADDR_64 + 32)) & M_WRB_CTXT_W2_BADDR_64;
	data[2] = (V_WRB_CTXT_W2_BADDR_64(v)) |
		  (V_WRB_CTXT_W2_DESC_SIZE(descq->conf.cmpl_desc_sz));

	data[3] = (1 << S_WRB_CTXT_W3_F_VALID);

	pr_debug("%s, WRB 0x%08x, 0x%08x, 0x%08x, 0x%08x.\n",
		descq->conf.name, data[3], data[2], data[1], data[0]);

	return 0;
}

#ifndef __QDMA_VF__
/* QID2VEC Interrupt Context */
static int make_qid2vec_context(struct xlnx_dma_dev *xdev, struct qdma_descq *descq,  u32 *data, int cnt)
{
	u32 mask;
	u32 shift;
	u32 coal_shift;
	int rv = 0;
	int ring_index = 0;

	memset(data, 0, (sizeof(u32) * cnt));

	rv = hw_indirect_ctext_prog(xdev, descq->qidx_hw, QDMA_CTXT_CMD_RD,
			QDMA_CTXT_SEL_QID2VEC, data, 1, 0);
	if (rv < 0)
		return rv;

	if (descq->conf.c2h) {
		mask = C2H_QID2VEC_MAP_QID_C2H_VEC_MASK;
		shift = C2H_QID2VEC_MAP_QID_C2H_VEC_SHIFT;
		coal_shift = C2H_QID2VEC_MAP_QID_C2H_COALEN_SHIFT;
	} else {
		mask = C2H_QID2VEC_MAP_QID_H2C_VEC_MASK;
		shift = C2H_QID2VEC_MAP_QID_H2C_VEC_SHIFT;
		coal_shift = C2H_QID2VEC_MAP_QID_H2C_COALEN_SHIFT;
	}

	data[0] &= ~(mask << shift);


	/* Enable interrupt coalescing */
	if (xdev->intr_coal_en) {
		data[0] |= 1 << coal_shift;
		ring_index = get_intr_ring_index(xdev, descq->intr_id);
		data[0] |= ring_index << shift;
	}
	else
		data[0] |= (descq->intr_id) << shift;

	pr_debug("qid2vec context = 0x%08x\n", data[0]);
	return rv;
}

#endif

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
			pr_info("%s, vec %d, +%d mbox, rcv status %d.\n",
				xdev->conf.name, i, copy, hdr->status);
			return -EINVAL;
		}

		i += copy;

	} while (i < xdev->num_vecs);

	return 0;
}

int qdma_descq_context_clear(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h, bool clr)
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
	pr_debug("%s, mbox rcv ack:%d, status 0x%x.\n",
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

	pr_debug("%s, mbox send to PF, QCTXT RD 0x%x.\n",
		xdev->conf.name, qid_hw);

	rv = qdma_mbox_send_msg(xdev, &m, 1);
	if (rv < 0) {
		pr_info("%s, qid_hw 0x%x mbox failed %d.\n",
			xdev->conf.name, qid_hw, rv);
		return rv;
	}
	pr_debug("%s, mbox rcv ack:%d, status 0x%x.\n",
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
	pr_debug("%s, mbox rcv ack:%d, status 0x%x.\n",
		xdev->conf.name, hdr->ack, hdr->status);
	if (hdr->ack)
		return hdr->status;
	else
		return -EINVAL;
}

#else /* PF only */

int qdma_intr_context_setup(struct xlnx_dma_dev *xdev)
{
	u32 data[QDMA_DATA_VEC_PER_PF_MAX << 1];
	int i = 0;
	int rv;
	u32 intr_ctxt[4];
	int ring_index;

	if (!xdev->intr_coal_en)
		return 0;

	rv = make_intr_context(xdev, data, QDMA_DATA_VEC_PER_PF_MAX << 1);
	if (rv < 0)
		return rv;

	for (i = 0; i < QDMA_DATA_VEC_PER_PF_MAX; i++) {
		ring_index = get_intr_ring_index(xdev, (i + xdev->dvec_start_idx));
		/* INTR COALESCING Context Programming */
		rv = hw_indirect_ctext_prog(xdev,
									ring_index,
									QDMA_CTXT_CMD_CLR,
									QDMA_CTXT_SEL_COAL,
									NULL, 4, 0);
		if (rv < 0)
			return rv;

		rv = hw_indirect_ctext_prog(xdev, ring_index, QDMA_CTXT_CMD_WR,
					QDMA_CTXT_SEL_COAL, data + 2*i, 2, 1);
		if (rv < 0)
			return rv;

		pr_debug("intr_ctxt WR: ring_index(Qid) = %d, data[1] = %x data[0] = %x\n",
						ring_index, *(data + ((2*i) + 1)), *(data + (2*i)));

		rv = hw_indirect_ctext_prog(xdev,
									ring_index,
									QDMA_CTXT_CMD_RD,
									QDMA_CTXT_SEL_COAL,
									intr_ctxt, 4, 1);
		if (rv < 0)
			return rv;

		pr_debug("intr_ctxt RD: ring_index(Qid) = %d, data[3] = %x data[2] = %x data[1] = %x data[0] = %x\n",
				ring_index, intr_ctxt[3],  intr_ctxt[2], intr_ctxt[1], intr_ctxt[0]);
	}

	return 0;
}

int qdma_descq_context_clear(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h, bool clr)
{
	u8 sel;
	int rv = 0;

	sel = c2h ? QDMA_CTXT_SEL_SW_C2H : QDMA_CTXT_SEL_SW_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw,
			clr ? QDMA_CTXT_CMD_CLR : QDMA_CTXT_CMD_INV,
			sel, NULL, 0, 0);
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
		rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_CLR,
					QDMA_CTXT_SEL_PFTCH, NULL, 0, 0);
		if (rv < 0)
			return rv;
		rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_CLR,
					QDMA_CTXT_SEL_WRB, NULL, 0, 0);
		if (rv < 0)
			return rv;
	}

	/* TODO pasid context (0x9) */

	return 0;
}

int qdma_descq_context_setup(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	struct hw_descq_context context;
	int rv;

	rv = qdma_descq_context_clear(xdev, descq->qidx_hw, descq->conf.st,
				descq->conf.c2h, 1);
	if (rv < 0)
		return rv;

	memset(&context, 0, sizeof(context));

	make_sw_context(descq, context.sw, 4);
	make_qid2vec_context(xdev, descq,  context.qid2vec, 1);

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

	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_RD,
			QDMA_CTXT_SEL_QID2VEC, context->qid2vec, 1, 0);
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

int qdma_intr_context_read(struct xlnx_dma_dev *xdev, int ring_index, u32 *context)
{
	int rv = 0;

	memset(context, 0, (sizeof(u32) * 4));

	rv = hw_indirect_ctext_prog(xdev, ring_index, QDMA_CTXT_CMD_RD, QDMA_CTXT_SEL_COAL,
				context, 4, 0);
	if (rv < 0)
		return rv;

	return 0;
}

int qdma_descq_context_program(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *context)

{
	u8 sel;
	int rv;

	/* always clear first */
	rv = qdma_descq_context_clear(xdev, qid_hw, st, c2h, 1);
	if (rv < 0)
		return rv;

	sel = c2h ?  QDMA_CTXT_SEL_SW_C2H : QDMA_CTXT_SEL_SW_H2C;
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_WR, sel,
				context->sw, 4, 1);
	if (rv < 0)
		return rv;

	/* qid2vec context */
	rv = hw_indirect_ctext_prog(xdev, qid_hw, QDMA_CTXT_CMD_WR,
			QDMA_CTXT_SEL_QID2VEC, context->qid2vec, 1, 1);
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
