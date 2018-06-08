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

#ifndef __QDMA_REGS_H__
#define __QDMA_REGS_H__

#include <linux/types.h>

/* polling a register */
#define	QDMA_REG_POLL_DFLT_INTERVAL_US	100		/* 100us per poll */
#define	QDMA_REG_POLL_DFLT_TIMEOUT_US	500*1000	/* 500ms */

/* desc. Q default */
#define	RNG_SZ_DFLT			256
#define WRB_RNG_SZ_DFLT			256
#define	C2H_TIMER_CNT_DFLT	0x1
#define	C2H_CNT_TH_DFLT	0x1
#define	C2H_BUF_SZ_DFLT	PAGE_SIZE

#ifndef __QDMA_VF__
/*
 * PF only registers
 */

/* Driver visible Attribute Space 0x100 */
#define QDMA_REG_FUNC_ID			0x12C

/* CSR space 0x200 */
#define QDMA_REG_GLBL_RNG_SZ_BASE		0x204
#define		QDMA_REG_GLBL_RNG_SZ_COUNT	16

#define QDMA_REG_GLBL_SCRATCH			0x244
#define QDMA_REG_GLBL_ERR_STAT			0x248
#define QDMA_REG_GLBL_ERR_MASK			0x24C

#define QDMA_REG_GLBL_WB_ACC			0x250

#define QDMA_REG_C2H_TIMER_CNT_BASE		0xA00
#define		QDMA_REG_C2H_TIMER_CNT_COUNT	16

#define QDMA_REG_C2H_CNT_TH_BASE		0xA40
#define		QDMA_REG_C2H_CNT_TH_COUNT	16

#define QDMA_REG_C2H_BUF_SZ_BASE		0xAB0
#define		QDMA_REG_C2H_BUF_SZ_COUNT	16

/*
 * FUnction registers
 */

#define QDMA_REG_TRQ_SEL_FMAP_BASE			0x400
#define 	QDMA_REG_TRQ_SEL_FMAP_STEP		0x4
#define		QDMA_REG_TRQ_SEL_FMAP_COUNT		256

#define		SEL_FMAP_QID_BASE_SHIFT			0
#define		SEL_FMAP_QID_BASE_MASK			0x7FFU
#define		SEL_FMAP_QID_MAX_SHIFT			11
#define		SEL_FMAP_QID_MAX_MASK			0xFFFU

#define QDMA_REG_C2H_QID2VEC_MAP_QID			0xa80
#define		C2H_QID2VEC_MAP_QID_C2H_VEC_SHIFT	0
#define		C2H_QID2VEC_MAP_QID_C2H_VEC_MASK	0xFFU
#define		C2H_QID2VEC_MAP_QID_C2H_COALEN_SHIFT	8
#define		C2H_QID2VEC_MAP_QID_C2H_COALEN_MASK	0x1U
#define		C2H_QID2VEC_MAP_QID_H2C_VEC_SHIFT	9
#define		C2H_QID2VEC_MAP_QID_H2C_VEC_MASK	0xFFU
#define		C2H_QID2VEC_MAP_QID_H2C_COALEN_SHIFT	18
#define		C2H_QID2VEC_MAP_QID_H2C_COALEN_MASK	0x1U

#define QDMA_REG_C2H_QID2VEC_MAP			0xa84

/*
 * Indirect Programming
 */

#define	QDMA_REG_IND_CTXT_REG_COUNT		4
#define QDMA_REG_IND_CTXT_DATA_BASE		0x804
#define QDMA_REG_IND_CTXT_MASK_BASE		0x814

#define QDMA_REG_IND_CTXT_CMD			0x824
#define		IND_CTXT_CMD_QID_SHIFT		7
#define		IND_CTXT_CMD_QID_MASK		0x7FFU
#define		IND_CTXT_CMD_OP_SHIFT		5
#define		IND_CTXT_CMD_OP_MASK		0x3U

#define		IND_CTXT_CMD_SEL_SHIFT		1
#define		IND_CTXT_CMD_SEL_MASK		0xFU

#define		IND_CTXT_CMD_BUSY_SHIFT		1
#define		IND_CTXT_CMD_BUSY_MASK		0x1U

/*
 * Queue registers
 */
#define 	QDMA_REG_MM_CONTROL_RUN		0x1U
#define 	QDMA_REG_MM_CONTROL_STEP	0x100

