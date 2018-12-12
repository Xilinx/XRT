/*
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@Xilinx.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* QDMA stream */
#include <linux/version.h>
#include <linux/anon_inodes.h>
#include <linux/dma-buf.h>
#include <linux/aio.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include "../xocl_drv.h"
#include "../userpf/common.h"
#include "../userpf/xocl_bo.h"
#include "../lib/libqdma/libqdma_export.h"
#include "qdma_ioctl.h"

#define	PROC_TABLE_HASH_SZ	512
#define	EBUF_LEN		256
#define	MINOR_NAME_MASK		0xffffffff

#define	STREAM_FLOWID_MASK	0xff
#define	STREAM_SLRID_SHIFT	16
#define	STREAM_SLRID_MASK	0xff
#define	STREAM_TDEST_MASK	0xffff

#define	STREAM_DEFAULT_H2C_RINGSZ_IDX		0
#define	STREAM_DEFAULT_C2H_RINGSZ_IDX		5
#define	STREAM_DEFAULT_WRB_RINGSZ_IDX		5

#define	QUEUE_POST_TIMEOUT	10000

static dev_t	str_dev;

struct stream_async_req;

struct stream_async_arg {
	struct stream_queue	*queue;
	struct drm_xocl_unmgd	unmgd;
	u32			nsg;
	struct drm_xocl_bo	*xobj;
	bool			is_unmgd;
	struct kiocb		*kiocb;
	struct stream_async_req *io_req;
};

struct stream_async_req {
	struct list_head list;
	struct stream_async_arg cb;
	struct qdma_request req;
};

struct stream_queue {
	struct device		dev;
	unsigned long		queue;
	struct qdma_queue_conf  qconf;
	u32			state;
	int			flowid;
	int			routeid;
	struct file		*file;
	int			qfd;
	int			refcnt;
	struct str_device	*sdev;
	kuid_t			uid;

	spinlock_t		req_lock;
	struct list_head	req_pend_list;
	struct list_head	req_free_list;
	struct stream_async_req *req_cache;
};

struct str_device {
	struct platform_device  *pdev;
	struct cdev		cdev;
	struct device		*sys_device;
	u32			h2c_ringsz_idx;
	u32			c2h_ringsz_idx;
	u32			wrb_ringsz_idx;

	struct mutex		str_dev_lock;

	u16			instance;

	struct qdma_dev_conf	dev_info;
};

/* sysfs */
#define	__SHOW_MEMBER(P, M)		off += snprintf(buf + off, 32,		\
	"%s:%lld\n", #M, (int64_t)P->M)

static ssize_t qinfo_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct stream_queue *queue = dev_get_drvdata(dev);
	kuid_t uid;
	int off = 0;
	struct qdma_queue_conf *qconf;

	uid = current_uid();
	if (memcmp(&uid, &queue->uid, sizeof(uid)))
		return sprintf(buf, "Permission denied\n");

	qconf = &queue->qconf;
	__SHOW_MEMBER(qconf, pipe);
	__SHOW_MEMBER(qconf, irq_en);
	__SHOW_MEMBER(qconf, desc_rng_sz_idx);
	__SHOW_MEMBER(qconf, cmpl_status_en);
	__SHOW_MEMBER(qconf, cmpl_status_acc_en);
	__SHOW_MEMBER(qconf, cmpl_status_pend_chk);
	__SHOW_MEMBER(qconf, desc_bypass);
	__SHOW_MEMBER(qconf, pfetch_en);
	__SHOW_MEMBER(qconf, st_pkt_mode);
	__SHOW_MEMBER(qconf, c2h_use_fl);
	__SHOW_MEMBER(qconf, c2h_buf_sz_idx);
	__SHOW_MEMBER(qconf, cmpl_rng_sz_idx);
	__SHOW_MEMBER(qconf, cmpl_desc_sz);
	__SHOW_MEMBER(qconf, cmpl_stat_en);
	__SHOW_MEMBER(qconf, cmpl_udd_en);
	__SHOW_MEMBER(qconf, cmpl_timer_idx);
	__SHOW_MEMBER(qconf, cmpl_cnt_th_idx);
	__SHOW_MEMBER(qconf, cmpl_trig_mode);
	__SHOW_MEMBER(qconf, cmpl_en_intr);
	__SHOW_MEMBER(qconf, cdh_max);
	__SHOW_MEMBER(qconf, pipe_gl_max);
	__SHOW_MEMBER(qconf, pipe_flow_id);
	__SHOW_MEMBER(qconf, pipe_slr_id);
	__SHOW_MEMBER(qconf, pipe_tdest);
	__SHOW_MEMBER(qconf, quld);
	__SHOW_MEMBER(qconf, rngsz);
	__SHOW_MEMBER(qconf, rngsz_cmpt);
	__SHOW_MEMBER(qconf, c2h_bufsz);

	return off;
}
static DEVICE_ATTR_RO(qinfo);

