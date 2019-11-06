/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@Xilinx.com
 *          Jan Stephan <j.stephan@hzdr.de>
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

#include <linux/version.h>
#include <linux/eventfd.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <linux/anon_inodes.h>
#include <linux/dma-buf.h>
#include <linux/aio.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include "../xocl_drv.h"
#include "../xocl_drm.h"
#include "../lib/libqdma/libqdma_export.h"
#include "qdma_ioctl.h"

#define XOCL_FILE_PAGE_OFFSET   0x100000
#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define	MM_QUEUE_LEN		8
#define	MM_EBUF_LEN		256

#define MM_DEFAULT_RINGSZ_IDX	0


/* streaming defines */
#define	MINOR_NAME_MASK		0xffffffff

#define	STREAM_FLOWID_MASK	0xff
#define	STREAM_SLRID_SHIFT	16
#define	STREAM_SLRID_MASK	0xff
#define	STREAM_TDEST_MASK	0xffff

#define	STREAM_DEFAULT_H2C_RINGSZ_IDX		0
#define	STREAM_DEFAULT_C2H_RINGSZ_IDX		0
#define	STREAM_DEFAULT_WRB_RINGSZ_IDX		0

#define	QUEUE_POST_TIMEOUT	10000
#define QDMA_MAX_INTR		16
#define QDMA_USER_INTR_MASK	0xfe

#define QDMA_QSETS_MAX		256

static dev_t	str_dev;

struct qdma_stream_async_req;

struct qdma_irq {
	struct eventfd_ctx	*event_ctx;
	bool			in_use;
	bool			enabled;
	irq_handler_t		handler;
	void			*arg;
};

struct qdma_stream_async_arg {
	struct qdma_stream_queue	*queue;
	struct drm_xocl_unmgd	unmgd;
	u32			nsg;
	struct drm_xocl_bo	*xobj;
	bool			is_unmgd;
	bool			cancel;
	struct kiocb		*kiocb;
	struct qdma_stream_async_req *io_req;
	spinlock_t		lock;
	struct work_struct	work;
};

struct qdma_stream_async_req {
	struct list_head list;
	struct qdma_stream_async_arg cb;
	struct qdma_request req;
};

enum {
	QUEUE_STATE_INITIALIZED,
	QUEUE_STATE_CLEANUP,
};

struct qdma_stream_queue {
	struct device		dev;
	struct xocl_qdma	*qdma;
	unsigned long		queue;
	struct qdma_queue_conf  qconf;
	u32			state;
	int			flowid;
	int			routeid;
	struct file		*file;
	int			qfd;
	kuid_t			uid;
	spinlock_t		req_lock;
	struct list_head	req_pend_list;
	struct list_head	req_free_list;
	struct qdma_stream_async_req *req_cache;
	/* stats */
	unsigned int 		req_pend_cnt;
	unsigned int 		req_free_cnt;
	unsigned int 		req_submit_cnt;
	unsigned int 		req_cmpl_cnt;
	unsigned int 		req_cancel_cnt;
	unsigned int 		req_cancel_cmpl_cnt;
};

struct xocl_qdma {
	void			*dma_handle;

	struct qdma_dev_conf	dev_conf;

	struct platform_device	*pdev;
	/* Number of bidirectional channels */
	u32			channel;
	/* Semaphore, one for each direction */
	struct semaphore	channel_sem[2];
	/*
	 * Channel usage bitmasks, one for each direction
	 * bit 1 indicates channel is free, bit 0 indicates channel is free
	 */
	volatile unsigned long	channel_bitmap[2];

	struct mm_channel	*chans[2];

	/* streaming */
	u32			h2c_ringsz_idx;
	u32			c2h_ringsz_idx;
	u32			wrb_ringsz_idx;

	u16			instance;

	struct qdma_irq		user_msix_table[QDMA_MAX_INTR];
	u32			user_msix_mask;
	spinlock_t		user_msix_table_lock;

	struct qdma_stream_queue	*queues[QDMA_QSETS_MAX * 2];
};

struct mm_channel {
	struct device		dev;
	struct xocl_qdma	*qdma;
	unsigned long		queue;
	struct qdma_queue_conf	qconf;
	uint64_t		total_trans_bytes;
};

static u32 get_channel_count(struct platform_device *pdev);
static u64 get_channel_stat(struct platform_device *pdev, u32 channel,
	u32 write);

static void dump_sgtable(struct device *dev, struct sg_table *sgt)
{
	int i;
	struct page *pg;
	struct scatterlist *sg = sgt->sgl;
	unsigned long long pgaddr;
	int nents = sgt->orig_nents;

	for (i = 0; i < nents; i++, sg = sg_next(sg)) {
		if (!sg)
			break;
		pg = sg_page(sg);
		if (!pg)
			continue;
		pgaddr = page_to_phys(pg);
		xocl_err(dev, "%i, 0x%llx, offset %d, len %d\n",
			i, pgaddr, sg->offset, sg->length);
	}
}

/* sysfs */
#define	__SHOW_MEMBER(P, M)		off += snprintf(buf + off, 64,		\
	"%s:%lld\n", #M, (int64_t)P->M)

static ssize_t qinfo_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct mm_channel *channel = dev_get_drvdata(dev);
	int off = 0;
	struct qdma_queue_conf *qconf;

	qconf = &channel->qconf;
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
	struct mm_channel *channel = dev_get_drvdata(dev);
	int off = 0;
	struct qdma_queue_stats stat, *pstat;

	if (qdma_queue_get_stats((unsigned long)channel->qdma->dma_handle,
				channel->queue, &stat) < 0)
		return sprintf(buf, "Input invalid\n");

	pstat = &stat;

	__SHOW_MEMBER(pstat, pending_bytes);
	__SHOW_MEMBER(pstat, pending_requests);
	__SHOW_MEMBER(pstat, complete_bytes);
	__SHOW_MEMBER(pstat, complete_requests);

	return off;
}
static DEVICE_ATTR_RO(stat);

static struct attribute *queue_attributes[] = {
	&dev_attr_stat.attr,
	&dev_attr_qinfo.attr,
	NULL,
};

static const struct attribute_group queue_attrgroup = {
	.attrs = queue_attributes,
};

static void channel_sysfs_destroy(struct mm_channel *channel)
{
	if (get_device(&channel->dev)) {
		sysfs_remove_group(&channel->dev.kobj, &queue_attrgroup);
		put_device(&channel->dev);
		device_unregister(&channel->dev);
	}

}

static void device_release(struct device *dev)
{
	xocl_dbg(dev, "dummy device release callback");
}

