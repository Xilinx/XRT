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
#include "xdev.h"


/* polling a register */
#define	QDMA_REG_POLL_DFLT_INTERVAL_US	100		/* 100us per poll */
#define	QDMA_REG_POLL_DFLT_TIMEOUT_US	(500*1000)	/* 500ms */

/* desc. Q default */
#define	RNG_SZ_DFLT			64
#define WRB_RNG_SZ_DFLT			64
#define	C2H_TIMER_CNT_DFLT	0x1
#define	C2H_CNT_TH_DFLT	0x1
#define	C2H_BUF_SZ_DFLT	PAGE_SIZE

/* QDMA IP limits */
#define QDMA_QSET_MAX		2048	/* 2K queues */
#define QDMA_FUNC_MAX		256	/* 256 functions */
#define QDMA_INTR_RNG_MAX	256	/* 256 interrupt aggregation rings */

/* Driver visible Attribute Space 0x100 */
#define QDMA_REG_GLBL_PF_BARLITE_INT             0x104
#define		PF_BARLITE_INT_3_SHIFT               18
#define		PF_BARLITE_INT_3_MASK                0xFC0000
#define		PF_BARLITE_INT_2_SHIFT               12
#define		PF_BARLITE_INT_2_MASK                0x3F000
#define		PF_BARLITE_INT_1_SHIFT               6
#define		PF_BARLITE_INT_1_MASK                0xFC0
#define		PF_BARLITE_INT_0_SHIFT               0
#define		PF_BARLITE_INT_0_MASK                0x3F

#define QDMA_REG_GLBL_QMAX                       0x120
/* TBD : needs to be changed once we get the register offset from the h/w */
#define QDMA_REG_GLBL_MM_ENGINES                 0xABCD
#define QDMA_REG_GLBL_MISC_CAP                   0x134
#define		MISC_CAP_FLR_PRESENT_SHIFT           1
#define		MISC_CAP_FLR_PRESENT_MASK            0x2
#define QDMA_REG_GLBL_MDMA_CHANNEL               0x118
#define		MDMA_CHANNEL_ST_C2H_ENABLED_SHIFT    16
#define		MDMA_CHANNEL_ST_C2H_ENABLED_MASK     0x10000
#define		MDMA_CHANNEL_ST_H2C_ENABLED_SHIFT    17
#define		MDMA_CHANNEL_ST_H2C_ENABLED_MASK     0x20000
#define		MDMA_CHANNEL_MM_C2H_ENABLED_SHIFT    8
#define		MDMA_CHANNEL_MM_C2H_ENABLED_MASK     0x100
#define		MDMA_CHANNEL_MM_H2C_ENABLED_SHIFT    0
#define		MDMA_CHANNEL_MM_H2C_ENABLED_MASK     0x1

#ifndef __QDMA_VF__
/*
 * PF only registers
 */
#define QDMA_REG_FUNC_ID			0x12C

/* CSR space 0x200 */
#define	QDMA_REG_GLBL_RNG_SZ_BASE		0x204
#define		QDMA_REG_GLBL_RNG_SZ_COUNT	16

#define QDMA_REG_GLBL_SCRATCH			0x244

#define QDMA_REG_GLBL_WB_ACC			0x250

#define QDMA_REG_C2H_TIMER_CNT_BASE		0xA00
#define		QDMA_REG_C2H_TIMER_CNT_COUNT	16

#define QDMA_REG_C2H_CNT_TH_BASE		0xA40
#define		QDMA_REG_C2H_CNT_TH_COUNT	16

#define QDMA_REG_C2H_BUF_SZ_BASE		0xAB0
#define		QDMA_REG_C2H_BUF_SZ_COUNT	16

