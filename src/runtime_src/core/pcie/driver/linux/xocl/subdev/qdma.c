/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
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
#include "../lib/libqdma/QDMA/linux-kernel/driver/libqdma/libqdma_export.h"
#include "../lib/libqdma/QDMA/linux-kernel/driver/libqdma/qdma_ul_ext.h"
#include "qdma_ioctl.h"

#define XOCL_FILE_PAGE_OFFSET   0x100000
#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define	MM_QUEUE_LEN		8
#define	MM_EBUF_LEN		256

#define MM_DEFAULT_RINGSZ_IDX	0

#define	MINOR_NAME_MASK		0xffffffff

#define QDMA_MAX_INTR		16
#define QDMA_USER_INTR_MASK	0xffff

#define QDMA_QSETS_MAX		256
#define QDMA_QSETS_BASE		0

#define QDMA_REQ_TIMEOUT_MS	10000

/* Module Parameters */
unsigned int qdma_max_channel = 8;
module_param(qdma_max_channel, uint, 0644);
MODULE_PARM_DESC(qdma_max_channel, "Set number of channels for qdma, default is 8");

static unsigned int qdma_interrupt_mode = DIRECT_INTR_MODE;
module_param(qdma_interrupt_mode, uint, 0644);
MODULE_PARM_DESC(interrupt_mode, "0:auto, 1:poll, 2:direct, 3:intr_ring, default is 2");

struct dentry *qdma_debugfs_root;

static dev_t	str_dev;

struct qdma_irq {
	struct eventfd_ctx	*event_ctx;
	bool			in_use;
	bool			enabled;
	irq_handler_t		handler;
	void			*arg;
};

enum {
	QUEUE_STATE_INITIALIZED,
	QUEUE_STATE_CLEANUP,
};

struct xocl_qdma {
	unsigned long 		dma_hndl;
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

	struct mutex		str_dev_lock;

	u16			instance;

	struct qdma_irq		user_msix_table[QDMA_MAX_INTR];
	u32			user_msix_mask;
	spinlock_t		user_msix_table_lock;
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
	nents = dma_map_sg(&XDEV(xdev)->pdev->dev, sgt->sgl, sgt->orig_nents,
			   dir);
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

	ret = qdma_request_submit(qdma->dma_hndl, chan->queue, req);

	if (ret >= 0) {
		chan->total_trans_bytes += ret;
	} else  {
		xocl_err(&pdev->dev, "DMA failed %ld, Dumping SG Page Table",
			ret);
		dump_sgtable(&pdev->dev, sgt);
	}

	dma_unmap_sg(&XDEV(xdev)->pdev->dev, sgt->sgl, nents, dir);
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
	char *ebuf;

	qdma = platform_get_drvdata(pdev);
	if (!qdma || !qdma->channel)
		return;

	ebuf = devm_kzalloc(&pdev->dev, MM_EBUF_LEN, GFP_KERNEL);
	if (ebuf == NULL) {
		xocl_err(&pdev->dev, "Alloc ebuf mem failed");
		return;
	}