#define QDMA_REG_C2H_MM_CONTROL_BASE		0x1004
#define QDMA_REG_H2C_MM_CONTROL_BASE		0x1204


/*
 * monitor
 */
#define QDMA_REG_C2H_STAT_AXIS_PKG_CMP		0xA94

#endif /* ifndef __QDMA_VF__ */

/* 
 * desc. Q pdix/cidx update
 */

#define 	QDMA_REG_PIDX_STEP		0x10
#define 	QDMA_REG_PIDX_COUNT		0x2048

#ifdef __QDMA_VF__ 

#define QDMA_REG_INT_CIDX_BASE                  0x3000
#define QDMA_REG_H2C_PIDX_BASE                  0x3004
#define QDMA_REG_C2H_PIDX_BASE                  0x3008
#define QDMA_REG_WRB_CIDX_BASE                  0x300C

#else

#define QDMA_REG_INT_CIDX_BASE			0x6400
#define QDMA_REG_H2C_PIDX_BASE			0x6404
#define QDMA_REG_C2H_PIDX_BASE			0x6408
#define QDMA_REG_WRB_CIDX_BASE			0x640C

#endif

/*
 * Q Context programming (indirect)
 */
enum ind_ctxt_cmd_op {
	QDMA_CTXT_CMD_CLR,
	QDMA_CTXT_CMD_WR,
	QDMA_CTXT_CMD_RD,
	QDMA_CTXT_CMD_INV
};

enum ind_ctxt_cmd_sel {
	QDMA_CTXT_SEL_SW_C2H,
	QDMA_CTXT_SEL_SW_H2C,
	QDMA_CTXT_SEL_HW_C2H,
	QDMA_CTXT_SEL_HW_H2C,
	QDMA_CTXT_SEL_CR_C2H,
	QDMA_CTXT_SEL_CR_H2C,
	QDMA_CTXT_SEL_WRB,
	QDMA_CTXT_SEL_PFTCH,
	QDMA_CTXT_SEL_COAL
};

/* Q Context: SW Descriptor */
enum tigger_mode {
	TRIG_MODE_DISABLE,	/* 0 */
	TRIG_MODE_ANY,		/* 1 */
	TRIG_MODE_TIMER,	/* 2 */
	TRIG_MODE_COUNTER,	/* 3 */
	TRIG_MODE_COMBO,	/* 4 */
	TRIG_MODE_USER,		/* 5 */
};

#define S_DESC_CTXT_W1_F_QEN		0
#define S_DESC_CTXT_W1_F_FCRD_EN	1
#define S_DESC_CTXT_W1_F_WBI_CHK	2
#define S_DESC_CTXT_W1_F_WB_ACC_EN	3

#define S_DESC_CTXT_W1_FUNC_ID		4
#define M_DESC_CTXT_W1_FUNC_ID		0xFFU
#define V_DESC_CTXT_W1_FUNC_ID(x)	((x) << S_DESC_CTXT_W1_FUNC_ID)

#define S_DESC_CTXT_W1_RNG_SZ		12
#define M_DESC_CTXT_W1_RNG_SZ		0xFU
#define V_DESC_CTXT_W1_RNG_SZ(x)	((x) << S_DESC_CTXT_W1_RNG_SZ)

#define S_DESC_CTXT_W1_DSC_SZ		16
#define M_DESC_CTXT_W1_DSC_SZ		0x3U
#define V_DESC_CTXT_W1_DSC_SZ(x)	((x) << S_DESC_CTXT_W1_DSC_SZ)

enum ctxt_desc_sz_sel {
	DESC_SZ_8B = 0,
	DESC_SZ_16B,
	DESC_SZ_32B,
	DESC_SZ_RSV
};

#define S_DESC_CTXT_W1_F_BYP		18
#define S_DESC_CTXT_W1_F_MM_CHN		19
#define S_DESC_CTXT_W1_F_WBK_EN		20
#define S_DESC_CTXT_W1_F_IRQ_EN		21

#define S_DESC_CTXT_W1_F_IRQ_PND	24
#define S_DESC_CTXT_W1_F_IRQ_NO_LAST	25

#define S_DESC_CTXT_W1_ERR		26
#define M_DESC_CTXT_W1_ERR		0x1FU
#define V_DESC_CTXT_W1_ERR(x)		((x) << S_DESC_CTXT_W1_ERR)

/* Context: C2H Writeback */
#define WRB_RING_SIZE_MAX		(1 << 16 - 1)

