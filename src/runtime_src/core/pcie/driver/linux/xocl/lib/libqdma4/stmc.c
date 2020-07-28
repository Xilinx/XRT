/*
* Streaming Platform STM-C.
* 
* Copyright (C) 2020-  Xilinx, Inc. All rights reserved.
*
* Authors: Karen.Xie@Xilinx.com
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/pci.h>

#include "stmc.h"
#include "qdma_ul_ext.h"

#define	STM_MAX_SUPPORTED_QID		64

#define STREAM_FLOWID_MASK		0xFF
#define STREAM_TDEST_MASK		0xFFFFFF

/*
 * STM-C v2 register map
 */

#define	STM_REG_REV			0x18

#define STM_REG_CONFIG_HINT		0x28
#define STM_REG_CONFIG_PORT_MAX		(4)
#define		S_STM_REG_CONFIG_PORT_NUM	24
#define		M_STM_REG_CONFIG_PORT_NUM	0xFU
#define		L_STM_REG_CONFIG_PORT_NUM	4
#define		V_STM_REG_CONFIG_PORT_NUM(x)	\
		(((x) & M_STM_REG_CONFIG_PORT_NUM) << S_STM_REG_CONFIG_PORT_NUM)

#define		S_STM_REG_CONFIG_PORT_MAP	16
#define		M_STM_REG_CONFIG_PORT_MAP	0xFFU
#define		L_STM_REG_CONFIG_PORT_MAP	8
#define		V_STM_REG_CONFIG_PORT_MAP(x)	\
		(((x) & M_STM_REG_CONFIG_PORT_MAP) << S_STM_REG_CONFIG_PORT_MAP)

#define STM_REG_H2C_MODE		0x30
#define		S_STM_REG_H2C_MODE_PORTMAP_H2C	24
#define		M_STM_REG_H2C_MODE_PORTMAP_H2C	0xFFU
#define		L_STM_REG_H2C_MODE_PORTMAP_H2C	8
#define		V_STM_REG_H2C_MODE_PORTMAP_H2C(x) \
		(((x) & M_STM_REG_H2C_MODE_PORTMAP_H2C) \
		 << S_STM_REG_H2C_MODE_PORTMAP_H2C)

#define		S_STM_REG_H2C_MODE_PORTMAP_C2H	16
#define		M_STM_REG_H2C_MODE_PORTMAP_C2H	0xFFU
#define		L_STM_REG_H2C_MODE_PORTMAP_C2H	8
#define		V_STM_REG_H2C_MODE_PORTMAP_C2H(x) \
		(((x) & M_STM_REG_H2C_MODE_PORTMAP_C2H) \
		 << S_STM_REG_H2C_MODE_PORTMAP_C2H)
#define		S_STM_EN_STMA_BKCHAN		15
#define		F_STM_EN_STMA_BKCHAN		(1 << S_STM_EN_STMA_BKCHAN)

#define STM_REG_C2H_MODE		0x38
#define STM_REG_C2H_MODE_WEIGHT_DFLT	0x00010200

#define		S_STM_REG_C2H_MODE_WEIGHT	8
#define		M_STM_REG_C2H_MODE_WEIGHT	0xFFU
#define		L_STM_REG_C2H_MODE_WEIGHT	8

/* STM indirect cmd & data registers */
#define STM_REG_CMD_DATA_0		0x0
#define STM_REG_CMD_DATA_1		0x4
#define STM_REG_CMD_DATA_2		0x8
#define STM_REG_CMD_DATA_3		0xC
#define STM_REG_CMD_DATA_4		0x10
#define STM_REG_CMD_DATA_5		0x24
#define STM_REG_CMD_DATA_C2H8		0x20

#define STM_REG_CMD			0x14
#define		STM_CMD_OP_WRITE	0x4
#define		STM_CMD_OP_READ		0x8

#define		STM_CMD_SEL_C2H_MAP	0x2
#define		STM_CMD_SEL_CAN_DIRECT	0x8
#define		STM_CMD_SEL_H2C_CTX	0x9
#define		STM_CMD_SEL_H2C_MAP	0xA
#define		STM_CMD_SEL_C2H_CTX	0xB

