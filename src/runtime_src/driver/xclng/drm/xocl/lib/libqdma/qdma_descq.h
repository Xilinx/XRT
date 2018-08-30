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
#ifdef ERR_DEBUG
#include "qdma_nl.h"
#endif

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
			/** is descriptor valid */
			u8 valid:1;
			/** start of the packet */
			u8 sop:1;
			/** end of the packet */
			u8 eop:1;
			/** filler for 5 bits */
			u8 filler:5;
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

enum q_state_t {
	Q_STATE_DISABLED = 0, /** Queue is not taken */
	Q_STATE_ENABLED,      /** Assigned/taken. Partial config is done */
	Q_STATE_ONLINE,       /** Resource/context is initialized for the queue and is available for data consumption */
};

enum qdma_req_submit_state {
	QDMA_REQ_NOT_SUBMITTED,
	QDMA_REQ_SUBMIT_PARTIAL,
	QDMA_REQ_SUBMIT_COMPLETE
};

/**
 * @struct - qdma_descq
 * @brief	qdma software descriptor book keeping fields
 */
struct qdma_descq {
	/** qdma queue configuration */
	struct qdma_queue_conf conf;
	/** lock to protect access to software descriptor */
	spinlock_t lock;
	/** pointer to dma device */
	struct xlnx_dma_dev *xdev;
	/** number of channels */
	u8 channel;
	/** flag to indicate error on the Q, in halted state */
	u8 err:1;
    /** color bit for the queue */
	u8 color:1;
    /** Indicate q state */
	enum q_state_t q_state;
	/** hw qidx associated for this queue */
	unsigned int qidx_hw;
	/** queue handler */
	struct work_struct work;
	/** interrupt list */
	struct list_head intr_list;
	/** interrupt id associated for this queue */
	int intr_id;
	/** worker therad handler to handle the queue processing */
	struct qdma_kthread *wrkthp;
	/** worker therad list */
	struct list_head wrkthp_list;
	/** work  list for the queue */
	struct list_head work_list;
	/** write back therad list */
	struct qdma_kthread *wbthp;
	/** write back thread list for the queue */
	struct list_head wbthp_list;
	/** pending qork thread list */
	struct list_head pend_list;
	/** canceled req list */
	struct list_head cancel_list;
	/** availed count */
	unsigned int avail;
	/** completed count out of the total req count */
	unsigned int cur_req_count_completed;
	/** IO batching cunt */
	unsigned int io_batch_cnt;
	/** current req count */
	unsigned int cur_req_count;
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
	u8 *desc_wb;

	/* ST C2H */
	/** programming order of the data in ST c2h mode*/
	unsigned char fl_pg_order;
	/** wb entry length*/
	unsigned char wb_entry_len;
	/** 2 bits reserved*/
	unsigned char rsvd[2];
	/** qdma function list*/
	struct qdma_flq flq;
	/**total # of udd outstanding */
	unsigned int udd_cnt;
	/** packet count/number of packets to be processed*/
	unsigned int pkt_cnt;
	/** packet data length */
	unsigned int pkt_dlen;
	/** pidx of the completion entry */
	unsigned int pidx_wrb;
	/** writeback cidx */
	unsigned int cidx_wrb;
	/** pending writeback cidx */
	unsigned int cidx_wrb_pend;
	/** descriptor writeback, data type depends on the wb_entry_len */
	void *desc_wrb_cur;
	/** pointer to completion entry */
	u8 *desc_wrb;
	/** descriptor dma bus address*/
	dma_addr_t desc_wrb_bus;
	/** descriptor writeback dma bus address*/
	u8 *desc_wrb_wb;

