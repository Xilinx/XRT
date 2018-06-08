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

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include "libqdma_export.h"

#include "qdma_descq.h"
#include "qdma_device.h"
#include "qdma_thread.h"
#include "qdma_regs.h"
#include "qdma_context.h"
#include "thread.h"
#include "version.h"

/* ********************* static function definitions ************************ */

static ssize_t qdma_sg_req_submit_st_c2h(struct xlnx_dma_dev *xdev,
			struct qdma_descq *descq, struct qdma_sg_req *req)
{
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct st_rx_queue *rxq = &descq->rx_queue;
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	struct sg_table *sgt = &req->sgt;
	unsigned int avail;
	int wait = req->fp_done ? 0 : 1;
	int rv = 0;

	spin_lock(&rxq->lock);
	avail = rxq->dlen;
	spin_unlock(&rxq->lock);

	if (avail >= req->count) {
		cb->offset = req->count;
		goto copy_data;
	} else
		cb->offset = req->count - avail;

	lock_descq(descq);
	if (descq->online) {
		list_add_tail(&cb->list, &descq->pend_list);
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

	if (req->timeout_ms)
		wait_event_interruptible_timeout(cb->wq, cb->done,
			msecs_to_jiffies(req->timeout_ms));
	else
		wait_event_interruptible(cb->wq, cb->done);

	if (!cb->done) { /* timed out */
		char* buf = kmalloc(2048, GFP_KERNEL);
		pr_info("%s: cb 0x%p, req 0x%x, timed out.\n",
			descq->conf.name, cb, req->count);
		qdma_queue_dump((unsigned long)xdev,
				descq->conf.qidx + qdev->qmax, buf, 2048);
		pr_info("%s", buf);
		kfree(buf);
		lock_descq(descq);
		list_del(&cb->list);
		unlock_descq(descq);
	}

	if (!cb->done || cb->status) {
		pr_info("%s: 0x%p, %u, tm %u, off %u, err 0x%x, cmpl %d.\n",
			descq->conf.name, req, req->count, req->timeout_ms,
			cb->offset, cb->status, cb->done);
	}

copy_data:
	pr_debug("%s: cb 0x%p, req 0x%x, copy data, %u, ...\n",
		descq->conf.name, cb, req->count, rxq->dlen);
	/* copy data from rx queue */
    rv = qdma_descq_rxq_read(descq, sgt, req->count);
	if (rv < 0)
		return rv;

	return rv;
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
	int len;
	int rv;

	if (!descq)
		return -EINVAL;

	/* TODO assume buflen is sufficient */
	if (!buf || !buflen)
		return -EINVAL;

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
			"\tSW CTXT:    0x%08x 0x%08x 0x%08x 0x%08x\n",
			ctxt.sw[3], ctxt.sw[2], ctxt.sw[1], ctxt.sw[0]);

	len += sprintf(buf + len,
			"\tHW CTXT:    0x%08x 0x%08x\n",
			ctxt.hw[1], ctxt.hw[0]);

	len += sprintf(buf + len,
			"\tCR CTXT:    0x%08x\n", ctxt.cr[0]);

	if (descq->conf.c2h && descq->conf.st) {
		len += sprintf(buf + len,
			"\tWRB CTXT:   0x%08x 0x%08x 0x%08x 0x%08x\n",
			ctxt.wrb[3], ctxt.wrb[2], ctxt.wrb[1], ctxt.wrb[0]);

		len += sprintf(buf + len,
			"\tPFTCH CTXT: 0x%08x 0x%08x\n",
			ctxt.prefetch[1], ctxt.prefetch[0]);
	}

	buf[len] = '\0';

	return 0;
}

int qdma_queue_dump_desc(unsigned long dev_hndl, unsigned long id,
			unsigned int start, unsigned int end, char *buf,
			int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					id, buf, buflen, 1);
	int len = 0;

	if (!descq)
		return -EINVAL;

	len = qdma_descq_dump_state(descq, buf);
	if (!descq->inited)
		return len;

	return qdma_descq_dump_desc(descq, start, end, buf + len, buflen - len);
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
		return -EINVAL;

	len = qdma_descq_dump_state(descq, buf);
	if (!descq->inited)
		return len;

	return qdma_descq_dump_wrb(descq, start, end, buf + len, buflen - len);
}

int qdma_queue_remove(unsigned long dev_hndl, unsigned long id, char *buf,
			int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_descq *descq = qdma_device_get_descq_by_id(xdev, id, buf, buflen, 1);
	struct qdma_dev *qdev = xdev_2_qdev(xdev);

	if (!descq)
		return -EINVAL;

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
	return 0;
}

