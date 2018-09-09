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
		struct intr_coal_conf *entry = (xdev->intr_coal_list + i);

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

/* STM */
#ifndef __QDMA_VF__
static int make_stm_c2h_context(struct qdma_descq *descq, u32 *data)
{
	int pipe_slr_id = descq->conf.pipe_slr_id;
	int pipe_flow_id = descq->conf.pipe_flow_id;
	int pipe_tdest = descq->conf.pipe_tdest;

	/* 128..159 */
	data[1] = (pipe_slr_id << S_STM_CTXT_C2H_SLR) |
		  ((pipe_tdest & 0xFF00) << S_STM_CTXT_C2H_TDEST_H);

	/* 96..127 */
	data[0] = ((pipe_tdest & 0xFF) << S_STM_CTXT_C2H_TDEST_L) |
		  (pipe_flow_id << S_STM_CTXT_C2H_FID);

	pr_debug("%s, STM 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x.\n",
		 descq->conf.name, data[0], data[1], data[2], data[3], data[4]);
	return 0;
}

static int make_stm_h2c_context(struct qdma_descq *descq, u32 *data)
{
	int pipe_slr_id = descq->conf.pipe_slr_id;
	int pipe_flow_id = descq->conf.pipe_flow_id;
	int pipe_tdest = descq->conf.pipe_tdest;
	int dppkt = descq->conf.pipe_gl_max;
	int log2_dppkt = ilog2(dppkt);
	int pkt_lim = 0;
	int max_ask = 8;

	/* 128..159 */
	data[4] = (descq->qidx_hw << S_STM_CTXT_QID);

	/* 96..127 */
	data[3] = (pipe_slr_id << S_STM_CTXT_H2C_SLR) |
		  ((pipe_tdest & 0xFF00) << S_STM_CTXT_H2C_TDEST_H);

	/* 64..95 */
	data[2] = ((pipe_tdest & 0xFF) << S_STM_CTXT_H2C_TDEST_L) |
		  (pipe_flow_id << S_STM_CTXT_H2C_FID) |
		  (pkt_lim << S_STM_CTXT_PKT_LIM) |
		  (max_ask << S_STM_CTXT_MAX_ASK);

	/* 32..63 */
	data[1] = (dppkt << S_STM_CTXT_DPPKT) |
		  (log2_dppkt << S_STM_CTXT_LOG2_DPPKT);

	/* 0..31 */
	data[0] = 0;

	pr_debug("%s, STM 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x.\n",
		 descq->conf.name, data[0], data[1], data[2], data[3], data[4]);
	return 0;
}
#endif

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
		data[1] |=
			(descq->conf.fetch_credit << S_DESC_CTXT_W1_F_FCRD_EN) |
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
	data[0] = (descq->conf.bypass << S_PFTCH_W0_F_BYPASS) |
		  (descq->conf.c2h_buf_sz_idx << S_PFTCH_W0_BUF_SIZE_IDX) |
			  /* TODO port id*/
		  (descq->conf.pfetch_en << S_PFTCH_W0_F_EN_PFTCH);

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

/* QID2VEC Interrupt Context */
static int make_qid2vec_context(struct xlnx_dma_dev *xdev,
	struct qdma_descq *descq,  u32 *data, int cnt)
{
	u32 mask;
	u32 shift;
	u32 coal_shift;
	int rv = 0;
	int ring_index = 0;

#ifndef __QDMA_VF__
	memset(data, 0, (sizeof(u32) * cnt));
	rv = hw_indirect_ctext_prog(xdev, descq->qidx_hw, QDMA_CTXT_CMD_RD,
			QDMA_CTXT_SEL_QID2VEC, data, 1, 0);
	if (rv < 0)
		return rv;
#endif
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
	} else
		data[0] |= (descq->intr_id) << shift;

	pr_debug("qid2vec context = 0x%08x\n", data[0]);
	return rv;
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
		struct mbox_msg *m = qdma_mbox_msg_alloc(xdev,
						 MBOX_OP_INTR_CTXT);
		struct mbox_msg_hdr *hdr = m ? &m->hdr : NULL;
		struct mbox_msg_intr_ctxt *ictxt = m ? &m->intr_ctxt : NULL;
		u8 copy = xdev->num_vecs  - i;

		if (!m)
			return -ENOMEM;

		if (copy > MBOX_INTR_CTXT_VEC_MAX)
			copy = MBOX_INTR_CTXT_VEC_MAX;

		ictxt->clear = 1;
		ictxt->vec_base = i;
		ictxt->vec_cnt = copy;

		memcpy(ictxt->w, data + 2 * i, copy * sizeof(u32));

		rv = qdma_mbox_msg_send(xdev, m, 1, MBOX_OP_INTR_CTXT_RESP,
					QDMA_MBOX_MSG_TIMEOUT_MS);
		if (rv < 0) {
			if (rv != -ENODEV)
				pr_info("%s, vec %d, +%d mbox failed %d.\n",
					xdev->conf.name, i, copy, rv);
			goto free_msg;
		}

		rv = hdr->status;