static ssize_t stat_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct stream_queue *queue = dev_get_drvdata(dev);
	struct xocl_dev *xdev;
	kuid_t uid;
	int off = 0;
	struct qdma_queue_stats stat, *pstat;

	uid = current_uid();
	if (memcmp(&uid, &queue->uid, sizeof(uid)))
		return sprintf(buf, "Permission denied\n");

       	xdev = xocl_get_xdev(queue->sdev->pdev);
	if (qdma_queue_get_stats((unsigned long)xdev->dma_handle, queue->queue,
				&stat) < 0)
		return sprintf(buf, "Input invalid\n");

	pstat = &stat;

	__SHOW_MEMBER(pstat, total_req_bytes);
	__SHOW_MEMBER(pstat, total_req_num);
	__SHOW_MEMBER(pstat, total_complete_bytes);
	__SHOW_MEMBER(pstat, total_complete_num);

	__SHOW_MEMBER(pstat, descq_rngsz);
	__SHOW_MEMBER(pstat, descq_pidx);
	__SHOW_MEMBER(pstat, descq_cidx);
	__SHOW_MEMBER(pstat, descq_avail);

	return off;
}
static DEVICE_ATTR_RO(stat);

static struct attribute *stream_attributes[] = {
	&dev_attr_stat.attr,
	&dev_attr_qinfo.attr,
	NULL,
};

static const struct attribute_group stream_attrgroup = {
	.attrs = stream_attributes,
};

static void stream_sysfs_destroy(struct stream_queue *queue)
{
	struct platform_device	*pdev = queue->sdev->pdev;
	char			name[32];

	if (queue->qconf.c2h)
		snprintf(name, sizeof(name) - 1, "flow%d", queue->flowid);
	else
		snprintf(name, sizeof(name) - 1, "route%d", queue->routeid);

	if (get_device(&queue->dev)) {
		sysfs_remove_link(&pdev->dev.kobj, (const char *)name);
		sysfs_remove_group(&queue->dev.kobj, &stream_attrgroup);
		put_device(&queue->dev);
		device_unregister(&queue->dev);
	}

}

static void stream_device_release(struct device *dev)
{
	xocl_dbg(dev, "dummy device release callback");
}

static int stream_sysfs_create(struct stream_queue *queue)
{
	struct platform_device	*pdev = queue->sdev->pdev;
	char			name[32];
	int			ret;

#if 0
	queue->dev = device_create(NULL, &pdev->dev,
                0, queue, "%sq%d", queue->qconf.c2h ? "r" : "w",
                queue->qconf.qidx);
#endif

	queue->dev.parent = &pdev->dev;
	queue->dev.release = stream_device_release;
	dev_set_drvdata(&queue->dev, queue);
	dev_set_name(&queue->dev, "%sq%d",
		queue->qconf.c2h ? "r" : "w",
		queue->qconf.qidx);
	ret = device_register(&queue->dev);
	if (ret) {
		xocl_err(&pdev->dev, "device create failed");
		goto failed;
	}

	ret = sysfs_create_group(&queue->dev.kobj, &stream_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs group failed");
		goto failed;
	}

	if (queue->qconf.c2h)
		snprintf(name, sizeof(name) - 1, "flow%d", queue->flowid);
	else
		snprintf(name, sizeof(name) - 1, "route%d", queue->routeid);

	ret = sysfs_create_link(&pdev->dev.kobj, &queue->dev.kobj, (const char *)name);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs link failed");
		sysfs_remove_group(&queue->dev.kobj, &stream_attrgroup);
	}

	return 0;

failed:
	if (get_device(&queue->dev)) {
		put_device(&queue->dev);
		device_unregister(&queue->dev);
	}
	return ret;
}
/* end of sysfs */

static u64 get_str_stat(struct platform_device *pdev, u32 q_idx)
{
	struct str_device *sdev;

	sdev = platform_get_drvdata(pdev);
	BUG_ON(!sdev);

	return 0;
}

static struct xocl_str_dma_funcs str_ops = {
	.get_str_stat = get_str_stat,
};

