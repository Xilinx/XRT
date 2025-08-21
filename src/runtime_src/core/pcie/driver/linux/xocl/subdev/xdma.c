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
#include <linux/eventfd.h>
#include "../xocl_drv.h"
#include "../xocl_drm.h"
#include "../lib/libxdma_api.h"

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

struct xdma_irq {
	struct eventfd_ctx	*event_ctx;
	bool			in_use;
	bool			enabled;
	irq_handler_t		handler;
	void			*arg;
};

struct xocl_xdma {
	void			*dma_handle;
	u32			max_user_intr;
	u32			start_user_intr;
	struct xdma_irq		*user_msix_table;
	spinlock_t		user_msix_table_lock;

	struct xocl_drm		*drm;
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
	struct xocl_xdma *xdma;
	struct page *pg;
	struct scatterlist *sg = sgt->sgl;
	int nents = sgt->orig_nents;
	pid_t pid = current->pid;
	int i = 0;
	ssize_t ret;
	unsigned long long pgaddr;

	xdma = platform_get_drvdata(pdev);
	xocl_dbg(&pdev->dev, "TID %d, Channel:%d, Offset: 0x%llx, Dir: %d",
		pid, channel, paddr, dir);
	ret = xdma_xfer_fastpath(xdma->dma_handle, channel, dir,
		paddr, sgt, false, 10000);
	if (ret >= 0) {
		xdma->channel_usage[dir][channel] += ret;
		return ret;
	}