#define S_STM_REG_CMD_QID		0
#define M_STM_REG_CMD_QID		0x7FFU
#define V_STM_REG_CMD_QID(x)		\
		(((x) & M_STM_REG_CMD_QID) << S_STM_REG_CMD_QID)

#define S_STM_REG_CMD_FID		12
#define M_STM_REG_CMD_FID		0xFFFU
#define V_STM_REG_CMD_FID(x)		\
		(((x) & M_STM_REG_CMD_FID) << S_STM_REG_CMD_FID)

#define S_STM_REG_CMD_SEL		24
#define M_STM_REG_CMD_SEL		0xFU
#define V_STM_REG_CMD_SEL(x)		\
		(((x) & M_STM_REG_CMD_SEL) << S_STM_REG_CMD_SEL)

#define S_STM_REG_CMD_OP		28
#define M_STM_REG_CMD_OP		0xEU
#define V_STM_REG_CMD_OP(x)		\
		(((x) & M_STM_REG_CMD_OP) << S_STM_REG_CMD_OP)

/* context */
#define S_STM_CTX_W0_H2C_TDEST		0
#define M_STM_CTX_W0_H2C_TDEST		0xFFFFFFU
#define V_STM_CTX_W0_H2C_TDEST(x)		\
		(((x) & M_STM_CTX_W0_H2C_TDEST) << S_STM_CTX_W0_H2C_TDEST)

#define S_STM_CTX_W0_H2C_FLOW_ID	24	
#define M_STM_CTX_W0_H2C_FLOW_ID	0xFFU
#define V_STM_CTX_W0_H2C_FLOW_ID(x)	\
		(((x) & M_STM_CTX_W0_H2C_FLOW_ID) << S_STM_CTX_W0_H2C_FLOW_ID)

#define S_STM_CTX_W1_DPPKT		0
#define M_STM_CTX_W1_DPPKT		0xFFU
#define V_STM_CTX_W1_DPPKT(x)		\
		(((x) & M_STM_CTX_W1_DPPKT) << S_STM_CTX_W1_DPPKT)

#define S_STM_CTX_W1_MIN_ASK		8
#define M_STM_CTX_W1_MIN_ASK		0xFFU
#define V_STM_CTX_W1_MIN_ASK(x)	\
		(((x) & M_STM_CTX_W1_MIN_ASK) << S_STM_CTX_W1_MIN_ASK)

#define S_STM_CTX_W1_MAX_ASK		16	
#define M_STM_CTX_W1_MAX_ASK		0xFFU
#define V_STM_CTX_W1_MAX_ASK(x)	\
		(((x) & M_STM_CTX_W1_MAX_ASK) << S_STM_CTX_W1_MAX_ASK)

#define S_STM_CTX_W1_PKT_LIM		24	
#define M_STM_CTX_W1_PKT_LIM		0xFFU
#define V_STM_CTX_W1_PKT_LIM(x)	\
		(((x) & M_STM_CTX_W1_PKT_LIM) << S_STM_CTX_W1_PKT_LIM)

#define S_STM_CTX_W2_PKT_CDT		0	
#define M_STM_CTX_W2_PKT_CDT		0xFFU
#define V_STM_CTX_W2_PKT_CDT(x)	\
		(((x) & M_STM_CTX_W2_PKT_CDT) << S_STM_CTX_W2_PKT_CDT)

#define S_STM_CTX_W3_DPPKT_LOG		8
#define M_STM_CTX_W3_DPPKT_LOG		0x3FU
#define V_STM_CTX_W3_DPPKT_LOG(x)	\
		(((x) & M_STM_CTX_W3_DPPKT_LOG) << S_STM_CTX_W3_DPPKT_LOG)

#define S_STM_CTX_W3_F_H2C_VALID	15

