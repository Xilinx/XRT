/*
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 */

#ifndef QDMA4_ACCESS_H_
#define QDMA4_ACCESS_H_

#include "qdma_platform_env.h"
#include "qdma_access_export.h"
#include "qdma_access_errors.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC: QDMA common library interface definitions
 *
 * Header file *qdma_access.h* defines data structures and function signatures
 * exported by QDMA common library.
 */

struct qdma_hw_err_info {
	enum qdma_error_idx idx;
	const char *err_name;
	uint32_t mask_reg_addr;
	uint32_t stat_reg_addr;
	uint32_t leaf_err_mask;
	uint32_t global_err_mask;
};


int qdma_set_default_global_csr(void *dev_hndl);

int qdma_get_version(void *dev_hndl, uint8_t is_vf,
		struct qdma_hw_version_info *version_info);

int qdma_pfetch_ctx_conf(void *dev_hndl, uint16_t hw_qid,
				struct qdma_descq_prefetch_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_sw_ctx_conf(void *dev_hndl, uint8_t c2h, uint16_t hw_qid,
				struct qdma_descq_sw_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_fmap_conf(void *dev_hndl, uint16_t func_id,
				struct qdma_fmap_cfg *config,
				enum qdma_hw_access_type access_type);

int qdma_cmpt_ctx_conf(void *dev_hndl, uint16_t hw_qid,
			struct qdma_descq_cmpt_ctxt *ctxt,
			enum qdma_hw_access_type access_type);

int qdma_hw_ctx_conf(void *dev_hndl, uint8_t c2h, uint16_t hw_qid,
				struct qdma_descq_hw_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_credit_ctx_conf(void *dev_hndl, uint8_t c2h, uint16_t hw_qid,
			struct qdma_descq_credit_ctxt *ctxt,
			enum qdma_hw_access_type access_type);

int qdma_indirect_intr_ctx_conf(void *dev_hndl, uint16_t ring_index,
				struct qdma_indirect_intr_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_queue_pidx_update(void *dev_hndl, uint8_t is_vf, uint16_t qid,
		uint8_t is_c2h, const struct qdma_q_pidx_reg_info *reg_info);

int qdma_queue_cmpt_cidx_update(void *dev_hndl, uint8_t is_vf,
		uint16_t qid, const struct qdma_q_cmpt_cidx_reg_info *reg_info);

int qdma_queue_intr_cidx_update(void *dev_hndl, uint8_t is_vf,
		uint16_t qid, const struct qdma_intr_cidx_reg_info *reg_info);

int qdma_init_ctxt_memory(void *dev_hndl);

int qdma_legacy_intr_conf(void *dev_hndl, enum status_type enable);

int qdma_clear_pend_legacy_intr(void *dev_hndl);

int qdma_is_legacy_intr_pend(void *dev_hndl);

int qdma_dump_intr_context(void *dev_hndl,
		struct qdma_indirect_intr_ctxt *intr_ctx,
		int ring_index,
		char *buf, uint32_t buflen);

uint32_t qdma_soft_reg_dump_buf_len(void);

int qdma_dump_config_regs(void *dev_hndl, uint8_t is_vf,
		char *buf, uint32_t buflen);

int qdma_hw_error_process(void *dev_hndl);

const char *qdma_hw_get_error_name(enum qdma_error_idx err_idx);

int qdma_hw_error_enable(void *dev_hndl, enum qdma_error_idx err_idx);

int qdma_get_device_attributes(void *dev_hndl,
		struct qdma_dev_attributes *dev_info);

int qdma_get_user_bar(void *dev_hndl, uint8_t is_vf,
		uint8_t func_id, uint8_t *user_bar);


#ifdef __cplusplus
}
#endif

#endif /* QDMA_ACCESS_H_ */