	struct completion cancel_comp;	
#ifdef ERR_DEBUG
	/** flag to indicate error inducing */
	u64 induce_err;
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
 * qdma_descq_service_wb() - process completion data for the request
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	budget:		number of descriptors to process
 * @param[in]	c2h_upd_cmpl:	C2H only: if update completion needed
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_service_wb(struct qdma_descq *descq, int budget,
			bool c2h_upd_cmpl);

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
 * qdma_descq_dump_wrb() - dump the queue completion descriptors
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
int qdma_descq_dump_wrb(struct qdma_descq *descq, int start, int end,
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
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
void intr_cidx_update(struct qdma_descq *descq, unsigned int sw_cidx);

/**
 * @struct - qdma_sgt_req_cb
 * @brief	qdma_sgt_req_cb fits in qdma_request.opaque
 */
struct qdma_sgt_req_cb {
	/** qdma read/write request list */
	struct list_head list;
	struct list_head list_cancel;
	bool canceled;
	/** request wait queue */
	qdma_wait_queue wq;
	/** number of descriptors to proccess*/
	unsigned int desc_nr;
	/** offset in the page*/
	unsigned int offset;
	/** number of descriptors yet to be proccessed*/
	unsigned int left;
	/** offset in the scatter gather list*/
	unsigned int sg_offset;
	/** scatter gather ebtry index*/
	unsigned int sg_idx;
	/** status of the request*/
	int status;
	/** indicates whether request processing is done or not*/
	u8 done;
	/** indicates whether to unmap the kernel pages*/
	u8 unmap_needed:1;
	/* flag to indicate partial req submit */
	enum qdma_req_submit_state req_state;
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
ssize_t qdma_descq_proc_sgt_request(struct qdma_descq *descq,
		struct qdma_sgt_req_cb *cb);

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
 * sgl_map() - handler to map the scatter gather list to kernel pages
 *
 * @param[in]	pdev:	pointer to struct pci_dev
 * @param[in]	sg:		scatter gather list
 * @param[in]	sgcnt:	number of entries in scatter gather list
 * @param[in]	dir:	direction of the request
 *
 * @return	none
 *****************************************************************************/
int sgl_map(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		enum dma_data_direction dir);

/*****************************************************************************/
/**
 * sgl_unmap() - handler to unmap the scatter gather list and free
 *				the kernel pages
 *
 * @param[in]	pdev:	pointer to struct pci_dev
 * @param[in]	sg:		scatter gather list
 * @param[in]	sgcnt:	number of entries in scatter gather list
 * @param[in]	dir:	direction of the request
 *
 * @return	none
 *****************************************************************************/
void sgl_unmap(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		enum dma_data_direction dir);

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

void qdma_notify_cancel(struct qdma_descq *descq);
static inline void descq_cancel_req(struct qdma_descq *descq,
	struct qdma_request *req)
{
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);

	if (!cb->canceled) {
		list_add_tail(&cb->list_cancel, &descq->cancel_list);
		cb->canceled = true;
	}
}

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
#ifdef ERR_DEBUG
	const char *dummy; /* to avoid compiler warnings */

	dummy = xnl_attr_str[0];
	dummy = xnl_op_str[0];
	if (descq->induce_err & (1 << qid_range)) {
		__write_reg(descq->xdev,
			QDMA_REG_H2C_PIDX_BASE +
			descq->xdev->conf.qsets_max * QDMA_REG_PIDX_STEP,
			pidx | (descq->conf.irq_en << S_WRB_PIDX_UPD_EN_INT));
		pr_info("Inducing err %d", qid_range);
	} else
#endif
	{
	pr_debug("%s: pidx %u -> 0x%x, reg 0x%x.\n", descq->conf.name, pidx,
		pidx | (descq->conf.irq_en << S_WRB_PIDX_UPD_EN_INT),
		QDMA_REG_H2C_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP);

	__write_reg(descq->xdev,
		QDMA_REG_H2C_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		pidx | (descq->conf.irq_en << S_WRB_PIDX_UPD_EN_INT));
	}
	dma_wmb();
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
#ifdef ERR_DEBUG
	if (descq->induce_err & (1 << qid_range)) {
		__write_reg(descq->xdev,
			QDMA_REG_C2H_PIDX_BASE +
			descq->xdev->conf.qsets_max * QDMA_REG_PIDX_STEP,
			pidx | (descq->conf.irq_en << S_WRB_PIDX_UPD_EN_INT));
		pr_info("Inducing err %d", qid_range);
	} else
#endif
	{
	pr_debug("%s: pidx 0x%x -> 0x%x, reg 0x%x.\n", descq->conf.name, pidx,
		pidx | (descq->conf.irq_en << S_WRB_PIDX_UPD_EN_INT),
		QDMA_REG_C2H_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP);

	__write_reg(descq->xdev,
		QDMA_REG_C2H_PIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		pidx | (descq->conf.irq_en << S_WRB_PIDX_UPD_EN_INT));
	}
	dma_wmb();
}

/*****************************************************************************/
/**
 * descq_wrb_cidx_update() - inline function to update the writeback cidx
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	cidx:	wrb consumer index
 *
 * @return	none
 *****************************************************************************/
static inline void descq_wrb_cidx_update(struct qdma_descq *descq,
					unsigned int cidx)
{
#ifdef ERR_DEBUG
	if (descq->induce_err & (1 << dsc)) {
		cidx = descq->conf.rngsz;
		pr_info("inducing error %d with pidx=%u cidx = %u", dsc,
			descq->pidx, cidx);
	}
#endif
	pr_debug("%s: cidx update 0x%x, reg 0x%x.\n", descq->conf.name, cidx,
		QDMA_REG_WRB_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP);

	cidx |= (descq->conf.irq_en << S_WRB_CIDX_UPD_EN_INT) |
		(descq->conf.cmpl_stat_en << S_WRB_CIDX_UPD_EN_STAT_DESC) |
		(V_WRB_CIDX_UPD_TRIG_MODE(descq->conf.cmpl_trig_mode)) |
		(V_WRB_CIDX_UPD_TIMER_IDX(descq->conf.cmpl_timer_idx)) |
		(V_WRB_CIDX_UPD_CNTER_IDX(descq->conf.cmpl_cnt_th_idx));

	pr_debug("%s: cidx update 0x%x, reg 0x%x.\n", descq->conf.name, cidx,
		QDMA_REG_WRB_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP);

	__write_reg(descq->xdev,
		QDMA_REG_WRB_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		cidx);
	dma_wmb();
}

#endif /* ifndef __QDMA_DESCQ_H__ */
