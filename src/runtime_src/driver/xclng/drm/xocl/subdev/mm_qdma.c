/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
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

/* XDMA version Memory Mapped DMA */

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include "../xocl_drv.h"
#include "../userpf/common.h"
#include "../lib/libqdma/libqdma_export.h"
#include "../lib/libqdma/qdma_wq.h"

#define XOCL_FILE_PAGE_OFFSET   0x100000
#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define	MM_QUEUE_LEN		8
#define	MM_EBUF_LEN		256

struct mm_channel {
	struct qdma_wq		queue;
	uint64_t		total_trans_bytes;
};
struct xocl_mm_device {
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

	struct mutex		stat_lock;
};

static ssize_t qdma_migrate_bo(struct platform_device *pdev,
	struct sg_table *sgt, u32 write, u64 paddr, u32 channel, u64 len)
{
	struct mm_channel *chan;
	struct xocl_mm_device *mdev;
	struct xocl_dev *xdev;
	struct qdma_wr wr;
	enum dma_data_direction dir;
	u32 nents;
	pid_t pid = current->pid;
	ssize_t ret;

	mdev = platform_get_drvdata(pdev);
	xocl_dbg(&pdev->dev, "TID %d, Channel:%d, Offset: 0x%llx, write: %d",
		pid, channel, paddr, write);
	xdev = xocl_get_xdev(pdev);

	memset(&wr, 0, sizeof (wr));
	wr.write = write;
	wr.len = len;
	wr.req.ep_addr = paddr;
	wr.sgt = sgt;

	chan = &mdev->chans[write][channel];

	dir = write ? DMA_TO_DEVICE : DMA_FROM_DEVICE; 
	nents = pci_map_sg(xdev->core.pdev, sgt->sgl, sgt->orig_nents, dir);
        if (!nents) {
		xocl_err(&pdev->dev, "map sgl failed, sgt 0x%p.\n", sgt);
		return -EIO;
	}
	sgt->nents = nents;

	ret = qdma_wq_post(&chan->queue, &wr);
	pci_unmap_sg(xdev->core.pdev, sgt->sgl, nents, dir);

	if (ret >= 0) {
		chan->total_trans_bytes += ret;
		return ret;
	}

	xocl_err(&pdev->dev, "DMA failed, Dumping SG Page Table");
	xocl_dump_sgtable(&pdev->dev, sgt);
	return ret;
}

static void release_channel(struct platform_device *pdev, u32 dir, u32 channel)
{
	struct xocl_mm_device *mdev;


	mdev = platform_get_drvdata(pdev);
        set_bit(channel, &mdev->channel_bitmap[dir]);
        up(&mdev->channel_sem[dir]);
}

static int acquire_channel(struct platform_device *pdev, u32 dir)
{
	struct xocl_mm_device *mdev;
	int channel = 0;
	int result = 0;
	u32 write;

	mdev = platform_get_drvdata(pdev);

	if (down_interruptible(&mdev->channel_sem[dir])) {
		channel = -ERESTARTSYS;
		goto out;
	}

	for (channel = 0; channel < mdev->channel; channel++) {
		result = test_and_clear_bit(channel,
			&mdev->channel_bitmap[dir]);
		if (result)
			break;
        }
        if (!result) {
		// How is this possible?
		up(&mdev->channel_sem[dir]);
		channel = -EIO;
		goto out;
	}

	write = dir ? 1 : 0;

	if (!(mdev->chans[write][channel].queue.flag &
		QDMA_WQ_QUEUE_STARTED)) {
		xocl_err(&pdev->dev, "queue not started, chan %d", channel);
		release_channel(pdev, dir, channel);
		channel = -EINVAL;
	}
out:
	return channel;
}

static void free_channels(struct platform_device *pdev)
{
	struct xocl_mm_device *mdev;
	struct xocl_dev *xdev;
	struct mm_channel *chan;
	u32	write, qidx;
	int i, ret = 0;

	mdev = platform_get_drvdata(pdev);

	xdev = xocl_get_xdev(pdev);
	for (i = 0; i < mdev->channel * 2; i++) {
		write = i / mdev->channel;
		qidx = i % mdev->channel;
		chan = &mdev->chans[write][qidx];

		ret = qdma_wq_destroy(&chan->queue);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Destroy queue for "
				"channel %d failed, ret %x", qidx, ret);
			return;
		}
	}
	devm_kfree(&pdev->dev, mdev->chans[0]);
	devm_kfree(&pdev->dev, mdev->chans[1]);
}

