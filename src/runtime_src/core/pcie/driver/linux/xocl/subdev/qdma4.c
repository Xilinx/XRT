/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2020- Xilinx, Inc. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/anon_inodes.h>
#include <linux/dma-buf.h>
#include <linux/aio.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include "../xocl_drv.h"
#include "../xocl_drm.h"
#include "../lib/libqdma4/libqdma4_export.h"
#include "../lib/libqdma4/qdma_ul_ext.h"
#include "../lib/libqdma4/stmc.h"
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

#define	STREAM_DEFAULT_H2C_RINGSZ_IDX		0
#define	STREAM_DEFAULT_C2H_RINGSZ_IDX		0
#define	STREAM_DEFAULT_WRB_RINGSZ_IDX		0

#define QDMA_MAX_INTR		16
#define QDMA_USER_INTR_MASK	0xff

#define QDMA_QSETS_MAX		256
#define QDMA_QSETS_BASE		0

#define QDMA_REQ_TIMEOUT_MS	10000

/* Module Parameters */
unsigned int qdma4_max_channel = 16;
module_param(qdma4_max_channel, uint, 0644);
MODULE_PARM_DESC(qdma4_max_channel, "Set number of channels for qdma, default is 16");

static unsigned int qdma4_interrupt_mode = DIRECT_INTR_MODE;
module_param(qdma4_interrupt_mode, uint, 0644);
MODULE_PARM_DESC(interrupt_mode, "0:auto, 1:poll, 2:direct, 3:intr_ring, default is 2");

struct dentry *qdma4_debugfs_root;

static dev_t	str_dev;

struct qdma_stream_iocb;
struct qdma_stream_ioreq;

struct qdma_irq {
	struct eventfd_ctx	*event_ctx;
	bool			in_use;
	bool			enabled;
	irq_handler_t		handler;
	void			*arg;
};

/* per dma request */
struct qdma_stream_req_cb {
	struct qdma_request	*req;
	struct qdma_stream_iocb *iocb;
	struct drm_xocl_bo	*xobj;
	struct drm_xocl_unmgd	unmgd;
	u32			nsg;
	bool			is_unmgd;
};

/* per i/o request, may contain > 1 dma requests */
struct qdma_stream_iocb {
	struct qdma_stream_ioreq  *ioreq;
	struct qdma_stream_queue *queue;
	struct work_struct	work;
	struct kiocb		*kiocb;
	unsigned long		req_count;
	spinlock_t		lock;
	bool			cancel;
	/* completion stats */
	ssize_t			res2;
	unsigned long		cmpl_count;
	unsigned long		err_cnt;
	/* dma request list */
	struct qdma_stream_req_cb	*reqcb;
	struct qdma_request	*reqv;
	struct qdma_sw_sg	*sgl;
};

struct qdma_stream_ioreq {
	struct list_head list;
	struct qdma_stream_iocb iocb;
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
	struct stmc_queue_conf  sqconf;
	u32			state;
	spinlock_t		qlock;
	unsigned long		refcnt;
	wait_queue_head_t 	wq;
	unsigned int		flowid;
	unsigned int		routeid;

	struct file		*file;
	int			qfd;
	kuid_t			uid;
	spinlock_t		req_lock;
	struct list_head	req_pend_list;
	/* stats */
	unsigned int 		req_pend_cnt;
	unsigned int 		req_submit_cnt;
	unsigned int 		req_cmpl_cnt;
	unsigned int 		req_cancel_cnt;
	unsigned int 		req_cancel_cmpl_cnt;
};

struct xocl_qdma {
	unsigned long 		dma_hndl;
	struct qdma_dev_conf	dev_conf;
	struct stmc_dev		stm_dev;

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

	struct mutex		str_dev_lock;

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
	__SHOW_MEMBER(qconf, wb_status_en);
	__SHOW_MEMBER(qconf, cmpl_status_acc_en);
	__SHOW_MEMBER(qconf, cmpl_status_pend_chk);
	__SHOW_MEMBER(qconf, desc_bypass);
	__SHOW_MEMBER(qconf, pfetch_en);
	__SHOW_MEMBER(qconf, st_pkt_mode);
	__SHOW_MEMBER(qconf, cmpl_rng_sz_idx);
	__SHOW_MEMBER(qconf, cmpl_desc_sz);
	__SHOW_MEMBER(qconf, cmpl_stat_en);
	__SHOW_MEMBER(qconf, cmpl_udd_en);
	__SHOW_MEMBER(qconf, cmpl_timer_idx);
	__SHOW_MEMBER(qconf, cmpl_cnt_th_idx);
	__SHOW_MEMBER(qconf, cmpl_trig_mode);
	__SHOW_MEMBER(qconf, cmpl_en_intr);
#if 0
	__SHOW_MEMBER(qconf, cdh_max);
	__SHOW_MEMBER(qconf, pipe_gl_max);
	__SHOW_MEMBER(qconf, pipe_flow_id);
	__SHOW_MEMBER(qconf, pipe_slr_id);
	__SHOW_MEMBER(qconf, pipe_tdest);
#endif
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
	int off = 0;
#if 0
	struct mm_channel *channel = dev_get_drvdata(dev);
	struct qdma_queue_stats stat, *pstat;

	if (qdma_queue_get_stats((unsigned long)channel->qdma->dma_handle,
                                channel->queue, &stat) < 0)
                return sprintf(buf, "Input invalid\n");

        pstat = &stat;

        __SHOW_MEMBER(pstat, pending_bytes);
        __SHOW_MEMBER(pstat, pending_requests);
        __SHOW_MEMBER(pstat, complete_bytes);
        __SHOW_MEMBER(pstat, complete_requests);
#endif 
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
		qconf->q_type == Q_C2H ? "r" : "w",
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

