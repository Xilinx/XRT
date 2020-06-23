/*
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 */

#ifndef __QDMA4_REG_DUMP_H__
#define __QDMA4_REG_DUMP_H__

#include "qdma_platform_env.h"
#include "qdma_access_common.h"

#define DEBUGFS_DEV_INFO_SZ		(300)

#define DEBUGFS_INTR_CNTX_SZ	(2048 * 2)
#define DBGFS_ERR_BUFLEN		(64)
#define DEBGFS_LINE_SZ			(81)
#define DEBGFS_GEN_NAME_SZ		(40)
#define REG_DUMP_SIZE_PER_LINE	(256)

#define MAX_QDMA_CFG_REGS			(200)

#define QDMA_MM_EN_SHIFT          0
#define QDMA_CMPT_EN_SHIFT        1
#define QDMA_ST_EN_SHIFT          2
#define QDMA_MAILBOX_EN_SHIFT     3

#define QDMA_MM_MODE              (1 << QDMA_MM_EN_SHIFT)
#define QDMA_COMPLETION_MODE      (1 << QDMA_CMPT_EN_SHIFT)
#define QDMA_ST_MODE              (1 << QDMA_ST_EN_SHIFT)
#define QDMA_MAILBOX              (1 << QDMA_MAILBOX_EN_SHIFT)


#define QDMA_MM_ST_MODE \
	(QDMA_MM_MODE | QDMA_COMPLETION_MODE | QDMA_ST_MODE)

#define GET_CAPABILITY_MASK(mm_en, st_en, mm_cmpt_en, mailbox_en)  \
	((mm_en << QDMA_MM_EN_SHIFT) | \
			((mm_cmpt_en | st_en) << QDMA_CMPT_EN_SHIFT) | \
			(st_en << QDMA_ST_EN_SHIFT) | \
			(mailbox_en << QDMA_MAILBOX_EN_SHIFT))

struct xreg_info {
	char name[32];
	uint32_t addr;
	uint32_t repeat;
	uint32_t step;
	uint8_t shift;
	uint8_t len;
	uint8_t mode;
};

extern struct xreg_info qdma_config_regs[MAX_QDMA_CFG_REGS];
extern struct xreg_info qdma_cpm_config_regs[MAX_QDMA_CFG_REGS];

extern int qdma_reg_dump_buf_len(void *dev_hndl, uint8_t is_vf,
	uint32_t *buflen);

extern int qdma_context_buf_len(void *dev_hndl, uint8_t is_vf,
	uint8_t st, enum qdma_dev_q_type q_type, uint32_t *buflen);

#endif
