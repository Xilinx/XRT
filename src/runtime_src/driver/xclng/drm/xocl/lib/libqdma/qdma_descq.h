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
#ifndef __QDMA_DESCQ_H__
#define __QDMA_DESCQ_H__

#include <linux/spinlock_types.h>
#include <linux/types.h>

#include "qdma_compat.h"
#include "libqdma_export.h"
#include "qdma_regs.h"
#ifdef ERR_DEBUG
#include "qdma_nl.h"
#endif

struct qdma_sdesc_info {
	struct qdma_sdesc_info *next;
	union {
		u8 fbits;
		struct flags {
			u8 valid:1;
			u8 sop:1;
			u8 eop:1;
			u8 filler:5;
		} f;
	};
	u8 rsvd[3];
	unsigned int cidx;
};

struct qdma_flq {
	/* RO fields */
	unsigned int size;
	unsigned char pg_order;
	unsigned char pg_shift;
	struct qdma_c2h_desc *desc;

	/* RW fields */
	unsigned int udd_cnt;	/* total # of udd outstanding */
	unsigned int pkt_cnt;	/* total # of packet outstanding */
	unsigned int pkt_dlen;	/* total # of pkt payload length outstanding */

	unsigned int avail;	/* # of available Rx buffers */
	unsigned long alloc_fail; /* # of times buffer allocation failed */
	unsigned long mapping_err; /* # of RX Buffer DMA Mapping failures */

	unsigned int cidx;	/* consumer index */
	unsigned int pidx;	/* producer index */
	unsigned int pidx_pend;
	struct qdma_sw_sg *sdesc;
	struct qdma_sdesc_info *sdesc_info;
};

struct qdma_descq {
	struct qdma_queue_conf conf;

	spinlock_t lock;
	struct xlnx_dma_dev *xdev;

	u8 channel;

	u8 err:1;	/* error on the Q, in halted state */
	u8 enabled:1;	/* taken/config'ed */
	u8 inited:1;	/* resource/context initialized */
	u8 online:1;	/* online */
	u8 color:1;	/* st c2h only */

	unsigned int qidx_hw;

	struct work_struct work;
	struct list_head intr_list;
	int intr_id;

	struct qdma_kthread *wrkthp;
	struct list_head wrkthp_list;
	struct list_head work_list;

	struct qdma_kthread *wbthp;
	struct list_head wbthp_list;
	struct list_head pend_list;
	struct list_head cancel_list;

	unsigned int avail;
	unsigned int pidx;
	unsigned int cidx;
	unsigned int credit;
	u8 *desc;
	dma_addr_t desc_bus;

	u8 *desc_wb;

	/* ST C2H */
	unsigned char fl_pg_order;
	unsigned char wb_entry_len;
	unsigned char rsvd[2];

	struct qdma_flq flq;
	unsigned int udd_cnt;
	unsigned int pkt_cnt;
	unsigned int pkt_dlen;

	unsigned int pidx_wrb;
	unsigned int cidx_wrb;
	unsigned int cidx_wrb_pend;
	void *desc_wrb_cur; /* data type depends on the wb_entry_len */
	u8 *desc_wrb;
	dma_addr_t desc_wrb_bus;
	u8 *desc_wrb_wb;

	struct completion cancel_comp;	
#ifdef ERR_DEBUG
	/* error inducing */
	u64 induce_err;
#endif
};
#ifdef DEBUG_THREADS
#define lock_descq(descq)	\
	do { \
		pr_debug("locking descq %s ...\n", (descq)->conf.name); \
		spin_lock_bh(&(descq)->lock); \
	} while(0)

#define unlock_descq(descq) \
	do { \
		pr_debug("unlock descq %s ...\n", (descq)->conf.name); \
		spin_unlock_bh(&(descq)->lock); \
	} while(0)
#else
#define lock_descq(descq)	spin_lock_bh(&(descq)->lock)
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




void qdma_descq_init(struct qdma_descq *descq, struct xlnx_dma_dev *xdev, int idx_hw,
		int idx_sw);

void qdma_descq_config(struct qdma_descq *descq, struct qdma_queue_conf *qconf,
		 int reconfig);
int qdma_queue_reconfig(unsigned long dev_hndl, unsigned long id,
                        struct qdma_queue_conf *qconf, char *buf, int buflen);
void qdma_descq_config_complete(struct qdma_descq *descq);

void qdma_descq_cleanup(struct qdma_descq *descq);

int qdma_descq_alloc_resource(struct qdma_descq *descq);

void qdma_descq_free_resource(struct qdma_descq *descq);

int qdma_descq_prog_hw(struct qdma_descq *descq);

int qdma_descq_context_cleanup(struct qdma_descq *descq);

void qdma_descq_service_wb(struct qdma_descq *descq);

int qdma_descq_rxq_read(struct qdma_descq *descq, struct qdma_request *req);

int qdma_descq_dump(struct qdma_descq *descq, char *buf, int buflen, int detail);

int qdma_descq_dump_desc(struct qdma_descq *descq, int start, int end, char *buf,
			int buflen);

int qdma_descq_dump_wrb(struct qdma_descq *descq, int start, int end, char *buf,
			int buflen);

int qdma_descq_dump_state(struct qdma_descq *descq, char *buf);

void intr_cidx_update(struct qdma_descq *descq, unsigned int sw_cidx);
/*
 * qdma_sgt_req_cb fits in qdma_request.opaque
 */
struct qdma_sgt_req_cb {
	struct list_head list;
	struct list_head list_cancel;
	bool canceled;
	wait_queue_head_t wq;
	unsigned int desc_nr;
	unsigned int offset;
	unsigned int left;
	unsigned int sg_offset;
	unsigned int sg_idx;
	int status;
	u8 done;
	u8 unmap_needed:1;
};
#define qdma_req_cb_get(req)	(struct qdma_sgt_req_cb *)((req)->opaque)

ssize_t qdma_descq_proc_sgt_request(struct qdma_descq *descq,
		struct qdma_sgt_req_cb *cb);

void qdma_sgt_req_done(struct qdma_descq *descq, struct qdma_sgt_req_cb *cb,
			int error);

int sgl_map(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		enum dma_data_direction dir);
void sgl_unmap(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		enum dma_data_direction dir);

void descq_flq_free_resource(struct qdma_descq *descq);
int descq_flq_alloc_resource(struct qdma_descq *descq);
int descq_process_completion_st_c2h(struct qdma_descq *descq, int budget);
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
		(V_WRB_CIDX_UPD_TIMER_IDX(descq->conf.cmpl_cnt_th_idx));

	pr_debug("%s: cidx update 0x%x, reg 0x%x.\n", descq->conf.name, cidx,
		QDMA_REG_WRB_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP);

	__write_reg(descq->xdev,
		QDMA_REG_WRB_CIDX_BASE + descq->conf.qidx * QDMA_REG_PIDX_STEP,
		cidx);
	dma_wmb();
}
#endif /* ifndef __QDMA_DESCQ_H__ */