#define S_STM_CTX_W4_C2H_TDEST		0
#define M_STM_CTX_W4_C2H_TDEST		0xFFFFFFU
#define V_STM_CTX_W4_C2H_TDEST(x)	\
		(((x) & M_STM_CTX_W4_C2H_TDEST) << S_STM_CTX_W4_C2H_TDEST)

#define S_STM_CTX_W4_C2H_FLOW_ID	8
#define M_STM_CTX_W4_C2H_FLOW_ID	0xFFFFFFU
#define V_STM_CTX_W4_C2H_FLOW_ID(x)	\
		(((x) & M_STM_CTX_W4_C2H_FLOW_ID) << S_STM_CTX_W4_C2H_FLOW_ID)

#define S_STM_CTX_W5_F_C2H_VALID	16

struct stm_queue_context {
	u32	data[6];
	u32	map;
};

/*
 * STMC initialization 
 */
void stmc_cleanup(struct stmc_dev *sdev)
{
	if (sdev && sdev->regs) {
		iounmap(sdev->regs);
		sdev->regs = NULL;
	}
}

int stmc_init(struct stmc_dev *sdev, struct qdma_dev_conf *conf)
{
	u32 v, nport, portmap;
	resource_size_t bar_start;
	void __iomem	*regs;

	if (!sdev)
		return -EINVAL;
	spin_lock_init(&sdev->ctx_prog_lock);
	sdev->name = conf->name;
	sdev->pdev = conf->pdev;

	bar_start = pci_resource_start(conf->pdev, sdev->bar_num);
	regs = ioremap_nocache(bar_start + sdev->reg_base, 4096);
	if (!regs) {
		pr_warn("%s unable to map STM-C bar %u.\n",
			conf->name, sdev->bar_num);
		return 0;
	}

	v = readl(regs + STM_REG_REV);
	if ((((v >> 24) & 0xFF)!= 'S') || (((v >> 16) & 0xFF) != 'T') ||
	    (((v >> 8) & 0xFF) != 'M')) {
		pr_warn("%s: Unknown STM bar 0x%x, base 0x%x, 0x%x(%c%c%c).\n",
			conf->name, sdev->bar_num, sdev->reg_base, v,
			(v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF);
		iounmap(regs);
		return 0;
	}
	sdev->regs = regs;

	pr_info("%s: STM enabled, bar %u, base 0x%x, rev 0x%x\n",
		conf->name, sdev->bar_num, sdev->reg_base, v & 0xFF);

	/* program STM port map */
	v = readl(regs + STM_REG_CONFIG_HINT);
	nport = (v >> S_STM_REG_CONFIG_PORT_NUM) & M_STM_REG_CONFIG_PORT_NUM;
	portmap = (v >> S_STM_REG_CONFIG_PORT_MAP) & M_STM_REG_CONFIG_PORT_MAP;

	v = V_STM_REG_H2C_MODE_PORTMAP_H2C(portmap) |
	    V_STM_REG_H2C_MODE_PORTMAP_C2H(portmap) | F_STM_EN_STMA_BKCHAN;
	writel(v, regs + STM_REG_H2C_MODE);

	/* C2H weight */
	v = STM_REG_C2H_MODE_WEIGHT_DFLT;
	if (nport < STM_REG_CONFIG_PORT_MAX) {
		int shift = (STM_REG_CONFIG_PORT_MAX - nport) *
				L_STM_REG_C2H_MODE_WEIGHT;
		v = (STM_REG_C2H_MODE_WEIGHT_DFLT >> shift) &
			(~((1 << shift) - 1));
	}
	writel(v, regs + STM_REG_C2H_MODE);

	return 0;
}

/*
 * STM-C queue contextp
 */
static int stmc_indirect_prog(struct stmc_dev *sdev, unsigned int qid_hw,
			 u8 fid, u32 op, u32 sel, struct stm_queue_context *ctx)
{
	void __iomem *regs = sdev->regs;
	u32 cmd = V_STM_REG_CMD_QID(qid_hw) | V_STM_REG_CMD_FID(fid) |
		  V_STM_REG_CMD_SEL(sel) | V_STM_REG_CMD_OP(op);
	int rv = 0;

	if ((op != STM_CMD_OP_WRITE) && (op != STM_CMD_OP_READ)) {
		pr_err("%s: %s, qid_hw %u, op 0x%x INVALID.\n",
			__func__, sdev->name, qid_hw, op);
		return -EINVAL;
	}

	spin_lock(&sdev->ctx_prog_lock);

	switch (sel) {
	case STM_CMD_SEL_H2C_CTX:
		if (op == STM_CMD_OP_WRITE) {
			writel(ctx->data[0], regs + STM_REG_CMD_DATA_0);
			writel(ctx->data[1], regs + STM_REG_CMD_DATA_1);
			writel(ctx->data[2], regs + STM_REG_CMD_DATA_2);
			writel(ctx->data[3], regs + STM_REG_CMD_DATA_3);
			writel(cmd, regs + STM_REG_CMD);
		} else {
			writel(cmd, regs + STM_REG_CMD);
			ctx->data[0] = readl(regs + STM_REG_CMD_DATA_0);
			ctx->data[1] = readl(regs + STM_REG_CMD_DATA_1);
			ctx->data[2] = readl(regs + STM_REG_CMD_DATA_2);
			ctx->data[3] = readl(regs + STM_REG_CMD_DATA_3);
			ctx->data[4] = readl(regs + STM_REG_CMD_DATA_4);
		}
		break;
	case STM_CMD_SEL_C2H_CTX:
		if (op == STM_CMD_OP_WRITE) {
			writel(ctx->data[4], regs + STM_REG_CMD_DATA_4);
			writel(ctx->data[5], regs + STM_REG_CMD_DATA_5);
			writel(cmd, regs + STM_REG_CMD);
		} else {
			writel(cmd, regs + STM_REG_CMD);
			ctx->data[4] = readl(regs + STM_REG_CMD_DATA_4);
			ctx->data[5] = readl(regs + STM_REG_CMD_DATA_5);
		}
		break;
	case STM_CMD_SEL_H2C_MAP:
		if (op == STM_CMD_OP_WRITE) {
			writel(ctx->map, regs + STM_REG_CMD_DATA_4);
			writel(cmd, regs + STM_REG_CMD);
		} else {
			writel(cmd, regs + STM_REG_CMD);
			ctx->map = readl(regs + STM_REG_CMD_DATA_4);
		}
		break;
	case STM_CMD_SEL_C2H_MAP:
		if (op == STM_CMD_OP_WRITE) {
			writel(ctx->map, regs + STM_REG_CMD_DATA_C2H8);
			writel(cmd, regs + STM_REG_CMD);
		} else {
			writel(cmd, regs + STM_REG_CMD);
			ctx->map = readl(regs + STM_REG_CMD_DATA_C2H8);
		}
		break;
	case STM_CMD_SEL_CAN_DIRECT:
		if (op == STM_CMD_OP_WRITE) {
			pr_err("%s: %s, STM_CMD_SEL_CAN_DIRECT is read-only.\n",
			__func__, sdev->name);
			rv = -EINVAL;
		} else {
			writel(cmd, regs + STM_REG_CMD);
			ctx->data[0] = readl(regs + STM_REG_CMD_DATA_0);
			ctx->data[1] = readl(regs + STM_REG_CMD_DATA_1);
			ctx->data[2] = readl(regs + STM_REG_CMD_DATA_2);
			ctx->data[3] = readl(regs + STM_REG_CMD_DATA_3);
		}
		break;
	default:
		pr_err("%s: %s, qid %u, fid %u, op 0x%x, sel 0x%x INVALID.\n",
			__func__, sdev->name, qid_hw, fid, op, sel);
		rv = -EINVAL;
		break;
	}

	spin_unlock(&sdev->ctx_prog_lock);

	return rv;
}

static void stmc_make_h2c_context(struct stmc_queue_conf *sqconf,
				struct stm_queue_context *ctx, bool clear)
{
	int dppkt = 1;
	int log2_dppkt = ilog2(dppkt);
	int max_ask = 8;

	memset(ctx, 0, sizeof(struct stm_queue_context));

	if (clear)
		return;

	/* 0..31 */
	ctx->data[0] = V_STM_CTX_W0_H2C_TDEST(sqconf->tdest) |
		  V_STM_CTX_W0_H2C_FLOW_ID(sqconf->flow_id);

	/* 32..63 */
	ctx->data[1] = V_STM_CTX_W1_DPPKT(dppkt) |
			V_STM_CTX_W1_MAX_ASK(max_ask);

	/* 64..95 */
	/** ?? explicitly init to 8 to workaround hw issue due to which the value
	 * is getting initialized to zero instead of its reset value of 8
	 */
	ctx->data[2] = V_STM_CTX_W2_PKT_CDT(8);

	/* 96..127 */
	ctx->data[3] = V_STM_CTX_W3_DPPKT_LOG(log2_dppkt) |
		  (1 << S_STM_CTX_W3_F_H2C_VALID);

	/* 128..159 */

	/* 191..160 */

	/* h2c map */
	ctx->map = sqconf->qid_hw;

	pr_debug("h2c qid %u, STM ctx 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x,"
		" 0x%08x, map 0x%08x.\n",
		 sqconf->qid_hw, ctx->data[0], ctx->data[1], ctx->data[2],
		 ctx->data[3], ctx->data[4], ctx->data[5], ctx->map);
}

static void stmc_make_c2h_context(struct stmc_queue_conf *sqconf,
				struct stm_queue_context *ctx, bool clear)
{
	memset(ctx, 0, sizeof(struct stm_queue_context));

	if (clear) {
		ctx->map = (DESC_SZ_8B << 11);
		return;
	} else {
		/* c2h map */
		ctx->map = sqconf->qid_hw | (DESC_SZ_8B << 11);
	}

	/* 0..31 */
	/* 32..63 */
	/* 64..95 */
	/* 96..127 */
	/* 128..159 */
	ctx->data[4] = V_STM_CTX_W4_C2H_TDEST(sqconf->tdest) |
		  V_STM_CTX_W4_C2H_FLOW_ID(sqconf->flow_id);
	/* 191..160 */
	ctx->data[5] = (1 << S_STM_CTX_W5_F_C2H_VALID);

	pr_debug("c2h qid %u, STM ctx 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x,"
		" 0x%08x, map 0x%08x.\n",
		 sqconf->qid_hw, ctx->data[0], ctx->data[1], ctx->data[2],
		 ctx->data[3], ctx->data[4], ctx->data[5], ctx->map);
}

static int stmc_queue_context_program(struct stmc_dev *sdev,
				struct stmc_queue_conf *sqconf, bool clear)
{
	struct stm_queue_context context;
	int rv;

	if (sqconf->c2h) {
		stmc_make_c2h_context(sqconf, &context, clear);

		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_WRITE, STM_CMD_SEL_C2H_CTX,
					&context);
		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_WRITE, STM_CMD_SEL_C2H_MAP,
					&context);
	} else {
		stmc_make_h2c_context(sqconf, &context, clear);

		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_WRITE, STM_CMD_SEL_H2C_CTX,
					&context);
		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_WRITE, STM_CMD_SEL_H2C_MAP,
					&context);
	}
	return rv;
}

