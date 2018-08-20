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

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/vmalloc.h>
#include <linux/sched.h>
#include "thread.h"
#include "qdma_descq.h"
#include "qdma_wq.h"

int qdma_wq_destroy(struct qdma_wq *queue)
{
	int			ret = 0;

	reinit_completion(&queue->wq_comp);

	if (queue->flag & QDMA_WQ_QUEUE_STARTED) {
		ret = qdma_queue_stop(queue->dev_hdl, queue->qhdl, NULL, 0);
		if (ret < 0) {
			pr_err("Stop queue failed ret=%d", ret);
			goto failed;
		}
		queue->flag &= ~QDMA_WQ_QUEUE_STARTED;
	}

	if (queue->flag & QDMA_WQ_QUEUE_ADDED) {
		ret = qdma_queue_remove(queue->dev_hdl, queue->qhdl, NULL, 0);
		if (ret < 0) {
			pr_err("Remove queue failed ret=%d", ret);
			goto failed;
		}
		queue->flag &= ~QDMA_WQ_QUEUE_ADDED;
	}

	if (queue->flag & QDMA_WQ_INITIALIZED) {
#if 0
		mutex_lock(&queue->wq_lock);
		if (queue->wq_head == queue->wq_pending) {
			mutex_unlock(&queue->wq_lock);
		} else {
			mutex_unlock(&queue->wq_lock);
			wait_for_completion(&queue->wq_comp);
		}
#endif
		queue->flag &= ~QDMA_WQ_INITIALIZED;
	}

	if (queue->wq)
		vfree(queue->wq);

	if (queue->sg_cache)
		vfree(queue->sg_cache);

	return 0;

failed:
	return ret;
}

int qdma_wq_create(unsigned long dev_hdl, struct qdma_queue_conf *qconf,
	struct qdma_wq *queue, u32 priv_data_len)
{
	int	i, ret;

	queue->dev_hdl = dev_hdl;
	ret = qdma_queue_add(dev_hdl, qconf, &queue->qhdl, NULL, 0);
	if (ret < 0) {
		pr_err("Creating queue failed, ret=%d", ret);
		goto failed;
	}
	queue->flag |= QDMA_WQ_QUEUE_ADDED;

	ret = qdma_queue_start(dev_hdl, queue->qhdl, NULL, 0);
	if (ret < 0) {
		pr_err("Starting queue failed, ret=%d", ret);
		goto failed;
	}
	queue->flag |= QDMA_WQ_QUEUE_STARTED;

	queue->qconf = qdma_queue_get_config(dev_hdl, queue->qhdl, NULL, 0);
	if (!queue->qconf) {
		pr_err("Query queue config failed");
		ret = -EFAULT;
		goto failed;
	}
	if (queue->qconf->st && queue->qconf->c2h &&
		queue->qconf->c2h_bufsz != PAGE_SIZE) {
		pr_err("Unsupported c2h_bufsz %d\n", queue->qconf->c2h_bufsz);
		ret = -EINVAL;
		goto failed;
	}
	queue->qlen = queue->qconf->rngsz;
	if (queue->qlen != (1 << (ffs(queue->qlen) - 1))) {
		pr_err("Invalid qlen %d", queue->qlen);
		ret = -EINVAL;
		goto failed;
	}

	queue->wq_len = queue->qlen << 3 ;
	queue->wqe_sz = roundup(sizeof (*queue->wq) + priv_data_len, 8);
	queue->wq = vzalloc(queue->wqe_sz * queue->wq_len);
	if (!queue->wq) {
		pr_err("Alloc wq failed");
		ret = -ENOMEM;
		goto failed;
	}
	queue->priv_data_len = priv_data_len;

	for (i = 0; i < queue->wq_len; i++) {
		queue->wq[i].queue = queue;
		init_waitqueue_head(&queue->wq[i].req_comp);
	}

	queue->sg_cache = vzalloc(queue->qlen * sizeof (*queue->sg_cache));
	if (!queue->sg_cache) {
		pr_err("Alloc sg_cache failed");
		ret = -ENOMEM;
		goto failed;
	}
	queue->sgc_avail = queue->qlen;
	queue->sgc_len = queue->qlen;
	queue->sgc_pidx = 0;

	spin_lock_init(&queue->wq_lock);
	init_completion(&queue->wq_comp);
	queue->flag |= QDMA_WQ_INITIALIZED;
	return 0;

failed:
	qdma_wq_destroy(queue);
	return ret;
}

