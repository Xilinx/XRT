/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef __QDMA_ST_C2H_H__
#define __QDMA_ST_C2H_H__
/**
 * @file
 * @brief This file contains the declarations for qdma st c2h processing
 *
 */
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include "qdma_descq.h"

/**
 * @struct - qdma_sdesc_info
 * @brief	qdma descriptor information
 */
struct qdma_sdesc_info {
	/** pointer to next descriptor  */
	struct qdma_sdesc_info *next;
	/**
	 * @union - desciptor flags
	 */
	union {
		/** 8 flag bits  */
		u8 fbits;
		/**
		 * @struct - flags
		 * @brief	desciptor flags
		 */
		struct flags {
			u8 valid:1;	/** is descriptor valid */
			u8 sop:1;	/** start of the packet */
			u8 eop:1;	/** end of the packet */
			u8 stm_eot:1;	/** stm: eot received */
			u8 filler:4;	/** filler for 5 bits */
		} f;
	};
	/** reserved 3 bits*/
	u8 rsvd[3];
	/** consumer index*/
	unsigned int cidx;
};

/**
 * @struct - qdma_flq
 * @brief qdma page allocation book keeping
 */
struct qdma_flq {
	/** RO: size of the decriptor */
	unsigned int size;
	/** RO: page order */
	unsigned char pg_order;
	/** RO: page shift */
	unsigned char pg_shift;
	/** RO: pointer to qdma c2h decriptor */
	struct qdma_c2h_desc *desc;

	/** RW: total # of udd outstanding */
	unsigned int udd_cnt;
	/** RW: total # of packet outstanding */
	unsigned int pkt_cnt;
	/** RW: total # of pkt payload length outstanding */
	unsigned int pkt_dlen;
	/** RW: # of available Rx buffers */
	unsigned int avail;
	/** RW: # of times buffer allocation failed */
	unsigned long alloc_fail;
	/** RW: # of RX Buffer DMA Mapping failures */
	unsigned long mapping_err;
	/** RW: consumer index */
	unsigned int cidx;
	/** RW: producer index */
	unsigned int pidx;
	/** RW: pending pidxes */
	unsigned int pidx_pend;
	/** RW: sw scatter gather list */
	struct qdma_sw_sg *sdesc;
	/** RW: sw descriptor info */
	struct qdma_sdesc_info *sdesc_info;
};

/*****************************************************************************/
/**
 * qdma_descq_rxq_read() - read the request queue
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[out]	req:		queue request
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_rxq_read(struct qdma_descq *descq, struct qdma_request *req);

/**
 * qdma_descq_dump_cmpt() - dump the queue completion descriptors
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	start:		start completion descriptor index
 * @param[in]	end:		end completion descriptor index
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_dump_cmpt(struct qdma_descq *descq, int start, int end,
		char *buf, int buflen);

/*****************************************************************************/
/**
 * incr_cmpl_desc_cnt() - update the interrupt cidx
 *
 * @param[in]   descq:      pointer to qdma_descq
 *
 *****************************************************************************/
void incr_cmpl_desc_cnt(struct qdma_descq *descq, unsigned int cnt);

/*****************************************************************************/
/**
 * descq_flq_free_resource() - handler to free the pages for the request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	none
 *****************************************************************************/
void descq_flq_free_resource(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * descq_flq_alloc_resource() - handler to allocate the pages for the request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int descq_flq_alloc_resource(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * descq_process_completion_st_c2h() - handler to process the st c2h
 *				completion request
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	budget:		number of descriptors to process
 * @param[in]	upd_cmpl:	if update completion required
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int descq_process_completion_st_c2h(struct qdma_descq *descq, int budget,
					bool upd_cmpl);

/*****************************************************************************/
/**
 * descq_st_c2h_read() - handler to process the st c2h read request
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	req:		pointer to read request
 * @param[in]	update:		flag to update the request
 * @param[in]	refill:		flag to indicate whether to
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int descq_st_c2h_read(struct qdma_descq *descq, struct qdma_request *req,
			bool update, bool refill);

void c2h_req_work(struct work_struct *work);

/*****************************************************************************/
/**
 * descq_cmpt_cidx_update() - inline function to update the writeback cidx
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	cidx:	cmpt consumer index
 *
 * @return	none
 *****************************************************************************/
static inline void descq_cmpt_cidx_update(struct qdma_descq *descq,
					unsigned int cidx)
{
	pr_debug("%s: cidx update 0x%x, reg 0x%x.\n", descq->conf.name, cidx,
		QDMA_REG_CMPT_CIDX_BASE +
		descq->conf.qidx * QDMA_REG_PIDX_STEP);

	cidx |= (descq->conf.irq_en << S_CMPT_CIDX_UPD_EN_INT) |
		(descq->conf.cmpl_stat_en << S_CMPT_CIDX_UPD_EN_STAT_DESC) |
		(V_CMPT_CIDX_UPD_TRIG_MODE(descq->conf.cmpl_trig_mode)) |
		(V_CMPT_CIDX_UPD_TIMER_IDX(descq->conf.cmpl_timer_idx)) |
		(V_CMPT_CIDX_UPD_CNTER_IDX(descq->conf.cmpl_cnt_th_idx));

	pr_debug("%s: cidx update 0x%x, reg 0x%x.\n", descq->conf.name, cidx,
		QDMA_REG_CMPT_CIDX_BASE +
		descq->conf.qidx * QDMA_REG_PIDX_STEP);

	__write_reg(descq->xdev,
		QDMA_REG_CMPT_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		cidx);
}

#endif /* ifndef __QDMA_ST_C2H_H__ */
