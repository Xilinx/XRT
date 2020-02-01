/*
 * MPSoC based OpenCL accelerators Generic Compute Units.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Min Ma      <min.ma@xilinx.com>
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

#include <drm/drmP.h>
#include <linux/anon_inodes.h>
#include "zocl_util.h"
#include "sched_exec.h"
#include "zocl_generic_cu.h"

#define WAIT_CONDITION (atomic_read(&gcu->event) > 0)

static int generic_cu_release(struct inode *inode, struct file *filp)
{
	struct generic_cu *gcu = filp->private_data;
	struct generic_cu_info *info = gcu->info;
	int ret;

	free_irq(info->irq, gcu);
	ret = sched_attach_cu(gcu->zdev, info->cu_idx);
	if (ret < 0)
		DRM_WARN("Scheduler attach CU[%d] failed.\n", info->cu_idx);

	kfree(info->name);
	kfree(info);
	kfree(gcu);
	return 0;
}

static __poll_t generic_cu_poll(struct file *filp, poll_table *wait)
{
	struct generic_cu *gcu = filp->private_data;
	__poll_t ret = 0;

	poll_wait(filp, &gcu->waitq, wait);

	if (atomic_read(&gcu->event) > 0)
		ret = POLLIN;

	return ret;
}

static ssize_t generic_cu_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct generic_cu *gcu = filp->private_data;
	ssize_t ret = 0;
	s32 events = 0;

	if (count != sizeof(s32))
		return -EINVAL;

	ret = wait_event_interruptible(gcu->waitq, WAIT_CONDITION);
	if (ret == -ERESTARTSYS)
		return 0;

	/* This could be different from when it just wake up.
	 * But it is okay, since this is the only place reset event count.
	 */
	events = atomic_xchg(&gcu->event, 0);
	if (copy_to_user(buf, &events, count))
		return -EFAULT;

	return sizeof(events);
}

static ssize_t generic_cu_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct generic_cu *gcu = filp->private_data;
	unsigned long flags;
	s32 enable;

	if (count != sizeof(s32))
		return -EINVAL;

	if (copy_from_user(&enable, buf, count))
		return -EFAULT;

	if (!gcu->info)
		return -EINVAL;

	spin_lock_irqsave(&gcu->lock, flags);
	if (enable) {
		if (__test_and_clear_bit(GCU_IRQ_DISABLED, &gcu->flag))
			enable_irq(gcu->info->irq);
	} else {
		if (!__test_and_set_bit(GCU_IRQ_DISABLED, &gcu->flag))
			disable_irq(gcu->info->irq);
	}
	spin_unlock_irqrestore(&gcu->lock, flags);

	return sizeof(s32);
}

static const struct file_operations generic_cu_fops = {
	.release	= generic_cu_release,
	.poll		= generic_cu_poll,
	.read		= generic_cu_read,
	.write		= generic_cu_write,
	.llseek		= noop_llseek,
};

static irqreturn_t generic_cu_isr(int irq, void *arg)
{
	struct generic_cu *gcu = arg;
	unsigned long flags;

	spin_lock_irqsave(&gcu->lock, flags);
	atomic_inc(&gcu->event);
	/* To handle level interrupt, have to disable this irq line.
	 * We could esaily support edge interrupt if needed.
	 * Like, provide one more gcu->flag to permanently enabl irq.
	 */
	if (!__test_and_set_bit(GCU_IRQ_DISABLED, &gcu->flag))
		disable_irq_nosync(irq);
	spin_unlock_irqrestore(&gcu->lock, flags);

	wake_up_interruptible(&gcu->waitq);

	return IRQ_HANDLED;
}

int _open_generic_cu(struct drm_zocl_dev *zdev, struct generic_cu_info *info)
{
	struct generic_cu *gcu;
	int fd;
	int ret;

	gcu = kzalloc(sizeof(*gcu), GFP_KERNEL);
	if (!gcu)
		return -ENOMEM;

	/* Do not use IRQF_SHARED! */
	ret = request_irq(info->irq, generic_cu_isr, 0, info->name, gcu);
	if (ret) {
		DRM_INFO("%s: request_irq() failed\n", __func__);
		return ret;
	}

	__set_bit(GCU_IRQ_DISABLED, &gcu->flag);
	disable_irq(info->irq);

	init_waitqueue_head(&gcu->waitq);
	gcu->zdev = zdev;
	gcu->info = info;
	atomic_set(&gcu->event, 0);
	spin_lock_init(&gcu->lock);

	fd = anon_inode_getfd("[generic_cu]", &generic_cu_fops, gcu, O_RDWR);
	if (fd < 0)
		kfree(gcu);

	return fd;
}

int zocl_open_gcu(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
		  struct sched_client_ctx *client)
{
	struct generic_cu_info *info;
	char *name;
	int ret;

	/* Check if the CU is exclusively reserved */
	ret = test_bit(ctx->cu_index, client->excus);
	if (!ret) {
		DRM_WARN("%s: CU[%d] is not exclusive\n",
			 __func__, ctx->cu_index);
		return -EINVAL;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	name = kzalloc(40, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	ret = sched_detach_cu(zdev, ctx->cu_index);
	if (ret)
		return ret;

	sprintf(name, "zocl_generic_cu[%d]", ctx->cu_index);
	info->name = name;
	info->cu_idx = ctx->cu_index;
	/* TODO: We have better not use exec here. Refine this in new KDS */
	info->irq    = zdev->exec->zcu[ctx->cu_index].irq;

	return _open_generic_cu(zdev, info);
}