void qdma_csr_read_wbacc(struct xlnx_dma_dev *xdev, unsigned int *wb_acc);
void qdma_csr_read_rngsz(struct xlnx_dma_dev *xdev, unsigned int *rngsz);
int qdma_csr_write_rngsz(struct xlnx_dma_dev *xdev, unsigned int *rngsz);
void qdma_csr_read_bufsz(struct xlnx_dma_dev *xdev, unsigned int *bufsz);
int qdma_csr_write_bufsz(struct xlnx_dma_dev *xdev, unsigned int *bufsz);
void qdma_csr_read_timer_cnt(struct xlnx_dma_dev *xdev, unsigned int *tmr_cnt);
int qdma_csr_write_timer_cnt(struct xlnx_dma_dev *xdev, unsigned int *tmr_cnt);
void qdma_csr_read_cnt_thresh(struct xlnx_dma_dev *xdev, unsigned int *cnt_th);
int qdma_csr_write_cnt_thresh(struct xlnx_dma_dev *xdev, unsigned int *cnt_th);

/*
 * Function registers
 */

#define QDMA_REG_TRQ_SEL_FMAP_BASE			0x400
#define		QDMA_REG_TRQ_SEL_FMAP_STEP		0x4
#define		QDMA_REG_TRQ_SEL_FMAP_COUNT		256

#define		SEL_FMAP_QID_BASE_SHIFT			0
#define		SEL_FMAP_QID_BASE_MASK			0x7FFU
#define		SEL_FMAP_QID_MAX_SHIFT			11
#define		SEL_FMAP_QID_MAX_MASK			0xFFFU

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
#define		QDMA_REG_MM_CONTROL_RUN		0x1U
#define		QDMA_REG_MM_CONTROL_STEP	0x100

#define QDMA_REG_C2H_MM_CONTROL_BASE		0x1004
#define QDMA_REG_H2C_MM_CONTROL_BASE		0x1204


/*
 * monitor
 */
#define QDMA_REG_C2H_STAT_AXIS_PKG_CMP		0xA94

#endif /* ifndef __QDMA_VF__ */

#define QDMA_REG_C2H_QID2VEC_MAP_QID			0xa80
#define		C2H_QID2VEC_MAP_QID_C2H_VEC_SHIFT	0
#define		C2H_QID2VEC_MAP_QID_C2H_VEC_MASK	0xFFU
#define		C2H_QID2VEC_MAP_QID_C2H_COALEN_SHIFT	8
#define		C2H_QID2VEC_MAP_QID_C2H_COALEN_MASK	0x1U
#define		C2H_QID2VEC_MAP_QID_H2C_VEC_SHIFT	9
#define		C2H_QID2VEC_MAP_QID_H2C_VEC_MASK	0xFFU
#define		C2H_QID2VEC_MAP_QID_H2C_COALEN_SHIFT	17
#define		C2H_QID2VEC_MAP_QID_H2C_COALEN_MASK	0x1U

#define QDMA_REG_C2H_QID2VEC_MAP			0xa84

/*
 * FLR
 */
#ifdef __QDMA_VF__
#define QDMA_REG_FLR_STATUS			0x1100
#else
#define QDMA_REG_FLR_STATUS			0x2500
#endif

/*
 * desc. Q pdix/cidx update
 */

#define		QDMA_REG_PIDX_STEP		0x10

#ifdef __QDMA_VF__

#define QDMA_REG_INT_CIDX_BASE			0x3000
#define QDMA_REG_H2C_PIDX_BASE			0x3004
#define QDMA_REG_C2H_PIDX_BASE			0x3008
#define QDMA_REG_WRB_CIDX_BASE			0x300C

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
	QDMA_CTXT_SEL_COAL,
	QDMA_CTXT_SEL_PASID_RAM_LOW,
	QDMA_CTXT_SEL_PASID_RAM_HIGH,
	QDMA_CTXT_SEL_TIMER,
	QDMA_CTXT_SEL_QID2VEC,
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
#define V_WRB_CTXT_W0_FNC_ID(x)		\
			((x & M_WRB_CTXT_W0_FNC_ID) << S_WRB_CTXT_W0_FNC_ID)

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

#define S_PFTCH_W0_PORT_ID		5
#define M_PFTCH_W0_PORT_ID		0x7U
#define V_PFTCH_W0_PORT_ID(x)		((x) << S_PFTCH_W0_PORT_ID)

#define S_PFTCH_W0_F_ERR		26
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

#define S_INTR_CIDX_UPD_DIR_SEL		16


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
/**
 * @struct - qdma_mm_desc
 * @brief	memory mapped descriptor format
 */