static int validate_stm_input(struct stmc_dev *sdev,
				struct stmc_queue_conf *sqconf)
{
	if (!sdev || !sdev->regs) {
		pr_info("%s: No STMC present.\n", __func__);
		return -EINVAL;
	}
	if (sqconf && !sqconf->qconf) {
		pr_info("%s: STMC context not set up.\n", __func__);
		return -EINVAL;
	}
	return 0;
}

int stmc_queue_context_cleanup(struct stmc_dev *sdev,
				struct stmc_queue_conf *sqconf)
{
	int rv = validate_stm_input(sdev, sqconf);

	if (rv < 0)
		return rv;

	sqconf->qconf = NULL; 
	return stmc_queue_context_program(sdev, sqconf, true);
}

int stmc_queue_context_setup(struct stmc_dev *sdev,
				struct qdma_queue_conf *qconf,
				struct stmc_queue_conf *sqconf,
				unsigned int flowid, unsigned int rid)
{
	int rv = validate_stm_input(sdev, NULL);

	if (rv < 0)
		return rv;

	if (!qconf || !qconf->st) {
		pr_info("%s: qconf 0x%p, %s Skipping STMC prog for MM queue.\n",
			__func__, qconf, qconf ? qconf->name : "?") ;
		return -EINVAL;
	}

	if (qconf->qidx_hw > STM_MAX_SUPPORTED_QID) {
		pr_err("%s: QID for STM cannot be > %d\n",
			qconf->name, STM_MAX_SUPPORTED_QID);
		return -EINVAL;
	}

	sqconf->qconf = qconf;
	sqconf->qid_hw = qconf->qidx_hw;
	sqconf->c2h = (qconf->q_type == Q_C2H);
        sqconf->flow_id = flowid & STREAM_FLOWID_MASK;
	sqconf->tdest = rid & STREAM_TDEST_MASK;

	pr_info("%s, %s: flowid 0x%x, rid 0x%x -> tdest %u, flow %u",
		sdev->name, qconf->name, flowid, rid, sqconf->tdest,
		sqconf->flow_id);

	return stmc_queue_context_program(sdev, sqconf, false);
}

