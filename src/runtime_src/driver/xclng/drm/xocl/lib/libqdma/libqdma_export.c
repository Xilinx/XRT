/*******************************************************************************
 *
 * Xilinx XDMA IP Core Linux Driver
 * Copyright(c) 2015 - 2017 Xilinx, Inc.
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

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include "libqdma_export.h"

#include "qdma_descq.h"
#include "qdma_device.h"
#include "qdma_thread.h"
#include "qdma_regs.h"
#include "qdma_context.h"
#include "qdma_intr.h"
#include "thread.h"
#include "version.h"

/* ********************* static function definitions ************************ */
static int qdma_request_wait_for_cmpl(struct xlnx_dma_dev *xdev,
			struct qdma_descq *descq, struct qdma_request *req)
{
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);

	if (req->timeout_ms)
		wait_event_interruptible_timeout(cb->wq, cb->done,
			msecs_to_jiffies(req->timeout_ms));
	else
		wait_event_interruptible(cb->wq, cb->done);

	lock_descq(descq);
	if (!cb->done)
		/* timed out */
		list_del(&cb->list);

	if (!cb->done || cb->status) {
		pr_info("%s: req 0x%p, %c,%u,%u/%u,0x%llx, done %d, err %d, tm %u.\n",
			descq->conf.name, req, req->write ? 'W':'R', cb->offset,
			cb->left, req->count, req->ep_addr, cb->done, cb->status,
			req->timeout_ms);
		qdma_descq_dump(descq, NULL, 0, 1);	
		unlock_descq(descq);

		return -EIO;
	}

	unlock_descq(descq);

	return 0;
}

static ssize_t qdma_request_submit_st_c2h(struct xlnx_dma_dev *xdev,
			struct qdma_descq *descq, struct qdma_request *req)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	int wait = req->fp_done ? 0 : 1;
	int rv = 0;

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return QDMA_ERR_INVALID_QDMA_DEVICE;
	}

	pr_debug("%s: %u, sgl 0x%p,%u, tm %u ms.\n", descq->conf.name,
		req->count, req->sgl, req->sgcnt, req->timeout_ms);

	cb->left = req->count;

	/* any rcv'ed packet not yet read ? */
	lock_descq(descq);
	descq_st_c2h_read(descq, req, 1, 1);
	unlock_descq(descq);
	if (!cb->left)
		return req->count;

	lock_descq(descq);
	if (descq->online) {
		list_add_tail(&cb->list, &descq->pend_list);
		/* trigger an interrupt in case the data already dma'ed but
		 * have not processed yet */
		descq_wrb_cidx_update(descq, descq->cidx_wrb_pend);
		unlock_descq(descq);
	} else {
		unlock_descq(descq);
		pr_info("%s descq %s NOT online.\n",
			xdev->conf.name, descq->conf.name);
		return -EINVAL;
	}

	if (descq->wbthp)
		qdma_kthread_wakeup(descq->wbthp);

	if (!wait) {
		pr_info("%s: cb 0x%p, 0x%x NO wait.\n",
			descq->conf.name, cb, req->count);
		return 0;
	}

	rv = qdma_request_wait_for_cmpl(xdev, descq, req);
	if (rv < 0) {
		if (!req->dma_mapped)
			sgl_unmap(xdev->conf.pdev, req->sgl, req->sgcnt,
				DMA_FROM_DEVICE);
		return rv;
	}

	return req->count - cb->left;
}

/* ********************* public function definitions ************************ */

struct qdma_queue_conf *qdma_queue_get_config(unsigned long dev_hndl,
				unsigned long id, char *buf, int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, buf, buflen, 0);

	if (descq)
		return &descq->conf;

	return NULL;
}

int qdma_queue_dump(unsigned long dev_hndl, unsigned long id, char *buf,
				int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl, id,
					buf, buflen, 0);
	struct hw_descq_context ctxt;
	int len = 0;
	int rv;
#ifndef __QDMA_VF__
	int ring_index = 0;
	u32 intr_ctxt[4];
	int i = 0;