static const struct vm_operations_struct stream_vm_ops = {
	.fault = xocl_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static struct stream_async_req *queue_req_new(struct stream_queue *queue)
{
	struct stream_async_req *io_req;

	spin_lock_bh(&queue->req_lock);
	if (list_empty(&queue->req_free_list)) {
		spin_unlock_bh(&queue->req_lock);
		return NULL;
	}

	io_req = list_first_entry(&queue->req_free_list,
				struct stream_async_req, list);
	list_del(&io_req->list);
	spin_unlock_bh(&queue->req_lock);

	memset(io_req, 0, sizeof(struct stream_async_req));
	return io_req;
}

static void queue_req_free(struct stream_queue *queue,
				struct stream_async_req *io_req)
{
	spin_lock_bh(&queue->req_lock);
        list_del(&io_req->list);
	list_add_tail(&io_req->list, &queue->req_free_list);
	spin_unlock_bh(&queue->req_lock);
}

static void queue_req_pending(struct stream_queue *queue,
				struct stream_async_req *io_req)
{
	spin_lock_bh(&queue->req_lock);
	list_add_tail(&io_req->list, &queue->req_pend_list);
	spin_unlock_bh(&queue->req_lock);
}

static int queue_req_complete(unsigned long priv, unsigned int done_bytes,
				int error)
{
	struct stream_async_arg *cb = (struct stream_async_arg *)priv;
	struct stream_async_req *io_req = cb->io_req;
	struct kiocb *kiocb = cb->kiocb;
	struct stream_queue *queue = cb->queue;

	if (cb->is_unmgd) {
		struct xocl_dev	*xdev = xocl_get_xdev(cb->queue->sdev->pdev);

		pci_unmap_sg(xdev->core.pdev, cb->unmgd.sgt->sgl, cb->nsg,
			cb->queue->qconf.c2h ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		xocl_finish_unmgd(&cb->unmgd);
	} else {
		drm_gem_object_unreference_unlocked(&cb->xobj->base);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
        kiocb->ki_complete(kiocb, done_bytes, error);
#else
	atomic_set(&kiocb->ki_users, 1);
	aio_complete(kiocb, done_bytes, error);
#endif

	queue_req_free(queue, io_req);

	return 0;
}

static ssize_t stream_post_bo(struct str_device *sdev,
	struct stream_queue *queue, struct drm_gem_object *gem_obj,
	loff_t offset, size_t len, bool write,
	struct xocl_qdma_req_header *header, struct kiocb *kiocb)
{
	struct drm_xocl_bo *xobj;
	struct xocl_dev *xdev;
	struct stream_async_req  *io_req = NULL;
	struct qdma_request *req;
	struct stream_async_arg *cb;
	ssize_t ret;

	xdev = xocl_get_xdev(sdev->pdev);
	if (gem_obj->size < offset + len) {
		xocl_err(&sdev->pdev->dev, "Invalid request, buf size: %ld, "
			"request size %ld, offset %lld",
			gem_obj->size, len, offset);
		ret = -EINVAL;
		goto failed;
	}

	drm_gem_object_reference(gem_obj);
	xobj = to_xocl_bo(gem_obj);

	io_req = queue_req_new(queue);
	if (!io_req) {
		xocl_err(&sdev->pdev->dev, "io request list full");
		ret = -ENOMEM;
		goto failed;
	}

	cb = &io_req->cb;
	cb->io_req = io_req;
	cb->queue = queue;

	req = &io_req->req;
	req->write = write;
	req->count = len;
	req->use_sgt = 1;
	req->sgt = xobj->sgt;
	if (header->flags & XOCL_QDMA_REQ_FLAG_EOT)
		req->eot = 1;
	req->uld_data = (unsigned long)cb;
	if (kiocb) {
		cb->is_unmgd = false;
		cb->kiocb = kiocb;
		cb->xobj = xobj;
		req->fp_done = queue_req_complete;

		kiocb->private = io_req;
	}
	queue_req_pending(queue, io_req);

	pr_debug("%s, %s req 0x%p hndl 0x%lx,0x%lx, sgl 0x%p,%u,%u, ST %s %lu.\n",
        	__func__, dev_name(&sdev->pdev->dev), req,
        	(unsigned long)xdev->dma_handle, queue->queue, req->sgt->sgl,
        	req->sgt->orig_nents, req->sgt->nents, write ? "W":"R", len);

	ret = qdma_request_submit((unsigned long)xdev->dma_handle, queue->queue,
				req);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev, "post wr failed ret=%ld", ret);
		queue_req_free(queue, io_req);
	}

failed:
	if (!kiocb) {
		drm_gem_object_unreference_unlocked(gem_obj);
	}

	return ret;
}

static ssize_t queue_rw(struct str_device *sdev, struct stream_queue *queue,
	char __user *buf, size_t sz, bool write, char __user *u_header,
	struct kiocb *kiocb)
{
	struct vm_area_struct	*vma;
	struct xocl_dev *xdev;
	struct drm_xocl_unmgd unmgd;
	unsigned long buf_addr = (unsigned long)buf;
	enum dma_data_direction dir;
	struct xocl_qdma_req_header header;
	u32 nents;
	struct stream_async_req  *io_req = NULL;
	struct qdma_request *req;
	struct stream_async_arg *cb;
	long	ret = 0;

	xocl_dbg(&sdev->pdev->dev, "Read / Write Queue 0x%lx",
		queue->queue);

	if (sz == 0)
		return 0;

	if (((uint64_t)(buf) & ~PAGE_MASK) && queue->qconf.c2h) {
		xocl_err(&sdev->pdev->dev,
			"C2H buffer has to be page aligned, buf %p", buf);
		return -EINVAL;
	}

	memset (&header, 0, sizeof (header));
	if (u_header &&  copy_from_user((void *)&header, u_header,
		sizeof (struct xocl_qdma_req_header))) {
		xocl_err(&sdev->pdev->dev, "copy header failed.");
		return -EFAULT;
	}

	if (!queue->qconf.c2h &&
		!(header.flags & XOCL_QDMA_REQ_FLAG_EOT) &&
		(sz & 0xfff)) {
		xocl_err(&sdev->pdev->dev,
			"H2C without EOT has to be multiple of 4k, sz 0x%lx",
			sz);
	}

	xdev = xocl_get_xdev(sdev->pdev);

	vma = find_vma(current->mm, buf_addr);
	if (vma && (vma->vm_ops == &stream_vm_ops)) {
		if (vma->vm_start > buf_addr || vma->vm_end <= buf_addr + sz) {
			return -EINVAL;
		}
		ret = stream_post_bo(sdev, queue, vma->vm_private_data,
			(buf_addr - vma->vm_start), sz, write, &header, kiocb);
		return ret;
	}

	ret = xocl_init_unmgd(&unmgd, (uint64_t)buf, sz, write);
	if (ret) {
		xocl_err(&sdev->pdev->dev, "Init unmgd buf failed, "
			"ret=%ld", ret);
		return ret;
	}

	dir = write ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
	nents = pci_map_sg(xdev->core.pdev, unmgd.sgt->sgl,
		unmgd.sgt->orig_nents, dir);
	if (!nents) {
		xocl_err(&sdev->pdev->dev, "map sgl failed");
		xocl_finish_unmgd(&unmgd);
		return -EFAULT;
	}

	io_req = queue_req_new(queue);
	if (!io_req) {
		xocl_err(&sdev->pdev->dev, "io request OOM");
		return -ENOMEM;
	}

	req = &io_req->req;
	cb = &io_req->cb;
	cb->io_req = io_req;
	cb->queue = queue;

	req->write = write;
	req->count = sz;
	req->use_sgt = 1;
	req->sgt = unmgd.sgt;
	if (header.flags & XOCL_QDMA_REQ_FLAG_EOT)
		req->eot = 1;
	if (kiocb) {
		memcpy(&cb->unmgd, &unmgd, sizeof (unmgd));
		cb->is_unmgd = true;
		cb->queue = queue;
		cb->kiocb = kiocb;
		cb->nsg = nents;
		req->uld_data = (unsigned long)cb;
		req->fp_done = queue_req_complete;

		kiocb->private = io_req;
	}
	queue_req_pending(queue, io_req);

	pr_debug("%s, %s req 0x%p hndl 0x%lx,0x%lx, sgl 0x%p,%u,%u, ST %s %lu.\n",
        	__func__, dev_name(&sdev->pdev->dev), req,
        	(unsigned long)xdev->dma_handle, queue->queue, req->sgt->sgl,
        	req->sgt->orig_nents, req->sgt->nents, write ? "W":"R", sz);

	ret = qdma_request_submit((unsigned long)xdev->dma_handle, queue->queue,
				req);
	if (ret < 0) {
		queue_req_free(queue, io_req);
		xocl_err(&sdev->pdev->dev, "post wr failed ret=%ld", ret);
	}

	if (!kiocb) {
		pci_unmap_sg(xdev->core.pdev, unmgd.sgt->sgl, nents, dir);
		xocl_finish_unmgd(&unmgd);
	}

	if (ret < 0)
		return ret;
	else if (kiocb)
		return -EIOCBQUEUED;
	else
		return ret;
}

static int queue_wqe_cancel(struct kiocb *kiocb)
{
	struct stream_async_req *io_req =
			(struct stream_async_req *)kiocb->private;
	struct stream_queue *queue = io_req->cb.queue;
	struct xocl_dev *xdev = xocl_get_xdev(queue->sdev->pdev);

	pr_debug("%s, %s cancel ST req 0x%p hndl 0x%lx,0x%lx, %s %u.\n",
		__func__, dev_name(&queue->sdev->pdev->dev),
		&io_req->req, (unsigned long)xdev->dma_handle, queue->queue,
		io_req->req.write ? "W":"R", io_req->req.count);

	return qdma_request_cancel((unsigned long)xdev->dma_handle,
				queue->queue, &io_req->req);
}

static ssize_t queue_aio_read(struct kiocb *kiocb, const struct iovec *iov,
	unsigned long nr, loff_t off)
{
	struct stream_queue	*queue;
	struct str_device	*sdev;

	queue = (struct stream_queue *)kiocb->ki_filp->private_data;
	sdev = queue->sdev;

	if (nr != 2) {
		xocl_err(&sdev->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (is_sync_kiocb(kiocb)) {
		return queue_rw(sdev, queue, iov[1].iov_base,
			iov[1].iov_len, false, iov[0].iov_base, NULL);
	}

	kiocb_set_cancel_fn(kiocb, (kiocb_cancel_fn *)queue_wqe_cancel);

	return queue_rw(sdev, queue, iov[1].iov_base, iov[1].iov_len,
		false, iov[0].iov_base, kiocb);
}

static ssize_t queue_aio_write(struct kiocb *kiocb, const struct iovec *iov,
	unsigned long nr, loff_t off)
{
	struct stream_queue	*queue;
	struct str_device	*sdev;

	queue = (struct stream_queue *)kiocb->ki_filp->private_data;
	sdev = queue->sdev;

	if (nr != 2) {
		xocl_err(&sdev->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (is_sync_kiocb(kiocb)) {
		return queue_rw(sdev, queue, iov[1].iov_base,
			iov[1].iov_len, true, iov[0].iov_base, NULL);
	}

	kiocb_set_cancel_fn(kiocb, (kiocb_cancel_fn *)queue_wqe_cancel);

	return queue_rw(sdev, queue, iov[1].iov_base, iov[1].iov_len,
		true, iov[0].iov_base, kiocb);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t queue_write_iter(struct kiocb *kiocb, struct iov_iter *io)
{
	struct stream_queue	*queue;
	struct str_device	*sdev;
	unsigned long		nr;

	queue = (struct stream_queue *)kiocb->ki_filp->private_data;
	sdev = queue->sdev;

	nr = io->nr_segs;
	if (!iter_is_iovec(io) || nr != 2) {
		xocl_err(&sdev->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}
		
	if (!is_sync_kiocb(kiocb)) {
		return queue_aio_write(kiocb, io->iov, nr, io->iov_offset);
	}

	return queue_rw(sdev, queue, io->iov[1].iov_base,
		io->iov[1].iov_len, true, io->iov[0].iov_base, NULL);
}

static ssize_t queue_read_iter(struct kiocb *kiocb, struct iov_iter *io)
{
	struct stream_queue	*queue;
	struct str_device	*sdev;
	unsigned long		nr;

	queue = (struct stream_queue *)kiocb->ki_filp->private_data;
	sdev = queue->sdev;

	nr = io->nr_segs;
	if (!iter_is_iovec(io) || nr != 2) {
		xocl_err(&sdev->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}
		
	if (!is_sync_kiocb(kiocb)) {
		return queue_aio_read(kiocb, io->iov, nr, io->iov_offset);
	}

	return queue_rw(sdev, queue, io->iov[1].iov_base,
		io->iov[1].iov_len, false, io->iov[0].iov_base, NULL);
}
#endif

static int queue_flush(struct file *file, fl_owner_t id)
{
	struct str_device *sdev;
	struct stream_queue *queue;
	struct xocl_dev *xdev;
	long	ret = 0;

	queue = (struct stream_queue *)file->private_data;
	if (!queue)
		return 0;

	sdev = queue->sdev;

	xdev = xocl_get_xdev(sdev->pdev);

	xocl_info(&sdev->pdev->dev, "Release Queue 0x%lx", queue->queue);

	if (queue->refcnt > 0) {
		xocl_err(&sdev->pdev->dev, "Queue is busy");
		return -EBUSY;
	}

	stream_sysfs_destroy(queue);

	ret = qdma_queue_stop((unsigned long)xdev->dma_handle, queue->queue,
			NULL, 0);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev,
			"Stop queue failed ret = %ld", ret);
		goto failed;
	}
	ret = qdma_queue_remove((unsigned long)xdev->dma_handle, queue->queue,
			NULL, 0);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev,
			"Destroy queue failed ret = %ld", ret);
		goto failed;
	}
	queue->queue = 0UL;

	spin_lock_bh(&queue->req_lock);
	while (!list_empty(&queue->req_pend_list)) {
		struct stream_async_req *io_req = list_first_entry(
							&queue->req_pend_list,
							struct stream_async_req,
							list);

		spin_unlock_bh(&queue->req_lock);
		xocl_info(&sdev->pdev->dev, "Queue 0x%lx, cancel req 0x%p",
			queue->queue, &io_req->req);
		queue_req_complete((unsigned long)&io_req->cb, 0, -ECANCELED);
		spin_lock_bh(&queue->req_lock);
	}

	if (queue->req_cache)
		vfree(queue->req_cache);
	devm_kfree(&sdev->pdev->dev, queue);
	file->private_data = NULL;

failed:
	return ret;
}

static struct file_operations queue_fops = {
	.owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	.write_iter = queue_write_iter,
	.read_iter = queue_read_iter,
#else
	.aio_read = queue_aio_read,
	.aio_write = queue_aio_write,
#endif
	.flush = queue_flush,
};

static long stream_ioctl_create_queue(struct str_device *sdev,
	void __user *arg)
{
	struct xocl_qdma_ioc_create_queue req;
	struct qdma_queue_conf *qconf;
	struct xocl_dev *xdev;
	struct stream_queue *queue;
	long	ret;

	if (copy_from_user((void *)&req, arg,
		sizeof (struct xocl_qdma_ioc_create_queue))) {
		xocl_err(&sdev->pdev->dev, "copy failed.");
		return -EFAULT;
	}

	queue = devm_kzalloc(&sdev->pdev->dev, sizeof (*queue), GFP_KERNEL);
	if (!queue) {
		xocl_err(&sdev->pdev->dev, "out of memeory");
		return -ENOMEM;
	}
	queue->qfd = -1;
	INIT_LIST_HEAD(&queue->req_pend_list);
	INIT_LIST_HEAD(&queue->req_free_list);
	spin_lock_init(&queue->req_lock);

	xdev = xocl_get_xdev(sdev->pdev);

	qconf = &queue->qconf;
	qconf->st = 1; /* stream queue */
	qconf->qidx = QDMA_QUEUE_IDX_INVALID; /* request libqdma to alloc */
	qconf->cmpl_status_en =1;
        qconf->cmpl_status_acc_en=1;
        qconf->cmpl_status_pend_chk=1;
        qconf->fetch_credit=1;
        qconf->cmpl_stat_en=1;
        qconf->cmpl_trig_mode=1;
	qconf->irq_en = (req.flags & XOCL_QDMA_QUEUE_FLAG_POLLING) ? 0 : 1;

	if (!req.write) {
		/* C2H */
		qconf->pipe_flow_id = req.flowid & STREAM_FLOWID_MASK;
		qconf->pipe_slr_id = (req.rid >> STREAM_SLRID_SHIFT) &
			STREAM_SLRID_MASK;
		qconf->pipe_tdest = req.rid & STREAM_TDEST_MASK;
		qconf->c2h = 1;
		qconf->desc_rng_sz_idx = sdev->c2h_ringsz_idx;
		qconf->cmpl_rng_sz_idx = sdev->wrb_ringsz_idx;
	} else {
		/* H2C */
		qconf->desc_bypass = 1;
		qconf->pipe_flow_id = req.flowid & STREAM_FLOWID_MASK;
		qconf->pipe_slr_id = (req.rid >> STREAM_SLRID_SHIFT) &
			STREAM_SLRID_MASK;
		qconf->pipe_tdest = req.rid & STREAM_TDEST_MASK;
		qconf->pipe_gl_max = 1;
		qconf->desc_rng_sz_idx = sdev->h2c_ringsz_idx;
	}
	queue->flowid = req.flowid;
	queue->routeid = req.rid;
	xocl_info(&sdev->pdev->dev, "Creating queue with tdest %d, flow %d, "
		"slr %d", qconf->pipe_tdest, qconf->pipe_flow_id,
		qconf->pipe_slr_id);

	ret = qdma_queue_add((unsigned long)xdev->dma_handle, qconf,
			&queue->queue, NULL, 0);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev, "Adding Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = qdma_queue_start((unsigned long)xdev->dma_handle, queue->queue,
			NULL, 0);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev, "Starting Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = qdma_queue_prog_stm((unsigned long)xdev->dma_handle, queue->queue,
			NULL, 0);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev, "STM prog. Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = qdma_queue_get_config((unsigned long)xdev->dma_handle,
				queue->queue, qconf, NULL, 0);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev, "Get Q conf. failed ret = %ld", ret);
		goto failed;
	}

	/* pre-allocate 2x io request struct */
	queue->req_cache = vzalloc((qconf->rngsz << 1) *
					sizeof(struct stream_async_req));
	if (!queue->req_cache) {
		xocl_err(&sdev->pdev->dev, "req. cache OOM %u", qconf->rngsz); 
		goto failed;
	} else {
		int i;
		struct stream_async_req *io_req = queue->req_cache;
		unsigned int max = qconf->rngsz << 1;

		for (i = 0; i < max; i++, io_req++)
			list_add_tail(&io_req->list, &queue->req_free_list);
	}

	xocl_info(&sdev->pdev->dev, "Created Queue handle 0x%lx, idx %d, sz %d",
		queue->queue, queue->qconf.qidx, queue->qconf.rngsz);

	queue->file = anon_inode_getfile("qdma_queue", &queue_fops, queue,
		O_CLOEXEC | O_RDWR);
	if (!queue->file) {
		ret = -EFAULT;
		goto failed;
	}
	queue->file->private_data = queue;
	queue->qfd = get_unused_fd_flags(0); /* e.g. O_NONBLOCK */
	if (queue->qfd < 0) {
		ret = -EFAULT;
		xocl_err(&sdev->pdev->dev, "Failed get fd");
		goto failed;
	}
	fd_install(queue->qfd, queue->file);
	req.handle = queue->qfd;

	if (copy_to_user(arg, &req, sizeof (req))) {
		xocl_err(&sdev->pdev->dev, "Copy to user failed");
		ret = -EFAULT;
		goto failed;
	}

	queue->sdev = sdev;

	ret = stream_sysfs_create(queue);
	if (ret) {
		xocl_err(&sdev->pdev->dev, "sysfs create failed");
		goto failed;
	}

	queue->uid = current_uid();

	return 0;