void stmc_queue_context_dump(struct stmc_dev *sdev,
				struct stmc_queue_conf *sqconf)
{
	struct stm_queue_context ctx;
	int rv = validate_stm_input(sdev, sqconf);

	if (rv < 0)
		return;

	memset(&ctx, 0, sizeof(struct stm_queue_context));

	if (sqconf->c2h) {
		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_READ, STM_CMD_SEL_C2H_CTX,
					&ctx);
		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_READ, STM_CMD_SEL_C2H_MAP,
					&ctx);
	} else {
		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_READ, STM_CMD_SEL_H2C_CTX,
					&ctx);
		rv = stmc_indirect_prog(sdev, sqconf->qid_hw, sqconf->flow_id,
					STM_CMD_OP_READ, STM_CMD_SEL_H2C_MAP,
					&ctx);
	}

	pr_info("%s qid %u, STM CTX 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x,"
		" 0x%08x, MAP 0x%08x.\n",
		sqconf->c2h ? "C2H" : "H2C",
		sqconf->qid_hw, ctx.data[0], ctx.data[1], ctx.data[2],
		ctx.data[3], ctx.data[4], ctx.data[5], ctx.map);
}

/*
 * H2C descriptor
 */
#define STM_MAX_PKT_SHIFT	(12)
#define STM_MAX_PKT_SIZE	(1U << STM_MAX_PKT_SHIFT)