#endif

	if (!descq)
		return -EINVAL;

	/* TODO assume buflen is sufficient */
	if (!buf || !buflen)
		return QDMA_ERR_INVALID_INPUT_PARAM;

	qdma_descq_dump(descq, buf, buflen, 1);
	len = strlen(buf);

	rv = qdma_descq_context_read(descq->xdev, descq->qidx_hw,
				descq->conf.st, descq->conf.c2h, &ctxt);
	if (rv < 0) {
		len += sprintf(buf + len, "%s read context failed %d.\n",
				descq->conf.name, rv);
		return rv;
	}

	len += sprintf(buf + len,
			"\tSW CTXT:    [3]:0x%08x [2]:0x%08x [1]:0x%08x [0]:0x%08x\n",
			ctxt.sw[3], ctxt.sw[2], ctxt.sw[1], ctxt.sw[0]);

	len += sprintf(buf + len,
			"\tHW CTXT:    [1]:0x%08x [0]:0x%08x\n",
			ctxt.hw[1], ctxt.hw[0]);

	len += sprintf(buf + len,
			"\tCR CTXT:    0x%08x\n", ctxt.cr[0]);

	len += sprintf(buf + len,
			"\tQID2VEC CTXT:    0x%08x\n", ctxt.qid2vec[0]);

	if (descq->conf.c2h && descq->conf.st) {
		len += sprintf(buf + len,
			"\tWRB CTXT:   [3]:0x%08x [2]:0x%08x [1]:0x%08x [0]:0x%08x\n",
			ctxt.wrb[3], ctxt.wrb[2], ctxt.wrb[1], ctxt.wrb[0]);

		len += sprintf(buf + len,
			"\tPFTCH CTXT: [1]:0x%08x [0]:0x%08x\n",
			ctxt.prefetch[1], ctxt.prefetch[0]);
	}

#ifndef __QDMA_VF__
	for(i = 0; i < QDMA_DATA_VEC_PER_PF_MAX; i++) {

		ring_index = get_intr_ring_index(descq->xdev, (i + descq->xdev->dvec_start_idx));
		rv = qdma_intr_context_read(descq->xdev, ring_index, intr_ctxt);
		if (rv < 0) {
			len += sprintf(buf + len, "%s read intr context failed %d.\n",
					descq->conf.name, rv);
			return rv;
		}

		len += sprintf(buf + len,
				"\tRING_INDEX[%d] INTR AGGR CTXT:    [3]:0x%08x [2]:0x%08x [1]:0x%08x [0]:0x%08x\n",
				ring_index,
				intr_ctxt[3], intr_ctxt[2], intr_ctxt[1], intr_ctxt[0]);
	}
#endif

	buf[len] = '\0';
	return len;
}

int qdma_queue_dump_desc(unsigned long dev_hndl, unsigned long id,
			unsigned int start, unsigned int end, char *buf,
			int buflen)
{
	struct qdma_descq *descq = NULL;
	int len = 0;

	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	if (!xdev || !buf || !buflen)
		return QDMA_ERR_INVALID_INPUT_PARAM;

	descq = qdma_device_get_descq_by_id(xdev, id, buf, buflen, 1);
	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	len = qdma_descq_dump_state(descq, buf);
	if (!descq->inited)
		return len;

	len += qdma_descq_dump_desc(descq, start, end, buf + len, buflen - len);
	return len;
}

int qdma_queue_dump_wrb(unsigned long dev_hndl, unsigned long id,
			unsigned int start, unsigned int end, char *buf,
			int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, buf, buflen, 1);
	int len = 0;

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	len = qdma_descq_dump_state(descq, buf);
	if (descq->inited)
		len += qdma_descq_dump_wrb(descq, start, end, buf + len,
				buflen - len);

	return len;
}

int qdma_queue_remove(unsigned long dev_hndl, unsigned long id, char *buf,
			int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_descq *descq = qdma_device_get_descq_by_id(xdev, id, buf, buflen, 1);
	struct qdma_dev *qdev = xdev_2_qdev(xdev);

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return QDMA_ERR_INVALID_QDMA_DEVICE;
	}

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	lock_descq(descq);
	descq->inited = 0;
	descq->online = 0;
	unlock_descq(descq);

	qdma_descq_cleanup(descq);

	lock_descq(descq);
	descq->enabled = 0;
	unlock_descq(descq);

	spin_lock(&qdev->lock);
	if (descq->conf.c2h)
		qdev->c2h_qcnt--;
	else
		qdev->h2c_qcnt--;
	spin_unlock(&qdev->lock);

	if (buf && buflen) {
		int len = sprintf(buf, "queue %s, id %u deleted.\n",
				descq->conf.name, descq->conf.qidx);
		buf[len] = '\0';
	}
	return QDMA_OPERATION_SUCCESSFUL;
}