failed:
	if (queue->qfd >= 0)
		put_unused_fd(queue->qfd);

	if (queue->file) {
		fput(queue->file);
		queue->file = NULL;
	}

	if (queue) {
		if (queue->req_cache)
			vfree(queue->req_cache);
		devm_kfree(&sdev->pdev->dev, queue);
	}

	ret = qdma_queue_stop((unsigned long)xdev->dma_handle, queue->queue,
			NULL, 0);
	ret = qdma_queue_remove((unsigned long)xdev->dma_handle, queue->queue,
			NULL, 0);
	queue->queue = 0UL;

	return ret;
}

static long stream_ioctl_alloc_buffer(struct str_device *sdev,
	void __user *arg)
{
	struct xocl_qdma_ioc_alloc_buf req;
	struct xocl_dev *xdev;
	struct drm_xocl_bo *xobj;
	struct dma_buf *dmabuf = NULL;
	int flags;
	int ret;

	if (copy_from_user((void *)&req, arg,
		sizeof (struct xocl_qdma_ioc_alloc_buf))) {
		xocl_err(&sdev->pdev->dev, "copy failed.");
		return -EFAULT;
	}

	xdev = xocl_get_xdev(sdev->pdev);

	xobj = xocl_create_bo(xdev->ddev, req.size, 0, DRM_XOCL_BO_EXECBUF);
	if (IS_ERR(xobj)) {
		ret = PTR_ERR(xobj);
		xocl_err(&sdev->pdev->dev, "create bo failed");
		return ret;
	}

	xobj->pages = drm_gem_get_pages(&xobj->base);
	if (IS_ERR(xobj->pages)) {
		ret = PTR_ERR(xobj->pages);
		xocl_err(&sdev->pdev->dev, "Get pages failed");
		goto failed;
	}

	xobj->sgt = drm_prime_pages_to_sg(xobj->pages,
		xobj->base.size >> PAGE_SHIFT);
	if (IS_ERR(xobj->sgt)) {
		ret = PTR_ERR(xobj->sgt);
		goto failed;
	}

	xobj->vmapping = vmap(xobj->pages, xobj->base.size >> PAGE_SHIFT,
		VM_MAP, PAGE_KERNEL);
	if (!xobj->vmapping) {
		ret = -ENOMEM;
		goto failed;
	}

	xobj->dma_nsg = pci_map_sg(xdev->core.pdev, xobj->sgt->sgl,
		xobj->sgt->orig_nents, PCI_DMA_BIDIRECTIONAL);
	if (!xobj->dma_nsg) {
		xocl_err(&sdev->pdev->dev, "map sgl failed, sgt");
		ret = -EIO;
		goto failed;
	}

	ret = drm_gem_create_mmap_offset(&xobj->base);
	if (ret < 0)
		goto failed;

	flags = O_CLOEXEC | O_RDWR;

	drm_gem_object_reference(&xobj->base);
	dmabuf = drm_gem_prime_export(xdev->ddev, &xobj->base, flags);
	if (IS_ERR(dmabuf)) {
		xocl_err(&sdev->pdev->dev, "failed to export dma_buf");
		ret = PTR_ERR(dmabuf);
		goto failed;
	}
	xobj->dmabuf = dmabuf;
	xobj->dmabuf_vm_ops = &stream_vm_ops;

	req.buf_fd = dma_buf_fd(dmabuf, flags);
	if (req.buf_fd < 0) {
		goto failed;
	}

	if (copy_to_user(arg, &req, sizeof (req))) {
		xocl_err(&sdev->pdev->dev, "Copy to user failed");
		ret = -EFAULT;
		goto failed;
	}

	return 0;
failed:
	if (req.buf_fd >= 0) {
		put_unused_fd(req.buf_fd);
	}

	if (!IS_ERR(dmabuf)) {
		dma_buf_put(dmabuf);
	}

	if (xobj) {
		xocl_free_bo(&xobj->base);
	}

	return ret;
}