	if (queue->qconf.q_type == Q_C2H)
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
		0, queue, "%sq%d", queue->qconf.q_type == Q_C2H ? "r" : "w",
		queue->qconf.qidx);
#endif

	for (i = 0; i < QDMA_QSETS_MAX * 2; i++) {
		temp_q = queue->qdma->queues[i];
		if (!temp_q)
		       continue;
		if ((temp_q->qconf.q_type == Q_C2H) &&
		    (queue->qconf.q_type == Q_C2H) &&
		    (temp_q->flowid == queue->flowid)) {
			xocl_err(&pdev->dev,
				"flowid overlapped with queue %d", i);
			return -EINVAL;
		}

		 if (!(temp_q->qconf.q_type == Q_C2H) &&
		     !(queue->qconf.q_type == Q_C2H) &&
		     (temp_q->routeid == queue->routeid)) {
			 xocl_err(&pdev->dev,
				"routeid overlapped with queue %d", i);
			 return -EINVAL;
		 }
	}

	queue->dev.parent = &pdev->dev;
	queue->dev.release = qdma_stream_device_release;
	dev_set_drvdata(&queue->dev, queue);
	dev_set_name(&queue->dev, "%sq%d",
		queue->qconf.q_type == Q_C2H ? "r" : "w",
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

	if (queue->qconf.q_type == Q_C2H)
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
#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_qdma *qdma;

	qdma = platform_get_drvdata(pdev);

	return qdma_device_error_stat_dump(qdma->dma_hndl, buf, 0);
#else
	return 0;
#endif
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

static void fill_qdma_request_sgl(struct qdma_request *req, struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg;
	struct qdma_sw_sg *sgl = req->sgl;
	unsigned int sgcnt = sgt->nents;

	req->sgcnt = sgcnt;
	for_each_sg(sgt->sgl, sg, sgcnt, i) {
		sgl->next = sgl + 1;	
		sgl->pg = sg_page(sg);
		sgl->offset = sg->offset;
		sgl->len = sg_dma_len(sg);
		sgl->dma_addr = sg_dma_address(sg);
		sgl++;
	}
	req->sgl[sgcnt - 1].next = NULL;
}

static ssize_t qdma_migrate_bo(struct platform_device *pdev,
	struct sg_table *sgt, u32 write, u64 paddr, u32 channel, u64 len)
{
	struct mm_channel *chan;
	struct xocl_qdma *qdma;
	xdev_handle_t xdev;
	struct qdma_request *req;
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
	
	req = kzalloc(sizeof(struct qdma_request) +
		      nents * sizeof(struct qdma_sw_sg),
			GFP_KERNEL);
	if (!req) {
		xocl_err(&pdev->dev, "qdma req. OOM, sgl %u.\n", nents);
		return -ENOMEM;
	}	
	req->write = write;
	req->count = len;
	req->ep_addr = paddr;
	req->timeout_ms = QDMA_REQ_TIMEOUT_MS;

	req->dma_mapped = 1;
	req->sgl = (struct qdma_sw_sg *)(req + 1);
	fill_qdma_request_sgl(req, sgt);

	ret = qdma4_request_submit(qdma->dma_hndl, chan->queue, req);

	if (ret >= 0) {
		chan->total_trans_bytes += ret;
	} else  {
		xocl_err(&pdev->dev, "DMA failed %ld, Dumping SG Page Table",
			ret);
		dump_sgtable(&pdev->dev, sgt);
	}

	pci_unmap_sg(XDEV(xdev)->pdev, sgt->sgl, nents, dir);
	kfree(req);

	return ret;
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

		ret = qdma4_queue_stop(qdma->dma_hndl, chan->queue, NULL, 0);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Stopping queue for "
				"channel %d failed, ret %x", qidx, ret);
		}
		ret = qdma4_queue_remove(qdma->dma_hndl, chan->queue, NULL, 0);
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

	if (count > sizeof(qdma->channel_bitmap[0]) * 8) {
		xocl_info(&pdev->dev, "Invalide number of channels set %d", count);
		ret = -EINVAL;
		goto failed_create_queue;
	}


	if (qdma->channel == count)
		reset = true;
	qdma->channel = count;

	sema_init(&qdma->channel_sem[0], qdma->channel);
	sema_init(&qdma->channel_sem[1], qdma->channel);

	/* Initialize bit mask to represent individual channels */
	qdma->channel_bitmap[0] = GENMASK_ULL(qdma->channel - 1, 0);
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
		qconf->wb_status_en =1;
		qconf->cmpl_status_acc_en=1;
		qconf->cmpl_status_pend_chk=1;
		qconf->fetch_credit=1;
		qconf->cmpl_stat_en=1;
		qconf->cmpl_trig_mode=1;
		qconf->desc_rng_sz_idx = MM_DEFAULT_RINGSZ_IDX;

		qconf->st = 0; /* memory mapped */
		qconf->q_type = write ? Q_H2C : Q_C2H;
		qconf->qidx = qidx;
		qconf->irq_en = (qdma->dev_conf.qdma_drv_mode == POLL_MODE) ?
					0 : 1;

		ret = qdma4_queue_add(qdma->dma_hndl, qconf, &chan->queue,
					ebuf, MM_EBUF_LEN);
		if (ret < 0) {
			pr_err("Creating queue failed, ret=%d, %s\n", ret, ebuf);
			goto failed_create_queue;
		}
		ret = qdma4_queue_start(qdma->dma_hndl, chan->queue, ebuf,
					MM_EBUF_LEN);
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
	else
		xocl_info(&qdma->pdev->dev, "user irq %d not in use", irq);
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

