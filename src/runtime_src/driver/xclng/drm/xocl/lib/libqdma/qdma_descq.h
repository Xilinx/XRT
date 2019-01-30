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

#ifndef __QDMA_DESCQ_H__
#define __QDMA_DESCQ_H__
/**
 * @file
 * @brief This file contains the declarations for qdma descq processing
 *
 */
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include "qdma_compat.h"
#include "libqdma_export.h"
#include "qdma_regs.h"

enum q_state_t {
	Q_STATE_DISABLED = 0,	/** Queue is not taken */
	Q_STATE_ENABLED,	/** Assigned/taken. Partial config is done */
	Q_STATE_ONLINE,		/** Resource/context is initialized for the
				 * queue and is available for data consumption
				 */
};

enum qdma_req_state {
	QDMA_REQ_NOT_SUBMITTED,
	QDMA_REQ_SUBMIT_PARTIAL,
	QDMA_REQ_SUBMITTED,
	QDMA_REQ_COMPLETE
};

#define QDMA_FLQ_SIZE 80

/**
 * @struct - qdma_descq
 * @brief	qdma software descriptor book keeping fields
 */
struct qdma_descq {
	spinlock_t lock;
	struct qdma_queue_conf conf; /** qdma queue configuration */
	struct xlnx_dma_dev *xdev; /** pointer to dma device */
	u8 channel;			/** MM channel # */
	/** flag to indicate error on the Q, in halted state */
	u8 err:1;
	u8 color:1;	/** color bit for the queue */
	u8 cpu_assigned:1;
	u8 proc_req_running;
	enum q_state_t q_state; /** Indicate q state */
	unsigned int qidx_hw; /** hw qidx associated for this queue */
	unsigned int intr_work_cpu;
	unsigned int cancel_cnt;	/* # of qdma_request to be cancelled */
	struct work_struct work;
	struct list_head intr_list; /** interrupt list */
	struct list_head legacy_intr_q_list; /** leagcy interrupt list */
	int intr_id; /** interrupt id associated for this queue */
	struct list_head work_list; /** qdma_request need to be worked on */
	struct qdma_kthread *cmplthp; /** completion thread */
	/** completion status thread list for the queue */
	struct list_head cmplthp_list;
	/** pending qork thread list */
	struct list_head pend_list;
	/** wait queue for pending list clear */
	qdma_wait_queue pend_list_wq;
	/** pending list empty count */
	unsigned int pend_list_empty;
	/* flag to indicate wwaiting for transfers to complete before q stop*/
	unsigned int q_stop_wait;

	/** availed count */
	unsigned int avail;
	/** IO batching cunt */
	unsigned int io_batch_cnt;
	/** current req count */
	unsigned int pend_req_desc;
	/** current producer index */
	unsigned int pidx;
	/** current consumer index */
	unsigned int cidx;
	/** number of descrtors yet to be processed*/
	unsigned int credit;
	/** desctor to be processed*/
	u8 *desc;
	/** desctor dma address*/
	dma_addr_t desc_bus;
	/** desctor writeback*/
	u8 *desc_cmpl_status;

	/* ST C2H */
	/** programming order of the data in ST c2h mode*/
	unsigned char fl_pg_order;
	/** cmpt entry length*/
	unsigned char cmpt_entry_len;
	/** 2 bits reserved*/
	unsigned char rsvd[2];
	/** qdma free list q*/
	unsigned char flq[QDMA_FLQ_SIZE];
	/**total # of udd outstanding */
	unsigned int udd_cnt;
	/** packet count/number of packets to be processed*/
	unsigned int pkt_cnt;
	/** packet data length */
	unsigned int pkt_dlen;
	/** pidx of the completion entry */
	unsigned int pidx_cmpt;
	/** completion cidx */
	unsigned int cidx_cmpt;
	/** pending writeback cidx */
	unsigned int cidx_cmpt_pend;
	/** number of packets processed in q */
	unsigned long long total_cmpl_descs;
	/** descriptor writeback, data type depends on the cmpt_entry_len */
	void *desc_cmpt_cur;
	/** pointer to completion entry */
	u8 *desc_cmpt;
	/** descriptor dma bus address*/
	dma_addr_t desc_cmpt_bus;
	/** descriptor writeback dma bus address*/
	u8 *desc_cmpt_cmpl_status;

#ifdef DEBUGFS
	/** debugfs queue index root */
	struct dentry *dbgfs_qidx_root;
	/** debugfs queue root */
	struct dentry *dbgfs_queue_root;
	/** debugfs cmpt queue root */
	struct dentry *dbgfs_cmpt_queue_root;
#endif
};
#ifdef DEBUG_THREADS
#define lock_descq(descq)	\
	do { \
		pr_debug("locking descq %s ...\n", (descq)->conf.name); \
		spin_lock_bh(&(descq)->lock); \
	} while (0)