struct qdma_mm_desc {
	/** source address */
	__be64 src_addr;
	/** flags */
	__be32 flag_len;
	/** reserved 32 bits */
	__be32 rsvd0;
	/** destination address */
	__be64 dst_addr;
	/** reserved 64 bits */
	__be64 rsvd1;
};

#define S_DESC_F_DV		    28
#define S_DESC_F_SOP		29
#define S_DESC_F_EOP		30

#define S_DESC_F_MASK		((1 << S_DESC_F_DV) - 1)


#define S_H2C_DESC_F_SOP		1
#define S_H2C_DESC_F_EOP		2


#define S_H2C_DESC_NUM_GL		0
#define M_H2C_DESC_NUM_GL		0x7U
#define V_H2C_DESC_NUM_GL(x)	((x) << S_H2C_DESC_NUM_GL)

#define S_H2C_DESC_NUM_CDH		3
#define M_H2C_DESC_NUM_CDH		0xFU
#define V_H2C_DESC_NUM_CDH(x)	((x) << S_H2C_DESC_NUM_CDH)

#define S_H2C_DESC_F_ZERO_CDH		13
#define S_H2C_DESC_F_EOT			14
#define S_H2C_DESC_F_REQ_WRB		15

/* FIXME pld_len and flags members are part of custom descriptor format needed
 * by example design for ST loopback and desc bypass
 */
/**
 * @struct - qdma_h2c_desc
 * @brief	memory mapped descriptor format
 */
struct qdma_h2c_desc {
	__be16 cdh_flags;	/**< cdh flags */
	__be16 pld_len;		/**< current packet length */
	__be16 len;			/**< total packet length */
	__be16 flags;		/**< descriptor flags */
	__be64 src_addr;	/**< source address */
};

/**
 * @struct - qdma_c2h_desc
 * @brief	qdma c2h descriptor
 */
struct qdma_c2h_desc {
	__be64 dst_addr;	/**< destination address */
};

/**
 * @struct - qdma_desc_wb
 * @brief	qdma writeback descriptor
 */
struct qdma_desc_wb {
	__be16 pidx;	/**< producer index */
	__be16 cidx;	/**< consumer index */
	__be32 rsvd;	/**< reserved 32 bits */
};

#define S_C2H_WB_ENTRY_F_FORMAT		0
#define F_C2H_WB_ENTRY_F_FORMAT		(1 << S_C2H_WB_ENTRY_F_FORMAT)
#define		DFORMAT0_CMPL_MASK	0xF	/* udd starts at bit 4 */
#define		DFORMAT1_CMPL_MASK	0xFFFFF	/* udd starts at bit 20 */


#define S_C2H_WB_ENTRY_F_COLOR		1
#define F_C2H_WB_ENTRY_F_COLOR		(1 << S_C2H_WB_ENTRY_F_COLOR)

#define S_C2H_WB_ENTRY_F_ERR		2
#define F_C2H_WB_ENTRY_F_ERR		(1 << S_C2H_WB_ENTRY_F_ERR)

#define S_C2H_WB_ENTRY_F_DESC_USED	3
#define F_C2H_WB_ENTRY_F_DESC_USED	(1 << S_C2H_WB_ENTRY_F_DESC_USED)

#define S_C2H_WB_ENTRY_LENGTH			4
#define M_C2H_WB_ENTRY_LENGTH			0xFFFFU
#define L_C2H_WB_ENTRY_LENGTH			16
#define V_C2H_WB_ENTRY_LENGTH(x)	\
			(((x) & M_C2H_WB_ENTRY_LENGTH) << S_C2H_WB_ENTRY_LENGTH)

#define S_C2H_WB_ENTRY_F_EOT			20
#define F_C2H_WB_ENTRY_F_EOT			(1 << S_C2H_WB_ENTRY_F_EOT)

#define S_C2H_WB_ENTRY_F_USET_INTR		21

#define S_C2H_WB_USER_DEFINED			22
#define V_C2H_WB_USER_DEFINED(x)		((x) << S_C2H_WB_USER_DEFINED)

#define M_C2H_WB_ENTRY_DMA_INFO		0xFFFFFF
#define L_C2H_WB_ENTRY_DMA_INFO		3 /* 20 bits */
/**
 * @struct - qdma_c2h_wrb_wb
 * @brief	qdma completion data descriptor
 */