free_msg:
		qdma_mbox_msg_free(m);
		if (rv < 0)
			return rv;

		i += copy;

	} while (i < xdev->num_vecs);

	return 0;
}

int qdma_descq_context_clear(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h, bool clr)
{
	struct mbox_msg *m = qdma_mbox_msg_alloc(xdev, MBOX_OP_QCTXT_CLR);
	struct mbox_msg_hdr *hdr = m ? &m->hdr : NULL;
	struct mbox_msg_qctxt *qctxt = m ? &m->qctxt : NULL ;
	int rv;

	if (!m)
		return -ENOMEM;

	qctxt->qid = qid_hw;
	qctxt->st = st;
	qctxt->c2h = c2h;

	rv = qdma_mbox_msg_send(xdev, m, 1, MBOX_OP_QCTXT_CLR_RESP,
				QDMA_MBOX_MSG_TIMEOUT_MS);
	if (rv < 0) {
		if (rv != -ENODEV)
			pr_info("%s, qid_hw 0x%x mbox failed %d.\n",
				xdev->conf.name, qid_hw, rv);
		goto err_out;
	}

	rv = hdr->status;

err_out:
	qdma_mbox_msg_free(m);
	return rv;
}

int qdma_descq_context_read(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *context)
{
	struct mbox_msg *m = qdma_mbox_msg_alloc(xdev, MBOX_OP_QCTXT_RD);
	struct mbox_msg_hdr *hdr = m ? &m->hdr : NULL;
	struct mbox_msg_qctxt *qctxt = m ? &m->qctxt : NULL;
	int rv;

	if (!m)
		return -ENOMEM;

	qctxt->qid = qid_hw;
	qctxt->st = st;
	qctxt->c2h = c2h;

	rv = qdma_mbox_msg_send(xdev, m, 1, MBOX_OP_QCTXT_RD_RESP,
				QDMA_MBOX_MSG_TIMEOUT_MS);
	if (rv < 0) {
		if (rv != -ENODEV)
			pr_info("%s, qid_hw 0x%x mbox failed %d.\n",
				xdev->conf.name, qid_hw, rv);
		goto err_out;
	}

	if (hdr->status) {
		rv = hdr->status;
		goto err_out;
	}

	memcpy(context, &qctxt->context, sizeof(struct hw_descq_context));

	return 0;

err_out:
	qdma_mbox_msg_free(m);
	return rv;
}