#define stmc_get_desc_cnt(x) 	\
	((x + STM_MAX_PKT_SIZE - 1) >> STM_MAX_PKT_SHIFT)

struct stmc_h2c_desc {
	__be16 cdh_flags;
#define S_H2C_DESC_GL_LEN	0
#define M_H2C_DESC_GL_LEN	0x7U
#define V_H2C_DESC_GL_LEN(x)	((x) << S_H2C_DESC_GL_LEN)

#define S_H2C_DESC_HDR_LEN	3
#define M_H2C_DESC_HDR_LEN	0xFU
#define V_H2C_DESC_HDR_LEN(x)	((x) << S_H2C_DESC_HDR_LEN)

#define S_H2C_DESC_GL_LEN_EXT	7
#define M_H2C_DESC_GL_LEN_EXT	0x3U
#define V_H2C_DESC_GL_LEN_EXT(x) ((x) << S_H2C_DESC_GL_LEN_EXT)

#define S_H2C_DESC_F_ZERO_CDH	13
#define S_H2C_DESC_F_EOT	14
#define S_H2C_DESC_F_REQ_SDI	15
	__be16 pld_len;
	__be16 len;
	__be16 flags;
#define S_H2C_DESC_F_SOP	1
#define S_H2C_DESC_F_EOP	2
	__be64 src_addr;
};