int qdma_queue_config(unsigned long dev_hndl, unsigned long qid,
			struct qdma_queue_conf *qconf, char *buf, int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct qdma_descq *descq = NULL;

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return QDMA_ERR_INVALID_QDMA_DEVICE;
	}

	descq = qdma_device_get_descq_by_id(xdev, qid, NULL, 0, 0);
	if (!descq) {
		pr_err("Invalid queue ID! qid=%lu, max=%u\n", qid, qdev->qmax);
		return QDMA_ERR_INVALID_QIDX;
	}

	lock_descq(descq);
	if (descq->enabled) {
		pr_err("queue_%lu already configured!\n", qid);
		unlock_descq(descq);
		return -EINVAL;
	}
	descq->enabled = 1;
	unlock_descq(descq);

	/* FIXME - Do we really need these queue counts? */
	spin_lock(&qdev->lock);
	if (qconf->c2h)
		qdev->c2h_qcnt += 1;
	else
		qdev->h2c_qcnt += 1;
	spin_unlock(&qdev->lock);

	/* configure descriptor queue */
	qdma_descq_config(descq, qconf, 0);

	return QDMA_OPERATION_SUCCESSFUL;
}

int qdma_queue_list(unsigned long dev_hndl, char *buf, int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct qdma_descq *descq = qdev->h2c_descq;
	int len = 0;
	int i;

	if (buf && buflen)
		len = sprintf(buf, "H2C Q: %u, C2H Q: %u.\n",
			qdev->h2c_qcnt, qdev->c2h_qcnt);

	if  (!qdev) {
		pr_err("dev %s, qdev null.\n",
			dev_name(&xdev->conf.pdev->dev));
		return QDMA_ERR_INVALID_QDMA_DEVICE;
	}

	if (!qdev->h2c_qcnt)
		goto c2h_queues;

	for (i = 0; i < qdev->qmax; i++, descq++) {
		lock_descq(descq);
		if (descq->enabled)
			len += qdma_descq_dump(descq, buf + len, buflen - len, 0);
		unlock_descq(descq);

		if (buf && len >= buflen)
			break;
	}

	if (buf && len >= buflen)
		goto done;

c2h_queues:
	if (!qdev->c2h_qcnt)
		goto done;

	descq = qdev->c2h_descq;
	for (i = 0; i < qdev->qmax; i++, descq++) {
		lock_descq(descq);
		if (descq->enabled)
			len += qdma_descq_dump(descq, buf + len, buflen - len, 0);
		unlock_descq(descq);

		if (buf && len >= buflen)
			break;
	}

done:
	if (buf && buflen)
		buf[len] = '\0';
	return QDMA_OPERATION_SUCCESSFUL;
}

int qdma_queue_reconfig(unsigned long dev_hndl, unsigned long id,
                        struct qdma_queue_conf *qconf, char *buf, int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					 id, buf, buflen, 1);

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	lock_descq(descq);
	if (!descq->enabled || descq->inited || descq->online) {
		pr_info("%s invalid state, init %d, en %d, online %d.\n",
			descq->conf.name, descq->enabled, descq->inited,
			descq->online);
		if (buf && buflen) {
			int l = strlen(buf);

			l += sprintf(buf + l,
				"%s invalid state, en %d, init %d, online %d.\n",
				descq->conf.name, descq->enabled, descq->inited,
				descq->online);
			buf[l] = '\0';
		}
		unlock_descq(descq);
		return QDMA_ERR_INVALID_DESCQ_STATE;
	}
	/* fill in config. info */
	qdma_descq_config(descq, qconf, 1);
	unlock_descq(descq);

	return 0;
}