int qdma_descq_context_setup(struct qdma_descq *descq)
{
	struct xlnx_dma_dev *xdev = descq->xdev;
	struct mbox_msg *m = qdma_mbox_msg_alloc(xdev, MBOX_OP_QCTXT_WRT);
	struct mbox_msg_hdr *hdr = m ? &m->hdr : NULL;
	struct mbox_msg_qctxt *qctxt = m ? &m->qctxt : NULL;
	struct hw_descq_context *context = m ? &qctxt->context : NULL;
	int rv;

	if (!m)
		return -ENOMEM;
	
		
	rv = qdma_descq_context_read(xdev, descq->qidx_hw,
				descq->conf.st, descq->conf.c2h,
				context);
	if (rv < 0) {
		pr_info("%s, qid_hw 0x%x, %s mbox failed %d.\n",
			xdev->conf.name, descq->qidx_hw, descq->conf.name, rv);
		goto err_out;
	}
	
	make_sw_context(descq, context->sw, 4);
	make_qid2vec_context(xdev, descq,  context->qid2vec, 1);
	if (descq->conf.st && descq->conf.c2h) {
		make_prefetch_context(descq, context->prefetch, 2);
		make_wrb_context(descq, context->wrb, 4);
	}
	
	qctxt->clear = 1;
	qctxt->verify = 1;
	qctxt->st = descq->conf.st;
	qctxt->c2h = descq->conf.c2h;
	qctxt->qid = descq->qidx_hw;

	rv = qdma_mbox_msg_send(xdev, m, 1, MBOX_OP_QCTXT_WRT_RESP,
				QDMA_MBOX_MSG_TIMEOUT_MS);
	if (rv < 0) {
		if (rv != -ENODEV)
			pr_info("%s, qid_hw 0x%x, %s mbox failed %d.\n",
				xdev->conf.name, descq->qidx_hw,
				descq->conf.name, rv);
		goto err_out;
	}

	rv = hdr->status;

err_out:
	qdma_mbox_msg_free(m);
	return rv;
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
		ring_index = get_intr_ring_index(xdev,
				(i + xdev->dvec_start_idx));
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
				ring_index,
				*(data + ((2*i) + 1)),
				*(data + (2*i)));

		rv = hw_indirect_ctext_prog(xdev,
					ring_index,
					QDMA_CTXT_CMD_RD,
					QDMA_CTXT_SEL_COAL,
					intr_ctxt, 4, 1);
		if (rv < 0)
			return rv;

		pr_debug("intr_ctxt RD: ring_index(Qid) = %d, data[3] = %x data[2] = %x data[1] = %x data[0] = %x\n",
				ring_index, intr_ctxt[3],
				intr_ctxt[2],
				intr_ctxt[1],
				intr_ctxt[0]);
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

/* STM */
int qdma_descq_stm_setup(struct qdma_descq *descq)
{
	struct stm_descq_context context;

	memset(&context, 0, sizeof(context));
	if (descq->conf.c2h)
		make_stm_c2h_context(descq, context.stm);
	else
		make_stm_h2c_context(descq, context.stm);

	return qdma_descq_stm_program(descq->xdev, descq->qidx_hw,
				      descq->conf.pipe_flow_id,
				      descq->conf.c2h, false,
				      &context);
}

int qdma_descq_stm_clear(struct qdma_descq *descq)
{
	struct stm_descq_context context;

	memset(&context, 0, sizeof(context));
	return qdma_descq_stm_program(descq->xdev, descq->qidx_hw,
				      descq->conf.pipe_flow_id,
				      descq->conf.c2h, true,
				      &context);
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

int qdma_intr_context_read(struct xlnx_dma_dev *xdev,
	int ring_index, u32 *context)
{
	int rv = 0;

	memset(context, 0, (sizeof(u32) * 4));

	rv = hw_indirect_ctext_prog(xdev, ring_index,
				QDMA_CTXT_CMD_RD, QDMA_CTXT_SEL_COAL,
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
	pr_debug("QDMA_CTXT_SEL_QID2VEC, context->qid2vec = 0x%08x\n", context->qid2vec[0]);
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

/* STM */
int qdma_descq_stm_program(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
			   u8 pipe_flow_id, bool c2h, bool clear,
			   struct stm_descq_context *context)
{
	int rv;

	if (!c2h) {
		/* need to program stm context */
		rv = hw_indirect_stm_prog(xdev, qid_hw, pipe_flow_id,
					  STM_CSR_CMD_WR,
					  STM_IND_ADDR_Q_CTX_H2C,
					  context->stm, 5, clear);
		if (rv < 0)
			return rv;
		rv = hw_indirect_stm_prog(xdev, qid_hw, pipe_flow_id,
					  STM_CSR_CMD_WR,
					  STM_IND_ADDR_H2C_MAP,
					  context->stm, 1, clear);
		if (rv < 0)
			return rv;
	}

	/* Only c2h st specific setup done below*/
	if (!c2h)
		return 0;

	rv = hw_indirect_stm_prog(xdev, qid_hw, pipe_flow_id,
				  STM_CSR_CMD_WR,
				  STM_IND_ADDR_Q_CTX_C2H,
				  context->stm, 2, clear);
	if (rv < 0)
		return rv;

	rv = hw_indirect_stm_prog(xdev, qid_hw, pipe_flow_id,
				  STM_CSR_CMD_WR, STM_IND_ADDR_C2H_MAP,
				  context->stm, 1, clear);
	if (rv < 0)
		return rv;

	return 0;
}
#endif