	xocl_err(&pdev->dev, "DMA failed, Dumping SG Page Table, ep addr %llx",
		paddr);
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

struct xdma_async_context {
	void (*callback_fn)(unsigned long data, int err);
	unsigned long callback_data;
	struct xdma_io_cb *iocb;
	struct xocl_xdma *xdma;
	u32 dir;
	u32 channel;
};

static void xdma_async_migrate_done(unsigned long data, int err)
{
	struct xdma_async_context *async_ctx = (struct xdma_async_context *)data;
	//pr_info("%s: async_ctx %llx, ", __func__, (u64)async_ctx);

	if (!err) {
		async_ctx->xdma->channel_usage
			[async_ctx->dir][async_ctx->channel] +=
						async_ctx->iocb->done_bytes;
	}

	if (async_ctx->callback_fn)
		async_ctx->callback_fn(async_ctx->callback_data, err);
	kfree(async_ctx->iocb);
	kfree(async_ctx);
}

#define MAX_REQS_ON_CHANNEL 32
atomic_t async_dma_count;

static ssize_t xdma_async_migrate_bo(struct platform_device *pdev,
	struct sg_table *sgt, u32 dir, u64 paddr, u32 channel, u64 len,
	void (*callback_fn)(unsigned long cb_hndl, int err), void *tx_ctx)
{
	struct xocl_xdma *xdma;
	struct page *pg;
	struct scatterlist *sg = sgt->sgl;
	int nents = sgt->orig_nents;
	pid_t pid = current->pid;
	int i = 0;
	ssize_t ret;
	unsigned long long pgaddr;
	struct xdma_io_cb *io_cb = NULL;
	struct xdma_async_context *async_ctx;

	xdma = platform_get_drvdata(pdev);
	xocl_dbg(&pdev->dev, "TID %d, Channel:%d, Offset: 0x%llx, Dir: %d",
		pid, channel, paddr, dir);


	if (callback_fn && tx_ctx) {
		//channel = (atomic_add_return(1, &async_dma_count)/MAX_REQS_ON_CHANNEL) % xdma->channel;
		channel = atomic_add_return(1, &async_dma_count) % xdma->channel;
		io_cb = kzalloc(sizeof(struct xdma_io_cb), GFP_KERNEL);
		if (!io_cb) {
			xocl_err(&pdev->dev, "alloc xdma dev failed");
			ret = -ENOMEM;
			goto failed;
		}

		async_ctx = kzalloc(sizeof(struct xdma_async_context), GFP_KERNEL);
		if (!async_ctx) {
			ret = -ENOMEM;
			goto failed;
		}

		async_ctx->dir = dir;
		async_ctx->xdma = xdma;
		async_ctx->channel = channel;
		async_ctx->iocb = io_cb;
		async_ctx->callback_fn = callback_fn;
		async_ctx->callback_data = (unsigned long)tx_ctx;

		//pr_info("%s: %llx %llx %llx\n", __func__, (u64)io_cb, (u64)callback_fn, (u64)tx_ctx);

		io_cb->io_done = xdma_async_migrate_done;
		io_cb->private = async_ctx;
	}

	//pr_info("%s: iocb:%llx ",__func__, (u64)io_cb);
	ret = xdma_xfer_submit(xdma->dma_handle, channel, dir,
		paddr, sgt, false, 10000, io_cb);
	if (ret >= 0) {
		xdma->channel_usage[dir][channel] += ret;
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

failed:
	kfree(io_cb);
	return ret;
}

static int acquire_channel(struct platform_device *pdev, u32 dir)
{
	struct xocl_xdma *xdma;
	int channel = 0;
	int result = 0;

	xdma = platform_get_drvdata(pdev);
	if (down_killable(&xdma->channel_sem[dir])) {
		channel = -ERESTARTSYS;
		goto out;
	}

	for (channel = 0; channel < xdma->channel; channel++) {
		result = test_and_clear_bit(channel,
			&xdma->channel_bitmap[dir]);
		if (result)
			break;
        }
        if (!result) {
		// How is this possible?
		up(&xdma->channel_sem[dir]);
		channel = -EIO;
	}

out:
	return channel;
}

static void release_channel(struct platform_device *pdev, u32 dir, u32 channel)
{
	struct xocl_xdma *xdma;


	xdma = platform_get_drvdata(pdev);
        set_bit(channel, &xdma->channel_bitmap[dir]);
        up(&xdma->channel_sem[dir]);
}

static u32 get_channel_count(struct platform_device *pdev)
{
	struct xocl_xdma *xdma;

        xdma= platform_get_drvdata(pdev);
        BUG_ON(!xdma);

        return xdma->channel;
}

static u64 get_channel_stat(struct platform_device *pdev, u32 channel,
	u32 write)
{
	struct xocl_xdma *xdma;

        xdma= platform_get_drvdata(pdev);
        BUG_ON(!xdma);

        return xdma->channel_usage[write][channel];
}

static int user_intr_config(struct platform_device *pdev, u32 intr, bool en)
{
	struct xocl_xdma *xdma;
	const unsigned int mask = 1 << intr;
	unsigned long flags;
	int ret;

	xdma= platform_get_drvdata(pdev);

	if (intr >= xdma->max_user_intr) {
		xocl_err(&pdev->dev, "Invalid intr %d, user start %d, max %d",
			intr, xdma->start_user_intr, xdma->max_user_intr);
		return -EINVAL;
	}

	spin_lock_irqsave(&xdma->user_msix_table_lock, flags);
	if (xdma->user_msix_table[intr].enabled == en) {
		ret = 0;
		goto end;
	}

	ret = en ? xdma_user_isr_enable(xdma->dma_handle, mask) :
		xdma_user_isr_disable(xdma->dma_handle, mask);
	if (!ret)
		xdma->user_msix_table[intr].enabled = en;
end:
	spin_unlock_irqrestore(&xdma->user_msix_table_lock, flags);

	return ret;
}

static irqreturn_t xdma_isr(int irq, void *arg)
{
	struct xdma_irq *irq_entry = arg;
	int ret = IRQ_HANDLED;

	if (irq_entry->handler)
		ret = irq_entry->handler(irq, irq_entry->arg);

	if (!IS_ERR_OR_NULL(irq_entry->event_ctx)) {
#if KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE || defined(RHEL_9_5_GE)
		eventfd_signal(irq_entry->event_ctx);
#else
		eventfd_signal(irq_entry->event_ctx, 1);
#endif
	}

	return ret;
}

static int user_intr_unreg(struct platform_device *pdev, u32 intr)
{
	struct xocl_xdma *xdma;
	const unsigned int mask = 1 << intr;
	unsigned long flags;
	int ret;

	xdma= platform_get_drvdata(pdev);

	if (intr >= xdma->max_user_intr) {
		xocl_err(&pdev->dev, "intr %d greater than max", intr);
		return -EINVAL;
	}

	spin_lock_irqsave(&xdma->user_msix_table_lock, flags);
	if (!xdma->user_msix_table[intr].in_use) {
		xocl_err(&pdev->dev, "intr %d is not in use", intr);
		ret = -EINVAL;
		goto failed;
	}
	xdma->user_msix_table[intr].handler = NULL;
	xdma->user_msix_table[intr].arg = NULL;

	ret = xdma_user_isr_register(xdma->dma_handle, mask, NULL, NULL);
	if (ret) {
		xocl_err(&pdev->dev, "xdma unregister isr failed");
		goto failed;
	}

	xdma->user_msix_table[intr].in_use = false;

failed:
	spin_unlock_irqrestore(&xdma->user_msix_table_lock, flags);
	return ret;
}

static int user_intr_register(struct platform_device *pdev, u32 intr,
	irq_handler_t handler, void *arg, int event_fd)
{
	struct xocl_xdma *xdma;
	struct eventfd_ctx *trigger = ERR_PTR(-EINVAL);
	const unsigned int mask = 1 << intr;
	unsigned long flags;
	int ret;

	xdma= platform_get_drvdata(pdev);

	if (intr >= xdma->max_user_intr ||
			(event_fd >= 0 && intr < xdma->start_user_intr)) {
		xocl_err(&pdev->dev, "Invalid intr %d, user start %d, max %d",
			intr, xdma->start_user_intr, xdma->max_user_intr);
		return -EINVAL;
	}

	if (event_fd >= 0) {
		trigger = eventfd_ctx_fdget(event_fd);
		if (IS_ERR(trigger)) {
			xocl_err(&pdev->dev, "get event ctx failed");
			return -EFAULT;
		}
	}

	spin_lock_irqsave(&xdma->user_msix_table_lock, flags);
	if (xdma->user_msix_table[intr].in_use) {
		xocl_err(&pdev->dev, "IRQ %d is in use", intr);
		ret = -EPERM;
		goto failed;
	}
	xdma->user_msix_table[intr].event_ctx = trigger;
	xdma->user_msix_table[intr].handler = handler;
	xdma->user_msix_table[intr].arg = arg;

	ret = xdma_user_isr_register(xdma->dma_handle, mask, xdma_isr,
			&xdma->user_msix_table[intr]);
	if (ret) {
		xocl_err(&pdev->dev, "IRQ register failed");
		xdma->user_msix_table[intr].handler = NULL;
		xdma->user_msix_table[intr].arg = NULL;
		xdma->user_msix_table[intr].event_ctx = NULL;
		goto failed;
	}

	xdma->user_msix_table[intr].in_use = true;

	spin_unlock_irqrestore(&xdma->user_msix_table_lock, flags);

	return 0;

failed:
	spin_unlock_irqrestore(&xdma->user_msix_table_lock, flags);
	if (!IS_ERR(trigger))
		eventfd_ctx_put(trigger);

	return ret;
}

static struct xocl_dma_funcs xdma_ops = {
	.migrate_bo = xdma_migrate_bo,
	.async_migrate_bo = xdma_async_migrate_bo,
	.ac_chan = acquire_channel,
	.rel_chan = release_channel,
	.get_chan_count = get_channel_count,
	.get_chan_stat = get_channel_stat,
	.user_intr_register = user_intr_register,
	.user_intr_config = user_intr_config,
	.user_intr_unreg = user_intr_unreg,
};

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

static struct attribute *xdma_attrs[] = {
	&dev_attr_channel_stat_raw.attr,
	NULL,
};

static struct attribute_group xdma_attr_group = {
	.attrs = xdma_attrs,
};

static int set_max_chan(struct platform_device *pdev,
		struct xocl_xdma *xdma)
{
	xdma->channel_usage[0] = devm_kzalloc(&pdev->dev, sizeof (u64) *
		xdma->channel, GFP_KERNEL);
	xdma->channel_usage[1] = devm_kzalloc(&pdev->dev, sizeof (u64) *
		xdma->channel, GFP_KERNEL);
	if (!xdma->channel_usage[0] || !xdma->channel_usage[1]) {
		xocl_err(&pdev->dev, "failed to alloc channel usage");
		return -ENOMEM;
	}

	sema_init(&xdma->channel_sem[0], xdma->channel);
	sema_init(&xdma->channel_sem[1], xdma->channel);

	/* Initialize bit mask to represent individual channels */
	xdma->channel_bitmap[0] = BIT(xdma->channel);
	xdma->channel_bitmap[0]--;
	xdma->channel_bitmap[1] = xdma->channel_bitmap[0];

	return 0;
}

static int xdma_probe(struct platform_device *pdev)
{
	struct xocl_xdma	*xdma = NULL;
	int	ret = 0;
	xdev_handle_t		xdev;

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	xdma = devm_kzalloc(&pdev->dev, sizeof(*xdma), GFP_KERNEL);
	if (!xdma) {
		xocl_err(&pdev->dev, "alloc xdma dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	xdma->dma_handle = xdma_device_open(XOCL_MODULE_NAME, XDEV(xdev)->pdev,
			&xdma->max_user_intr,
			&xdma->channel, &xdma->channel, false);
	if (xdma->dma_handle == NULL) {
		xocl_err(&pdev->dev, "XDMA Device Open failed");
		ret = -EIO;
		goto failed;
	}

	if (XOCL_DSA_IS_VERSAL_ES3(xdev)) {
		xocl_info(&pdev->dev, "VERSAL ES3, set to 2 channels");
		xdma->channel = 2;
	}

	xdma->user_msix_table = devm_kzalloc(&pdev->dev,
			xdma->max_user_intr *
			sizeof(struct xdma_irq), GFP_KERNEL);
	if (!xdma->user_msix_table) {
		xocl_err(&pdev->dev, "alloc user_msix_table failed");
		ret = -ENOMEM;
		goto failed;
	}

	ret = set_max_chan(pdev, xdma);
	if (ret) {
		xocl_err(&pdev->dev, "Set max channel failed");
		goto failed;
	}

	platform_set_drvdata(pdev, xdma);
	ret = sysfs_create_group(&pdev->dev.kobj, &xdma_attr_group);
	if (ret) {
		xocl_err(&pdev->dev, "create attrs failed: %d", ret);
		goto failed;
	}

	mutex_init(&xdma->stat_lock);
	spin_lock_init(&xdma->user_msix_table_lock);

	return 0;

failed:
	if (xdma) {
		if (xdma->dma_handle)
			xdma_device_close(XDEV(xdev)->pdev, xdma->dma_handle);
		if (xdma->channel_usage[0])
			devm_kfree(&pdev->dev, xdma->channel_usage[0]);
		if (xdma->channel_usage[1])
			devm_kfree(&pdev->dev, xdma->channel_usage[1]);
		if (xdma->user_msix_table) {
			devm_kfree(&pdev->dev, xdma->user_msix_table);
		}

		devm_kfree(&pdev->dev, xdma);
	}

	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int xdma_remove(struct platform_device *pdev)
{
	struct xocl_xdma *xdma = platform_get_drvdata(pdev);
	xdev_handle_t xdev;
	struct xdma_irq *irq_entry;
	int i;

	if (!xdma) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xdev = xocl_get_xdev(pdev);
	BUG_ON(!xdev);

	sysfs_remove_group(&pdev->dev.kobj, &xdma_attr_group);

	if (xdma->dma_handle)
		xdma_device_close(XDEV(xdev)->pdev, xdma->dma_handle);

	for (i = 0; i < xdma->max_user_intr; i++) {
		irq_entry = &xdma->user_msix_table[i];
		if (irq_entry->in_use) {
			if (irq_entry->enabled) {
				xocl_err(&pdev->dev,
					"ERROR: Interrupt %d is still on", i);
			}
			if(!IS_ERR_OR_NULL(irq_entry->event_ctx))
				eventfd_ctx_put(irq_entry->event_ctx);
		}
	}

	if (xdma->channel_usage[0])
		devm_kfree(&pdev->dev, xdma->channel_usage[0]);
	if (xdma->channel_usage[1])
		devm_kfree(&pdev->dev, xdma->channel_usage[1]);

	mutex_destroy(&xdma->stat_lock);

	devm_kfree(&pdev->dev, xdma->user_msix_table);
	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, xdma);

	return 0;
}

struct xocl_drv_private xdma_priv = {
	.ops = &xdma_ops,
};

static struct platform_device_id xdma_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XDMA), (kernel_ulong_t)&xdma_priv },
	{ },
};

static struct platform_driver	xdma_driver = {
	.probe		= xdma_probe,
	.remove		= xdma_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_XDMA),
	},
	.id_table	= xdma_id_table,
};

int __init xocl_init_xdma(void)
{
	return platform_driver_register(&xdma_driver);
}

void xocl_fini_xdma(void)
{
	return platform_driver_unregister(&xdma_driver);
}
