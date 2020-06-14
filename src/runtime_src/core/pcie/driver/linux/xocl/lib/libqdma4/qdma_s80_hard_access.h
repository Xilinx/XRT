/*
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 */

#ifndef QDMA_S80_HARD_ACCESS_H_
#define QDMA_S80_HARD_ACCESS_H_

#include "qdma_access_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int qdma_s80_hard_init_ctxt_memory(void *dev_hndl);

int qdma_s80_hard_qid2vec_conf(void *dev_hndl, uint8_t c2h, uint16_t hw_qid,
			 struct qdma_qid2vec *ctxt,
			 enum qdma_hw_access_type access_type);

int qdma_s80_hard_fmap_conf(void *dev_hndl, uint16_t func_id,
			struct qdma_fmap_cfg *config,
			enum qdma_hw_access_type access_type);

int qdma_s80_hard_sw_ctx_conf(void *dev_hndl, uint8_t c2h, uint16_t hw_qid,
				struct qdma_descq_sw_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_s80_hard_pfetch_ctx_conf(void *dev_hndl, uint16_t hw_qid,
				struct qdma_descq_prefetch_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_s80_hard_cmpt_ctx_conf(void *dev_hndl, uint16_t hw_qid,
			struct qdma_descq_cmpt_ctxt *ctxt,
			enum qdma_hw_access_type access_type);

int qdma_s80_hard_hw_ctx_conf(void *dev_hndl, uint8_t c2h, uint16_t hw_qid,
				struct qdma_descq_hw_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_s80_hard_credit_ctx_conf(void *dev_hndl, uint8_t c2h, uint16_t hw_qid,
			struct qdma_descq_credit_ctxt *ctxt,
			enum qdma_hw_access_type access_type);

int qdma_s80_hard_indirect_intr_ctx_conf(void *dev_hndl, uint16_t ring_index,
				struct qdma_indirect_intr_ctxt *ctxt,
				enum qdma_hw_access_type access_type);

int qdma_s80_hard_set_default_global_csr(void *dev_hndl);

int qdma_s80_hard_queue_pidx_update(void *dev_hndl, uint8_t is_vf, uint16_t qid,
		uint8_t is_c2h, const struct qdma_q_pidx_reg_info *reg_info);

int qdma_s80_hard_queue_cmpt_cidx_update(void *dev_hndl, uint8_t is_vf,
		uint16_t qid, const struct qdma_q_cmpt_cidx_reg_info *reg_info);

int qdma_s80_hard_queue_intr_cidx_update(void *dev_hndl, uint8_t is_vf,
		uint16_t qid, const struct qdma_intr_cidx_reg_info *reg_info);

int qdma_cmp_get_user_bar(void *dev_hndl, uint8_t is_vf,
		uint8_t func_id, uint8_t *user_bar);

int qdma_s80_hard_get_device_attributes(void *dev_hndl,
		struct qdma_dev_attributes *dev_info);

uint32_t qdma_s80_hard_reg_dump_buf_len(void);

uint32_t qdma_s80_hard_context_buf_len(uint8_t st,
		enum qdma_dev_q_type q_type);

int qdma_s80_hard_dump_config_regs(void *dev_hndl, uint8_t is_vf,
		char *buf, uint32_t buflen);

int qdma_s80_hard_dump_queue_context(void *dev_hndl,
		uint8_t st,
		enum qdma_dev_q_type q_type,
		struct qdma_descq_context *ctxt_data,
		char *buf, uint32_t buflen);

int qdma_s80_hard_dump_intr_context(void *dev_hndl,
		struct qdma_indirect_intr_ctxt *intr_ctx,
		int ring_index,
		char *buf, uint32_t buflen);

int qdma_s80_hard_read_dump_queue_context(void *dev_hndl,
		uint16_t qid_hw,
		uint8_t st,
		enum qdma_dev_q_type q_type,
		struct qdma_descq_context *context,
		char *buf, uint32_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* QDMA_S80_HARD_ACCESS_H_ */