static int descq_mm_fill(struct qdma_descq *descq, struct qdma_wqe *wqe)
{
	struct qdma_mm_desc	*desc;
	struct qdma_sgt_req_cb	*cb;
	struct scatterlist	*sg, *next;
	dma_addr_t		dma_addr;
	loff_t			off;
	ssize_t			len, total = 0;
	int			i;

	if (!descq->avail)
		return -ENOENT;

	desc = (struct qdma_mm_desc *)descq->desc + descq->pidx;
	cb = qdma_req_cb_get(&wqe->wr.req);
	desc->flag_len |= (1 << S_DESC_F_SOP);
	wqe->state = QDMA_WQE_STATE_PENDING;
	sg = wqe->unproc_sg;
	for(i = 0; i < wqe->unproc_sg_num; i++, sg = next) {
		off = 0;
		len = sg->length;
		if (wqe->unproc_sg_off) {
			off += wqe->unproc_sg_off;
			len -= wqe->unproc_sg_off;
		}
		len = min_t(ssize_t, len, wqe->unproc_bytes);
		if (len > QDMA_DESC_BLEN_MAX) {
			wqe->unproc_sg_off += QDMA_DESC_BLEN_MAX;
			i--;
			len = QDMA_DESC_BLEN_MAX;
			next = sg;
		} else {
			wqe->unproc_sg_off = 0;
			next = sg_next(sg);
		}

		dma_addr = sg_dma_address(sg) + off;

		desc->rsvd1 = 0UL;
		desc->rsvd0 = 0U;

		if (descq->conf.c2h) {
			desc->src_addr = wqe->unproc_ep_addr;
			desc->dst_addr = dma_addr;
		} else {
			desc->dst_addr = wqe->unproc_ep_addr;
			desc->src_addr = dma_addr;
		}
		desc->flag_len = len;
		desc->flag_len |= (1 << S_DESC_F_DV);
		
		descq->pidx++;
		descq->pidx &= descq->conf.rngsz - 1;
		descq->avail--;

		wqe->unproc_bytes -= len;
		wqe->unproc_ep_addr += len;
		total += len;
		if (wqe->unproc_bytes == 0 || descq->avail == 0) {
			wqe->wr.req.count = total;
			cb->desc_nr = i + 1;
			cb->offset = total;
			list_add_tail(&cb->list, &descq->pend_list);
			break;
		}
		desc = (struct qdma_mm_desc *)descq->desc + descq->pidx;
	}
	desc->flag_len |= (1 << S_DESC_F_EOP);
	BUG_ON(i == wqe->unproc_sg_num && wqe->unproc_bytes != 0);

	wqe->unproc_sg = sg;
	wqe->unproc_sg_num =  wqe->unproc_sg_num - i;

	if (descq->conf.c2h)
		descq_c2h_pidx_update(descq, descq->pidx);
	else
		descq_h2c_pidx_update(descq, descq->pidx);

	return 0;
}

static int descq_st_h2c_fill(struct qdma_descq *descq, struct qdma_wqe *wqe)
{
	struct qdma_h2c_desc	*desc;
	struct qdma_sgt_req_cb	*cb;
	struct scatterlist	*sg, *next;
	dma_addr_t		dma_addr;
	loff_t			off;
	ssize_t			len, total = 0;
	int			i;

	if (!descq->avail)
		return -ENOENT;

	desc = (struct qdma_h2c_desc *)descq->desc + descq->pidx;
	cb = qdma_req_cb_get(&wqe->wr.req);
	desc->flags |= S_H2C_DESC_F_SOP;
	wqe->state = QDMA_WQE_STATE_PENDING;
	sg = wqe->unproc_sg;
	for(i = 0; i < wqe->unproc_sg_num; i++, sg = next) {
		off = 0;
		len = sg->length;
		if (wqe->unproc_sg_off) {
			off += wqe->unproc_sg_off;
			len -= wqe->unproc_sg_off;
		}
		len = min_t(ssize_t, len, wqe->unproc_bytes);
		if (len > PAGE_SIZE) {
			wqe->unproc_sg_off += PAGE_SIZE;
			i--;
			len = PAGE_SIZE;
			next = sg;
		} else {
			wqe->unproc_sg_off = 0;
			next = sg_next(sg);
		}

		dma_addr = sg_dma_address(sg) + off;
		desc->src_addr = dma_addr;
		desc->len = len;

		desc->pld_len = len;
		desc->cdh_flags |= S_H2C_DESC_F_ZERO_CDH;
		
		descq->pidx++;
		descq->pidx &= descq->conf.rngsz - 1;
		descq->avail--;

		descq_h2c_pidx_update(descq, descq->pidx);

		wqe->unproc_bytes -= len;
		total += len;
		if (wqe->unproc_bytes == 0 || descq->avail == 0) {
			wqe->wr.req.count = total;
			cb->desc_nr = i + 1;
			cb->offset = total;
			list_add_tail(&cb->list, &descq->pend_list);
			break;
		}
		desc = (struct qdma_h2c_desc *)descq->desc + descq->pidx;
	}
	desc->flags |= S_H2C_DESC_F_EOP;
	BUG_ON(i == wqe->unproc_sg_num && wqe->unproc_bytes != 0);

	wqe->unproc_sg = sg;
	wqe->unproc_sg_num =  wqe->unproc_sg_num - i;

	return 0;
}