#define S_WRB_CTXT_W0_F_EN_STAT_DESC	0
#define S_WRB_CTXT_W0_F_EN_INT		1

#define S_WRB_CTXT_W0_TRIG_MODE		2
#define M_WRB_CTXT_W0_TRIG_MODE		0x7U
#define V_WRB_CTXT_W0_TRIG_MODE(x)	((x) << S_WRB_CTXT_W0_TRIG_MODE)

#define S_WRB_CTXT_W0_FNC_ID		5
#define M_WRB_CTXT_W0_FNC_ID		0xFFU
#define V_WRB_CTXT_W0_FNC_ID(x)		((x & M_WRB_CTXT_W0_FNC_ID) << S_WRB_CTXT_W0_FNC_ID)

#define S_WRB_CTXT_W0_COUNTER_IDX	13
#define M_WRB_CTXT_W0_COUNTER_IDX	0xFU
#define V_WRB_CTXT_W0_COUNTER_IDX(x)	((x) << S_WRB_CTXT_W0_COUNTER_IDX)

#define S_WRB_CTXT_W0_TIMER_IDX		17
#define M_WRB_CTXT_W0_TIMER_IDX		0xFU
#define V_WRB_CTXT_W0_TIMER_IDX(x)	((x) << S_WRB_CTXT_W0_TIMER_IDX)

#define S_WRB_CTXT_W0_INT_ST		21
#define M_WRB_CTXT_W0_INT_ST		0x3U
#define V_WRB_CTXT_W0_INT_ST(x)		((x) << S_WRB_CTXT_W0_INT_ST)

#define S_WRB_CTXT_W0_F_COLOR		23

#define M_WRB_CTXT_SIZE_64_ALIGN	0x3F

#define S_WRB_CTXT_W0_RNG_SZ		24
#define M_WRB_CTXT_W0_RNG_SZ		0xFU
#define L_WRB_CTXT_W0_RNG_SZ		4
#define V_WRB_CTXT_W0_RNG_SZ(x)		((x) << S_WRB_CTXT_W0_RNG_SZ)

#define M_WRB_CTXT_BADDR_64_ALIGN	0x3F

#define S_WRB_CTXT_W0_BADDR_64		28
#define M_WRB_CTXT_W0_BADDR_64		0xFU
#define L_WRB_CTXT_W0_BADDR_64		4
#define V_WRB_CTXT_W0_BADDR_64(x)	\
	(((x) & M_WRB_CTXT_W0_BADDR_64) << S_WRB_CTXT_W0_BADDR_64)

#define S_WRB_CTXT_W2_BADDR_64		0
#define M_WRB_CTXT_W2_BADDR_64		0x3FFFFFU
#define L_WRB_CTXT_W2_BADDR_64		22
#define V_WRB_CTXT_W2_BADDR_64(x)	\
	(((x) & M_WRB_CTXT_W2_BADDR_64) << S_WRB_CTXT_W2_BADDR_64)

#define S_WRB_CTXT_W2_DESC_SIZE		22
#define M_WRB_CTXT_W2_DESC_SIZE		0x3U
#define V_WRB_CTXT_W2_DESC_SIZE(x)	((x) << S_WRB_CTXT_W2_DESC_SIZE)

#define S_WRB_CTXT_W2_PIDX_L		24
#define M_WRB_CTXT_W2_PIDX_L		0xFFU
#define L_WRB_CTXT_W2_PIDX_L		8
#define V_WRB_CTXT_W2_PIDX_L(x)		((x) << S_WRB_CTXT_W2_PIDX_L)

#define S_WRB_CTXT_W3_PIDX_H		0
#define M_WRB_CTXT_W3_PIDX_H		0xFFU
#define L_WRB_CTXT_W3_PIDX_H		8
#define V_WRB_CTXT_W3_PIDX_H(x)		((x) << S_WRB_CTXT_W3_PIDX_H)

#define S_WRB_CTXT_W3_CIDX		8
#define M_WRB_CTXT_W3_CIDX		0xFFFFU
#define L_WRB_CTXT_W3_CIDX		16
#define V_WRB_CTXT_W3_CIDX(x)		((x) << S_WRB_CTXT_W3_CIDX)

#define S_WRB_CTXT_W3_F_VALID		24


/* Context: C2H Prefetch */
#define S_PFTCH_W0_F_BYPASS		0