static long stream_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct str_device	*sdev;
	long result = 0;

	sdev = (struct str_device *)filp->private_data;

	switch (cmd) {
	case XOCL_QDMA_IOC_CREATE_QUEUE:
		result = stream_ioctl_create_queue(sdev, (void __user *)arg);
		break;
	case XOCL_QDMA_IOC_ALLOC_BUFFER:
		result = stream_ioctl_alloc_buffer(sdev, (void __user *)arg);
		break;
	default:
		xocl_err(&sdev->pdev->dev, "Invalid request %u", cmd & 0xff);
		result = -EINVAL;
		break;
	}

	return result;
}

static int stream_open(struct inode *inode, struct file *file)
{
	struct str_device *sdev;

	sdev = container_of(inode->i_cdev, struct str_device, cdev);

	file->private_data = sdev;

	xocl_info(&sdev->pdev->dev, "opened file %p by pid: %d",
		file, pid_nr(task_tgid(current)));

	return 0;
}

static int stream_close(struct inode *inode, struct file *file)
{
	struct str_device *sdev;
	struct xocl_dev *xdev;

	sdev = (struct str_device *)file->private_data;

	xdev = xocl_get_xdev(sdev->pdev);


	xocl_info(&sdev->pdev->dev, "Closing file %p by pid: %d",
		file, pid_nr(task_tgid(current)));

	return 0;
}