static int channel_sysfs_create(struct mm_channel *channel)
{
	struct platform_device	*pdev = channel->qdma->pdev;
	int			ret;
	struct qdma_queue_conf *qconf = &channel->qconf;

	channel->dev.parent = &pdev->dev;
	channel->dev.release = device_release;
	dev_set_drvdata(&channel->dev, channel);
	dev_set_name(&channel->dev, "%sq%d",
		qconf->c2h ? "r" : "w",
		qconf->qidx);
	ret = device_register(&channel->dev);
	if (ret) {
		xocl_err(&pdev->dev, "device create failed");
		goto failed;
	}

	ret = sysfs_create_group(&channel->dev.kobj, &queue_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs group failed");
		goto failed;
	}

	return 0;

failed:
	if (get_device(&channel->dev)) {
		put_device(&channel->dev);
		device_unregister(&channel->dev);
	}
	return ret;
}

static void qdma_stream_sysfs_destroy(struct qdma_stream_queue *queue)
{
	struct platform_device	*pdev = queue->qdma->pdev;
	char			name[32];

	if (queue->qconf.c2h)
		snprintf(name, sizeof(name) - 1, "flow%d", queue->flowid);
	else
		snprintf(name, sizeof(name) - 1, "route%d", queue->routeid);

	if (get_device(&queue->dev)) {
		sysfs_remove_link(&pdev->dev.kobj, (const char *)name);
		sysfs_remove_group(&queue->dev.kobj, &queue_attrgroup);
		put_device(&queue->dev);
		device_unregister(&queue->dev);
	}
}

static void qdma_stream_device_release(struct device *dev)
{
	xocl_dbg(dev, "dummy device release callback");
}

static int qdma_stream_sysfs_create(struct qdma_stream_queue *queue)
{
	struct platform_device	*pdev = queue->qdma->pdev;
	struct qdma_stream_queue	*temp_q;
	char			name[32];
	int			ret, i;

#if 0
	queue->dev = device_create(NULL, &pdev->dev,
		0, queue, "%sq%d", queue->qconf.c2h ? "r" : "w",
		queue->qconf.qidx);
#endif

	for (i = 0; i < QDMA_QSETS_MAX * 2; i++) {
		temp_q = queue->qdma->queues[i];
		if (!temp_q)
		       continue;
		if (temp_q->qconf.c2h && queue->qconf.c2h &&
			temp_q->flowid == queue->flowid) {
			xocl_err(&pdev->dev,
				"flowid overlapped with queue %d", i);
			return -EINVAL;
		}

		 if (!(temp_q->qconf.c2h) && !(queue->qconf.c2h) &&
			temp_q->routeid == queue->routeid) {
			 xocl_err(&pdev->dev,
				"routeid overlapped with queue %d", i);
			 return -EINVAL;
		 }
	}

	queue->dev.parent = &pdev->dev;
	queue->dev.release = qdma_stream_device_release;
	dev_set_drvdata(&queue->dev, queue);
	dev_set_name(&queue->dev, "%sq%d",
		queue->qconf.c2h ? "r" : "w",
		queue->qconf.qidx);
	ret = device_register(&queue->dev);
	if (ret) {
		xocl_err(&pdev->dev, "device create failed");
		goto failed;
	}

	ret = sysfs_create_group(&queue->dev.kobj, &queue_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs group failed");
		goto failed;
	}

	if (queue->qconf.c2h)
		snprintf(name, sizeof(name) - 1, "flow%d", queue->flowid);
	else
		snprintf(name, sizeof(name) - 1, "route%d", queue->routeid);

	ret = sysfs_create_link(&pdev->dev.kobj, &queue->dev.kobj,
			(const char *)name);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs link %s failed", name);
		sysfs_remove_group(&queue->dev.kobj, &queue_attrgroup);
		goto failed;
	}

	return 0;

failed:
	if (get_device(&queue->dev)) {
		put_device(&queue->dev);
		device_unregister(&queue->dev);
	}
	return ret;
}

static ssize_t error_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_qdma *qdma;

	qdma = platform_get_drvdata(pdev);

	return qdma_device_error_stat_dump((unsigned long)qdma->dma_handle,
						buf, 0);
}
static DEVICE_ATTR_RO(error);

static ssize_t channel_stat_raw_show(struct device *dev,
		        struct device_attribute *attr, char *buf)
{
	u32 i;
	ssize_t nbytes = 0;
	struct platform_device *pdev = to_platform_device(dev);
	u32 chs = get_channel_count(pdev);

	for (i = 0; i < chs; i++) {
		nbytes += sprintf(buf + nbytes, "%llu %llu\n",
		get_channel_stat(pdev, i, 0),
		get_channel_stat(pdev, i, 1));
	}
	return nbytes;
}
static DEVICE_ATTR_RO(channel_stat_raw);

static struct attribute *qdma_attributes[] = {
	&dev_attr_error.attr,
	&dev_attr_channel_stat_raw.attr,
	NULL,
};

static const struct attribute_group qdma_attrgroup = {
	.attrs = qdma_attributes,
};

/* end of sysfs */

static ssize_t qdma_migrate_bo(struct platform_device *pdev,
	struct sg_table *sgt, u32 write, u64 paddr, u32 channel, u64 len)
{
	struct mm_channel *chan;
	struct xocl_qdma *qdma;
	xdev_handle_t xdev;
	struct qdma_request req;
	enum dma_data_direction dir;
	u32 nents;
	pid_t pid = current->pid;
	ssize_t ret;

	qdma = platform_get_drvdata(pdev);
	xocl_dbg(&pdev->dev, "TID %d, Channel:%d, Offset: 0x%llx, write: %d",
		pid, channel, paddr, write);
	xdev = xocl_get_xdev(pdev);

	chan = &qdma->chans[write][channel];

	dir = write ? DMA_TO_DEVICE : DMA_FROM_DEVICE; 
	nents = pci_map_sg(XDEV(xdev)->pdev, sgt->sgl, sgt->orig_nents, dir);
        if (!nents) {
		xocl_err(&pdev->dev, "map sgl failed, sgt 0x%p.\n", sgt);
		return -EIO;
	}
	sgt->nents = nents;
	
	memset(&req, 0, sizeof(struct qdma_request));
	req.write = write;
	req.count = len;
	req.use_sgt = 1;
	req.ep_addr = paddr;
	req.sgt = sgt;

	req.dma_mapped = 1;

	ret = qdma_request_submit((unsigned long)qdma->dma_handle, chan->queue,
				&req);

	if (ret >= 0) {
		chan->total_trans_bytes += ret;
	} else  {
		xocl_err(&pdev->dev, "DMA failed, Dumping SG Page Table");
		dump_sgtable(&pdev->dev, sgt);
	}

	pci_unmap_sg(XDEV(xdev)->pdev, sgt->sgl, nents, dir);

	return len;
}

static void release_channel(struct platform_device *pdev, u32 dir, u32 channel)
{
	struct xocl_qdma *qdma;


	qdma = platform_get_drvdata(pdev);
        set_bit(channel, &qdma->channel_bitmap[dir]);
        up(&qdma->channel_sem[dir]);
}

