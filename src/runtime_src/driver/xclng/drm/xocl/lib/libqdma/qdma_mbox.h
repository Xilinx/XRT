/*******************************************************************************
 *
 * Xilinx QDMA IP Core Linux Driver
 * Copyright(c) 2017 Xilinx, Inc.
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
#ifndef __QDMA_MBOX_H__
#define __QDMA_MBOX_H__

/*
 * mailbox registers
 */
#ifdef __QDMA_VF__
#define MBOX_BASE		0x1000
#else
#define MBOX_BASE		0x2400
#endif

#define MBOX_FN_STATUS			0x0
#define		S_MBOX_FN_STATUS_IN_MSG	0
#define		M_MBOX_FN_STATUS_IN_MSG	0x1
#define		F_MBOX_FN_STATUS_IN_MSG	0x1

#define		S_MBOX_FN_STATUS_OUT_MSG	1
#define		M_MBOX_FN_STATUS_OUT_MSG	0x1
#define		F_MBOX_FN_STATUS_OUT_MSG	(1 << S_MBOX_FN_STATUS_OUT_MSG)

#define		S_MBOX_FN_STATUS_ACK	2	/* PF only, ack status */
#define		M_MBOX_FN_STATUS_ACK	0x1
#define		F_MBOX_FN_STATUS_ACK	(1 << S_MBOX_FN_STATUS_ACK)

#define		S_MBOX_FN_STATUS_SRC	4	/* PF only, source func.*/
#define		M_MBOX_FN_STATUS_SRC	0xFF
#define		G_MBOX_FN_STATUS_SRC(x)	\
		(((x) >> S_MBOX_FN_STATUS_SRC) & M_MBOX_FN_STATUS_SRC)

#define		S_MBOX_FN_STATUS_RESET	12	/* TBD: reset status */
#define		M_MBOX_FN_STATUS_RESET	0x1

#define MBOX_FN_STATUS_MASK \
		(F_MBOX_FN_STATUS_IN_MSG | \
		 F_MBOX_FN_STATUS_OUT_MSG | \
		 F_MBOX_FN_STATUS_ACK)

#define MBOX_FN_CMD			0x4
#define		S_MBOX_FN_CMD_SND	0
#define		M_MBOX_FN_CMD_SND	0x1
#define		F_MBOX_FN_CMD_SND	(1 << S_MBOX_FN_CMD_SND)

#define		S_MBOX_FN_CMD_RCV	1
#define		M_MBOX_FN_CMD_RCV	0x1
#define		F_MBOX_FN_CMD_RCV	(1 << S_MBOX_FN_CMD_RCV)

#define		S_MBOX_FN_CMD_VF_RESET	3	/* TBD PF only: reset VF */
#define		M_MBOX_FN_CMD_VF_RESET	0x1

#define MBOX_ISR_VEC			0x8
#define		S_MBOX_ISR_VEC		0
#define		M_MBOX_ISR_VEC		0x1F
#define		V_MBOX_ISR_VEC(x)	((x) & M_MBOX_ISR_VEC) 

#define MBOX_FN_TARGET			0xC
#define		S_MBOX_FN_TARGET_ID	0
#define		M_MBOX_FN_TARGET_ID	0xFF
#define		V_MBOX_FN_TARGET_ID(x)	((x) & M_MBOX_FN_TARGET_ID) 

#define MBOX_ISR_EN			0x10
#define		S_MBOX_ISR_EN		0
#define		M_MBOX_ISR_EN		0x1
#define		F_MBOX_ISR_EN		0x1

#define MBOX_PF_ACK_BASE		0x20
#define MBOX_PF_ACK_STEP		4
#define MBOX_PF_ACK_COUNT		8

#define MBOX_IN_MSG_BASE		0x800
#define MBOX_OUT_MSG_BASE		0xc00
#define MBOX_MSG_STEP			4
#define MBOX_MSG_REG_MAX		32

struct hw_descq_context {
	u32 sw[4];
	u32 prefetch[2];
	u32 wrb[4];
	u32 hw[2];	/* for retrieve only */
	u32 cr[1];	/* for retrieve only */
	u32 qid2vec[1];
};

/*
 * mailbox messages
 * NOTE: make sure the message is <= 64 bytes (16x u32) long:
 *	mbox_msg_hdr: 4 bytes
 *	body: <= 60 bytes (15x u32)
 *
 */
enum mbox_msg_op {
	/* 0 ~ 0xF */
	MBOX_OP_HELLO = 1,
	MBOX_OP_BYE,

	MBOX_OP_RESET,		/* device reset */

	MBOX_OP_FMAP,
	MBOX_OP_CSR,

	MBOX_OP_INTR_CTXT,	/* intr context */

	MBOX_OP_QCTXT_WRT,	/* queue context write */
	MBOX_OP_QCTXT_RD,	/* queue context read */
	MBOX_OP_QCTXT_CLR,	/* queue context clear */
};

enum mbox_status_t {
	MBOX_STATUS_GOOD = 0,
	MBOX_STATUS_ERR,	/* generic error code */
	MBOX_STATUS_EINVAL,	/* EINVAL */
	MBOX_STATUS_EBUSY,
};

struct mbox_msg_hdr {
	u8 op:4;
	u8 sent:1;
	u8 rcv:1; 
	u8 ack:1; 
	u8 wait:1;

	u8 src;
	u8 dst;

	char status;
};

struct mbox_msg_fmap {
	struct mbox_msg_hdr hdr;
	unsigned int qbase;
	unsigned int qmax;
};

enum mbox_csr_type {
	CSR_UNDEFINED,
	CSR_RNGSZ,
	CSR_BUFSZ,
	CSR_TIMER_CNT,
	CSR_CNT_TH,
};

struct mbox_msg_csr {
	struct mbox_msg_hdr hdr;
	unsigned int type;
	unsigned int v[16];
	unsigned int wb_acc;
};

struct mbox_msg_bye {
	struct mbox_msg_hdr hdr;
	int status;
};

#define MBOX_INTR_CTXT_VEC_MAX	7
struct mbox_msg_intr_ctxt {
	struct mbox_msg_hdr hdr;

	u8 clear:1;
	u8 filler;
	u8 vec_base;	/* 0 ~ 7 */
	u8 vec_cnt;	/* 1 ~ 8 */

	u32 w[MBOX_INTR_CTXT_VEC_MAX << 1];
};

struct mbox_msg_qctxt {
	struct mbox_msg_hdr hdr;

	u8 clear:1;
	u8 verify:1;
	u8 c2h:1;
	u8 st:1;
	u8 intr_en:1;
	u8 intr_coal_en:1;
	u8 intr_id;
	unsigned short qid;
	struct hw_descq_context context;
};

struct mbox_msg {
	union {
		struct mbox_msg_hdr hdr;
		struct mbox_msg_fmap fmap;
		struct mbox_msg_bye bye;
		struct mbox_msg_intr_ctxt intr_ctxt;
		struct mbox_msg_qctxt qctxt;
		struct mbox_msg_csr csr;
		u32 raw[MBOX_MSG_REG_MAX];
	};
};

struct xlnx_dma_dev; 
void qdma_mbox_timer_init(struct xlnx_dma_dev *xdev);
void qdma_mbox_timer_start(struct xlnx_dma_dev *xdev);
void qdma_mbox_timer_stop(struct xlnx_dma_dev *xdev);

int qdma_mbox_send_msg(struct xlnx_dma_dev *xdev, struct mbox_msg *m,
			bool wait_resp);

#endif /* #ifndef __QDMA_MBOX_H__ */