/*
 * char device for QDMA
 */
static const struct file_operations stream_fops = {
	.owner = THIS_MODULE,
	.open = stream_open,
	.release = stream_close,
	.unlocked_ioctl = stream_ioctl,
};

static int str_dma_probe(struct platform_device *pdev)
{
	struct str_device	*sdev = NULL;
	struct xocl_dev		*xdev;
	char			ebuf[EBUF_LEN + 1];
	int			ret = 0;

	sdev = devm_kzalloc(&pdev->dev, sizeof (*sdev), GFP_KERNEL);
	if (!sdev) {
		xocl_err(&pdev->dev, "alloc stream dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	sdev->pdev = pdev;
	xdev = xocl_get_xdev(pdev);

	ret = qdma_device_get_config((unsigned long)xdev->dma_handle,
		&sdev->dev_info, ebuf, EBUF_LEN);
	if (ret) {
		xocl_err(&pdev->dev, "Failed to get device info");
		goto failed;
	}

	cdev_init(&sdev->cdev, &stream_fops);
	sdev->cdev.owner = THIS_MODULE;
	sdev->instance = XOCL_DEV_ID(xdev->core.pdev);
	sdev->cdev.dev = MKDEV(MAJOR(str_dev), sdev->instance);
	ret = cdev_add(&sdev->cdev, sdev->cdev.dev, 1);
	if (ret) {
		xocl_err(&pdev->dev, "failed cdev_add, ret=%d", ret);
		goto failed;
	}

	sdev->sys_device = device_create(xrt_class, &pdev->dev,
		sdev->cdev.dev, NULL, "%s%d",
		platform_get_device_id(pdev)->name,
		sdev->instance & MINOR_NAME_MASK);
	if (IS_ERR(sdev->sys_device)) {
		ret = PTR_ERR(sdev->sys_device);
		xocl_err(&pdev->dev, "failed to create cdev");
		goto failed_create_cdev;
	}

	sdev->h2c_ringsz_idx = STREAM_DEFAULT_H2C_RINGSZ_IDX;
	sdev->c2h_ringsz_idx = STREAM_DEFAULT_C2H_RINGSZ_IDX;
	sdev->wrb_ringsz_idx = STREAM_DEFAULT_WRB_RINGSZ_IDX;

	mutex_init(&sdev->str_dev_lock);

	xocl_subdev_register(pdev, XOCL_SUBDEV_STR_DMA, &str_ops);
	platform_set_drvdata(pdev, sdev);

	return 0;

failed_create_cdev:
	cdev_del(&sdev->cdev);
failed:
	if (sdev) {
		devm_kfree(&pdev->dev, sdev);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int str_dma_remove(struct platform_device *pdev)
{
	struct str_device *sdev = platform_get_drvdata(pdev);

	if (!sdev) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	if (sdev->sys_device)
		device_destroy(xrt_class, sdev->cdev.dev);
	devm_kfree(&pdev->dev, sdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_device_id str_dma_id_table[] = {
	{ XOCL_STR_QDMA, 0 },
	{ },
};

static struct platform_driver	str_dma_driver = {
	.probe		= str_dma_probe,
	.remove		= str_dma_remove,
	.driver		= {
		.name = "xocl_str_qdma",
	},
	.id_table = str_dma_id_table,
};

int __init xocl_init_str_qdma(void)
{
	int		err = 0;

	err = alloc_chrdev_region(&str_dev, 0, XOCL_CHARDEV_REG_COUNT,
		XOCL_STR_QDMA);
	if (err < 0)
		goto err_reg_chrdev;

	err = platform_driver_register(&str_dma_driver);
	if (err)
		goto err_drv_reg;

	return 0;

err_drv_reg:
	unregister_chrdev_region(str_dev, XOCL_CHARDEV_REG_COUNT);
err_reg_chrdev:
	return err;
}

void xocl_fini_str_qdma(void)
{
	unregister_chrdev_region(str_dev, XOCL_CHARDEV_REG_COUNT);
	platform_driver_unregister(&str_dma_driver);
}