#define S_PFTCH_W0_BUF_SIZE_IDX		1
#define M_PFTCH_W0_BUF_SIZE_IDX		0xFU
#define V_PFTCH_W0_BUF_SIZE_IDX(x)	((x) << S_PFTCH_W0_BUF_SIZE_IDX)

#define S_PFTCH_W0_FNC_ID		5
#define M_PFTCH_W0_FNC_ID		0xFFU
#define V_PFTCH_W0_FNC_ID(x)		((x & M_PFTCH_W0_FNC_ID) << S_PFTCH_W0_FNC_ID)

#define S_PFTCH_W0_PORT_ID		13
#define M_PFTCH_W0_PORT_ID		0x7U
#define V_PFTCH_W0_PORT_ID(x)		((x) << S_PFTCH_W0_PORT_ID)

#define S_PFTCH_W0_F_EN_PFTCH		27
#define S_PFTCH_W0_F_Q_IN_PFTCH		28

#define S_PFTCH_W0_SW_CRDT_L		29
#define M_PFTCH_W0_SW_CRDT_L		0x7U
#define L_PFTCH_W0_SW_CRDT_L		3
#define V_PFTCH_W0_SW_CRDT_L(x)		((x) << S_PFTCH_W0_SW_CRDT_L)

#define S_PFTCH_W1_SW_CRDT_H		0
#define M_PFTCH_W1_SW_CRDT_H		0x1FFFU
#define L_PFTCH_W1_SW_CRDT_H		13
#define V_PFTCH_W1_SW_CRDT_H(x)		((x) << S_PFTCH_W1_SW_CRDT_H)

#define S_PFTCH_W1_F_VALID		13


/* Context: Interrupt Coalescing */
#define S_INT_COAL_W0_F_VALID		0

#define S_INT_COAL_W0_VEC_ID		1
#define M_INT_COAL_W0_VEC_ID		0x3FU
#define V_INT_COAL_W0_VEC_ID(x)		((x) << S_INT_COAL_W0_VEC_ID)

#define S_INT_COAL_W0_F_INT_ST		7
#define S_INT_COAL_W0_F_COLOR		8

#define S_INT_COAL_W0_BADDR_64		9
#define M_INT_COAL_W0_BADDR_64		0x7FFFFFU
#define L_INT_COAL_W0_BADDR_64		23
#define V_INT_COAL_W0_BADDR_64(x)	\
	(((x) & M_INT_COAL_W0_BADDR_64) << S_INT_COAL_W0_BADDR_64)

#define S_INT_COAL_W1_BADDR_64		0
#define M_INT_COAL_W1_BADDR_64		0x1FFFFFFFU
#define L_INT_COAL_W1_BADDR_64		29
#define V_INT_COAL_W1_BADDR_64(x)	\
	(((x) & M_INT_COAL_W1_BADDR_64) << S_INT_COAL_W1_BADDR_64)

#define S_INT_COAL_W1_VEC_SIZE		1
#define M_INT_COAL_W1_VEC_SIZE		0x3FU
#define V_INT_COAL_W1_VEC_SIZE(x)	((x) << S_INT_COAL_W1_VEC_SIZE)

#define S_INT_COAL_W2_PIDX		0
#define M_INT_COAL_W2_PIDX		0xFFFU
#define V_INT_COAL_W2_PIDX		((x) << S_INT_COAL_W2_PIDX)


/*
 * PIDX/CIDX update
 */

#define S_INTR_CIDX_UPD_SW_CIDX		0
#define M_INTR_CIDX_UPD_SW_CIDX		0xFFFFU
#define V_INTR_CIDX_UPD_SW_CIDX(x)	((x) << S_INTR_CIDX_UPD_SW_CIDX)

#define S_INTR_CIDX_UPD_DIR_SEL	    16


#define S_WRB_PIDX_UPD_EN_INT		16

/*
 * WRB CIDX update
 */
#define S_WRB_CIDX_UPD_SW_CIDX		0
#define M_WRB_CIDX_UPD_SW_IDX		0xFFFFU
#define V_WRB_CIDX_UPD_SW_IDX(x)	((x) << S_WRB_CIDX_UPD_SW_CIDX)

#define S_WRB_CIDX_UPD_CNTER_IDX	16
#define M_WRB_CIDX_UPD_CNTER_IDX	0xFU
#define V_WRB_CIDX_UPD_CNTER_IDX(x)	((x) << S_WRB_CIDX_UPD_CNTER_IDX)