static void queue_req_free(struct qdma_stream_queue *queue,
			struct qdma_stream_ioreq *io_req,
			bool completed)
{
	spin_lock_bh(&queue->req_lock);

	if (completed) {
		if (io_req->iocb.cancel)
			queue->req_cancel_cmpl_cnt++;
		else
			queue->req_cmpl_cnt++;
	}

	queue->req_pend_cnt--;
	list_del(&io_req->list);
	spin_unlock_bh(&queue->req_lock);

	kfree(io_req);
}

static void inline cmpl_aio(struct kiocb *kiocb, unsigned int done_bytes,
		int error)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	kiocb->ki_complete(kiocb, done_bytes, error);
#else
	struct qdma_stream_iocb *iocb;

	iocb = (struct qdma_stream_iocb *)kiocb->private;

	if (iocb->cancel)
		atomic_set(&kiocb->ki_users, 1);
	aio_complete(kiocb, done_bytes, error);
#endif
}

static void cmpl_aio_cancel(struct work_struct *work)
{
	struct qdma_stream_iocb *iocb = container_of(work,
				struct qdma_stream_iocb, work);

	spin_lock_bh(&iocb->lock);
	if (iocb->kiocb) {
		cmpl_aio(iocb->kiocb, 0, -ECANCELED);
		iocb->kiocb = NULL;
	}
	spin_unlock_bh(&iocb->lock);
}

static void queue_req_release_resource(struct qdma_stream_queue *queue,
		struct qdma_stream_req_cb *reqcb)
{
	if (reqcb->is_unmgd) {
		xdev_handle_t xdev = xocl_get_xdev(queue->qdma->pdev);

		pci_unmap_sg(XDEV(xdev)->pdev, reqcb->unmgd.sgt->sgl,
			     reqcb->nsg, queue->qconf.q_type == Q_C2H ?  DMA_FROM_DEVICE :
			    				     DMA_TO_DEVICE);
		xocl_finish_unmgd(&reqcb->unmgd);
	} else {
		BUG_ON(!reqcb->xobj);

		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&reqcb->xobj->base);
	}

	reqcb->xobj = NULL;
}


static int queue_req_complete(struct qdma_request *req, unsigned int done_bytes,
				int error)
{
	struct qdma_stream_req_cb *reqcb =
				(struct qdma_stream_req_cb *)req->uld_data;
	struct qdma_stream_iocb *iocb = reqcb->iocb;
	struct qdma_stream_queue *queue = iocb->queue;
	bool free_req = false;

	xocl_dbg(&queue->qdma->pdev->dev,
		"q 0x%lx, reqcb 0x%p,err %d, %u,%u, %u,%u, pend %u.\n",
		queue->queue, reqcb, error, queue->req_submit_cnt,
		queue->req_cmpl_cnt, queue->req_cancel_cnt,
		queue->req_cancel_cmpl_cnt, queue->req_pend_cnt);

	queue_req_release_resource(queue, reqcb);

	spin_lock_bh(&iocb->lock);
	if (error < 0) {
		iocb->res2 |= error;
		iocb->err_cnt++;
	}
	iocb->cmpl_count++;

	/* if aio cancel already called on the request, kiocb could be NULL */
	if (iocb->cmpl_count == iocb->req_count) {
		if (iocb->kiocb) {
			cmpl_aio(iocb->kiocb, done_bytes, iocb->res2);
			iocb->kiocb = NULL;
		}
		free_req = true;
	}
	spin_unlock_bh(&iocb->lock);

	if (free_req)
		queue_req_free(queue, iocb->ioreq, true);

	return 0;
}


static ssize_t queue_rw(struct xocl_qdma *qdma, struct qdma_stream_queue *queue,
			bool write, const struct iovec *iov, unsigned long nr,
			struct kiocb *kiocb)
{
	xdev_handle_t xdev = xocl_get_xdev(qdma->pdev);
	enum dma_data_direction dir = write ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
	bool eot;
	struct qdma_stream_ioreq  *ioreq = NULL;
	struct qdma_stream_iocb *iocb = NULL;
	struct qdma_stream_req_cb *reqcb;
	struct qdma_request *req;
	unsigned long reqcnt = nr >> 1;
	unsigned long i = 0;
	long ret = 0;
	bool pend = false;

	xocl_dbg(&qdma->pdev->dev, "Read / Write Queue 0x%lx",
		queue->queue);
	if (nr < 2 || (nr & 0x1) ) {
		xocl_err(&qdma->pdev->dev, "%s dma iov %lu",
			write ? "W":"R", nr);
		return -EINVAL;
	}

	if (!kiocb && reqcnt > 1) {
		xocl_err(&qdma->pdev->dev, "sync %s dma iov %lu > 2",
			write ? "W":"R", nr);
		return -EINVAL;
	}

	ioreq = kzalloc(sizeof(struct qdma_stream_ioreq) + 
			reqcnt * (sizeof(struct qdma_request) +
				    sizeof(struct qdma_stream_req_cb) +
				sizeof(struct qdma_sw_sg)),
			GFP_KERNEL);
	if (!ioreq) {
		xocl_err(&qdma->pdev->dev,
			"%s, queue 0x%lx io request OOM, %s, iov %lu",
			dev_name(&qdma->pdev->dev), queue->queue,
			write ? "W":"R", nr);
		return -ENOMEM;
	}