static int acquire_channel(struct platform_device *pdev, u32 dir)
{
	struct xocl_qdma *qdma;
	int channel = 0;
	int result = 0;
	u32 write;

	qdma = platform_get_drvdata(pdev);

	if (down_killable(&qdma->channel_sem[dir])) {
		channel = -ERESTARTSYS;
		goto out;
	}

	for (channel = 0; channel < qdma->channel; channel++) {
		result = test_and_clear_bit(channel,
			&qdma->channel_bitmap[dir]);
		if (result)
			break;
        }
        if (!result) {
		// How is this possible?
		up(&qdma->channel_sem[dir]);
		channel = -EIO;
		goto out;
	}

	write = dir ? 1 : 0;
	if (strlen(qdma->chans[write][channel].qconf.name) == 0) {
		xocl_err(&pdev->dev, "queue not started, chan %d", channel);
		release_channel(pdev, dir, channel);
		channel = -EINVAL;
	}
out:
	return channel;
}

static void free_channels(struct platform_device *pdev)
{
	struct xocl_qdma *qdma;
	struct mm_channel *chan;
	u32	write, qidx;
	int i, ret = 0;

	qdma = platform_get_drvdata(pdev);
	if (!qdma || !qdma->channel)
		return;

	for (i = 0; i < qdma->channel * 2; i++) {
		write = i / qdma->channel;
		qidx = i % qdma->channel;
		chan = &qdma->chans[write][qidx];

		channel_sysfs_destroy(chan);

		ret = qdma_queue_stop((unsigned long)qdma->dma_handle,
				chan->queue, NULL, 0);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Stopping queue for "
				"channel %d failed, ret %x", qidx, ret);
		}
		ret = qdma_queue_remove((unsigned long)qdma->dma_handle,
				chan->queue, NULL, 0);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Destroy queue for "
				"channel %d failed, ret %x", qidx, ret);
		}
	}
	if (qdma->chans[0])
		devm_kfree(&pdev->dev, qdma->chans[0]);
	if (qdma->chans[1])
		devm_kfree(&pdev->dev, qdma->chans[1]);
}

static int set_max_chan(struct xocl_qdma *qdma, u32 count)
{
	struct platform_device *pdev = qdma->pdev;
	struct qdma_queue_conf *qconf;
	struct mm_channel *chan;
	u32	write, qidx;
	char	ebuf[MM_EBUF_LEN + 1];
	int	i, ret;
	bool	reset = false;

	if (qdma->channel == count)
		reset = true;
	qdma->channel = count;

	sema_init(&qdma->channel_sem[0], qdma->channel);
	sema_init(&qdma->channel_sem[1], qdma->channel);

	/* Initialize bit mask to represent individual channels */
	qdma->channel_bitmap[0] = BIT(qdma->channel);
	qdma->channel_bitmap[0]--;
	qdma->channel_bitmap[1] = qdma->channel_bitmap[0];

	xocl_info(&pdev->dev, "Creating MM Queues, Channel %d", qdma->channel);
	if (!reset) {
		qdma->chans[0] = devm_kzalloc(&pdev->dev,
			sizeof(struct mm_channel) * qdma->channel, GFP_KERNEL);
		qdma->chans[1] = devm_kzalloc(&pdev->dev,
			sizeof(struct mm_channel) * qdma->channel, GFP_KERNEL);
		if (qdma->chans[0] == NULL || qdma->chans[1] == NULL) {
			xocl_err(&pdev->dev, "Alloc channel mem failed");
			ret = -ENOMEM;
			goto failed_create_queue;
		}
	}

	for (i = 0; i < qdma->channel * 2; i++) {
		write = i / qdma->channel;
		qidx = i % qdma->channel;
		chan = &qdma->chans[write][qidx];
		qconf = &chan->qconf;
		chan->qdma = qdma;

		memset(qconf, 0, sizeof (struct qdma_queue_conf));
		memset(&ebuf, 0, sizeof (ebuf));
		qconf->cmpl_status_en =1;
		qconf->cmpl_status_acc_en=1;
		qconf->cmpl_status_pend_chk=1;
		qconf->fetch_credit=1;
		qconf->cmpl_stat_en=1;
		qconf->cmpl_trig_mode=1;
		qconf->desc_rng_sz_idx = MM_DEFAULT_RINGSZ_IDX;

		qconf->st = 0; /* memory mapped */
		qconf->c2h = write ? 0 : 1;
		qconf->qidx = qidx;
		qconf->irq_en = 1;

		ret = qdma_queue_add((unsigned long)qdma->dma_handle, qconf,
				&chan->queue, ebuf, MM_EBUF_LEN);
		if (ret < 0) {
			pr_err("Creating queue failed, ret=%d, %s\n", ret, ebuf);
			goto failed_create_queue;
		}
		ret = qdma_queue_start((unsigned long)qdma->dma_handle,
				chan->queue, ebuf, MM_EBUF_LEN);
		if (ret < 0) {
			pr_err("Starting queue failed, ret=%d %s.\n", ret, ebuf);
			goto failed_create_queue;
		}

		if (!reset) {
			ret = channel_sysfs_create(chan);
			if (ret)
				goto failed_create_queue;
		}
	}

	xocl_info(&pdev->dev, "Created %d MM channels (Queues)", qdma->channel);

	return 0;

failed_create_queue:
	free_channels(pdev);

	return ret;
}

static u32 get_channel_count(struct platform_device *pdev)
{
	struct xocl_qdma *qdma;

        qdma = platform_get_drvdata(pdev);
        BUG_ON(!qdma);

        return qdma->channel;
}

static u64 get_channel_stat(struct platform_device *pdev, u32 channel,
	u32 write)
{
	struct xocl_qdma *qdma;

        qdma = platform_get_drvdata(pdev);
        BUG_ON(!qdma);

        return qdma->chans[write][channel].total_trans_bytes;
}

static u64 get_str_stat(struct platform_device *pdev, u32 q_idx)
{
	struct xocl_qdma *qdma;

	qdma = platform_get_drvdata(pdev);
	BUG_ON(!qdma);

	return 0;
}