static int set_max_chan(struct platform_device *pdev, u32 count)
{
	struct xocl_mm_device *mdev;
	struct xocl_dev *xdev;
	struct qdma_queue_conf qconf;
	struct mm_channel *chan;
	u32	write, qidx;
	char	ebuf[MM_EBUF_LEN + 1];
	int	i, ret;

	mdev = platform_get_drvdata(pdev);
	mdev->channel = count;

	sema_init(&mdev->channel_sem[0], mdev->channel);
	sema_init(&mdev->channel_sem[1], mdev->channel);

	/* Initialize bit mask to represent individual channels */
	mdev->channel_bitmap[0] = BIT(mdev->channel);
	mdev->channel_bitmap[0]--;
	mdev->channel_bitmap[1] = mdev->channel_bitmap[0];

	xdev = xocl_get_xdev(pdev);

	xocl_info(&pdev->dev, "Creating MM Queues, Channel %d", mdev->channel);
	mdev->chans[0] = devm_kzalloc(&pdev->dev, sizeof (struct mm_channel) *
		mdev->channel, GFP_KERNEL);
	mdev->chans[1] = devm_kzalloc(&pdev->dev, sizeof (struct mm_channel) *
		mdev->channel, GFP_KERNEL);
	if (mdev->chans[0] == NULL || mdev->chans[1] == NULL) {
		xocl_err(&pdev->dev, "Alloc channel mem failed");
		ret = -ENOMEM;
		goto failed_create_queue;
	}

	for (i = 0; i < mdev->channel * 2; i++) {
		write = i / mdev->channel;
		qidx = i % mdev->channel;
		chan = &mdev->chans[write][qidx];

		memset(&qconf, 0, sizeof (qconf));
		memset(&ebuf, 0, sizeof (ebuf));
		qconf.wbk_en =1;
		qconf.wbk_acc_en=1;
		qconf.wbk_pend_chk=1;
		qconf.fetch_credit=1;
		qconf.cmpl_stat_en=1;
		qconf.cmpl_trig_mode=1;

		qconf.st = 0; /* memory mapped */
		qconf.c2h = write ? 0 : 1;
		qconf.qidx = qidx;
		ret = qdma_wq_create((unsigned long)xdev->dma_handle, &qconf,
			&chan->queue, 0);
		if (ret) {
			goto failed_create_queue;
		}
	}

	xocl_info(&pdev->dev, "Created %d MM channels (Queues)", mdev->channel);

	return 0;

failed_create_queue:
	free_channels(pdev);

	return ret;
}

static u32 get_channel_count(struct platform_device *pdev)
{
	struct xocl_mm_device *mdev;

        mdev = platform_get_drvdata(pdev);
        BUG_ON(!mdev);

        return mdev->channel;
}

static u64 get_channel_stat(struct platform_device *pdev, u32 channel,
	u32 write)
{
	struct xocl_mm_device *mdev;

        mdev = platform_get_drvdata(pdev);
        BUG_ON(!mdev);

        return mdev->chans[write][channel].total_trans_bytes;
}

static struct xocl_mm_dma_funcs mm_ops = {
	.migrate_bo = qdma_migrate_bo,
	.ac_chan = acquire_channel,
	.rel_chan = release_channel,
	.set_max_chan = set_max_chan,
	.get_chan_count = get_channel_count,
	.get_chan_stat = get_channel_stat,
};

static int mm_dma_probe(struct platform_device *pdev)
{
	struct xocl_mm_device	*mdev = NULL;
	int	ret = 0;

        xocl_info(&pdev->dev, "QDMA detected");
	mdev = devm_kzalloc(&pdev->dev, sizeof (*mdev), GFP_KERNEL);
	if (!mdev) {
		xocl_err(&pdev->dev, "alloc mm dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	mutex_init(&mdev->stat_lock);

	xocl_subdev_register(pdev, XOCL_SUBDEV_MM_DMA, &mm_ops);
	platform_set_drvdata(pdev, mdev);

	return 0;

failed:
	if (mdev) {
		devm_kfree(&pdev->dev, mdev);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int mm_dma_remove(struct platform_device *pdev)
{
	struct xocl_mm_device *mdev = platform_get_drvdata(pdev);

	if (!mdev) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	free_channels(pdev);

	mutex_destroy(&mdev->stat_lock);

	devm_kfree(&pdev->dev, mdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_device_id mm_dma_id_table[] = {
	{ XOCL_MM_QDMA, 0 },
	{ },
};

static struct platform_driver	mm_dma_driver = {
	.probe		= mm_dma_probe,
	.remove		= mm_dma_remove,
	.driver		= {
		.name = "xocl_mm_qdma",
	},
	.id_table	= mm_dma_id_table,
};

int __init xocl_init_mm_qdma(void)
{
	return platform_driver_register(&mm_dma_driver);
}

void xocl_fini_mm_qdma(void)
{
	return platform_driver_unregister(&mm_dma_driver);
}