	spin_lock(&queue->qlock);
	if (queue->state == QUEUE_STATE_CLEANUP) {
		xocl_err(&qdma->pdev->dev, "Invalid queue state");
		spin_unlock(&queue->qlock);
		kfree(ioreq);
		return -EINVAL;
	}
	queue->refcnt++;
	spin_unlock(&queue->qlock);

	iocb = &ioreq->iocb;
	spin_lock_init(&iocb->lock);
	iocb->ioreq = ioreq;
	iocb->queue = queue;
	iocb->kiocb = kiocb;
	iocb->req_count = reqcnt;
	iocb->reqcb = reqcb = (struct qdma_stream_req_cb *)(ioreq + 1);
	iocb->reqv = req = (struct qdma_request *)(iocb->reqcb + reqcnt);
	iocb->sgl = (struct qdma_sw_sg *)(iocb->reqv + reqcnt);
	if (kiocb)
		kiocb->private = ioreq;

	for (i = 0; i < reqcnt; i++, iov++, reqcb++, req++) {
		struct vm_area_struct *vma;
		struct drm_xocl_unmgd unmgd;
		unsigned long buf = (unsigned long)iov->iov_base;
                size_t sz;
		u32 nents;
		struct xocl_qdma_req_header header = {.flags = 0UL};

		req->sgl = iocb->sgl + i;
		req->dma_mapped = 1;

		if (iov->iov_base && copy_from_user((void *)&header,
					iov->iov_base,
					sizeof (struct xocl_qdma_req_header))) {
			xocl_err(&qdma->pdev->dev, "copy header failed.");
			ret = -EFAULT;
			goto error_out;
		}
       		eot = (header.flags & XOCL_QDMA_REQ_FLAG_EOT) ? true : false;
		iov++;

		buf = (unsigned long)iov->iov_base;
                sz = iov->iov_len;

		reqcb->req = req;
		reqcb->iocb = iocb;

		req->uld_data = (unsigned long)reqcb;
		req->write = write;
		req->count = sz;

		if (kiocb)
			req->fp_done = queue_req_complete;
		if (eot)
			req->h2c_eot = 1;

		if (sz == 0)
			continue;

		if (!write && !eot && (sz & 0xfff)) {
			xocl_err(&qdma->pdev->dev,
				"H2C w/o EOT, sz 0x%lx != N*4K", sz);
			ret = -EINVAL;
			goto error_out;
		}

		vma = find_vma(current->mm, buf);
		if (vma && (vma->vm_ops == &qdma_stream_vm_ops)) {
			struct drm_gem_object *gem_obj = vma->vm_private_data;
			struct drm_xocl_bo *xobj;

			if (vma->vm_start > buf || vma->vm_end <= buf + sz) {
				xocl_err(&qdma->pdev->dev,
					"invalid BO address 0x%lx, 0x%lx~0x%lx",
					buf, vma->vm_start, vma->vm_end);
				ret = -EINVAL;
				goto error_out;
			}

			XOCL_DRM_GEM_OBJECT_GET(gem_obj);
			xobj = to_xocl_bo(gem_obj);

			fill_qdma_request_sgl(req, xobj->sgt);

			reqcb->xobj = xobj;
			reqcb->is_unmgd = false;

			continue;
		}

		ret = xocl_init_unmgd(&unmgd, (uint64_t)buf, sz, write);
		if (ret) {
			xocl_err(&qdma->pdev->dev,
				"Init unmgd buf failed, ret=%ld", ret);
			ret = -EFAULT;
			goto error_out;
		}

		nents = pci_map_sg(XDEV(xdev)->pdev, unmgd.sgt->sgl,
			unmgd.sgt->orig_nents, dir);
		if (!nents) {
			xocl_err(&qdma->pdev->dev, "map sgl failed");
			xocl_finish_unmgd(&unmgd);
			ret = -EFAULT;
			goto error_out;
		}
if (nents != 1) {
	xocl_err(&qdma->pdev->dev, "sgcnt %d > 1", nents);
	xocl_finish_unmgd(&unmgd);
	ret = -EFAULT;
	goto error_out;
}

		req->sgl = iocb->sgl + i;
		req->dma_mapped = 1;
		fill_qdma_request_sgl(req, unmgd.sgt);

		memcpy(&reqcb->unmgd, &unmgd, sizeof (unmgd));
		reqcb->is_unmgd = true;
		reqcb->nsg = nents;
	}

	spin_lock_bh(&queue->req_lock);
	queue->req_pend_cnt++;
	list_add_tail(&ioreq->list, &queue->req_pend_list);
	spin_unlock_bh(&queue->req_lock);
	pend = true;

	xocl_dbg(&qdma->pdev->dev,
		"%s, ST %s req 0x%p, hndl 0x%lx,0x%lx.\n",
		__func__, write ? "W":"R", ioreq, qdma->dma_hndl, queue->queue);

#if 0
	if (reqcnt > 1)
		ret = qdma4_batch_request_submit(qdma->dma_hndl, queue->queue,
						reqcnt, iocb->reqv);
	else
#endif
		ret = qdma4_request_submit(qdma->dma_hndl, queue->queue,
					  iocb->reqv); 

error_out:
	if (ret < 0 || !kiocb) {
		xocl_dbg(&qdma->pdev->dev, "%s ret %ld, kiocb 0x%p.\n",
			  __func__, ret, (void *)kiocb);

		for (i = 0, reqcb = iocb->reqcb; i < reqcnt; i++, reqcb++)
			queue_req_release_resource(queue, reqcb);

		if (pend) {
			spin_lock_bh(&queue->req_lock);
			queue->req_pend_cnt--;
			if (!ret)
				queue->req_cmpl_cnt++;
			list_del(&ioreq->list);
			spin_unlock_bh(&queue->req_lock);
		}
		kfree(ioreq);
	} else {

		spin_lock_bh(&queue->req_lock);
		queue->req_submit_cnt++;
		spin_unlock_bh(&queue->req_lock);
		ret = -EIOCBQUEUED;
	}