static int descq_st_c2h_fill(struct qdma_descq *descq, struct qdma_wqe *wqe)
{
	struct qdma_sw_sg	*sgc, *next_sgc;
	struct qdma_wq		*queue;
	struct scatterlist	*sg;
	loff_t			off;
	ssize_t			len, total = 0;
	int			i, ret;

	queue = wqe->queue;
	if (!queue->sgc_avail)
		return -ENOENT;

	sgc = queue->sg_cache + queue->sgc_pidx;
	wqe->wr.req.sgl = sgc;

	wqe->state = QDMA_WQE_STATE_PENDING;
	sg = wqe->unproc_sg;
	for(i = 0; i < wqe->unproc_sg_num; i++, sg = sg_next(sg)) {
		off = sg->offset;
		len = sg->length;
		if (wqe->unproc_sg_off) {
			off += wqe->unproc_sg_off;
			len -= wqe->unproc_sg_off;
			wqe->unproc_sg_off = 0;
		}
		len = min_t(ssize_t, len, wqe->unproc_bytes);

		sgc->pg = sg_page(sg);
		sgc->offset = off;
		sgc->len = len;
		sgc->dma_addr = 0;

		queue->sgc_pidx++;
		queue->sgc_pidx &= queue->sgc_len - 1;
		queue->sgc_avail--;
		next_sgc = queue->sg_cache + queue->sgc_pidx;

		sgc->next = next_sgc;

		wqe->unproc_bytes -= len;
		total += len;
		if (wqe->unproc_bytes == 0 || queue->sgc_avail == 0) {
			sgc->next = NULL;
			wqe->wr.req.count = total;
			wqe->wr.req.sgcnt = i + 1;
			break;
		}
		sgc = next_sgc;
	}
	//desc->flags |= S_H2C_DESC_F_EOP;
	BUG_ON(i == wqe->unproc_sg_num && wqe->unproc_bytes != 0);

	wqe->unproc_sg = sg;
	wqe->unproc_sg_num =  wqe->unproc_sg_num - i;

	//wqe->wr.req.fp_done = NULL;
	ret = qdma_request_submit(queue->dev_hdl, queue->qhdl, &wqe->wr.req);

	return ret;
}

static void descq_proc_req(struct qdma_wq *queue)
{
	struct qdma_wqe		*wqe;
	struct xlnx_dma_dev	*xdev;
	struct qdma_descq	*descq;
	int			ret;

	xdev = (struct xlnx_dma_dev *)queue->dev_hdl;
	descq = qdma_device_get_descq_by_id(xdev, queue->qhdl, NULL, 0, 0);

	wqe = wq_next_unproc(queue);
	while (wqe) {
		if (wqe->state == QDMA_WQE_STATE_CANCELED)
			goto next;
		if(descq->conf.st) {
			if (descq->conf.c2h) {
				ret = descq_st_c2h_fill(descq, wqe);
			} else {
				ret = descq_st_h2c_fill(descq, wqe);
			}
		} else {
			ret = descq_mm_fill(descq, wqe);
		}
		if (ret)
			break;

		if (descq->wbthp)
			qdma_kthread_wakeup(descq->wbthp);
next:
		wqe = wq_next_unproc(queue);
	}
}