struct qdma_c2h_wrb_wb {
	__be16 pidx;				/**< producer index */
	__be16 cidx;				/**< consumer index */
	__be32 color_isr_status;	/**< isr color and status */
};
#define S_C2H_WB_F_COLOR	0

#define S_C2H_WB_INT_STATE	1
#define M_C2H_WB_INT_STATE	0x3U

#define STM_REG_BASE			0x02000000
#define STM_REG_IND_CTXT_DATA_BASE	0x0
#define STM_REG_IND_CTXT_DATA3		0xC
#define STM_REG_IND_CTXT_CMD		0x14
#define STM_REG_REV			0x18
#define STM_REG_C2H_DATA8		0x20
#define STM_REG_H2C_MODE		0x30
#define STM_REG_IND_CTXT_REG_COUNT	5
#define STM_SUPPORTED_REV		0xFF
#define STM_ENABLED_DEVICE		0x6aa0
#define	STM_MAX_SUPPORTED_QID		64
#define STM_MAX_PKT_SIZE		4096
#define STM_PORT_MAP			0xE1E1

enum ind_stm_addr {
	STM_IND_ADDR_C2H_MAP=0x2,
	STM_IND_ADDR_FORCED_CAN=0x8,
	STM_IND_ADDR_Q_CTX_H2C,
	STM_IND_ADDR_H2C_MAP,
	STM_IND_ADDR_Q_CTX_C2H,
};

enum ind_stm_cmd_op {
	STM_CSR_CMD_WR=4,
	STM_CSR_CMD_RD=8,
};

#define S_STM_CTXT_QID		16
#define S_STM_CTXT_C2H_SLR	8
#define S_STM_CTXT_C2H_TDEST_H	0
#define S_STM_CTXT_C2H_TDEST_L	24
#define S_STM_CTXT_C2H_FID	16
#define S_STM_CTXT_H2C_SLR	8
#define S_STM_CTXT_H2C_TDEST_H	0
#define S_STM_CTXT_H2C_TDEST_L	24
#define S_STM_CTXT_H2C_FID	16
#define S_STM_CTXT_PKT_LIM	8
#define S_STM_CTXT_MAX_ASK	0
#define S_STM_CTXT_DPPKT	24
#define S_STM_CTXT_LOG2_DPPKT	18

#define S_STM_CMD_QID		0
#define S_STM_CMD_FID		12
#define S_STM_CMD_ADDR		24
#define S_STM_CMD_OP		28

/*
 * HW API
 */

#include "xdev.h"

#define __read_reg(xdev, reg_addr) (readl(xdev->regs + reg_addr))
#ifdef DEBUG__
#define __write_reg(xdev, reg_addr, val) \
	do { \
		pr_debug("%s, reg 0x%x, val 0x%x.\n", \
			xdev->conf.name, reg_addr, (u32)val); \
		writel(val, xdev->regs + reg_addr); \
	} while (0)
#else
#define __write_reg(xdev, reg_addr, val) (writel(val, xdev->regs + reg_addr))
#endif /* #ifdef DEBUG__ */

struct xlnx_dma_dev;
int hw_monitor_reg(struct xlnx_dma_dev *xdev, unsigned int reg, u32 mask,
		u32 val, unsigned int interval_us, unsigned int timeout_us);
#ifndef __QDMA_VF__
void qdma_device_attributes_get(struct xlnx_dma_dev *xdev);
void hw_mm_channel_enable(struct xlnx_dma_dev *xdev, int channel, bool c2h);
void hw_mm_channel_disable(struct xlnx_dma_dev *xdev, int channel, bool c2h);
void hw_set_global_csr(struct xlnx_dma_dev *xdev);
void hw_set_fmap(struct xlnx_dma_dev *xdev, u8 func_id, unsigned int qbase,
			unsigned int qmax);
void hw_read_fmap(struct xlnx_dma_dev *xdev, u8 func_id, unsigned int *qbase,
			unsigned int *qmax);