	spin_lock(&queue->qlock);
	queue->refcnt--;
	if (!queue->refcnt && queue->state == QUEUE_STATE_CLEANUP)
		wake_up(&queue->wq);
	spin_unlock(&queue->qlock);

	return ret;
}

static int queue_wqe_cancel(struct kiocb *kiocb)
{
	struct qdma_stream_ioreq *ioreq = (struct qdma_stream_ioreq *)
					kiocb->private;
	struct qdma_stream_iocb *iocb = &ioreq->iocb;
	struct qdma_stream_queue *queue = ioreq->iocb.queue;
	struct xocl_qdma *qdma = queue->qdma;
	struct qdma_stream_req_cb *reqcb = iocb->reqcb;
	unsigned long flags;

	xocl_dbg(&qdma->pdev->dev,
		"%s cancel ST req 0x%p/0x%lu hndl 0x%lx,0x%lx, %s %u.\n",
		__func__, iocb->reqv, iocb->req_count, qdma->dma_hndl,
		queue->queue,
		(queue->qconf.q_type == Q_C2H) ? "R":"W", reqcb->req->count);

	spin_lock_irqsave(&queue->req_lock, flags);
	iocb->cancel = 1;
	queue->req_cancel_cnt++;;
	spin_unlock_irqrestore(&queue->req_lock, flags);

	/* delayed aio cancel completion */
	INIT_WORK(&iocb->work, cmpl_aio_cancel);
	schedule_work(&iocb->work);

	qdma4_request_cancel(qdma->dma_hndl, queue->queue, iocb->reqv,
				iocb->req_count);

	return -EINPROGRESS;
}

static ssize_t queue_aio_read(struct kiocb *kiocb, const struct iovec *iov,
			unsigned long nr, loff_t off)
{
	struct qdma_stream_queue	*queue;
	struct xocl_qdma	*qdma;

	queue = (struct qdma_stream_queue *)kiocb->ki_filp->private_data;
	qdma = queue->qdma;

	if (nr < 2) {
		xocl_err(&qdma->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (is_sync_kiocb(kiocb)) {
		return queue_rw(qdma, queue, false, iov, nr, NULL);
	}

	kiocb_set_cancel_fn(kiocb, (kiocb_cancel_fn *)queue_wqe_cancel);

	return queue_rw(qdma, queue, false, iov, nr, kiocb);
}

static ssize_t queue_aio_write(struct kiocb *kiocb, const struct iovec *iov,
			unsigned long nr, loff_t off)
{
	struct qdma_stream_queue	*queue;
	struct xocl_qdma	*qdma;

	queue = (struct qdma_stream_queue *)kiocb->ki_filp->private_data;
	qdma = queue->qdma;

	if (nr < 2) {
		xocl_err(&qdma->pdev->dev, "Invalid request nr = %ld", nr);
		return -EINVAL;
	}

	if (is_sync_kiocb(kiocb)) {
		return queue_rw(qdma, queue, true, iov, nr, NULL);
	}

	kiocb_set_cancel_fn(kiocb, (kiocb_cancel_fn *)queue_wqe_cancel);

	return queue_rw(qdma, queue, true, iov, nr, kiocb);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t queue_write_iter(struct kiocb *kiocb, struct iov_iter *io)
{
	struct qdma_stream_queue *queue;
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

	return queue_rw(qdma, queue, true, io->iov, nr, NULL);
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
	return queue_rw(qdma, queue, false, io->iov, nr, NULL);
}
#endif

static int queue_flush(struct qdma_stream_queue *queue)
{
	struct xocl_qdma *qdma;
	long	ret = 0;

	qdma = queue->qdma;

	xocl_info(&qdma->pdev->dev, "Release Queue 0x%lx", queue->queue);
	spin_lock(&queue->qlock);
	if (queue->state != QUEUE_STATE_INITIALIZED) {
		xocl_info(&qdma->pdev->dev, "Already released 0x%lx",
				queue->queue);
		spin_unlock(&queue->qlock);
		return 0;
	}
	queue->state = QUEUE_STATE_CLEANUP;
	spin_unlock(&queue->qlock);

	wait_event(queue->wq, queue->refcnt == 0);

	mutex_lock(&qdma->str_dev_lock);
	qdma_stream_sysfs_destroy(queue);
	if (queue->qconf.q_type == Q_C2H)
		qdma->queues[queue->qconf.qidx] = NULL;
	else
		qdma->queues[QDMA_QSETS_MAX + queue->qconf.qidx] = NULL;
	mutex_unlock(&qdma->str_dev_lock);

	ret = qdma4_queue_stop(qdma->dma_hndl, queue->queue, NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev,
			"Stop queue failed ret = %ld", ret);
		return ret;
	}

	if (queue->qconf.st)
		stmc_queue_context_cleanup(&qdma->stm_dev, &queue->sqconf);

	ret = qdma4_queue_remove(qdma->dma_hndl, queue->queue, NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev,
			"Destroy queue failed ret = %ld", ret);
		return ret;
	}

	spin_lock_bh(&queue->req_lock);
	while (!list_empty(&queue->req_pend_list)) {
		struct qdma_stream_ioreq *ioreq = list_first_entry(
						&queue->req_pend_list,
						struct qdma_stream_ioreq,
						list);
		struct qdma_stream_iocb *iocb = &ioreq->iocb;
		struct qdma_stream_req_cb *reqcb = iocb->reqcb;
		int i;

		spin_unlock_bh(&queue->req_lock);
		for (i = 0; i < iocb->req_count; i++, reqcb++) {
			xocl_info(&qdma->pdev->dev,
				"Queue 0x%lx, cancel ioreq 0x%p,%d/%lu,0x%p, 0x%x",
				queue->queue, ioreq, i, iocb->req_count,
				reqcb->req, reqcb->req->count);
			queue_req_complete(reqcb->req, 0, -ECANCELED);
		}
		spin_lock_bh(&queue->req_lock);
	}
	spin_unlock_bh(&queue->req_lock);

	return ret;
}

static long queue_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_qdma *qdma;
	struct qdma_stream_queue *queue;
	long result = 0;

	queue = (struct qdma_stream_queue *)filp->private_data;
	qdma = queue->qdma;

	switch (cmd) {
	case XOCL_QDMA_IOC_QUEUE_FLUSH:
		result = queue_flush(queue);
		break;
	default:
		xocl_err(&qdma->pdev->dev, "Invalid request %u", cmd & 0xff);
		result = -EINVAL;
		break;
	}

	return result;
}