#define S_WRB_CIDX_UPD_TIMER_IDX	20
#define M_WRB_CIDX_UPD_TIMER_IDX	0xFU
#define V_WRB_CIDX_UPD_TIMER_IDX(x)	((x) << S_WRB_CIDX_UPD_TIMER_IDX)

#define S_WRB_CIDX_UPD_TRIG_MODE	24
#define M_WRB_CIDX_UPD_TRIG_MODE	0x7U
#define V_WRB_CIDX_UPD_TRIG_MODE(x)	((x) << S_WRB_CIDX_UPD_TRIG_MODE)

#define S_WRB_CIDX_UPD_EN_STAT_DESC	27
#define S_WRB_CIDX_UPD_EN_INT		28

/*
 * descriptor & writeback status
 */

struct qdma_mm_desc {
	__be64 src_addr;
	__be32 flag_len;
	__be32 rsvd0;
	__be64 dst_addr;
	__be64 rsvd1;
};
#define S_DESC_F_DV		28
#define S_DESC_F_SOP		29
#define S_DESC_F_EOP		30

struct qdma_h2c_desc {
	__be64 src_addr;
	__be32 flag_len;
	__be32 rsvd;
};

struct qdma_c2h_desc {
	__be64 dst_addr;
};

struct qdma_desc_wb {
	__be16 pidx;
	__be16 cidx;
	__be32 rsvd;
};

#define S_C2H_WB_ENTRY_F_DATA_FORMAT	0
#define S_C2H_WB_ENTRY_F_COLOR		1
#define S_C2H_WB_ENTRY_F_DESC_ERR	2
#define S_C2H_WB_ENTRY_F_RSVD		3

#define S_C2H_WB_ENTRY_LENGTH		4
#define M_C2H_WB_ENTRY_LENGTH		0xFFFFU
#define L_C2H_WB_ENTRY_LENGTH		16
#define V_C2H_WB_ENTRY_LENGTH(x)	(((x) & M_C2H_WB_ENTRY_LENGTH) << S_C2H_WB_ENTRY_LENGTH)

#define S_C2H_WB_USER_DEFINED		20
#define V_C2H_WB_USER_DEFINED(x)	((x) << S_C2H_WB_USER_DEFINED)

#define M_C2H_WB_ENTRY_DMA_INFO     0xFFFFFF
#define L_C2H_WB_ENTRY_DMA_INFO     3 /* 20 bits */

struct qdma_c2h_wrb_wb {
	__be16 pidx;
	__be16 cidx;
	__be32 color_isr_status;
};
#define S_C2H_WB_F_COLOR	0

#define S_C2H_WB_INT_STATE	1
#define M_C2H_WB_INT_STATE	0x3U

/*
 * HW API
 */

#include "xdev.h"

#define __read_reg(xdev, reg_addr) (readl(xdev->regs + reg_addr))
#ifdef DEBUG__
#define __write_reg(xdev,reg_addr, val) \
	do { \
		pr_debug("%s, reg 0x%x, val 0x%x.\n", \
			xdev->conf.name, reg_addr, (u32)val); \
		writel(val, xdev->regs + reg_addr); \
	} while(0)
#else
#define __write_reg(xdev,reg_addr, val) (writel(val, xdev->regs + reg_addr))
#endif /* #ifdef DEBUG__ */


int hw_monitor_reg(struct xlnx_dma_dev *xdev, unsigned int reg, u32 mask,
		u32 val, unsigned int interval_us, unsigned int timeout_us);

#ifndef __QDMA_VF__
void hw_mm_channel_enable(struct xlnx_dma_dev *xdev, int channel, bool c2h);
void hw_mm_channel_disable(struct xlnx_dma_dev *xdev, int channel, bool c2h);
void hw_set_global_csr(struct xlnx_dma_dev *xdev);
void hw_set_fmap(struct xlnx_dma_dev *xdev, u8 func_id, unsigned int qbase,
			unsigned int qmax);
int hw_indirect_ctext_prog(struct xlnx_dma_dev *xdev, unsigned int qid,
				enum ind_ctxt_cmd_op op,
				enum ind_ctxt_cmd_sel sel, u32 *data,
				unsigned int cnt, bool verify);
void hw_prog_qid2vec(struct xlnx_dma_dev *xdev, unsigned int qid_hw, bool c2h,
                        unsigned int intr_id, bool intr_coal_en);

#endif /* #ifndef __QDMA_VF__ */

#endif /* ifndef __QDMA_REGS_H__ */
