/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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
#include "../lib/libxdma_api.h"

#define XOCL_FILE_PAGE_OFFSET   0x100000
#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

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
	unsigned long long	*channel_usage[2];

	struct mutex		stat_lock;
};

static ssize_t xdma_migrate_bo(struct platform_device *pdev,
	struct sg_table *sgt, u32 dir, u64 paddr, u32 channel, u64 len)
{
	struct xocl_mm_device *mdev;
	struct xocl_dev *xdev;
	struct page *pg;
	struct scatterlist *sg = sgt->sgl;
	int nents = sgt->orig_nents;
	pid_t pid = current->pid;
	int i = 0;
	ssize_t ret;
	unsigned long long pgaddr;

	mdev = platform_get_drvdata(pdev);
	xocl_dbg(&pdev->dev, "TID %d, Channel:%d, Offset: 0x%llx, Dir: %d",
		pid, channel, paddr, dir);
	xdev = xocl_get_xdev(pdev);
	ret = xdma_xfer_submit(xdev->dma_handle, channel, dir,
		paddr, sgt, false, 10000);
	if (ret >= 0) {
		mdev->channel_usage[dir][channel] += ret;
		return ret;
	}

	xocl_err(&pdev->dev, "DMA failed, Dumping SG Page Table");
	for (i = 0; i < nents; i++, sg = sg_next(sg)) {
        if (!sg)
            break;
		pg = sg_page(sg);
		if (!pg)
			continue;
		pgaddr = page_to_phys(pg);
		xocl_err(&pdev->dev, "%i, 0x%llx\n", i, pgaddr);
	}
	return ret;
}

static int acquire_channel(struct platform_device *pdev, u32 dir)
{
	struct xocl_mm_device *mdev;
	int channel = 0;
	int result = 0;

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
	}

out:
	return channel;
}

static void release_channel(struct platform_device *pdev, u32 dir, u32 channel)
{
	struct xocl_mm_device *mdev;


	mdev = platform_get_drvdata(pdev);
        set_bit(channel, &mdev->channel_bitmap[dir]);
        up(&mdev->channel_sem[dir]);
}

static int set_max_chan(struct platform_device *pdev, u32 count)
{
	struct xocl_mm_device *mdev;

	mdev = platform_get_drvdata(pdev);
	mdev->channel = count;

	mdev->channel_usage[0] = devm_kzalloc(&pdev->dev, sizeof (u64) *
		mdev->channel, GFP_KERNEL);
	mdev->channel_usage[1] = devm_kzalloc(&pdev->dev, sizeof (u64) *
		mdev->channel, GFP_KERNEL);
	if (!mdev->channel_usage[0] || !mdev->channel_usage[1]) {
		xocl_err(&pdev->dev, "failed to alloc channel usage");
		return -ENOMEM;
	}

	sema_init(&mdev->channel_sem[0], mdev->channel);
	sema_init(&mdev->channel_sem[1], mdev->channel);

	/* Initialize bit mask to represent individual channels */
	mdev->channel_bitmap[0] = BIT(mdev->channel);
	mdev->channel_bitmap[0]--;
	mdev->channel_bitmap[1] = mdev->channel_bitmap[0];

	return 0;
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

        return mdev->channel_usage[write][channel];
}

static struct xocl_mm_dma_funcs mm_ops = {
	.migrate_bo = xdma_migrate_bo,
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
	xdev_handle_t	xdev_hdl;

	xdev_hdl = xocl_get_xdev(pdev);
	BUG_ON(!xdev_hdl);

	xocl_info(&pdev->dev, "XDMA detected");
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
		if (mdev->channel_usage[0])
			devm_kfree(&pdev->dev, mdev->channel_usage[0]);
		if (mdev->channel_usage[1])
			devm_kfree(&pdev->dev, mdev->channel_usage[1]);

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

	if (mdev->channel_usage[0])
		devm_kfree(&pdev->dev, mdev->channel_usage[0]);
	if (mdev->channel_usage[1])
		devm_kfree(&pdev->dev, mdev->channel_usage[1]);

	mutex_destroy(&mdev->stat_lock);

	devm_kfree(&pdev->dev, mdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_device_id mm_dma_id_table[] = {
	{ XOCL_MM_XDMA, 0 },
	{ },
};

static struct platform_driver	mm_dma_driver = {
	.probe		= mm_dma_probe,
	.remove		= mm_dma_remove,
	.driver		= {
		.name = "xocl_mm_xdma",
	},
	.id_table	= mm_dma_id_table,
};

int __init xocl_init_mm_xdma(void)
{
	return platform_driver_register(&mm_dma_driver);
}

void xocl_fini_mm_xdma(void)
{
	return platform_driver_unregister(&mm_dma_driver);
}
