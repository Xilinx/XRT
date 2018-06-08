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

#include "qdma_regs.h"

#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/stddef.h>
#include <linux/string.h>

#include "xdev.h"

/*
 * hw_monitor_reg() - polling a register repeatly until 
 *	(the register value & mask) == val or time is up
 *
 * return -EBUSY if register value didn't match, 1 other wise
 */
int hw_monitor_reg(struct xlnx_dma_dev *xdev, unsigned int reg, u32 mask,
		u32 val, unsigned int interval_us, unsigned int timeout_us)
{
	int count;
	u32 v;

	if (!interval_us)
		interval_us = QDMA_REG_POLL_DFLT_INTERVAL_US;
	if (!timeout_us)
		timeout_us = QDMA_REG_POLL_DFLT_TIMEOUT_US;

	count = timeout_us / interval_us;

	do {
		v = __read_reg(xdev, reg);
		if ((v & mask) == val)
			return 1;
		udelay(interval_us);
	} while (--count);

	v = __read_reg(xdev, reg);
	if ((v & mask) == val)
		return 1;

	pr_debug("%s, reg 0x%x, timed out %uus, 0x%x & 0x%x != 0x%x.\n",
		xdev->conf.name, reg, timeout_us, v, mask, val);

	return -EBUSY;
}


#ifndef __QDMA_VF__
void hw_set_global_csr(struct xlnx_dma_dev *xdev)
{
	int i;
	unsigned int reg;

	__write_reg(xdev, QDMA_REG_GLBL_WB_ACC, 0x1);

	reg = QDMA_REG_GLBL_RNG_SZ_BASE;
	for (i = 0; i < QDMA_REG_GLBL_RNG_SZ_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, RNG_SZ_DFLT + 1);

	reg = QDMA_REG_C2H_BUF_SZ_BASE;
	for (i = 0; i < QDMA_REG_C2H_BUF_SZ_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, C2H_BUF_SZ_DFLT);

	reg = QDMA_REG_C2H_TIMER_CNT_BASE;
	for (i = 0; i < QDMA_REG_C2H_TIMER_CNT_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, C2H_TIMER_CNT_DFLT);

	reg = QDMA_REG_C2H_CNT_TH_BASE;
	for (i = 0; i < QDMA_REG_C2H_CNT_TH_COUNT; i++, reg += 4)
		__write_reg(xdev, reg, C2H_CNT_TH_DFLT);
}

void hw_mm_channel_enable(struct xlnx_dma_dev *xdev, int channel, bool c2h)
{
	int reg = (c2h) ?  QDMA_REG_H2C_MM_CONTROL_BASE :
			QDMA_REG_C2H_MM_CONTROL_BASE;

	__write_reg(xdev, reg + channel * QDMA_REG_MM_CONTROL_STEP,
			 QDMA_REG_MM_CONTROL_RUN);
}

void hw_mm_channel_disable(struct xlnx_dma_dev *xdev, int channel, bool c2h)
{
	int reg = (c2h) ?  QDMA_REG_H2C_MM_CONTROL_BASE :
			QDMA_REG_C2H_MM_CONTROL_BASE;

	__write_reg(xdev, reg + channel * QDMA_REG_MM_CONTROL_STEP, 0U);
}

void hw_set_fmap(struct xlnx_dma_dev *xdev, u8 func_id, unsigned int qbase,
		unsigned int qmax)
{
#if 1 /* split queues across all functions */
	__write_reg(xdev, QDMA_REG_TRQ_SEL_FMAP_BASE +
			func_id * QDMA_REG_TRQ_SEL_FMAP_STEP,
			(qbase << SEL_FMAP_QID_BASE_SHIFT) |
			(qmax << SEL_FMAP_QID_MAX_SHIFT));
#else /* share queues across all functions */
	__write_reg(xdev, QDMA_REG_TRQ_SEL_FMAP_BASE +
			func_id * QDMA_REG_TRQ_SEL_FMAP_STEP,
			2048 << SEL_FMAP_QID_MAX_SHIFT);
#endif
}