int qdma_queue_add(unsigned long dev_hndl, struct qdma_queue_conf *qconf,
			unsigned long *qhndl, char *buf, int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct qdma_descq *descq;
	struct qdma_descq *pairq;
	unsigned int qcnt;
	int len = 0;
	int rv = 0;

	if (!qconf)
		return QDMA_ERR_INVALID_INPUT_PARAM;

	if (!qhndl) {
		pr_info("qhndl NULL.\n");
		if (buf) {
			len += sprintf(buf + len,
				"%s, add, qhndl NULL.\n", xdev->conf.name);
			buf[len] = '\0';
		}
		spin_unlock(&qdev->lock);
		return QDMA_ERR_INVALID_QIDX;
	}

	/* reset qhandle to an invalid value
	 * cant use 0 or NULL here because queue idx 0 has same value */
	*qhndl = QDMA_QUEUE_IDX_INVALID;

	/* requested mode enabled? */
	if ((qconf->st && !xdev->st_mode_en) ||
	    (!qconf->st && !xdev->mm_mode_en)) {
		pr_info("%s, %s mode not enabled.\n",
			xdev->conf.name, qconf->st ? "ST" : "MM");
		if (buf) {
			len += sprintf(buf + len,
				"qdma%d %s mode not enabled.\n",
				xdev->conf.idx, qconf->st ? "ST" : "MM");
			buf[len] = '\0';
		}
		return QDMA_ERR_INTERFACE_NOT_ENABLED_IN_DEVICE;
	}

	spin_lock(&qdev->lock);
	/* check if valid qidx */
	if ((qconf->qidx != QDMA_QUEUE_IDX_INVALID) &&
	    (qconf->qidx >= qdev->qmax)) {
		spin_unlock(&qdev->lock);
		pr_info("invalid descq qidx %u/%u.\n", qconf->qidx, qdev->qmax);
		if (buf) {
			len += sprintf(buf + len,
				"qdma%d invalid idx %u >= %u.\n",
				xdev->conf.idx, qconf->qidx, qdev->qmax);
			buf[len] = '\0';
		}
		return QDMA_ERR_INVALID_QIDX;
	}

	/* make if any free qidx available */
	qcnt = qconf->c2h ? qdev->c2h_qcnt : qdev->h2c_qcnt;
	if (qcnt >= qdev->qmax) {
		spin_unlock(&qdev->lock);
		pr_info("No free queues %u/%u.\n", qcnt, qdev->qmax);

		if (buf) {
			len += sprintf(buf + len,
					"qdma%d No free queues %u/%u.\n",
					xdev->conf.idx, qcnt, qdev->qmax);
			buf[len] = '\0';
		}
		return QDMA_ERR_DESCQ_FULL;
	}

	/* add to the count first, need to rewind if failed later */
	if (qconf->c2h)
		qdev->c2h_qcnt++;
	else
		qdev->h2c_qcnt++;
	spin_unlock(&qdev->lock);

	if (qconf->c2h) {
		descq = qdev->c2h_descq;
		pairq = qdev->h2c_descq;
	} else {
		descq = qdev->h2c_descq;
		pairq = qdev->c2h_descq;
	}

	/* need to allocate a free qidx */
	if (qconf->qidx == QDMA_QUEUE_IDX_INVALID) {
		int i;

		for (i = 0; i < qdev->qmax; i++, descq++, pairq++) {
			/* make sure the queue pair are the same mode */
			lock_descq(pairq);
			if (pairq->enabled && qconf->st != pairq->conf.st) {
				unlock_descq(pairq);
				continue;
			}
			unlock_descq(pairq);

			lock_descq(descq);
			if (descq->enabled) {
				unlock_descq(descq);
				continue;
			}
			descq->enabled = 1;
			qconf->qidx = i;
			unlock_descq(descq);

			break;
		}

		if (i == qdev->qmax) {
			pr_info("no free %s qp found, %u.\n",
				qconf->st ? "ST" : "MM", qdev->qmax);
			if (buf) {
				len += sprintf(buf + len,
					"qdma%d no %s QP, %u.\n",
					xdev->conf.idx,
					qconf->st ? "ST" : "MM", qdev->qmax);
				buf[len] = '\0';
			}
			rv = QDMA_ERR_DESCQ_FULL;
			goto rewind_qcnt;
		}
	} else { /* qidx specified */
		pairq += qconf->qidx;
		descq += qconf->qidx;

		/* make sure the queue pair are the same mode */
		lock_descq(pairq);
		if (pairq->enabled && (qconf->st != pairq->conf.st)) {
			unlock_descq(pairq);
			if (buf) {
				len += sprintf(buf + len,
					"Need to have same mode for Q pair.\n");
				buf[len] = '\0';
			}
			rv = -EINVAL;
			goto rewind_qcnt;
		}
		unlock_descq(pairq);

		lock_descq(descq);
		if (descq->enabled) {
			unlock_descq(descq);
			pr_info("descq idx %u already added.\n", qconf->qidx);
			if (buf) {
				len += sprintf(buf + len,
						"q idx %u already added.\n",
						qconf->qidx);
				buf[len] = '\0';
			}
			rv = QDMA_ERR_DESCQ_IDX_ALREADY_ADDED;
			goto rewind_qcnt;
		}
		descq->enabled = 1;
		unlock_descq(descq);
	}

	rv = qdma_device_prep_q_resource(xdev);
	if (rv < 0)
		goto rewind_qcnt;

	/* fill in config. info */
	qdma_descq_config(descq, qconf, 0);

	/* copy back the name in config */
	memcpy(qconf->name, descq->conf.name, QDMA_QUEUE_NAME_MAXLEN + 1);
	*qhndl = (unsigned long)descq->conf.qidx;
	if (qconf->c2h)
		*qhndl += qdev->qmax;

	pr_debug("added %s, %s, qidx %u.\n",
		descq->conf.name, qconf->c2h ? "C2H" : "H2C", qconf->qidx);
	if (buf && len < buflen) {
		len += sprintf(buf + len,
			"%s %s added.\n",
			descq->conf.name, qconf->c2h ? "C2H" : "H2C");
		buf[len] = '\0';
	}

	return QDMA_OPERATION_SUCCESSFUL;

rewind_qcnt:
	spin_lock(&qdev->lock);
	if (qconf->c2h)
		qdev->c2h_qcnt--;
	else
		qdev->h2c_qcnt--;
	spin_unlock(&qdev->lock);

	return rv;
}