int hw_indirect_ctext_prog(struct xlnx_dma_dev *xdev, unsigned int qid,
				enum ind_ctxt_cmd_op op,
				enum ind_ctxt_cmd_sel sel, u32 *data,
				unsigned int cnt, bool verify);
int hw_init_global_context_memory(struct xlnx_dma_dev *xdev);
int hw_init_qctxt_memory(struct xlnx_dma_dev *xdev, unsigned int qbase,
			 unsigned int qmax);
int hw_indirect_stm_prog(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
			 u8 fid, enum ind_stm_cmd_op op,
			 enum ind_stm_addr addr, u32 *data, unsigned int cnt,
			 bool clear);

#endif /* #ifndef __QDMA_VF__ */


/* HW Error Registers */

#define QDMA_C2H_ERR_INT				0x0B04
#define		S_QDMA_C2H_ERR_INT_FUNC		0
#define		M_QDMA_C2H_ERR_INT_FUNC		0xFFU
#define		V_QDMA_C2H_ERR_INT_FUNC(x)	((x) << S_QDMA_C2H_ERR_INT_FUNC)

#define		S_QDMA_C2H_ERR_INT_VEC		8
#define		M_QDMA_C2H_ERR_INT_VEC		0xFFU
#define		V_QDMA_C2H_ERR_INT_VEC(x)	((x) << S_QDMA_C2H_ERR_INT_VEC)

#define		S_QDMA_C2H_ERR_INT_F_EN_COAL		16
#define		S_QDMA_C2H_ERR_INT_F_ERR_INT_ARM	17


#define QDMA_REG_GLBL_ERR_STAT				0x248
#define QDMA_REG_GLBL_ERR_MASK				0x24C
#define QDMA_REG_GLBL_ERR_MASK_VALUE			0xFFFU
#define		QDMA_REG_GLBL_F_ERR_RAM_SBE			0
#define		QDMA_REG_GLBL_F_ERR_RAM_DBE			1
#define		QDMA_REG_GLBL_F_ERR_DSC				2
#define		QDMA_REG_GLBL_F_ERR_TRQ				3
#define		QDMA_REG_GLBL_F_ERR_H2C_MM_0		4
#define		QDMA_REG_GLBL_F_ERR_H2C_MM_1		5
#define		QDMA_REG_GLBL_F_ERR_C2H_MM_0		6
#define		QDMA_REG_GLBL_F_ERR_C2H_MM_1		7
#define		QDMA_REG_GLBL_F_ERR_C2H_ST			8
#define		QDMA_REG_GLBL_F_ERR_IND_CTXT_CMD	9
#define		QDMA_REG_GLBL_F_ERR_BDG				10
#define		QDMA_REG_GLBL_F_ERR_H2C_ST			11


/* Global Descriptor Error */
#define QDMA_GLBL_DSC_ERR_STS					0x254
#define QDMA_GLBL_DSC_ERR_MSK					0x258
#define QDMA_GLBL_DSC_ERR_MSK_VALUE				0x1F9023FU
#define		QDMA_GLBL_DSC_ERR_STS_A_F_HDR_POIS		0
#define		QDMA_GLBL_DSC_ERR_STS_A_F_UR_CA			1
#define		QDMA_GLBL_DSC_ERR_STS_A_F_PARAM_MISMATCH	2
#define		QDMA_GLBL_DSC_ERR_STS_A_F_UNEXP_ADDR		3
#define		QDMA_GLBL_DSC_ERR_STS_A_F_TAG			4
#define		QDMA_GLBL_DSC_ERR_STS_A_F_FLR			5
#define		QDMA_GLBL_DSC_ERR_STS_A_F_TIMEOUT		9
#define		QDMA_GLBL_DSC_ERR_STS_A_F_DATA_POIS		16
#define		QDMA_GLBL_DSC_ERR_STS_A_F_FLR_CANCEL		19
#define		QDMA_GLBL_DSC_ERR_STS_A_F_DMA			20
#define		QDMA_GLBL_DSC_ERR_STS_A_F_DSC			21
#define		QDMA_GLBL_DSC_ERR_STS_A_F_RQ_CHAN		22
#define		QDMA_GLBL_DSC_ERR_STS_A_F_RAM_DBE		23
#define		QDMA_GLBL_DSC_ERR_STS_A_F_RAM_SBE		24