static int user_intr_register(struct platform_device *pdev, u32 intr,
	irq_handler_t handler, void *arg, int event_fd)
{
	struct xocl_qdma *qdma;
	struct eventfd_ctx *trigger = ERR_PTR(-EINVAL);
	unsigned long flags;
	int ret;

	qdma = platform_get_drvdata(pdev);

	if (!((1 << intr) & qdma->user_msix_mask)) {
		xocl_err(&pdev->dev, "Invalid intr %d, user intr mask %x",
				intr, qdma->user_msix_mask);
		return -EINVAL;
	}

	if (event_fd >= 0) {
		trigger = eventfd_ctx_fdget(event_fd);
		if (IS_ERR(trigger)) {
			xocl_err(&pdev->dev, "get event ctx failed");
			return -EFAULT;
		}
	}

	spin_lock_irqsave(&qdma->user_msix_table_lock, flags);
	if (qdma->user_msix_table[intr].in_use) {
		xocl_err(&pdev->dev, "IRQ %d is in use", intr);
		ret = -EPERM;
		goto failed;
	}

	qdma->user_msix_table[intr].event_ctx = trigger;
	qdma->user_msix_table[intr].handler = handler;
	qdma->user_msix_table[intr].arg = arg;
	qdma->user_msix_table[intr].in_use = true;

	spin_unlock_irqrestore(&qdma->user_msix_table_lock, flags);


	return 0;

failed:
	spin_unlock_irqrestore(&qdma->user_msix_table_lock, flags);
	if (!IS_ERR(trigger))
		eventfd_ctx_put(trigger);

	return ret;
}

static int user_intr_unreg(struct platform_device *pdev, u32 intr)
{
	struct xocl_qdma *qdma;
	unsigned long flags;
	int ret;

	qdma= platform_get_drvdata(pdev);

	if (!((1 << intr) & qdma->user_msix_mask)) {
		xocl_err(&pdev->dev, "Invalid intr %d, user intr mask %x",
				intr, qdma->user_msix_mask);
		return -EINVAL;
	}

	spin_lock_irqsave(&qdma->user_msix_table_lock, flags);
	if (!qdma->user_msix_table[intr].in_use) {
		ret = -EINVAL;
		goto failed;
	}

	qdma->user_msix_table[intr].handler = NULL;
	qdma->user_msix_table[intr].arg = NULL;
	qdma->user_msix_table[intr].in_use = false;

	spin_unlock_irqrestore(&qdma->user_msix_table_lock, flags);
	return 0;
failed:
	spin_unlock_irqrestore(&qdma->user_msix_table_lock, flags);


	return ret;
}

static int user_intr_config(struct platform_device *pdev, u32 intr, bool en)
{
	return 0;
}

static void qdma_isr(unsigned long dma_handle, int irq, unsigned long arg)
{
	struct xocl_qdma *qdma = (struct xocl_qdma *)arg;
	struct qdma_irq *irq_entry;

	irq_entry = &qdma->user_msix_table[irq];
	if (irq_entry->in_use)
		irq_entry->handler(irq, irq_entry->arg);
}

static struct xocl_dma_funcs qdma_ops = {
	.migrate_bo = qdma_migrate_bo,
	.ac_chan = acquire_channel,
	.rel_chan = release_channel,
	.get_chan_count = get_channel_count,
	.get_chan_stat = get_channel_stat,
	.user_intr_register = user_intr_register,
	.user_intr_config = user_intr_config,
	.user_intr_unreg = user_intr_unreg,
	/* qdma */
	.get_str_stat = get_str_stat,
};

/* stream queue file operations */
static const struct vm_operations_struct qdma_stream_vm_ops = {
	.fault = xocl_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static struct qdma_stream_async_req *queue_req_new(struct qdma_stream_queue *queue)
{
	struct qdma_stream_async_req *io_req;

	spin_lock_bh(&queue->req_lock);
	if (list_empty(&queue->req_free_list)) {
		spin_unlock_bh(&queue->req_lock);
		return NULL;
	}

	io_req = list_first_entry(&queue->req_free_list,
		struct qdma_stream_async_req, list);
	list_del(&io_req->list);
	queue->req_free_cnt--;
	spin_unlock_bh(&queue->req_lock);

	memset(io_req, 0, sizeof(struct qdma_stream_async_req));
	spin_lock_init(&io_req->cb.lock);

	return io_req;
}

static void queue_req_free(struct qdma_stream_queue *queue,
			struct qdma_stream_async_req *io_req,
			bool completed)
{
	spin_lock_bh(&queue->req_lock);

	if (completed) {
		if (io_req->cb.cancel)
			queue->req_cancel_cmpl_cnt++;
		else
			queue->req_cmpl_cnt++;
	}

	list_del(&io_req->list);
	list_add_tail(&io_req->list, &queue->req_free_list);
	queue->req_free_cnt++;
	spin_unlock_bh(&queue->req_lock);
}

static void queue_req_pending(struct qdma_stream_queue *queue,
	struct qdma_stream_async_req *io_req)
{
	spin_lock_bh(&queue->req_lock);
	queue->req_pend_cnt++;
	list_add_tail(&io_req->list, &queue->req_pend_list);
	spin_unlock_bh(&queue->req_lock);
}

static void inline cmpl_aio(struct kiocb *kiocb, unsigned int done_bytes,
		int error)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	kiocb->ki_complete(kiocb, done_bytes, error);
#else
	if (is_sync_kiocb(kiocb))
		atomic_set(&kiocb->ki_users, 1);
	aio_complete(kiocb, done_bytes, error);
#endif
}

static void cmpl_aio_cancel(struct work_struct *work)
{
	struct qdma_stream_async_arg *cb = container_of(work,
				struct qdma_stream_async_arg, work);

	spin_lock_bh(&cb->lock);
	if (cb->kiocb) {
		cmpl_aio(cb->kiocb, 0, -ECANCELED);
		cb->kiocb = NULL;
	}
	spin_unlock_bh(&cb->lock);
}

static int queue_req_complete(unsigned long priv, unsigned int done_bytes,
	int error)
{
	struct qdma_stream_async_arg *cb = (struct qdma_stream_async_arg *)priv;
	struct qdma_stream_async_req *io_req = cb->io_req;
	struct qdma_stream_queue *queue = cb->queue;

	pr_debug("%s, q 0x%lx, req 0x%p,err %d, %u,%u, %u,%u, mem %u,%u.\n",
		__func__, queue->queue, &io_req->req, error,
		queue->req_submit_cnt, queue->req_cmpl_cnt,
		queue->req_cancel_cnt, queue->req_cancel_cmpl_cnt,
		queue->req_pend_cnt, queue->req_free_cnt);

	if (cb->is_unmgd) {
		xdev_handle_t xdev = xocl_get_xdev(cb->queue->qdma->pdev);

		pci_unmap_sg(XDEV(xdev)->pdev, cb->unmgd.sgt->sgl, cb->nsg,
			cb->queue->qconf.c2h ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		xocl_finish_unmgd(&cb->unmgd);
	} else {
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&cb->xobj->base);
	}

	spin_lock_bh(&cb->lock);
	if (cb->kiocb) {
		cmpl_aio(cb->kiocb, done_bytes, error);
		cb->kiocb = NULL;
	}
	spin_unlock_bh(&cb->lock);

	queue_req_free(queue, io_req, true);

	return 0;
}

