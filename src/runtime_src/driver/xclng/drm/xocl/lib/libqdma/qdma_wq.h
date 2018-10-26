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
 * Lizhi Hou <lizhi.hou@xilinx.com>
 *
 ******************************************************************************/

#ifndef	_QDMA_WQ_H
#define	_QDMA_WQ_H

#include <linux/aio.h>
#include "qdma_device.h"
#include "libqdma_export.h"

struct qdma_complete_event {
	u64			done_bytes;
	int			error;
	struct kiocb		*kiocb;
	void			*req_priv;
};

struct qdma_wr {
	struct qdma_request	req;
	struct sg_table		*sgt;
	loff_t			offset;
	size_t			len;
	struct kiocb		*kiocb;
	bool			write;
	bool			eot;

	int (*complete)(struct qdma_complete_event *compl_event);
	void			*priv_data;
};

struct qdma_wqe {
	struct qdma_wq		*queue;
	u32			state;
	struct qdma_wr		wr;
	wait_queue_head_t	req_comp;

	u64			unproc_bytes;
	u64			unproc_ep_addr;
	u32			unproc_sg_num;
	loff_t			unproc_sg_off;
	struct scatterlist	*unproc_sg;

	u64			done_bytes;
	u64			priv_data[1];
};

struct qdma_wq_stat {
	u32			total_slots;
	u32			free_slots;
	u32			pending_slots;
	u32			unproc_slots;

	u64			total_req_bytes;
        u64			total_complete_bytes;
        u32			total_req_num;
        u32			total_complete_num;

	u64			hw_submit_bytes;
	u64			hw_complete_bytes;

	u32			descq_rngsz;
	u32			descq_pidx;
	u32			descq_cidx;
	u32			descq_avail;
	u32			desc_wb_cidx;
        u32			desc_wb_pidx;

        u32			descq_rngsz_wrb;
        u32			descq_cidx_wrb;
        u32			descq_pidx_wrb;
        u32			descq_cidx_wrb_pend;
        u32			c2h_wrb_cidx;
        u32			c2h_wrb_pidx;

        u32			flq_cidx;
        u32			flq_pidx;
        u32			flq_pidx_pend;

};

struct qdma_wq {
	unsigned long		dev_hdl;
	unsigned long		qhdl;
	u64			flag;
	u32			qlen;
	struct qdma_queue_conf	*qconf;
	struct qdma_wqe		*wq;
	u32			wqe_sz;
	u32			wq_len;
	u32			wq_free;
	u32			wq_pending;
	u32			wq_unproc;
	spinlock_t		wq_lock;
	struct completion	wq_comp;
	u32			priv_data_len;
	u64			trans_bytes;

	struct qdma_sw_sg	*sg_cache;
	u32			sgc_avail;
	u32			sgc_pidx;
	u32			sgc_len;

	u64			req_nbytes;
        u64			compl_nbytes;
        u32			req_num;
        u32			compl_num;

	u64			proc_nbytes;
	u64			wb_nbytes;
};

enum {
	QDMA_WQ_QUEUE_ADDED		= 0x1,
	QDMA_WQ_QUEUE_STARTED		= 0x2,
	QDMA_WQ_INITIALIZED		= 0x4,
};

enum {
	QDMA_WQE_STATE_SUBMITTED,
	QDMA_WQE_STATE_PENDING,
	QDMA_WQE_STATE_CANCELED,
	QDMA_WQE_STATE_CANCELED_HW,
	QDMA_WQE_STATE_DONE,
};

enum {
	QDMA_EVT_SUCCESS,
	QDMA_EVT_CANCELED,
	QDMA_EVT_ERROR
};

#define	_wqe(q, i)	((struct qdma_wqe *)((char *)q->wq + q->wqe_sz * i))

static inline struct qdma_wqe *wq_next_unproc(struct qdma_wq *q) 
{
	while (q->wq_unproc != q->wq_free &&
		((_wqe(q, q->wq_unproc)->unproc_bytes == 0) ||
		_wqe(q, q->wq_unproc)->state == QDMA_WQE_STATE_CANCELED ||
		_wqe(q, q->wq_unproc)->state == QDMA_WQE_STATE_CANCELED_HW)) {
		q->wq_unproc++;
		q->wq_unproc &= q->wq_len - 1;
	}
	return (q->wq_unproc != q->wq_free) ? _wqe(q, q->wq_unproc) : NULL;
}

static inline struct qdma_wqe *wq_next_pending(struct qdma_wq *q)
{
	u32 curr;

	if (q->wq_pending != q->wq_unproc) {
		curr = q->wq_pending;
		q->wq_pending++;
		q->wq_pending &= q->wq_len - 1;
		return _wqe(q, curr);
	}
	return NULL;
}

static inline struct qdma_wqe *wq_next_free(struct qdma_wq *q)
{
	u32 next = ((q->wq_free + 1) & (q->wq_len - 1));
	u32 curr = q->wq_free;

	if (next != q->wq_pending) {
		q->wq_free = next; 
		return _wqe(q, curr);
	}

	return NULL;
}

#if 0
static inline struct qdma_wqe *wq_last_nonblock(struct qdma_wq *q)
{
	u32		last = q->wq_free;
	struct qdma_wqe	*wqe = NULL;

	while (last != q->wq_pending) {
		last = (last - 1) & (q->wq_len - 1);
		if (_wqe(q, last)->state != QDMA_WQE_STATE_CANCELED &&
			_wqe(q, last)->state != QDMA_WQE_STATE_CANCELED_HW &&
			!(_wqe(q, last)->wr.block)) {
			wqe = _wqe(q, last);
			break;
		}
	}
	return wqe;
}
#endif

int qdma_wq_create(unsigned long dev_hdl, struct qdma_queue_conf *qconf,
	struct qdma_wq *queue, u32 priv_data_len);
int qdma_wq_destroy(struct qdma_wq *queue);
ssize_t qdma_wq_post(struct qdma_wq *queue, struct qdma_wr *wr);
int qdma_cancel_req(struct qdma_wq *queue, struct kiocb *kiocb);
void qdma_wq_getstat(struct qdma_wq *queue, struct qdma_wq_stat *stat);
int qdma_wq_update_pidx(struct qdma_wq *queue, u32 pidx);
void qdma_arm_err_intr(unsigned long dev_hdl);

#endif /* _QDMA_WR_H */