#define QDMA_GLBL_DSC_ERR_LOG0						0x25C
#define QDMA_GLBL_DSC_ERR_LOG1						0x260



#define QDMA_GLBL_TRQ_ERR_STS						0x264
#define QDMA_GLBL_TRQ_ERR_MSK						0x268
#define QDMA_GLBL_TRQ_ERR_MSK_VALUE					0xFU
#define		QDMA_GLBL_TRQ_ERR_STS_F_UN_MAPPED		0
#define		QDMA_GLBL_TRQ_ERR_STS_F_QID_RANGE		1
#define		QDMA_GLBL_TRQ_ERR_STS_F_VF_ACCESS		2
#define		QDMA_GLBL_TRQ_ERR_STS_F_TCP_TIMEOUT		3


#define QDMA_GLBL_TRQ_ERR_LOG						0x26C
#define		S_QDMA_GLBL_TRQ_ERR_LOG_ADDR			0
#define		M_QDMA_GLBL_TRQ_ERR_LOG_ADDR			0xFFFFU
#define		V_QDMA_GLBL_TRQ_ERR_LOG_ADDR(x)		\
					((x) << S_QDMA_GLBL_TRQ_ERR_LOG_ADDR)
#define		S_QDMA_GLBL_TRQ_ERR_LOG_FUNC			16
#define		M_QDMA_GLBL_TRQ_ERR_LOG_FUNC			0xFFU
#define		V_QDMA_GLBL_TRQ_ERR_LOG_FUNC(x)		\
					((x) << S_QDMA_GLBL_TRQ_ERR_LOG_FUNC)
#define		S_QDMA_GLBL_TRQ_ERR_LOG_TARGET			24
#define		M_QDMA_GLBL_TRQ_ERR_LOG_TARGET			0xFU
#define		V_QDMA_GLBL_TRQ_ERR_LOG_TARGET(x)	\
					((x) << S_QDMA_GLBL_TRQ_ERR_LOG_TARGET)

/* TRQ errors */
/**
 * trq_err_sel - possible trq errors
 */
enum trq_err_sel {
	/**< trq errors being to global 1 registers*/
	QDMA_TRQ_ERR_SEL_GLBL1 = 1,
	/**< trq errors being to global 2 registers*/
	QDMA_TRQ_ERR_SEL_GLBL2 = 2,
	/**< trq errors being to global registers*/
	QDMA_TRQ_ERR_SEL_GLBL,
	/**< trq errors being to fmap registers*/
	QDMA_TRQ_ERR_SEL_FMAP,
	/**< trq errors being to indirect interrupt*/
	QDMA_TRQ_ERR_SEL_IND,
	/**< trq errors being to c2h registers*/
	QDMA_TRQ_ERR_SEL_C2H,
	/**< trq errors being to c2h mm0 registers*/
	QDMA_TRQ_ERR_SEL_C2H_MM0 =      9,
	/**< trq errors being to h2c mm0 registers*/
	QDMA_TRQ_ERR_SEL_H2C_MM0  = 11,
	/**< trq errors being to pf queue registers */
	QDMA_TRQ_ERR_SEL_QUEUE_PF = 13,
};

/* C2H ERROR Status Registers */
#define QDMA_REG_C2H_ERR_STAT					0xAF0
#define QDMA_REG_C2H_ERR_MASK					0xAF4
#define QDMA_REG_C2H_ERR_MASK_VALUE				0x3EDBU
#define		QDMA_REG_C2H_ERR_F_MTY_MISMATCH			0
#define		QDMA_REG_C2H_ERR_F_LEN_MISMATCH			1
#define		QDMA_REG_C2H_ERR_F_QID_MISMATCH			3
#define		QDMA_REG_C2H_ERR_F_DSC_RSP_ERR			4
#define		QDMA_REG_C2H_ERR_F_ENG_WPL_DATA_PAR		6
#define		QDMA_REG_C2H_ERR_F_MSI_INT_FAIL			7
#define		QDMA_REG_C2H_ERR_F_DESC_CNT			9
#define		QDMA_REG_C2H_ERR_F_PORT_ID_CTXT_MISMATCH	10
#define		QDMA_REG_C2H_ERR_F_PORT_ID_BYP_IN_MISMATCH	11
#define		QDMA_REG_C2H_ERR_F_WRB_INV_Q			12
#define		QDMA_REG_C2H_ERR_F_WRB_QFULL			13