int stmc_req_bypass_desc_fill(void *qhndl, enum qdma_q_mode q_mode,
			enum qdma_q_dir q_dir, struct qdma_request *req)
{
	struct qdma_sw_sg *sg;
	unsigned int sg_offset;
	unsigned int sg_max = req->sgcnt;
	unsigned int data_cnt = 0;
	unsigned int desc_used = 0;
	unsigned int desc_avail;
	bool sop;
	int i, rv;

	if ((q_mode != QDMA_Q_MODE_ST) || (q_dir != QDMA_Q_DIR_H2C)) {
		pr_debug("%s: mode %u != %u (ST), dir %u != %u (H2C).\n",
			__func__, q_mode, QDMA_Q_MODE_ST,
			q_dir, QDMA_Q_DIR_H2C);
        	return -EINVAL;
	}

	/* process the H2C request */
	i = qdma_sgl_find_offset(req, &sg, &sg_offset);
	if (i < 0) 
		return -EINVAL;

	sop = (i == 0 && sg_offset == 0);

	desc_avail = qdma_q_desc_avail_count(qhndl);

	for (; i < sg_max; i++, sg++) {
		struct qdma_q_desc_list *qdesc_head = NULL;
		struct qdma_q_desc_list *qdesc = NULL;
		struct stmc_h2c_desc *desc;
		unsigned int tlen = sg->len;
		dma_addr_t addr = sg->dma_addr;
		unsigned int desc_cnt;
		int j;

		if (sg_offset) {
			tlen -= sg_offset;
			addr += sg_offset;
			sg_offset = 0;
		}

		desc_cnt = stmc_get_desc_cnt(tlen);
                rv = qdma_q_desc_get(qhndl, desc_cnt, &qdesc_head);
                if (rv < 0) {
                        if (desc_used)
                                goto update_req;
                        else
                                return 0;
                }

                for (j = 0, qdesc = qdesc_head; j < desc_cnt; j++,
			qdesc = qdesc->next) {
			unsigned int len = min_t(unsigned int, tlen,
						STM_MAX_PKT_SIZE);

                        desc = qdesc->desc;
                        desc->flags = 0;

			if (!desc_cnt && sop)
				desc->flags |= S_H2C_DESC_F_SOP;

			desc->src_addr = addr;
			desc->len = len;
			desc->pld_len = len;
			desc->cdh_flags = (1 << S_H2C_DESC_F_ZERO_CDH) |
						V_H2C_DESC_GL_LEN(1); 
			tlen -= len;
			addr += len;
			data_cnt += len;
		}

		desc_used += desc_cnt;
		desc_avail -= desc_cnt;

		/* sg not used up */
		if (tlen) {
			desc->cdh_flags |= 1 << S_H2C_DESC_F_REQ_SDI;
			sg_offset = sg->len - tlen;
			break;
		} else if ((i + 1) == sg_max) {
			desc->flags |= S_H2C_DESC_F_EOP;
			desc->cdh_flags |= 1 << S_H2C_DESC_F_REQ_SDI;
			if (req->h2c_eot)
				desc->cdh_flags |=  1 << S_H2C_DESC_F_EOT;
		} else if (!desc_avail) {
			desc = qdesc_head[desc_cnt].desc;
			desc->cdh_flags |= 1 << S_H2C_DESC_F_REQ_SDI;
		}
#if 0
		/* dump out the descriptors */
                for (j = 0, qdesc = qdesc_head; j < desc_cnt; j++,
			qdesc = qdesc->next) {
                        desc = qdesc->desc;
			pr_info("%s: desc %d, cdh_flags 0x%x, pld_len 0x%x, "
				"len 0x%x, flags 0x%x, addr 0x%x.\n",
				__func__, j, desc->cdh_flags, desc->pld_len,
				desc->len, desc->flags, desc->src_addr);
		}
#endif
        }

update_req:
        qdma_update_request(qhndl, req, desc_used, data_cnt, sg_offset, sg);

        return desc_used;
}