	for (i = 0; i < qdma->channel * 2; i++) {
		memset(ebuf, 0, MM_EBUF_LEN);
		write = i / qdma->channel;
		qidx = i % qdma->channel;
		chan = &qdma->chans[write][qidx];

		channel_sysfs_destroy(chan);

		ret = qdma_queue_stop(qdma->dma_hndl, chan->queue, ebuf, MM_EBUF_LEN);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Stopping queue for "
				"channel %d failed, ret: %x, ebuf: %s", qidx, ret, ebuf);
		}
		ret = qdma_queue_remove(qdma->dma_hndl, chan->queue, ebuf, MM_EBUF_LEN);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Destroy queue for "
				"channel %d failed, ret: %x, ebuf: %s", qidx, ret, ebuf);
		}
	}
	if (ebuf)
		devm_kfree(&pdev->dev, ebuf);
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

		ret = qdma_queue_add(qdma->dma_hndl, qconf, &chan->queue,
					ebuf, MM_EBUF_LEN);
		if (ret < 0) {
			pr_err("Creating queue failed, ret=%d, %s\n", ret, ebuf);
			goto failed_create_queue;
		}
		ret = qdma_queue_start(qdma->dma_hndl, chan->queue, ebuf,
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

static int qdma_csr_prog_ta(struct pci_dev *pdev, int bar,
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

static int qdma_probe(struct platform_device *pdev)
{
	struct xocl_qdma *qdma = NULL;
	struct qdma_dev_conf *conf;
	xdev_handle_t	xdev;
	struct resource *res = NULL;
	int	i, ret = 0, dma_bar = -1;
	int csr_bar = -1;
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
		if (!strncmp(res->name, NODE_QDMA, strlen(NODE_QDMA))) {
			ret = xocl_ioaddr_to_baroff(xdev, res->start, &dma_bar,
							NULL);
			if (ret) {
				xocl_err(&pdev->dev,
					"Invalid resource %pR", res);
				return -EINVAL;
			}
		} else if (!strncmp(res->name, NODE_QDMA_CSR,
					strlen(NODE_QDMA_CSR))) {
			ret = xocl_ioaddr_to_baroff(xdev, res->start, &csr_bar,
							NULL);
			if (ret) {
				xocl_err(&pdev->dev,
					 "CSR: Invalid resource %pR", res);
				return -EINVAL;
			}

			csr_base = res->start -
				pci_resource_start(XDEV(xdev)->pdev, csr_bar);
		} else {
			xocl_err(&pdev->dev, "Unknown resource: %s", res->name);
			return -EINVAL;
		}
	}

	if (dma_bar == -1) {
		xocl_err(&pdev->dev, "missing resource, dma_bar %d", dma_bar);
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
	conf->data_msix_qvec_max = 1;
	conf->user_msix_qvec_max = 16;
	conf->msix_qvec_max = 32;
	conf->qdma_drv_mode = qdma_interrupt_mode;

	conf->fp_user_isr_handler = (void*)qdma_isr;
	conf->uld = (unsigned long)qdma;

	xocl_info(&pdev->dev, "dma %d, mode 0x%x.\n",
		dma_bar, conf->qdma_drv_mode);
	ret = qdma_device_open(XOCL_MODULE_NAME, conf, &qdma->dma_hndl);
	if (ret < 0) {
		xocl_err(&pdev->dev, "QDMA Device Open failed");
		goto failed;
	}

	if (csr_bar >= 0) {
		xocl_info(&pdev->dev, "csr bar %d, base 0x%lx.",
			csr_bar, (unsigned long)csr_base);

		ret = qdma_csr_prog_ta(XDEV(xdev)->pdev, csr_bar, csr_base);
		if (ret < 0)
			xocl_err(&pdev->dev,
				"Host memory BDF program failed (%d,0x%lx).",
				csr_bar, (unsigned long)csr_base);
		else
			xocl_info(&pdev->dev,
				"Host memory BDF programmed (%d,0x%lx).",
				csr_bar, (unsigned long)csr_base);
	}

	if (!XOCL_DSA_IS_SMARTN(xdev)) {
		ret = set_max_chan(qdma, qdma_max_channel);
		if (ret) {
			xocl_err(&pdev->dev, "Set max channel failed");
			goto failed;
		}
	}

	ret = qdma_device_get_config(qdma->dma_hndl, &qdma->dev_conf, NULL, 0);
	if (ret) {
		xocl_err(&pdev->dev, "Failed to get device info");
		goto failed;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &qdma_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs group failed");
		goto failed;
	}

	qdma->user_msix_mask = QDMA_USER_INTR_MASK;

	mutex_init(&qdma->str_dev_lock);
	spin_lock_init(&qdma->user_msix_table_lock);

	return 0;

failed:
	if (qdma) {
		free_channels(qdma->pdev);

		if (qdma->dma_hndl)
			qdma_device_close(XDEV(xdev)->pdev, qdma->dma_hndl);

		xocl_drvinst_release(qdma, NULL);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int __qdma_remove(struct platform_device *pdev)
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

	qdma_device_close(XDEV(xdev)->pdev, qdma->dma_hndl);

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void qdma_remove(struct platform_device *pdev)
{
	__qdma_remove(pdev);
}
#else
#define qdma_remove __qdma_remove
#endif

struct xocl_drv_private qdma_priv = {
	.ops = &qdma_ops,
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

	qdma_debugfs_root = debugfs_create_dir("qdma_dev", NULL);
	if (!qdma_debugfs_root) {
		pr_err("%s: Failed to create debugfs\n", __func__);
		return -ENODEV;
	}

	err = libqdma_init(0, qdma_debugfs_root);
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

	if (qdma_debugfs_root) {
		debugfs_remove_recursive(qdma_debugfs_root);
		qdma_debugfs_root = NULL;
	}

	return err;
}

void xocl_fini_qdma(void)
{
	unregister_chrdev_region(str_dev, XOCL_CHARDEV_REG_COUNT);
	platform_driver_unregister(&qdma_driver);
	libqdma_exit();
	if (qdma_debugfs_root) {
		debugfs_remove_recursive(qdma_debugfs_root);
		qdma_debugfs_root = NULL;
	}
}