static ssize_t qdma_stream_post_bo(struct xocl_qdma *qdma,
	struct qdma_stream_queue *queue, struct drm_gem_object *gem_obj,
	loff_t offset, size_t len, bool write,
	struct xocl_qdma_req_header *header, struct kiocb *kiocb)
{
	struct drm_xocl_bo *xobj;
	struct qdma_stream_async_req  *io_req = NULL;
	struct qdma_request *req;
	struct qdma_stream_async_arg *cb;
	ssize_t ret;

	if (gem_obj->size < offset + len) {
		xocl_err(&qdma->pdev->dev, "Invalid request, buf size: %ld, "
			"request size %ld, offset %lld",
			gem_obj->size, len, offset);
		return -EINVAL;
	}

	XOCL_DRM_GEM_OBJECT_GET(gem_obj);
	xobj = to_xocl_bo(gem_obj);

	io_req = queue_req_new(queue);
	if (!io_req) {
		xocl_err(&qdma->pdev->dev, "io request list full");
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

	pr_debug("%s, %s req 0x%p,0x%p, hndl 0x%lx,0x%lx, sgl 0x%p,%u,%u, "
		"ST %s %lu.\n",
		__func__, dev_name(&qdma->pdev->dev), io_req, req,
		(unsigned long)qdma->dma_handle, queue->queue, req->sgt->sgl,
	req->sgt->orig_nents, req->sgt->nents, write ? "W":"R", len);

	ret = qdma_request_submit((unsigned long)qdma->dma_handle, queue->queue,
		req);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "submit request failed %ld", ret);
		goto failed;
	}

	if (!kiocb) {
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
		queue_req_free(queue, io_req, false);
	} else {
		spin_lock_bh(&queue->req_lock);
		queue->req_submit_cnt++;
		spin_unlock_bh(&queue->req_lock);
	}

	return ret;
failed:
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
	if (io_req)
		queue_req_free(queue, io_req, false);

	return ret;

}

static ssize_t queue_rw(struct xocl_qdma *qdma, struct qdma_stream_queue *queue,
	char __user *buf, size_t sz, bool write, char __user *u_header,
	struct kiocb *kiocb)
{
	struct vm_area_struct	*vma;
	struct drm_xocl_unmgd unmgd;
	unsigned long buf_addr = (unsigned long)buf;
	enum dma_data_direction dir;
	struct xocl_qdma_req_header header;
	u32 nents;
	xdev_handle_t xdev;
	struct qdma_stream_async_req  *io_req = NULL;
	struct qdma_request *req;
	struct qdma_stream_async_arg *cb;
	long	ret = 0;

	xocl_dbg(&qdma->pdev->dev, "Read / Write Queue 0x%lx",
		queue->queue);

	if (sz == 0)
		return 0;

	if (((uint64_t)(buf) & ~PAGE_MASK) && queue->qconf.c2h) {
		xocl_err(&qdma->pdev->dev,
			"C2H buffer has to be page aligned, buf %p", buf);
		ret = -EINVAL;
		goto failed;
	}

	memset (&header, 0, sizeof (header));
	if (u_header &&  copy_from_user((void *)&header, u_header,
		sizeof (struct xocl_qdma_req_header))) {
		xocl_err(&qdma->pdev->dev, "copy header failed.");
		ret = -EFAULT;
		goto failed;
	}

	if (!queue->qconf.c2h &&
		!(header.flags & XOCL_QDMA_REQ_FLAG_EOT) &&
		(sz & 0xfff)) {
		xocl_err(&qdma->pdev->dev,
			"H2C without EOT has to be multiple of 4k, sz 0x%lx",
			sz);
		ret = -EINVAL;
		goto failed;
	}

	vma = find_vma(current->mm, buf_addr);
	if (vma && (vma->vm_ops == &qdma_stream_vm_ops)) {
		if (vma->vm_start > buf_addr || vma->vm_end <= buf_addr + sz) {
			xocl_err(&qdma->pdev->dev, "invalid BO address");
			ret = -EINVAL;
			goto failed;
		}
		ret = qdma_stream_post_bo(qdma, queue, vma->vm_private_data,
			(buf_addr - vma->vm_start), sz, write, &header, kiocb);
		goto failed;
	}

	ret = xocl_init_unmgd(&unmgd, (uint64_t)buf, sz, write);
	if (ret) {
		xocl_err(&qdma->pdev->dev, "Init unmgd buf failed, "
			"ret=%ld", ret);
		goto failed;
	}

	xdev = xocl_get_xdev(qdma->pdev);
	dir = write ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
	nents = pci_map_sg(XDEV(xdev)->pdev, unmgd.sgt->sgl,
	unmgd.sgt->orig_nents, dir);
	if (!nents) {
		xocl_err(&qdma->pdev->dev, "map sgl failed");
		xocl_finish_unmgd(&unmgd);
		ret = -EFAULT;
		goto failed;
	}

	io_req = queue_req_new(queue);
	if (!io_req) {
		xocl_err(&qdma->pdev->dev,
			"%s, queue 0x%lx io request OOM, %s, sz 0x%lx",
			dev_name(&qdma->pdev->dev), queue->queue,
			write ? "W":"R", sz);
		xocl_finish_unmgd(&unmgd);
		ret = -ENOMEM;
		goto failed;
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

	pr_debug("%s, %s req 0x%p,0x%p hndl 0x%lx,0x%lx, sgl 0x%p,%u,%u, "
		"ST %s %lu.\n",
		__func__, dev_name(&qdma->pdev->dev), io_req, req,
		(unsigned long)qdma->dma_handle, queue->queue, req->sgt->sgl,
		req->sgt->orig_nents, req->sgt->nents, write ? "W":"R", sz);

	ret = qdma_request_submit((unsigned long)qdma->dma_handle, queue->queue,
		req);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "post wr failed ret=%ld", ret);
		xocl_finish_unmgd(&unmgd);
		goto failed;
	}

	if (!kiocb) {
		pci_unmap_sg(XDEV(xdev)->pdev, unmgd.sgt->sgl, nents, dir);
		xocl_finish_unmgd(&unmgd);
		queue_req_free(queue, io_req, false);
	} else {
		spin_lock_bh(&queue->req_lock);
		queue->req_submit_cnt++;
		spin_unlock_bh(&queue->req_lock);
	}

failed:

	if (ret < 0) {
		if (io_req)
			queue_req_free(queue, io_req, false);
		return ret;
	} else if (kiocb)
		ret = -EIOCBQUEUED;

	return ret;
}