int qdma_queue_start(unsigned long dev_hndl, unsigned long id,
		     char *buf, int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					 id, buf, buflen, 1);
	int rv;

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	qdma_descq_config_complete(descq);

	lock_descq(descq);
	if (!descq->enabled || descq->inited || descq->online) {
		pr_info("%s invalid state, init %d, en %d, online %d.\n",
			descq->conf.name, descq->enabled, descq->inited,
			descq->online);
		if (buf && buflen) {
			int l = strlen(buf);

			l += sprintf(buf + l,
				"%s invalid state, en %d, init %d, online %d.\n",
				descq->conf.name, descq->enabled, descq->inited,
				descq->online);
		}
		unlock_descq(descq);
		return QDMA_ERR_INVALID_DESCQ_STATE;
	}

	if (!descq->inited) {
		descq->inited = 1;
		unlock_descq(descq);

		rv = qdma_descq_alloc_resource(descq);

		lock_descq(descq);
		if (rv < 0)
			goto err_out;
	}

	rv = qdma_descq_prog_hw(descq);
	if (rv < 0) {
		pr_err("%s 0x%x setup failed.\n",
			descq->conf.name, descq->qidx_hw);
		goto err_out;
	}

	descq->online = 1;
	unlock_descq(descq);

	qdma_thread_add_work(descq);

	if (descq->xdev->num_vecs) {	/* Interrupt mode */
		unsigned long flags;

		spin_lock_irqsave(&descq->xdev->lock, flags);
		list_add_tail(&descq->intr_list,
				&descq->xdev->intr_list[descq->intr_id]);
		spin_unlock_irqrestore(&descq->xdev->lock, flags);
	}

	if (buf && buflen) {
		rv = snprintf(buf, buflen, "%s started\n", descq->conf.name);
		if (rv <= 0 || rv >= buflen) {
			return QDMA_ERR_INVALID_INPUT_PARAM;
		}
	}

	return QDMA_OPERATION_SUCCESSFUL;

err_out:
	qdma_descq_context_clear(descq->xdev, descq->qidx_hw, descq->conf.st,
				descq->conf.c2h, 1);
	qdma_descq_free_resource(descq);

	descq->inited = 0;
	unlock_descq(descq);

	return rv;
}