static int queue_close(struct inode *inode, struct file *file)
{
	struct xocl_qdma *qdma;
	struct qdma_stream_queue *queue;

	queue = (struct qdma_stream_queue *)file->private_data;
	if (!queue) 
		return 0;

	queue_flush(queue);

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
		.unlocked_ioctl = queue_ioctl,
};

/* stream device file operations */
static long qdma4_stream_ioctl_create_queue(struct xocl_qdma *qdma,
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
	spin_lock_init(&queue->req_lock);
	spin_lock_init(&queue->qlock);
	init_waitqueue_head(&queue->wq);

	qconf = &queue->qconf;
	qconf->quld = (unsigned long)queue;
	qconf->st = 1; /* stream queue */
	qconf->qidx = QDMA_QUEUE_IDX_INVALID; /* request libqdma to alloc */

	if (!req.write) {
		/* C2H */
		qconf->cmpl_desc_sz = DESC_SZ_8B;
		qconf->c2h_buf_sz_idx = 0;	/* 4K */
		qconf->cmpl_trig_mode = TRIG_MODE_ANY;
		qconf->cmpl_en_intr = (qdma->dev_conf.qdma_drv_mode == POLL_MODE) ?  0 : 1;

		qconf->q_type = Q_C2H;
		qconf->desc_rng_sz_idx = qdma->c2h_ringsz_idx;
		qconf->cmpl_rng_sz_idx = qdma->wrb_ringsz_idx;

		qconf->init_pidx_dis = 1;
	} else {
		/* H2C */
		qconf->q_type = Q_H2C;
		qconf->desc_bypass = 1;
		qconf->desc_rng_sz_idx = qdma->h2c_ringsz_idx;
		qconf->fp_bypass_desc_fill = stmc_req_bypass_desc_fill;
	}
	qconf->wb_status_en =1;
	qconf->fetch_credit=1;
	qconf->cmpl_status_acc_en=1;
	qconf->cmpl_status_pend_chk=1;
	qconf->cmpl_stat_en=1;
	qconf->cmpl_trig_mode=1;
	qconf->irq_en = (qdma->dev_conf.qdma_drv_mode == POLL_MODE) ?  0 : 1;
	qconf->init_pidx_dis = 1;


	queue->flowid = req.flowid;
	queue->routeid = req.rid;

	ret = qdma4_queue_add(qdma->dma_hndl, qconf, &queue->queue, NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "Adding Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = qdma4_queue_start(qdma->dma_hndl, queue->queue, NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "Starting Queue failed ret = %ld",
			ret);
		goto failed;
	}

	ret = stmc_queue_context_setup(&qdma->stm_dev, qconf, &queue->sqconf,
					req.flowid, req.rid);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, 
			"%s STM prog. Queue failed ret = %ld",
			qconf->name, ret);
		goto failed;
	}

	/* update pidx/cidx for C2H */
	if (qconf->q_type == Q_C2H) {
		ret = qdma_q_init_pointers(qdma->dma_hndl, queue->queue);
		if (ret < 0) {
			xocl_err(&qdma->pdev->dev,
				"%s update pidx/cidx failed = %ld",
				qconf->name, ret);
			goto failed;
		}
	}

	ret = qdma4_queue_get_config(qdma->dma_hndl, queue->queue, qconf, NULL, 0);
	if (ret < 0) {
		xocl_err(&qdma->pdev->dev, "Get Q conf. failed ret = %ld", ret);
		goto failed;
	}

	xocl_info(&qdma->pdev->dev,
		"Created %s Queue handle 0x%lx, idx %d, sz %d",
		qconf->q_type == Q_C2H ? "C2H" : "H2C",
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

	mutex_lock(&qdma->str_dev_lock);
	ret = qdma_stream_sysfs_create(queue);
	if (ret) {
		mutex_unlock(&qdma->str_dev_lock);
		xocl_err(&qdma->pdev->dev, "sysfs create failed");
		goto failed;
	}

	queue->uid = current_uid();
	if (queue->qconf.q_type == Q_C2H)
		qdma->queues[queue->qconf.qidx] = queue;
	else
		qdma->queues[QDMA_QSETS_MAX + queue->qconf.qidx] = queue;
	mutex_unlock(&qdma->str_dev_lock);

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
		devm_kfree(&qdma->pdev->dev, queue);
	}

	qdma4_queue_stop(qdma->dma_hndl, queue->queue, NULL, 0);
	stmc_queue_context_cleanup(&qdma->stm_dev, &queue->sqconf);
	qdma4_queue_remove(qdma->dma_hndl, queue->queue, NULL, 0);
	queue->queue = 0UL;

	return ret;
}