int qdma_queue_config(unsigned long dev_hndl, unsigned long qid,
			struct qdma_queue_conf *qconf, char *buf, int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct qdma_descq *descq = NULL;

	descq = qdma_device_get_descq_by_id(xdev, qid, NULL, 0, 0);
	if (!descq) {
		pr_err("Invalid queue ID! qid=%lu, max=%u\n", qid, qdev->qmax);
		return -EINVAL;
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

	return 0;
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
	return 0;
}

int qdma_queue_add(unsigned long dev_hndl, struct qdma_queue_conf *qconf,
			unsigned long *qhndl, char *buf, int buflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_dev *qdev = xdev_2_qdev(xdev);
	struct qdma_descq *descq = qconf->c2h ?
				qdev->c2h_descq : qdev->h2c_descq;
	unsigned int qcnt;
	int len = 0;
	int i;
	int rv = 0;

	rv = qdma_device_prep_q_resource(xdev);		
	if (rv < 0)
		return rv;

	if (!qhndl) {
		pr_info("qhndl NULL.\n");
		if (buf) {
			len += sprintf(buf + len,
				"%s, add, qhndl NULL.\n", xdev->conf.name);
			buf[len] = '\0';
		}
		spin_unlock(&qdev->lock);
		return -EINVAL;
	}

	/* reset qhandle to an invalid value
	 * cant use 0 or NULL here because queue idx 0 has same value */
	*qhndl = QDMA_QUEUE_IDX_INVALID;

	spin_lock(&qdev->lock);
	qcnt = qconf->c2h ? qdev->c2h_qcnt : qdev->h2c_qcnt;

	if (qcnt >= qdev->qmax) {
		pr_info("No free descq %u/%u.\n", qcnt, qdev->qmax);
		if (buf) {
			len += sprintf(buf + len,
				"qdma%d No free descq %u/%u.\n",
				xdev->conf.idx, qcnt, qdev->qmax);
			buf[len] = '\0';
		}
		spin_unlock(&qdev->lock);
		return -EAGAIN;
	}

	if ((qconf->qidx != QDMA_QUEUE_IDX_INVALID) &&
		(qconf->qidx >= qdev->qmax)) {
		pr_info("invalid descq qidx %u/%u.\n", qconf->qidx, qdev->qmax);
		if (buf) {
			len += sprintf(buf + len,
				"qdma%d invalid idx %u >= %u.\n",
				xdev->conf.idx, qconf->qidx, qdev->qmax);
			buf[len] = '\0';
		}
		spin_unlock(&qdev->lock);
		return -EINVAL;
	}

	if (qconf->c2h)
		qdev->c2h_qcnt++;
	else
		qdev->h2c_qcnt++;
	spin_unlock(&qdev->lock);

	if (qconf->qidx == QDMA_QUEUE_IDX_INVALID) {
		for (i = 0; i < qdev->qmax; i++, descq++) {
			lock_descq(descq);
			if (descq->enabled) {
				unlock_descq(descq);
				continue;
			}

			descq->enabled = 1;
			unlock_descq(descq);
			break;
		}

		if (i == qdev->qmax) {
			pr_info("No free descq, full %u.\n", qdev->qmax);
			if (buf) {
				len += sprintf(buf + len,
					"qdma%d No free descq, full %u.\n",
					xdev->conf.idx, qdev->qmax);
				buf[len] = '\0';
			}
			rv = -EAGAIN;
			goto err_out;
		}
	} else {
		descq += qconf->qidx;
		lock_descq(descq);
		if (descq->enabled) {
			pr_info("descq idx %u already added.\n", qconf->qidx);
			if (buf) {
				len += sprintf(buf + len,
						"q idx %u already added.\n",
						qconf->qidx);
				buf[len] = '\0';
			}
			unlock_descq(descq);
			rv = -EINVAL;
			goto err_out;
		}
		descq->enabled = 1;
		unlock_descq(descq);
	}

	/* fill in config. info */
	qdma_descq_config(descq, qconf, 0);

	memcpy(qconf, &descq->conf, sizeof(*qconf));
	*qhndl = (unsigned long)descq->conf.qidx;
	if (qconf->c2h)
		*qhndl += qdev->qmax;

	if (buf && len < buflen) {
		len += sprintf(buf + len,
			"queue added: idx %u, name %s.\n",
			qconf->qidx, descq->conf.name);
		buf[len] = '\0';
	}

	return 0;

err_out:
	spin_lock(&qdev->lock);
	if (qconf->c2h)
		qdev->c2h_qcnt--;
	else
		qdev->h2c_qcnt--;
	spin_unlock(&qdev->lock);

	return rv;
}

int qdma_queue_start(unsigned long dev_hndl, unsigned long id, char *buf,
            int buflen)
{
	struct qdma_descq *descq = qdma_device_get_descq_by_id(
					(struct xlnx_dma_dev *)dev_hndl,
					 id, buf, buflen, 1);
	int rv;

	if (!descq)
		return -EINVAL;

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
		return -EINVAL;
	}

	if (!descq->inited) {
		rv = qdma_descq_alloc_resource(descq);
		if (rv < 0)
			goto err_out;
		descq->inited = 1;

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
	if (buf && buflen) {
		rv = sprintf(buf, "queue %s, idx %u started.\n",
			descq->conf.name, descq->conf.qidx);
		buf[rv] = '\0';
	}

	return 0;

err_out:
	qdma_descq_context_clear(descq->xdev, descq->qidx_hw, descq->conf.st,
				descq->conf.c2h);
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
		return -EINVAL;

	qdma_thread_remove_work(descq);

	qdma_descq_context_clear(descq->xdev, descq->qidx_hw, descq->conf.st,
				descq->conf.c2h);

	qdma_descq_free_resource(descq);

	lock_descq(descq);
	descq->online = 0;
	descq->inited = 0;
	unlock_descq(descq);

	if (buf && buflen) {
		int len = sprintf(buf, "queue %s, idx %u stopped.\n",
			descq->conf.name, descq->conf.qidx);
		buf[len] = '\0';
	}
	return 0;
}

ssize_t qdma_sg_req_submit(unsigned long dev_hndl, unsigned long id,
			struct qdma_sg_req *req)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_descq *descq = qdma_device_get_descq_by_id(xdev, id, NULL, 0, 1);
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	struct sg_table *sgt = &req->sgt;
	int wait = req->fp_done ? 0 : 1;

	if (!descq)
		return -EINVAL;

	if ((req->write && descq->conf.c2h) ||
	    (!req->write && !descq->conf.c2h)) {
		pr_info("%s: bad direction, %c.\n",
			descq->conf.name, req->write ? 'W' : 'R');
		return -EINVAL;
	}

	memset(cb, 0, QDMA_REQ_OPAQUE_SIZE);
	init_waitqueue_head(&cb->wq);

	if (descq->conf.st && descq->conf.c2h)
		return qdma_sg_req_submit_st_c2h(xdev, descq, req);

	if (!req->dma_mapped) {
		int rv = pci_map_sg(xdev->conf.pdev, sgt->sgl, sgt->orig_nents,
				descq->conf.c2h ? DMA_FROM_DEVICE :
						DMA_TO_DEVICE);
		if (!rv) {
			pr_info("%s map sgl failed, sgt 0x%p, %u.\n",
				descq->conf.name, sgt, sgt->orig_nents);
			return -EIO;
		}
		sgt->nents = rv;
	}

	lock_descq(descq);
	if (!descq->online) {
		unlock_descq(descq);
		pr_info("%s descq %s NOT online.\n",
			xdev->conf.name, descq->conf.name);
		return -EINVAL;
	}
	list_add_tail(&cb->list, &descq->work_list);
	unlock_descq(descq);

	pr_debug("%s: cb 0x%p submitted.\n", descq->conf.name, cb);

	qdma_kthread_wakeup(descq->wrkthp);

	if (!wait)
		return 0;

	if (req->timeout_ms)
		wait_event_interruptible_timeout(cb->wq, cb->done,
			msecs_to_jiffies(req->timeout_ms));
	else
		wait_event_interruptible(cb->wq, cb->done);

	if (!cb->done) {
		/* timed out */
		lock_descq(descq);
		list_del(&cb->list);
		unlock_descq(descq);
	}

	if (!req->dma_mapped && sgt->nents) {
		pci_unmap_sg(xdev->conf.pdev, sgt->sgl, sgt->orig_nents,
				descq->conf.c2h ? DMA_FROM_DEVICE :
						DMA_TO_DEVICE);
	}

	if (!cb->done || cb->status || !cb->offset) {
		pr_info("%s: %c,%u,0x%llx, tm %u, off %u, err 0x%x, cmpl %d.\n",
			descq->conf.name, req->write ? 'W':'R', req->count,
			req->ep_addr, req->timeout_ms, cb->offset, cb->status,
			cb->done);
		return -EIO;
	}

	return cb->offset;
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
MODULE_LICENSE("Dual GPL/BSD");

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