int qdma_queue_stop(unsigned long dev_hndl, unsigned long id, char *buf,
			int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, buf, buflen, 1);

	if (!descq)
		return QDMA_ERR_INVALID_QIDX;

	qdma_thread_remove_work(descq);
	if (descq->xdev->num_vecs) {	/* Interrupt mode */
		unsigned long flags;
		spin_lock_irqsave(&descq->xdev->lock, flags);
		list_del(&descq->intr_list);
		spin_unlock_irqrestore(&descq->xdev->lock, flags);
	}

	qdma_descq_context_clear(descq->xdev, descq->qidx_hw, descq->conf.st,
				descq->conf.c2h, 0);

	qdma_descq_free_resource(descq);

	lock_descq(descq);
	descq->online = 0;
	descq->inited = 0;
	unlock_descq(descq);

	if (buf && buflen) {
		int len = snprintf(buf, buflen, "queue %s, idx %u stopped.\n",
				descq->conf.name, descq->conf.qidx);
		if (len <= 0 || len >= buflen) {
			return QDMA_ERR_INVALID_INPUT_PARAM;
		}
	}

	return QDMA_OPERATION_SUCCESSFUL;
}

int qdma_intr_ring_dump(unsigned long dev_hndl, unsigned int vector_idx, int start_idx, int end_idx, char *buf, int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_intr_ring *ring_entry;
	struct intr_coal_conf *coal_entry;
	int counter = 0;
	int len = 0;
	u32 data[2];

	if(!xdev->intr_coal_en) {
		pr_info("Interrupt Coalescing not enabled\n");
		if (buf)  {
			len += sprintf(buf + len,"Interrupt Coalescing not enabled\n");
			buf[len] = '\0';
		}
		return -1;
	}

	if((vector_idx < xdev->dvec_start_idx) ||
			(vector_idx >= (xdev->dvec_start_idx + QDMA_DATA_VEC_PER_PF_MAX))) {
		pr_info("Vector idx %d is invalid. Shall be in range: %d -  %d.\n", vector_idx,
				xdev->dvec_start_idx, (xdev->dvec_start_idx + QDMA_DATA_VEC_PER_PF_MAX - 1));
		if (buf)  {
			len += sprintf(buf + len,"Vector idx %d is invalid. Shall be in range: %d -  %d.\n", vector_idx,
					xdev->dvec_start_idx, (xdev->dvec_start_idx + QDMA_DATA_VEC_PER_PF_MAX - 1));
			buf[len] = '\0';
		}
		return -1;
	}

	coal_entry = xdev->intr_coal_list + (vector_idx - xdev->dvec_start_idx);

	if(start_idx > coal_entry->intr_rng_num_entries) {
		pr_info("start_idx %d is invalid. Shall be less than: %d \n", start_idx,
				coal_entry->intr_rng_num_entries);
		if (buf)  {
			len += sprintf(buf + len,"start_idx %d is invalid. Shall be less than: %d \n", start_idx,
					coal_entry->intr_rng_num_entries);
			buf[len] = '\0';
		}
		return -1;
	}

	if(end_idx == -1 || end_idx >= coal_entry->intr_rng_num_entries)
		end_idx = coal_entry->intr_rng_num_entries - 1;

	if(start_idx == -1)
		start_idx = 0;

	if(start_idx > end_idx) {
		pr_info("start_idx can't be greater than end_idx \n");
		if (buf)  {
			len += sprintf(buf + len,"start_idx can't be greater than end_idx \n");
			buf[len] = '\0';
		}
		return -1;
	}

	for(counter = start_idx; counter <= end_idx; counter++)	{
		ring_entry = coal_entry->intr_ring_base + counter;
		memcpy(data, ring_entry, sizeof(u32) * 2);
		if (buf) {
			len += sprintf(buf + len, "intr_ring_entry = %d: 0x%08x 0x%08x\n", counter, data[1], data[0]);
			buf[len] = '\0';
		}
    }

	return 0;
}

void sgl_unmap(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		 enum dma_data_direction dir)
{
	int i;

	for (i = 0; i < sgcnt; i++, sg++) {
		if (!sg->pg)
                        break;
		if (sg->dma_addr) {
			pci_unmap_page(pdev, sg->dma_addr - sg->offset,
                                PAGE_SIZE, dir);
			sg->dma_addr = 0UL;
		}
	}
}