#define unlock_descq(descq) \
	do { \
		pr_debug("unlock descq %s ...\n", (descq)->conf.name); \
		spin_unlock_bh(&(descq)->lock); \
	} while (0)
#else
/** macro to lock descq */
#define lock_descq(descq)	spin_lock_bh(&(descq)->lock)
/** macro to un lock descq */
#define unlock_descq(descq)	spin_unlock_bh(&(descq)->lock)
#endif

static inline unsigned int ring_idx_delta(unsigned int new, unsigned int old,
					unsigned int rngsz)
{
	return new >= old ? (new - old) : new + (rngsz - old);
}

static inline unsigned int ring_idx_incr(unsigned int idx, unsigned int cnt,
					unsigned int rngsz)
{
	idx += cnt;
	return idx >= rngsz ? idx - rngsz : idx;
}

static inline unsigned int ring_idx_decr(unsigned int idx, unsigned int cnt,
					unsigned int rngsz)
{
	return idx >= cnt ?  idx - cnt : rngsz - (cnt - idx);
}

/*****************************************************************************/
/**
 * qdma_descq_init() - initialize the sw descq entry
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	idx_hw:		hw queue index
 * @param[in]	idx_sw:		sw queue index
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_init(struct qdma_descq *descq, struct xlnx_dma_dev *xdev,
			int idx_hw, int idx_sw);

/*****************************************************************************/
/**
 * qdma_descq_config() - configure the sw descq entry
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	qconf:		queue configuration
 * @param[in]	reconfig:	flag to indicate whether to refig the sw descq
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_config(struct qdma_descq *descq, struct qdma_queue_conf *qconf,
		 int reconfig);

/*****************************************************************************/
/**
 * qdma_descq_config_complete() - initialize the descq with default values
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_config_complete(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_cleanup() - clean up the resources assigned to a request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_cleanup(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_alloc_resource() - allocate the resources for a request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_alloc_resource(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_free_resource() - free up the resources assigned to a request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_free_resource(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_prog_hw() - program the hw descriptors
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_prog_hw(struct qdma_descq *descq);

#ifndef __QDMA_VF__
/*****************************************************************************/
/**
 * qdma_descq_prog_stm() - program the STM
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]   clear:		flag to program/clear stm context
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_prog_stm(struct qdma_descq *descq, bool clear);
#endif

/*****************************************************************************/
/**
 * qdma_descq_context_cleanup() - clean up the queue context
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_context_cleanup(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_service_cmpl_update() - process completion data for the request
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	budget:		number of descriptors to process
 * @param[in]	c2h_upd_cmpl:	C2H only: if update completion needed
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_service_cmpl_update(struct qdma_descq *descq, int budget,
			bool c2h_upd_cmpl);

/*****************************************************************************/
/**
 * qdma_descq_dump() - dump the queue sw desciptor data
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	detail:		indicate whether full details or abstact details
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_dump(struct qdma_descq *descq,
					char *buf, int buflen, int detail);

/*****************************************************************************/
/**
 * qdma_descq_dump_desc() - dump the queue hw descriptors
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	start:		start descriptor index
 * @param[in]	end:		end descriptor index
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_dump_desc(struct qdma_descq *descq, int start, int end,
		char *buf, int buflen);

/*****************************************************************************/
/**
 * qdma_descq_dump_state() - dump the queue desciptor state
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_dump_state(struct qdma_descq *descq, char *buf, int buflen);

/*****************************************************************************/
/**
 * intr_cidx_update() - update the interrupt cidx
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	sw_cidx:	sw cidx
 * @param[in]	ring_index:	ring index
 *
 *****************************************************************************/