static int queue_wqe_cancel(struct kiocb *kiocb)
{
	struct qdma_stream_async_req *io_req =
	(struct qdma_stream_async_req *)kiocb->private;
	struct qdma_stream_queue *queue = io_req->cb.queue;
	struct xocl_qdma *qdma = queue->qdma;
	struct qdma_stream_async_arg *cb = &io_req->cb;

	pr_debug("%s, %s cancel ST req 0x%p hndl 0x%lx,0x%lx, %s %u.\n",
		__func__, dev_name(&queue->qdma->pdev->dev),
		&io_req->req, (unsigned long)qdma->dma_handle, queue->queue,
	io_req->req.write ? "W":"R", io_req->req.count);

	spin_lock_bh(&queue->req_lock);
	cb->cancel = 1;
	queue->req_cancel_cnt++;;
	spin_unlock_bh(&queue->req_lock);

	/* delayed aio cancel completion */
	INIT_WORK(&cb->work, cmpl_aio_cancel);
	schedule_work(&cb->work);

	qdma_request_cancel((unsigned long)qdma->dma_handle, queue->queue,
		&io_req->req);

	return -EINPROGRESS;
}

static ssize_t queue_aio_read(struct kiocb *kiocb, const struct iovec *iov,
			unsigned long nr, loff_t off)
{
	struct qdma_stream_queue	*queue;
	struct xocl_qdma	*qdma;

	queue = (struct qdma_stream_queue *)kiocb->ki_filp->private_data;
	qdma = queue->qdma;

	if (nr != 2) {
		xocl_err(&qdma->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (is_sync_kiocb(kiocb)) {
		return queue_rw(qdma, queue, iov[1].iov_base,
			iov[1].iov_len, false, iov[0].iov_base, NULL);
	}

	kiocb_set_cancel_fn(kiocb, (kiocb_cancel_fn *)queue_wqe_cancel);

	return queue_rw(qdma, queue, iov[1].iov_base, iov[1].iov_len,
			false, iov[0].iov_base, kiocb);
}

static ssize_t queue_aio_write(struct kiocb *kiocb, const struct iovec *iov,
			unsigned long nr, loff_t off)
{
	struct qdma_stream_queue	*queue;
	struct xocl_qdma	*qdma;

	queue = (struct qdma_stream_queue *)kiocb->ki_filp->private_data;
	qdma = queue->qdma;

	if (nr != 2) {
		xocl_err(&qdma->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (is_sync_kiocb(kiocb)) {
		return queue_rw(qdma, queue, iov[1].iov_base,
			iov[1].iov_len, true, iov[0].iov_base, NULL);
	}

	kiocb_set_cancel_fn(kiocb, (kiocb_cancel_fn *)queue_wqe_cancel);

	return queue_rw(qdma, queue, iov[1].iov_base, iov[1].iov_len,
			true, iov[0].iov_base, kiocb);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t queue_write_iter(struct kiocb *kiocb, struct iov_iter *io)
{
	struct qdma_stream_queue	*queue;
	struct xocl_qdma	*qdma;
	unsigned long		nr;

	queue = (struct qdma_stream_queue *)kiocb->ki_filp->private_data;
	qdma = queue->qdma;

	nr = io->nr_segs;
	if (!iter_is_iovec(io) || nr != 2) {
		xocl_err(&qdma->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (!is_sync_kiocb(kiocb)) {
		return queue_aio_write(kiocb, io->iov, nr, io->iov_offset);
	}

	return queue_rw(qdma, queue, io->iov[1].iov_base,
		io->iov[1].iov_len, true, io->iov[0].iov_base, NULL);
}

static ssize_t queue_read_iter(struct kiocb *kiocb, struct iov_iter *io)
{
	struct qdma_stream_queue	*queue;
	struct xocl_qdma	*qdma;
	unsigned long		nr;

	queue = (struct qdma_stream_queue *)kiocb->ki_filp->private_data;
	qdma = queue->qdma;

	nr = io->nr_segs;
	if (!iter_is_iovec(io) || nr != 2) {
		xocl_err(&qdma->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (!is_sync_kiocb(kiocb)) {
		return queue_aio_read(kiocb, io->iov, nr, io->iov_offset);
	}

	return queue_rw(qdma, queue, io->iov[1].iov_base,
		io->iov[1].iov_len, false, io->iov[0].iov_base, NULL);
}
#endif

static int queue_flush(struct qdma_stream_queue *queue)
{
	struct xocl_qdma *qdma;
	long	ret = 0;

	qdma = queue->qdma;

	xocl_info(&qdma->pdev->dev, "Release Queue 0x%lx", queue->queue);

	qdma_stream_sysfs_destroy(queue);
	if (queue->qconf.c2h)
		qdma->queues[queue->qconf.qidx] = NULL;
	else
		qdma->queues[QDMA_QSETS_MAX + queue->qconf.qidx] = NULL;

	ret = qdma_queue_stop((unsigned long)qdma->dma_handle, queue->queue,
		NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev,
			"Stop queue failed ret = %ld", ret);
		goto failed;
	}
	ret = qdma_queue_remove((unsigned long)qdma->dma_handle, queue->queue,
		NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev,
			"Destroy queue failed ret = %ld", ret);
		goto failed;
	}

	spin_lock_bh(&queue->req_lock);
	while (!list_empty(&queue->req_pend_list)) {
		struct qdma_stream_async_req *io_req = list_first_entry(
			&queue->req_pend_list,
			struct qdma_stream_async_req,
			list);

		spin_unlock_bh(&queue->req_lock);
		xocl_info(&qdma->pdev->dev, "Queue 0x%lx, cancel req 0x%p,0x%p",
			queue->queue, io_req, &io_req->req);
		queue_req_complete((unsigned long)&io_req->cb, 0, -ECANCELED);
		spin_lock_bh(&queue->req_lock);
	}
	spin_unlock_bh(&queue->req_lock);

failed:
	return ret;
}

static int queue_close(struct inode *inode, struct file *file)
{
	struct xocl_qdma *qdma;
	struct qdma_stream_queue *queue;

	queue = (struct qdma_stream_queue *)file->private_data;
	if (!queue) 
		return 0;

	queue_flush(queue);

	if (queue->req_cache)
		vfree(queue->req_cache);

	qdma = queue->qdma;
	devm_kfree(&qdma->pdev->dev, queue);
	file->private_data = NULL;

	return 0;
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
		.release = queue_close,
};

/* stream device file operations */
static long qdma_stream_ioctl_create_queue(struct xocl_qdma *qdma,
			void __user *arg)
{
	struct xocl_qdma_ioc_create_queue req;
	struct qdma_queue_conf *qconf;
	struct qdma_stream_queue *queue;
	long	ret;

	if (copy_from_user((void *)&req, arg,
		sizeof (struct xocl_qdma_ioc_create_queue))) {
		xocl_err(&qdma->pdev->dev, "copy failed.");
		return -EFAULT;
	}

	queue = devm_kzalloc(&qdma->pdev->dev, sizeof (*queue), GFP_KERNEL);
	if (!queue) {
		xocl_err(&qdma->pdev->dev, "out of memeory");
		return -ENOMEM;
	}
	queue->qfd = -1;
	INIT_LIST_HEAD(&queue->req_pend_list);
	INIT_LIST_HEAD(&queue->req_free_list);
	spin_lock_init(&queue->req_lock);

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
		qconf->desc_rng_sz_idx = qdma->c2h_ringsz_idx;
		qconf->cmpl_rng_sz_idx = qdma->wrb_ringsz_idx;
	} else {
		/* H2C */
		qconf->desc_bypass = 1;
		qconf->pipe_flow_id = req.flowid & STREAM_FLOWID_MASK;
		qconf->pipe_slr_id = (req.rid >> STREAM_SLRID_SHIFT) &
		STREAM_SLRID_MASK;
		qconf->pipe_tdest = req.rid & STREAM_TDEST_MASK;
		qconf->pipe_gl_max = 1;
		qconf->desc_rng_sz_idx = qdma->h2c_ringsz_idx;
	}
	queue->flowid = req.flowid;
	queue->routeid = req.rid;
	xocl_info(&qdma->pdev->dev, "Creating %s queue with tdest %d, flow %d, "
		"slr %d", qconf->c2h ? "C2H" : "H2C",
		qconf->pipe_tdest, qconf->pipe_flow_id,
		qconf->pipe_slr_id);

	ret = qdma_queue_add((unsigned long)qdma->dma_handle, qconf,
		&queue->queue, NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "Adding Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = qdma_queue_start((unsigned long)qdma->dma_handle, queue->queue,
		NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "Starting Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = qdma_queue_prog_stm((unsigned long)qdma->dma_handle, queue->queue,
		NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "STM prog. Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = qdma_queue_get_config((unsigned long)qdma->dma_handle,
		queue->queue, qconf, NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "Get Q conf. failed ret = %ld", ret);
		goto failed;
	}

	/* pre-allocate 2x io request struct */
	queue->req_cache = vzalloc((qconf->rngsz << 1) *
		sizeof(struct qdma_stream_async_req));
	if (!queue->req_cache) {
		xocl_err(&qdma->pdev->dev, "req. cache OOM %u", qconf->rngsz);
		goto failed;
	} else {
		int i;
		struct qdma_stream_async_req *io_req = queue->req_cache;
		unsigned int max = qconf->rngsz << 1;

		for (i = 0; i < max; i++, io_req++)
			list_add_tail(&io_req->list, &queue->req_free_list);
		queue->req_free_cnt = i;
	}

	xocl_info(&qdma->pdev->dev,
		"Created %s Queue handle 0x%lx, idx %d, sz %d, %u",
		qconf->c2h ? "C2H" : "H2C",
		queue->queue, queue->qconf.qidx, queue->qconf.rngsz,
		queue->req_free_cnt);

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
		xocl_err(&qdma->pdev->dev, "Failed get fd");
		goto failed;
	}
	req.handle = queue->qfd;

	if (copy_to_user(arg, &req, sizeof (req))) {
		xocl_err(&qdma->pdev->dev, "Copy to user failed");
		ret = -EFAULT;
		goto failed;
	}

	queue->qdma = qdma;

	ret = qdma_stream_sysfs_create(queue);
	if (ret) {
		xocl_err(&qdma->pdev->dev, "sysfs create failed");
		goto failed;
	}

	queue->uid = current_uid();
	if (queue->qconf.c2h)
		qdma->queues[queue->qconf.qidx] = queue;
	else
		qdma->queues[QDMA_QSETS_MAX + queue->qconf.qidx] = queue;

	fd_install(queue->qfd, queue->file);

	return 0;

failed:
	if (queue->qfd >= 0)
		put_unused_fd(queue->qfd);

	if (queue->file) {
		queue->file->private_data = NULL;
		fput(queue->file);
		queue->file = NULL;
	}

	if (queue) {
		if (queue->req_cache)
			vfree(queue->req_cache);
		devm_kfree(&qdma->pdev->dev, queue);
	}

	qdma_queue_stop((unsigned long)qdma->dma_handle, queue->queue,
		NULL, 0);
	qdma_queue_remove((unsigned long)qdma->dma_handle, queue->queue,
		NULL, 0);
	queue->queue = 0UL;

	return ret;
}

static long qdma_stream_ioctl_alloc_buffer(struct xocl_qdma *qdma,
			void __user *arg)
{
	struct xocl_qdma_ioc_alloc_buf req;
	struct drm_xocl_bo *xobj;
	xdev_handle_t	xdev;
	struct dma_buf *dmabuf = NULL;
	int flags;
	int ret;

	if (copy_from_user((void *)&req, arg,
		sizeof (struct xocl_qdma_ioc_alloc_buf))) {
		xocl_err(&qdma->pdev->dev, "copy failed.");
		return -EFAULT;
	}

	xdev = xocl_get_xdev(qdma->pdev);

	xobj = xocl_drm_create_bo(XOCL_DRM(xdev), req.size,
			XCL_BO_FLAGS_EXECBUF);
	if (IS_ERR(xobj)) {
		ret = PTR_ERR(xobj);
		xocl_err(&qdma->pdev->dev, "create bo failed");
		return ret;
	}

	xobj->pages = drm_gem_get_pages(&xobj->base);
	if (IS_ERR(xobj->pages)) {
		ret = PTR_ERR(xobj->pages);
		xocl_err(&qdma->pdev->dev, "Get pages failed");
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

	xobj->dma_nsg = pci_map_sg(XDEV(xdev)->pdev, xobj->sgt->sgl,
	xobj->sgt->orig_nents, PCI_DMA_BIDIRECTIONAL);
	if (!xobj->dma_nsg) {
		xocl_err(&qdma->pdev->dev, "map sgl failed, sgt");
		ret = -EIO;
		goto failed;
	}

	ret = drm_gem_create_mmap_offset(&xobj->base);
	if (ret < 0)
		goto failed;

	flags = O_CLOEXEC | O_RDWR;

	XOCL_DRM_GEM_OBJECT_GET(&xobj->base);
	dmabuf = drm_gem_prime_export(XOCL_DRM(xdev)->ddev,
				&xobj->base, flags);
	if (IS_ERR(dmabuf)) {
		xocl_err(&qdma->pdev->dev, "failed to export dma_buf");
		ret = PTR_ERR(dmabuf);
		goto failed;
	}
	xobj->dmabuf = dmabuf;
	xobj->dmabuf_vm_ops = &qdma_stream_vm_ops;

	req.buf_fd = dma_buf_fd(dmabuf, flags);
	if (req.buf_fd < 0) {
		goto failed;
	}

	if (copy_to_user(arg, &req, sizeof (req))) {
		xocl_err(&qdma->pdev->dev, "Copy to user failed");
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

	xocl_drm_free_bo(&xobj->base);

	return ret;
}

static long qdma_stream_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_qdma *qdma;
	long result = 0;

	qdma = (struct xocl_qdma *)filp->private_data;

	switch (cmd) {
	case XOCL_QDMA_IOC_CREATE_QUEUE:
		result = qdma_stream_ioctl_create_queue(qdma, (void __user *)arg);
		break;
	case XOCL_QDMA_IOC_ALLOC_BUFFER:
		result = qdma_stream_ioctl_alloc_buffer(qdma, (void __user *)arg);
		break;
	default:
		xocl_err(&qdma->pdev->dev, "Invalid request %u", cmd & 0xff);
		result = -EINVAL;
		break;
	}

	return result;
}

static int qdma_stream_open(struct inode *inode, struct file *file)
{
	struct xocl_qdma *qdma;

	qdma = xocl_drvinst_open(inode->i_cdev);
	if (!qdma)
		return -ENXIO;

	file->private_data = qdma;

	xocl_info(&qdma->pdev->dev, "opened file %p by pid: %d",
		file, pid_nr(task_tgid(current)));

	return 0;
}

static int qdma_stream_close(struct inode *inode, struct file *file)
{
	struct xocl_qdma *qdma;

	qdma = (struct xocl_qdma *)file->private_data;

	xocl_drvinst_close(qdma);
	xocl_info(&qdma->pdev->dev, "Closing file %p by pid: %d",
		file, pid_nr(task_tgid(current)));

	return 0;
}


/*
 * char device for QDMA
 */
static const struct file_operations qdma_stream_fops = {
	.owner = THIS_MODULE,
	.open = qdma_stream_open,
	.release = qdma_stream_close,
	.unlocked_ioctl = qdma_stream_ioctl,
};

static int qdma_probe(struct platform_device *pdev)
{
	struct xocl_qdma *qdma = NULL;
	struct qdma_dev_conf *conf;
	xdev_handle_t	xdev;
	struct resource *res = NULL;
	int	ret = 0, dma_bar;

	xdev = xocl_get_xdev(pdev);

	qdma = xocl_drvinst_alloc(&pdev->dev, sizeof(*qdma));
	if (!qdma) {
		xocl_err(&pdev->dev, "alloc mm dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	qdma->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "Empty resource");
		return -EINVAL;
	}

	ret = xocl_ioaddr_to_baroff(xdev, res->start, &dma_bar, NULL);
	if (ret) {
		xocl_err(&pdev->dev, "Invalid resource %pR", res);
		return -EINVAL;
	}

	conf = &qdma->dev_conf;
	memset(conf, 0, sizeof(*conf));
	conf->pdev = XDEV(xdev)->pdev;
	conf->intr_rngsz = QDMA_INTR_COAL_RING_SIZE;
	conf->master_pf = XOCL_DSA_IS_SMARTN(xdev) ? 0 : 1;
	conf->qsets_max = QDMA_QSETS_MAX;
	conf->bar_num_config = dma_bar;
	conf->bar_num_stm = XDEV(xdev)->bar_idx;

	conf->fp_user_isr_handler = qdma_isr;
	conf->uld = (unsigned long)qdma;

	ret = qdma_device_open(XOCL_MODULE_NAME, conf, (unsigned long *)
			(&qdma->dma_handle));
	if (ret < 0) {
		xocl_err(&pdev->dev, "QDMA Device Open failed");
		goto failed;
	}

	if (!XOCL_DSA_IS_SMARTN(xdev)) {
		ret = set_max_chan(qdma, 2);
		if (ret) {
			xocl_err(&pdev->dev, "Set max channel failed");
			goto failed;
		}
	}

	ret = qdma_device_get_config((unsigned long)qdma->dma_handle,
			&qdma->dev_conf, NULL, 0);
	if (ret) {
		xocl_err(&pdev->dev, "Failed to get device info");
		goto failed;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &qdma_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs group failed");
		goto failed;
	}

	qdma->h2c_ringsz_idx = STREAM_DEFAULT_H2C_RINGSZ_IDX;
	qdma->c2h_ringsz_idx = STREAM_DEFAULT_C2H_RINGSZ_IDX;
	qdma->wrb_ringsz_idx = STREAM_DEFAULT_WRB_RINGSZ_IDX;

	qdma->user_msix_mask = QDMA_USER_INTR_MASK;

	spin_lock_init(&qdma->user_msix_table_lock);

	platform_set_drvdata(pdev, qdma);

	return 0;

failed:
	if (qdma) {
		free_channels(qdma->pdev);

		if (qdma->dma_handle)
			qdma_device_close(XDEV(xdev)->pdev,
					(unsigned long)qdma->dma_handle);

		xocl_drvinst_free(qdma);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int qdma_remove(struct platform_device *pdev)
{
	struct xocl_qdma *qdma= platform_get_drvdata(pdev);
	xdev_handle_t xdev;
	struct qdma_irq *irq_entry;
	int i;

	sysfs_remove_group(&pdev->dev.kobj, &qdma_attrgroup);

	if (!qdma) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xdev = xocl_get_xdev(pdev);

	free_channels(pdev);

	qdma_device_close(XDEV(xdev)->pdev, (unsigned long)qdma->dma_handle);

	for (i = 0; i < ARRAY_SIZE(qdma->user_msix_table); i++) {
		irq_entry = &qdma->user_msix_table[i];
		if (irq_entry->in_use) {
			if (irq_entry->enabled)
				xocl_err(&pdev->dev,
					"ERROR: Interrupt %d is still on", i);
			if(!IS_ERR_OR_NULL(irq_entry->event_ctx))
				eventfd_ctx_put(irq_entry->event_ctx);
		}
	}


	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(qdma);

	return 0;
}

struct xocl_drv_private qdma_priv = {
	.ops = &qdma_ops,
	.fops = &qdma_stream_fops,
	.dev = -1,
};

static struct platform_device_id qdma_id_table[] = {
	{ XOCL_DEVNAME(XOCL_QDMA), (kernel_ulong_t)&qdma_priv },
	{ },
};

static struct platform_driver	qdma_driver = {
	.probe		= qdma_probe,
	.remove		= qdma_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_QDMA),
	},
	.id_table	= qdma_id_table,
};

int __init xocl_init_qdma(void)
{
	int		err = 0;

	err = libqdma_init(0);
	if (err)
		return err;
	err = alloc_chrdev_region(&str_dev, 0, XOCL_CHARDEV_REG_COUNT,
			XOCL_QDMA);
	if (err < 0)
		goto err_reg_chrdev;

	err = platform_driver_register(&qdma_driver);
	if (err)
		goto err_drv_reg;

	return 0;

err_drv_reg:
	unregister_chrdev_region(str_dev, XOCL_CHARDEV_REG_COUNT);
err_reg_chrdev:
	libqdma_exit();
	return err;
}

void xocl_fini_qdma(void)
{
	unregister_chrdev_region(str_dev, XOCL_CHARDEV_REG_COUNT);
	platform_driver_unregister(&qdma_driver);
	libqdma_exit();
}