int sgl_map(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		enum dma_data_direction dir)
{
	int i;

	for (i = 0; i < sgcnt; i++, sg++) {
		/* !! TODO  page size !! */
		sg->dma_addr = pci_map_page(pdev, sg->pg, 0, PAGE_SIZE, dir);
		if (unlikely(pci_dma_mapping_error(pdev, sg->dma_addr))) {
			pr_info("map sgl failed, sg %d, %u.\n", i, sg->len);
			return -EIO;
		}
		sg->dma_addr += sg->offset;
	}
	return 0;
}

ssize_t qdma_request_submit(unsigned long dev_hndl, unsigned long id,
			struct qdma_request *req)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_descq *descq = qdma_device_get_descq_by_id(xdev, id, NULL, 0, 1);
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	enum dma_data_direction dir;
	int wait = req->fp_done ? 0 : 1;
	int rv = 0;

	if (!descq)
		return -EINVAL;

	dir = descq->conf.c2h ?  DMA_FROM_DEVICE : DMA_TO_DEVICE;

	if ((req->write && descq->conf.c2h) ||
	    (!req->write && !descq->conf.c2h)) {
		pr_info("%s: bad direction, %c.\n",
			descq->conf.name, req->write ? 'W' : 'R');
		return -EINVAL;
	}

	memset(cb, 0, QDMA_REQ_OPAQUE_SIZE);
	init_waitqueue_head(&cb->wq);

	pr_debug("%s: %u, ep 0x%llx, sgl 0x%p,%u, tm %u ms.\n",
		descq->conf.name, req->count, req->ep_addr, req->sgl,
		req->sgcnt, req->timeout_ms);

	if (descq->conf.st && descq->conf.c2h)
		return qdma_request_submit_st_c2h(xdev, descq, req);

	if (!req->dma_mapped) {
		rv = sgl_map(xdev->conf.pdev,  req->sgl, req->sgcnt, dir);
		if (rv < 0) {
			pr_info("%s map sgl %u failed, %u.\n",
				descq->conf.name, req->sgcnt, req->count);
			goto unmap_sgl;
		}
		cb->unmap_needed = 1;
	}

	lock_descq(descq);
	if (!descq->online) {
		unlock_descq(descq);
		pr_info("%s descq %s NOT online.\n",
			xdev->conf.name, descq->conf.name);
		rv = -EINVAL;
		goto unmap_sgl;
	}
	list_add_tail(&cb->list, &descq->work_list);
	unlock_descq(descq);

	pr_debug("%s: cb 0x%p submitted.\n", descq->conf.name, cb);

	qdma_kthread_wakeup(descq->wrkthp);

	if (!wait)
		return 0;

	rv = qdma_request_wait_for_cmpl(xdev, descq, req);
	if (rv < 0)
		goto unmap_sgl;

	return cb->offset;

unmap_sgl:
	if (!req->dma_mapped)
		sgl_unmap(xdev->conf.pdev,  req->sgl, req->sgcnt, dir);

	return rv;
}

int libqdma_init(void)
{
	if (sizeof(struct qdma_sgt_req_cb) > QDMA_REQ_OPAQUE_SIZE) {
		pr_info("ERR, dma req. opaque data size too big %lu > %d.\n",
			sizeof(struct qdma_sgt_req_cb), QDMA_REQ_OPAQUE_SIZE);
		return -1;
	}

	qdma_threads_create();
	return 0;
}

void libqdma_exit(void)
{
	qdma_threads_destroy();
}

#ifdef __LIBQDMA_MOD__
/* for module support only */
#include "version.h"

static char version[] =
	DRV_MODULE_DESC " " DRV_MODULE_NAME " v" DRV_MODULE_VERSION "\n";

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("GPL v2");

static int __init libqdma_mod_init(void)
{
	pr_info("%s", version);

	return libqdma_init();
}

static void __exit libqdma_mod_exit(void)
{
	libqdma_exit();
}

module_init(libqdma_mod_init);
module_exit(libqdma_mod_exit);
#endif /* ifdef __LIBQDMA_MOD__ */