void intr_cidx_update(struct qdma_descq *descq, unsigned int sw_cidx,
		      int ring_index);

/*****************************************************************************/
/**
 * incr_cmpl_desc_cnt() - update the interrupt cidx
 *
 * @param[in]   descq:      pointer to qdma_descq
 *
 *****************************************************************************/
void incr_cmpl_desc_cnt(struct qdma_descq *descq, unsigned int cnt);

/**
 * @struct - qdma_sgt_req_cb
 * @brief	qdma_sgt_req_cb fits in qdma_request.opaque
 */
struct qdma_sgt_req_cb {
	struct list_head list; /** qdma read/write request list */
	int status;		/** status of the request*/

	u8 cancel:1;	
	u8 canceled:1;
	u8 pending:1;
	u8 unmap_needed:1; /** indicates whether to unmap the kernel pages*/
	u8 c2h_eot:1; /** indicates whether tlast is received on c2h side */
	u8 done:1; /** indicates whether request processing is done or not*/

	enum qdma_req_state req_state; /* request state */

	qdma_wait_queue wq;	/** request wait queue */

	unsigned int desc_nr;	/** # descriptors used */
	unsigned int offset;	/** offset in the page*/
	unsigned int left;	/** number of descriptors yet to be proccessed*/

	void *sg;		/** sg entry being worked on currently */
	unsigned int sg_idx;	/** sg's index */
	unsigned int sg_offset; /** offset into the sg's data buffer */
};

/** macro to get the request call back data */
#define qdma_req_cb_get(req)	(struct qdma_sgt_req_cb *)((req)->opaque)

/*****************************************************************************/
/**
 * qdma_descq_proc_sgt_request() - handler to process the qdma
 *				read/write request
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	cb:		scatter gather list call back data
 *
 * @return	size of the request
 *****************************************************************************/
ssize_t qdma_descq_proc_sgt_request(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_sgt_req_done() - handler to track the progress of the request
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	cb:		scatter gather list call back data
 * @param[out]	error:	indicates the error status
 *
 * @return	none
 *****************************************************************************/
void qdma_sgt_req_done(struct qdma_descq *descq, struct qdma_sgt_req_cb *cb,
			int error);

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
 * descq_h2c_pidx_update() - inline function to update the h2c pidx
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	pidx:	c2h producer index
 *
 * @return	none
 *****************************************************************************/
static inline void descq_h2c_pidx_update(struct qdma_descq *descq,
					unsigned int pidx)
{
	pr_debug("%s: pidx %u -> 0x%x, reg 0x%x.\n", descq->conf.name, pidx,
		pidx | (descq->conf.irq_en << S_CMPL_STATUS_PIDX_UPD_EN_INT),
		QDMA_REG_H2C_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP);

	__write_reg(descq->xdev,
		QDMA_REG_H2C_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		pidx | (descq->conf.irq_en << S_CMPL_STATUS_PIDX_UPD_EN_INT));
}

/*****************************************************************************/
/**
 * descq_c2h_pidx_update() - inline function to update the c2h pidx
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	pidx:	c2h producer index
 *
 * @return	none
 *****************************************************************************/
static inline void descq_c2h_pidx_update(struct qdma_descq *descq,
					unsigned int pidx)
{
	pr_debug("%s: pidx 0x%x -> 0x%x, reg 0x%x.\n", descq->conf.name, pidx,
		pidx | (descq->conf.irq_en << S_CMPL_STATUS_PIDX_UPD_EN_INT),
		QDMA_REG_C2H_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP);

	__write_reg(descq->xdev,
		QDMA_REG_C2H_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		pidx | (descq->conf.irq_en << S_CMPL_STATUS_PIDX_UPD_EN_INT));
}

#endif /* ifndef __QDMA_DESCQ_H__ */
