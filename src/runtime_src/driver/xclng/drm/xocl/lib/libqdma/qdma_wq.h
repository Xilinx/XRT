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

#include "qdma_device.h"
#include "libqdma_export.h"

struct qdma_complete_event {
	u64			done_bytes;
	int			error;
	void			*req_priv;
};

struct qdma_wr {
	struct qdma_request	req;
	struct sg_table		*sgt;
	loff_t			offset;
	size_t			len;
	bool			write;
	bool			block;

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
	struct mutex		wq_lock;
	struct completion	wq_comp;
	u32			priv_data_len;
	u64			trans_bytes;

	struct qdma_sw_sg	*sg_cache;
	u32			sgc_avail;
	u32			sgc_pidx;
	u32			sgc_len;
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
	QDMA_WQE_STATE_DONE,
};

#define	_wqe(q, i)	((struct qdma_wqe *)((char *)q->wq + q->wqe_sz * i))

static inline struct qdma_wqe *wq_next_unproc(struct qdma_wq *q) 
{
	while (q->wq_unproc != q->wq_free &&
		(_wqe(q, q->wq_unproc)->state == QDMA_WQE_STATE_CANCELED ||
		_wqe(q, q->wq_unproc)->unproc_bytes == 0)) {
		q->wq_unproc++;
		q->wq_unproc &= q->wq_len - 1;
	}
	return (q->wq_unproc != q->wq_free) ? _wqe(q, q->wq_unproc) : NULL;
}

static inline struct qdma_wqe *wq_next_pending(struct qdma_wq *q)
{
	while (q->wq_pending != q->wq_unproc &&
		(_wqe(q, q->wq_pending)->state == QDMA_WQE_STATE_CANCELED ||
		_wqe(q, q->wq_pending)->state == QDMA_WQE_STATE_DONE)) {
		q->wq_pending++;
		q->wq_pending &= q->wq_len - 1;
	}
	return (q->wq_pending != q->wq_unproc) ? _wqe(q, q->wq_pending) : NULL;
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

int qdma_wq_create(unsigned long dev_hdl, struct qdma_queue_conf *qconf,
	struct qdma_wq *queue, u32 priv_data_len);
int qdma_wq_destroy(struct qdma_wq *queue);
ssize_t qdma_wq_post(struct qdma_wq *queue, struct qdma_wr *wr);

#endif /* _QDMA_WR_H */