int hw_indirect_ctext_prog(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				enum ind_ctxt_cmd_op op,
				enum ind_ctxt_cmd_sel sel, u32 *data,
				unsigned int cnt, bool verify)
{
	unsigned int reg;
	u32 rd[4] = {0, 0, 0, 0};
	u32 v;
	int i;
	int rv = 0;

	pr_debug("qid_hw 0x%x, op 0x%x, sel 0x%x, data 0x%p,%u, verify %d.\n",
		qid_hw, op, sel, data, cnt, verify);


	if ((op == QDMA_CTXT_CMD_WR) || (op == QDMA_CTXT_CMD_RD)) {
		if (unlikely(!cnt || cnt > QDMA_REG_IND_CTXT_REG_COUNT)) {
			pr_warn("Q 0x%x, op 0x%x, sel 0x%x, cnt %u/%d.\n",
				qid_hw, op, sel, cnt,
				QDMA_REG_IND_CTXT_REG_COUNT);	
			return -EINVAL;
		}

		if (unlikely(!data)) {
			pr_warn("Q 0x%x, op 0x%x, sel 0x%x, data NULL.\n",
				qid_hw, op, sel);
			return -EINVAL;
		}

		reg = QDMA_REG_IND_CTXT_MASK_BASE;
		for (i = 0; i < QDMA_REG_IND_CTXT_REG_COUNT; i++, reg += 4)
			__write_reg(xdev, reg, 0xFFFFFFFF);

		if (op == QDMA_CTXT_CMD_WR) {
			reg = QDMA_REG_IND_CTXT_DATA_BASE;
			for (i = 0; i < cnt; i++, reg += 4)
				__write_reg(xdev, reg, data[i]);
			for (; i < QDMA_REG_IND_CTXT_REG_COUNT; i++, reg += 4)
				__write_reg(xdev, reg, 0);
		}
	}

	v = (qid_hw << IND_CTXT_CMD_QID_SHIFT) | (op << IND_CTXT_CMD_OP_SHIFT) |
		(sel << IND_CTXT_CMD_SEL_SHIFT);

	pr_debug("ctxt_cmd reg 0x%x, qid 0x%x, op 0x%x, sel 0x%x -> 0x%08x.\n",
		 QDMA_REG_IND_CTXT_CMD, qid_hw, op, sel, v);

	__write_reg(xdev, QDMA_REG_IND_CTXT_CMD, v);

	rv = hw_monitor_reg(xdev, QDMA_REG_IND_CTXT_CMD,
			IND_CTXT_CMD_BUSY_MASK, 0, 100, 500*1000);
	if (rv < 0) {
		pr_info("%s, Q 0x%x, op 0x%x, sel 0x%x, timeout.\n",
			xdev->conf.name, qid_hw, op, sel);
		return -EBUSY;
	}

	if (op == QDMA_CTXT_CMD_RD) {
		reg = QDMA_REG_IND_CTXT_DATA_BASE;
		for (i = 0; i < cnt; i++, reg += 4)
			data[i] = __read_reg(xdev, reg);

		return 0;
	}

	if (!verify)
		return 0;

	v = (qid_hw << IND_CTXT_CMD_QID_SHIFT) |
		(QDMA_CTXT_CMD_RD << IND_CTXT_CMD_OP_SHIFT) |
		(sel << IND_CTXT_CMD_SEL_SHIFT);

	pr_debug("reg 0x%x, Q 0x%x, RD, sel 0x%x -> 0x%08x.\n",
		QDMA_REG_IND_CTXT_CMD, qid_hw, sel, v);

	__write_reg(xdev, QDMA_REG_IND_CTXT_CMD, v);

	rv = hw_monitor_reg(xdev, QDMA_REG_IND_CTXT_CMD,
			IND_CTXT_CMD_BUSY_MASK, 0, 100, 500*1000);
	if (rv < 0) {
		pr_warn("%s, Q 0x%x, op 0x%x, sel 0x%x, readback busy.\n",
			xdev->conf.name, qid_hw, op, sel);
		return rv;
	}

	reg = QDMA_REG_IND_CTXT_DATA_BASE;
	for (i = 0; i < cnt; i++, reg += 4)
		rd[i] = __read_reg(xdev, reg);

	v = 4 * cnt;
	if (memcmp(data, rd, v)) {
		pr_warn("%s, indirect write data mismatch:\n", xdev->conf.name);
		print_hex_dump(KERN_INFO, "WR ", DUMP_PREFIX_OFFSET,
			16, 1, (void *)data, v, false);
		print_hex_dump(KERN_INFO, "RD ", DUMP_PREFIX_OFFSET,
			16, 1, (void *)rd, v, false);

		return -EINVAL;
	}

	return 0;
}

void hw_prog_qid2vec(struct xlnx_dma_dev *xdev, unsigned int qid_hw, bool c2h,
			unsigned int intr_id, bool intr_coal_en)
{
	u32 mask;
	u32 shift;
	u32 coal_shift;
	u32 v;

	if (c2h) {
		mask = C2H_QID2VEC_MAP_QID_C2H_VEC_MASK;
		shift = C2H_QID2VEC_MAP_QID_C2H_VEC_SHIFT;
		coal_shift = C2H_QID2VEC_MAP_QID_C2H_COALEN_SHIFT;
	} else {
		mask = C2H_QID2VEC_MAP_QID_H2C_VEC_MASK;
		shift = C2H_QID2VEC_MAP_QID_H2C_VEC_SHIFT;
		coal_shift = C2H_QID2VEC_MAP_QID_H2C_COALEN_SHIFT;
	}

	pr_info("reg 0x%x, qid 0x%x, c2h %d.\n",
		QDMA_REG_C2H_QID2VEC_MAP_QID, qid_hw, c2h);

	__write_reg(xdev, QDMA_REG_C2H_QID2VEC_MAP_QID, qid_hw);

	v = __read_reg(xdev, QDMA_REG_C2H_QID2VEC_MAP);
	v &= ~(mask << shift);
	v |= intr_id << shift;

	/* Enable interrupt coalescing */
	if (intr_coal_en)
		v |= 1 << coal_shift;

	pr_info("reg 0x%x -> 0x%08x  intr_id = 0x%x, intr_coal_en %d.\n",
		QDMA_REG_C2H_QID2VEC_MAP, v, intr_id, intr_coal_en);

	__write_reg(xdev, QDMA_REG_C2H_QID2VEC_MAP, v);
}
#endif