static int qdma_wqe_complete(struct qdma_request *req, unsigned int bytes_done,
	int err)
{
	struct qdma_wqe			*wqe;
	struct qdma_wq			*queue;
	struct qdma_complete_event	compl_evt;
	unsigned long			flags;

	wqe = container_of(req, struct qdma_wqe, wr.req);
	queue = wqe->queue;

	spin_lock_irqsave(&queue->wq_lock, flags);
	wqe->done_bytes += bytes_done;
	queue->sgc_avail += req->sgcnt;
	if (wqe->done_bytes == wqe->wr.len &&
		wqe->state != QDMA_WQE_STATE_CANCELED &&
		wqe->state != QDMA_WQE_STATE_CANCELED_HW) {
		if (wqe->wr.block) {
			wake_up(&wqe->req_comp);
		} else {
			compl_evt.done_bytes = wqe->done_bytes;
			compl_evt.error = QDMA_EVT_SUCCESS;
			compl_evt.req_priv = wqe->priv_data;
			wqe->wr.complete(&compl_evt);
		}
		wqe->state = QDMA_WQE_STATE_DONE;
		wqe = wq_next_pending(queue);
	} else if (wqe->state == QDMA_WQE_STATE_CANCELED_HW) {
		if (!wqe->wr.block) {
			compl_evt.done_bytes = 0;
			compl_evt.error = QDMA_EVT_CANCELED;
			compl_evt.req_priv = wqe->priv_data;
			wqe->wr.complete(&compl_evt);
		}
	}

	/* walk through all canceled reqs */
	while (wqe && wqe->state == QDMA_WQE_STATE_CANCELED) {
		if (!wqe->wr.block) {
			compl_evt.done_bytes = 0;
			compl_evt.error = QDMA_EVT_CANCELED;
			compl_evt.req_priv = wqe->priv_data;
			wqe->wr.complete(&compl_evt);
		}
		wqe = wq_next_pending(queue);
	}

	descq_proc_req(queue);
	spin_unlock_irqrestore(&queue->wq_lock, flags);

	return 0;
}

int qdma_cancel_req(struct qdma_wq *queue)
{
	struct qdma_wqe			*wqe;
	unsigned long			flags;
	struct xlnx_dma_dev		*xdev;
	struct qdma_descq		*descq;

        xdev = (struct xlnx_dma_dev *)queue->dev_hdl;
        descq = qdma_device_get_descq_by_id(xdev, queue->qhdl, NULL, 0, 0);


	spin_lock_irqsave(&queue->wq_lock, flags);
	wqe = wq_last_nonblock(queue);
	if (unlikely(!wqe)) { /* req is processed, nothing to cancel */
		spin_unlock_irqrestore(&queue->wq_lock, flags);
		return -EINVAL;
	}
	if (wqe->state == QDMA_WQE_STATE_PENDING) {
		descq_cancel_req(descq, &wqe->wr.req);
		wqe->state = QDMA_WQE_STATE_CANCELED_HW;
	} else {
		wqe->state = QDMA_WQE_STATE_CANCELED;
	}
	spin_unlock_irqrestore(&queue->wq_lock, flags);

	return 0;
}

ssize_t qdma_wq_post(struct qdma_wq *queue, struct qdma_wr *wr)
{
	struct qdma_wqe		*wqe;
	struct qdma_sgt_req_cb	*cb;
	struct scatterlist	*sg;
	loff_t			off;
	int			sg_num;
	int			i;
	ssize_t			ret = 0;

	sg_num = wr->sgt->nents;
	off = wr->offset;
	for_each_sg(wr->sgt->sgl, sg, sg_num, i) {
		if (off < sg->length) {
			break;
		}
		off -= sg->length;
	}
	BUG_ON(i == sg_num && off > sg->length);
	sg_num -= i;

	spin_lock(&queue->wq_lock);
	wqe = wq_next_free(queue);
	if (!wqe) {
		ret = -EAGAIN;
		goto again;
	}
	wqe->state = QDMA_WQE_STATE_SUBMITTED;

	memcpy(&wqe->wr, wr, sizeof (*wr));
	wqe->done_bytes = 0;
	wqe->unproc_bytes = wr->len;
	wqe->unproc_sg_num = sg_num;
	wqe->unproc_ep_addr = wr->req.ep_addr;
	wqe->unproc_sg = sg;
	wqe->unproc_sg_off = off;
	wqe->wr.req.fp_done = qdma_wqe_complete;
	wqe->wr.req.write = wr->write;

	if (wr->priv_data) {
		memcpy(wqe->priv_data, wr->priv_data, queue->priv_data_len);
	}

	cb = qdma_req_cb_get(&wqe->wr.req);
	memset(cb, 0, QDMA_REQ_OPAQUE_SIZE);
	init_waitqueue_head(&cb->wq);

again:
	descq_proc_req(queue);
	if (!ret) {
		if (wqe->wr.block) {
			spin_unlock(&queue->wq_lock);
			ret = wait_event_killable(wqe->req_comp,
				(wqe->state == QDMA_WQE_STATE_DONE));
			spin_lock(&queue->wq_lock);
			if (ret < 0) {
				if (wqe->state == QDMA_WQE_STATE_PENDING) {
					wqe->state = QDMA_WQE_STATE_CANCELED_HW;
				} else {
					wqe->state = QDMA_WQE_STATE_CANCELED;
				}
			}
			ret = wqe->done_bytes;
		} else {
			ret = wr->len;
		}
	}
		
	spin_unlock(&queue->wq_lock);

	return ret;
}