static long qdma4_stream_ioctl_alloc_buffer(struct xocl_qdma *qdma,
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
#if defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3)
        dmabuf = drm_gem_prime_export(&xobj->base, flags);
#else
	dmabuf = drm_gem_prime_export(XOCL_DRM(xdev)->ddev,
                               &xobj->base, flags);
#endif
#else
        dmabuf = drm_gem_prime_export(XOCL_DRM(xdev)->ddev,
                               &xobj->base, flags);
#endif
#else
	dmabuf = drm_gem_prime_export(&xobj->base, flags);
#endif
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

static long qdma4_stream_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_qdma *qdma;
	long result = 0;

	qdma = (struct xocl_qdma *)filp->private_data;

	switch (cmd) {
	case XOCL_QDMA_IOC_CREATE_QUEUE:
		result = qdma4_stream_ioctl_create_queue(qdma, (void __user *)arg);
		break;
	case XOCL_QDMA_IOC_ALLOC_BUFFER:
		result = qdma4_stream_ioctl_alloc_buffer(qdma, (void __user *)arg);
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
	.unlocked_ioctl = qdma4_stream_ioctl,
};

static int qdma4_csr_prog_ta(struct pci_dev *pdev, int bar,
				resource_size_t base)
{
	resource_size_t bar_start;
	void __iomem    *regs;

	bar_start = pci_resource_start(pdev, bar);
        regs = ioremap_nocache(bar_start + base, 0x4000);
        if (!regs) {
                pr_warn("%s unable to map csr bar %d, base 0x%lx.\n",
			dev_name(&pdev->dev), bar, (unsigned long)base);
                return -EINVAL;
        }

	/* To enable slave bridge:
	 * First entry of the BDF table programming.
	 * Offset	Program Value	Register info
	 * 0x2420	0x0	Address translation value Low
	 * 0x2424	0x0	Address translation value High
	 * 0x2428	0x0	PASID
	 * 0x242C	0x1	[11:0]: Function Number
	 * 0x2430	0xC2000000	[31:30] Read/Write Access permission
	 *				[25:0] Window Size
	 * 				([25:0]*4K  = actual size of the window)
	 * 0x2434	0x0	SMID
	 */

	writel(0, regs + 0x2420);
	writel(0, regs + 0x2424);
	writel(0, regs + 0x2428);
	writel(1, regs + 0x242C);
	writel(0xC2000000, regs + 0x2430);
	writel(0, regs + 0x2434);

	iounmap(regs);
	return 0;
}

static int qdma4_probe(struct platform_device *pdev)
{
	struct xocl_qdma *qdma = NULL;
	struct qdma_dev_conf *conf;
	xdev_handle_t	xdev;
	struct resource *res = NULL;
	int	i, ret = 0, dma_bar = -1, stm_bar = -1;
	int csr_bar = -1;
	resource_size_t stm_base = -1;
	resource_size_t csr_base = -1;

	xdev = xocl_get_xdev(pdev);

	qdma = xocl_drvinst_alloc(&pdev->dev, sizeof(*qdma));
	if (!qdma) {
		xocl_err(&pdev->dev, "alloc mm dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	qdma->pdev = pdev;
	platform_set_drvdata(pdev, qdma);

	for (i = 0, res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		res;	
		res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		if (!strncmp(res->name, NODE_QDMA4, strlen(NODE_QDMA4))) {
			ret = xocl_ioaddr_to_baroff(xdev, res->start, &dma_bar,
							NULL);
			if (ret) {
				xocl_err(&pdev->dev,
					"Invalid resource %pR", res);
				return -EINVAL;
			}
		} else if (!strncmp(res->name, NODE_QDMA4_CSR,
					strlen(NODE_QDMA4_CSR))) {
			ret = xocl_ioaddr_to_baroff(xdev, res->start, &csr_bar,
							NULL);
			if (ret) {
				xocl_err(&pdev->dev,
					 "CSR: Invalid resource %pR", res);
				return -EINVAL;
			}

			csr_base = res->start -
				pci_resource_start(XDEV(xdev)->pdev, csr_bar);

		} else if (!strncmp(res->name, NODE_STM4, strlen(NODE_STM4))) {
			ret = xocl_ioaddr_to_baroff(xdev, res->start, &stm_bar,
							NULL);
			if (ret) {
				xocl_err(&pdev->dev,
					"STM Invalid resource %pR", res);
				return -EINVAL;
			}
			if (stm_bar == -1)
				return -EINVAL;

			stm_base = res->start -
				pci_resource_start(XDEV(xdev)->pdev, stm_bar);
		} else {
			xocl_err(&pdev->dev, "Unknown resource: %s", res->name);
			return -EINVAL;
		}
	}

	if (dma_bar == -1) {
		xocl_err(&pdev->dev,
			"missing resource, dma_bar %d, stm_bar %d, stm_base 0x%lx.",
			dma_bar, stm_bar, (unsigned long)stm_base);
		return -EINVAL;
	}

	conf = &qdma->dev_conf;
	memset(conf, 0, sizeof(*conf));
	conf->pdev = XDEV(xdev)->pdev;
	//conf->intr_rngsz = QDMA_INTR_COAL_RING_SIZE;
	//conf->master_pf = XOCL_DSA_IS_SMARTN(xdev) ? 0 : 1;
	conf->master_pf = 1;
	conf->qsets_base = QDMA_QSETS_BASE;
	conf->qsets_max = QDMA_QSETS_MAX;
	conf->bar_num_config = dma_bar;
	conf->bar_num_user = -1;
	conf->bar_num_bypass = -1;
	conf->no_mailbox = 1;
	conf->data_msix_qvec_max = 1;
	conf->user_msix_qvec_max = 8;
	conf->msix_qvec_max = 16;
	conf->qdma_drv_mode = qdma4_interrupt_mode;

	conf->fp_user_isr_handler = qdma_isr;
	conf->uld = (unsigned long)qdma;

	xocl_info(&pdev->dev, "dma %d, mode 0x%x.\n",
		dma_bar, conf->qdma_drv_mode);
	ret = qdma4_device_open(XOCL_MODULE_NAME, conf, &qdma->dma_hndl);
	if (ret < 0) {
		xocl_err(&pdev->dev, "QDMA Device Open failed");
		goto failed;
	}

	if (csr_bar >= 0) {
		xocl_info(&pdev->dev, "csr bar %d, base 0x%lx.",
			csr_bar, (unsigned long)csr_base);

		ret = qdma4_csr_prog_ta(XDEV(xdev)->pdev, csr_bar, csr_base);
		if (ret < 0)
			xocl_err(&pdev->dev,
				"Slave bridge BDF program failed (%d,0x%lx).",
				csr_bar, (unsigned long)csr_base);
		else
			xocl_info(&pdev->dev,
				"Slave bridge BDF programmed (%d,0x%lx).",
				csr_bar, (unsigned long)csr_base);
	}

	if (stm_bar >= 0) {
		struct stmc_dev *sdev = &qdma->stm_dev;

		xocl_info(&pdev->dev, "stm bar %d, base 0x%lx.",
			stm_bar, (unsigned long)stm_base);

		sdev->reg_base = stm_base;
		sdev->bar_num = stm_bar;
		ret = stmc_init(sdev, conf);
		if (ret < 0)
			xocl_warn(&pdev->dev, "QDMA Device STM-C failed");
	} else
		xocl_info(&pdev->dev, "QDMA Device STM-C not present");

	if (!XOCL_DSA_IS_SMARTN(xdev)) {
		ret = set_max_chan(qdma, qdma4_max_channel);
		if (ret) {
			xocl_err(&pdev->dev, "Set max channel failed");
			goto failed;
		}
	}

	ret = qdma4_device_get_config(qdma->dma_hndl, &qdma->dev_conf, NULL, 0);
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

	mutex_init(&qdma->str_dev_lock);
	spin_lock_init(&qdma->user_msix_table_lock);

	return 0;

failed:
	if (qdma) {
		free_channels(qdma->pdev);

		stmc_cleanup(&qdma->stm_dev);

		if (qdma->dma_hndl)
			qdma4_device_close(XDEV(xdev)->pdev, qdma->dma_hndl);

		xocl_drvinst_release(qdma, NULL);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int qdma4_remove(struct platform_device *pdev)
{
	struct xocl_qdma *qdma= platform_get_drvdata(pdev);
	xdev_handle_t xdev;
	struct qdma_irq *irq_entry;
	void *hdl;
	int i;

	xocl_drvinst_release(qdma, &hdl);
	sysfs_remove_group(&pdev->dev.kobj, &qdma_attrgroup);

	if (!qdma) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xdev = xocl_get_xdev(pdev);

	free_channels(pdev);

	stmc_cleanup(&qdma->stm_dev);

	qdma4_device_close(XDEV(xdev)->pdev, qdma->dma_hndl);

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


	mutex_destroy(&qdma->str_dev_lock);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

struct xocl_drv_private qdma4_priv = {
	.ops = &qdma_ops,
	.fops = &qdma_stream_fops,
	.dev = -1,
};

static struct platform_device_id qdma4_id_table[] = {
	{ XOCL_DEVNAME(XOCL_QDMA4), (kernel_ulong_t)&qdma4_priv },
	{ },
};

static struct platform_driver	qdma4_driver = {
	.probe		= qdma4_probe,
	.remove		= qdma4_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_QDMA4),
	},
	.id_table	= qdma4_id_table,
};

int __init xocl_init_qdma4(void)
{
	int		err = 0;

	qdma4_debugfs_root = debugfs_create_dir("qdma4_dev", NULL);
	if (!qdma4_debugfs_root) {
		pr_err("%s: Failed to create debugfs\n", __func__);
		return -ENODEV;
	}

	err = libqdma4_init(0, qdma4_debugfs_root);
	if (err)
		return err;
	err = alloc_chrdev_region(&str_dev, 0, XOCL_CHARDEV_REG_COUNT,
			XOCL_QDMA);
	if (err < 0)
		goto err_reg_chrdev;

	err = platform_driver_register(&qdma4_driver);
	if (err)
		goto err_drv_reg;

	return 0;

err_drv_reg:
	unregister_chrdev_region(str_dev, XOCL_CHARDEV_REG_COUNT);
err_reg_chrdev:
	libqdma4_exit();

	if (qdma4_debugfs_root) {
		debugfs_remove_recursive(qdma4_debugfs_root);
		qdma4_debugfs_root = NULL;
	}

	return err;
}

void xocl_fini_qdma4(void)
{
	unregister_chrdev_region(str_dev, XOCL_CHARDEV_REG_COUNT);
	platform_driver_unregister(&qdma4_driver);
	libqdma4_exit();
	if (qdma4_debugfs_root) {
		debugfs_remove_recursive(qdma4_debugfs_root);
		qdma4_debugfs_root = NULL;
	}
}