#define QDMA_C2H_FATAL_ERR_STAT                                      0xAF8
#define QDMA_C2H_FATAL_ERR_MASK                                      0xAFC
#define QDMA_C2H_FATAL_ERR_MASK_VALUE                                0x7FF1BU
#define		QDMA_C2H_FATAL_ERR_STAT_MTY_MISMATCH                 0
#define		QDMA_C2H_FATAL_ERR_STAT_LEN_MISMATCH                 1
#define		QDMA_C2H_FATAL_ERR_STAT_QID_MISMATCH                 3
#define		QDMA_C2H_FATAL_ERR_STAT_TIMER_FIFO_RAM_RDBE          4
#define		QDMA_C2H_FATAL_ERR_STAT_PFTCH_LL_RAM_RDBE            8
#define		QDMA_C2H_FATAL_ERR_STAT_WRB_CTXT_RAM_RDBE            9
#define		QDMA_C2H_FATAL_ERR_STAT_PFTCH_CTXT_RAM_RDBE          10
#define		QDMA_C2H_FATAL_ERR_STAT_DESC_REQ_FIFO_RAM_RDBE       11
#define		QDMA_C2H_FATAL_ERR_STAT_INT_CTXT_RAM_RDBE            12
#define		QDMA_C2H_FATAL_ERR_STAT_INT_QID2VEC_RAM_RDBE         13
#define		QDMA_C2H_FATAL_ERR_STAT_WRB_COAL_DAT_RAM_DBE         14
#define		QDMA_C2H_FATAL_ERR_STAT_TUSER_FIFO_RAM_DBE           15
#define		QDMA_C2H_FATAL_ERR_STAT_QID_FIFO_RAM_DBE             16
#define		QDMA_C2H_FATAL_ERR_STAT_PLD_FIFO_RAM_DBE             17
#define		QDMA_C2H_FATAL_ERR_STAT_WPL_DAT_PAR                  18

#define	QDMA_C2H_FATAL_ERR_ENABLE					0xB00
#define		QDMA_C2H_FATAL_ERR_ENABLE_F_EN_WRQ_DIS			0
#define		QDMA_C2H_FATAL_ERR_ENABLE_F_EN_WPL_PAR_INV		1


#define QDMA_C2H_FIRST_ERR_QID				0xB30
#define		S_QDMA_C2H_FIRST_ERR_QID		0
#define		M_QDMA_C2H_FIRST_ERR_QID		0xFFFU
#define		V_QDMA_C2H_FIRST_ERR_QID(x)	\
				((x) << S_QDMA_C2H_FIRST_ERR_QID)
#define		S_QDMA_C2H_FIRST_ERR_TYPE		16
#define		M_QDMA_C2H_FIRST_ERR_TYPE		0x1FU
#define		V_QDMA_C2H_FIRST_ERR_TYPE(x)	\
				((x) << S_QDMA_C2H_FIRST_ERR_TYPE)

#define QDMA_H2C_ERR_STAT					0xE00
#define QDMA_H2C_ERR_MASK					0xE04
#define QDMA_H2C_ERR_MASK_VALUE				0x7U
#define		QDMA_H2C_ERR_ZERO_LEN_DSC		0
#define		QDMA_H2C_ERR_WBI_MOP			1
#define		QDMA_H2C_ERR_NO_DMA_DSC			2

/* TRQ errors */
/**
 * hw_err_type_sel - hw error types
 */
enum hw_err_type_sel {
	GLBL_ERR = 1,		/**< global errors*/
	GLBL_DSC_ERR = 2,	/**< descriptor errors*/
	GLBL_TRQ_ERR,		/**< trq errors*/
	C2H_ERR,			/**< c2h errors*/
	C2H_FATAL_ERR,		/**< c2h fatal errors*/
	H2C_ERR,			/**< h2c errors*/
	HW_ERRS				/**< hardware errors*/
};
#endif /* ifndef __QDMA_REGS_H__ */
