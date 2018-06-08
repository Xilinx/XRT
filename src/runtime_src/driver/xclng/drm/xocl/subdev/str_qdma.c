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
#include "../xocl_drv.h"
#include "../userpf/common.h"
#include "../lib/libqdma/libqdma_export.h"
#include "qdma_ioctl.h"

#define	MAX_QUEUE_NUM		2048

#define	PROC_TABLE_HASH_SZ	512
#define	EBUF_LEN		256
#define	MINOR_NAME_MASK		0xffff

static dev_t	str_dev;

struct stream_queue {
	struct list_head	node;
	unsigned long		qhdl;
	u32			state;

	u64			trans_bytes;
};

struct stream_context {
	struct list_head	node;
	struct str_device	*sdev;

	struct list_head	queue_list;
	struct mutex		ctx_lock;
};

struct str_device {
	struct platform_device  *pdev;
	struct cdev		cdev;
	struct device		*sys_device;

	struct list_head	ctx_list;
	struct mutex		str_dev_lock;

	u16			instance;
};

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

static long stream_ioctl_create_queue(struct stream_context *ctx,
	void __user *arg)
{
	struct str_device *sdev = ctx->sdev;
	struct xocl_qdma_ioc_create_queue q_info;
	struct qdma_queue_conf qconf;
	struct xocl_dev *xdev;
	struct stream_queue *s_queue;
	unsigned long qhdl;
	char	ebuf[EBUF_LEN + 1];
	long	ret;

	if (copy_from_user((void *)&q_info, arg,
		sizeof (struct xocl_qdma_ioc_create_queue))) {
		xocl_err(&sdev->pdev->dev, "copy failed.");
		return -EFAULT;
	}

	xdev = xocl_get_xdev(sdev->pdev);

	memset(&qconf, 0, sizeof (qconf));
	qconf.st = 1; /* stream queue */
	if (!q_info.write)
		qconf.c2h = 1;
	ret = qdma_queue_add((unsigned long)xdev->dma_handle,
		&qconf, &qhdl, ebuf, EBUF_LEN);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev, "Creating Queue failed ret = %ld",
			ret);
		xocl_err(&sdev->pdev->dev, "Error: %s", ebuf);
		goto failed_to_create;
	}
	ret = qdma_queue_start((unsigned long)xdev->dma_handle,
		qhdl, ebuf, EBUF_LEN);
	if (ret < 0) {
		xocl_err(&sdev->pdev->dev, "Starting Queue failed ret = %ld",
			ret);
		xocl_err(&sdev->pdev->dev, "Error: %s", ebuf);
		goto start_failed;
	}

	s_queue = devm_kzalloc(&sdev->pdev->dev, sizeof (*s_queue), GFP_KERNEL);
	if (!s_queue) {
		xocl_err(&sdev->pdev->dev, "out of memeory");
		ret = -ENOMEM;
		goto alloc_queue_failed;
	}

	q_info.handle = qhdl;
	if (copy_to_user(arg, &q_info, sizeof (q_info))) {
		xocl_err(&sdev->pdev->dev, "Copy to user failed");
		ret = -EFAULT;
		goto copy_to_user_failed;
	}

	s_queue->qhdl = qhdl;
	mutex_lock(&ctx->ctx_lock);
	list_add(&s_queue->node, &ctx->queue_list);
	mutex_unlock(&ctx->ctx_lock);

	return 0;

copy_to_user_failed:
	if (s_queue)
		devm_kfree(&sdev->pdev->dev, s_queue);

alloc_queue_failed:
	if (qdma_queue_stop((unsigned long)xdev->dma_handle,
		qhdl, ebuf, EBUF_LEN) < 0) {
		xocl_err(&sdev->pdev->dev, "Stopping queue failed ret=%ld",
			ret);
		xocl_err(&sdev->pdev->dev, "Error: %s", ebuf);
	}

start_failed:
	if (qdma_queue_remove((unsigned long)xdev->dma_handle,
		qhdl, ebuf, EBUF_LEN) < 0) {
		xocl_err(&sdev->pdev->dev, "Removing queue failed ret = %ld",
			ret);
		xocl_err(&sdev->pdev->dev, "Error: %s", ebuf);
	}
failed_to_create:

	return ret;
}

static long stream_queue_remove(unsigned long dma_hdl,
	struct stream_queue *entry, char *ebuf, u32 buf_len)
{
	long		ret = 0;

	if (entry->state == XOCL_QDMA_QSTATE_STARTED) {
		ret = qdma_queue_stop(dma_hdl, entry->qhdl, ebuf, EBUF_LEN);
		if (ret < 0)
			goto failed;
		entry->state = XOCL_QDMA_QSTATE_STOPPED;
	}

	ret = qdma_queue_remove(dma_hdl, entry->qhdl, ebuf, EBUF_LEN);
failed:
	return ret;
}

static long stream_ioctl_destroy_queue(struct stream_context *ctx,
	const void __user *arg)
{
	struct str_device *sdev = ctx->sdev;
	struct xocl_qdma_ioc_destroy_queue q_info;
	struct stream_queue *entry, *next;
	struct xocl_dev *xdev;
	char	ebuf[EBUF_LEN + 1];
	bool	found = false;
	long	ret = 0;

	if (copy_from_user((void *)&q_info, arg,
		sizeof (struct xocl_qdma_ioc_destroy_queue))) {
		xocl_err(&sdev->pdev->dev, "copy failed.");
		return -EFAULT;
	}

	xdev = xocl_get_xdev(sdev->pdev);

	mutex_lock(&ctx->ctx_lock);
	list_for_each_entry_safe(entry, next, &ctx->queue_list, node) {
		if (entry->qhdl == q_info.handle) {
			ret = stream_queue_remove(
				(unsigned long)xdev->dma_handle,
				entry, ebuf, EBUF_LEN);
			if (ret < 0) {
				xocl_err(&sdev->pdev->dev,
					"Removing queue failed ret = %ld",
					ret);
				mutex_unlock(&ctx->ctx_lock);
				goto failed;
			}
			list_del(&entry->node);
			found = true;
			break;
		}
	}
		
	mutex_unlock(&ctx->ctx_lock);

	if (found) {
		devm_kfree(&sdev->pdev->dev, entry);
	} else {
		ret = -ENOENT;
	}

failed:
	return ret;
}

static long stream_ioctl_modify_queue(struct stream_context *ctx,
	const void __user *arg)
{
	return 0;
}

static long stream_ioctl_post_wr(struct stream_context *ctx,
	const void __user *arg)
{
	return 0;
}

static long stream_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct stream_context	*ctx;
	struct str_device	*sdev;
	long result = 0;

	ctx = (struct stream_context *)filp->private_data;
	BUG_ON(!ctx);
	sdev = ctx->sdev;

	if (_IOC_TYPE(cmd) != XOCL_QDMA_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		result = !access_ok(VERIFY_WRITE, (void __user *)arg,
			_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		result =  !access_ok(VERIFY_READ, (void __user *)arg,
			_IOC_SIZE(cmd));

	if (result)
		return -EFAULT;

	switch (cmd) {
	case XOCL_QDMA_IOC_CREATE_QUEUE:
		result = stream_ioctl_create_queue(ctx, (void __user *)arg);
		break;
	case XOCL_QDMA_IOC_DESTROY_QUEUE:
		result = stream_ioctl_destroy_queue(ctx, (void __user *)arg);
		break;
	case XOCL_QDMA_IOC_MODIFY_QUEUE:
		result = stream_ioctl_modify_queue(ctx, (void __user *)arg);
		break;
	case XOCL_QDMA_IOC_POST_WR:
		result = stream_ioctl_post_wr(ctx, (void __user *)arg);
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
	struct stream_context *ctx;

	sdev = container_of(inode->i_cdev, struct str_device, cdev);

	ctx = devm_kzalloc(&sdev->pdev->dev, sizeof (*ctx), GFP_KERNEL);
	if (!ctx) {
		xocl_err(&sdev->pdev->dev, "out of memory");
		return -ENOMEM;
	}

	mutex_init(&ctx->ctx_lock);
	INIT_LIST_HEAD(&ctx->queue_list);
	file->private_data = ctx;

	ctx->sdev = sdev;
	mutex_lock(&sdev->str_dev_lock);
	list_add(&ctx->node, &sdev->ctx_list);
	mutex_unlock(&sdev->str_dev_lock);

	xocl_info(&sdev->pdev->dev, "opened file %p by pid: %d",
		file, pid_nr(task_tgid(current)));

	return 0;
}

static int stream_close(struct inode *inode, struct file *file)
{
	struct str_device *sdev;
	struct stream_context *ctx;
	struct xocl_dev *xdev;
	struct stream_queue *entry, *next;
	char	ebuf[EBUF_LEN + 1];
	long ret;

	ctx = (struct stream_context *)file->private_data;
	BUG_ON(!ctx);

	sdev = ctx->sdev;
	xdev = xocl_get_xdev(sdev->pdev);

	mutex_lock(&ctx->ctx_lock);
	list_for_each_entry_safe(entry, next, &ctx->queue_list, node) {
		ret = stream_queue_remove((unsigned long)xdev->dma_handle,
			entry, ebuf, EBUF_LEN);
		if (ret < 0) {
			xocl_err(&sdev->pdev->dev,
				"Removing queue failed ret = %ld", ret);
		}
		list_del(&entry->node);
	}
	mutex_unlock(&ctx->ctx_lock);

	mutex_lock(&sdev->str_dev_lock);
	list_del(&ctx->node);
	mutex_unlock(&sdev->str_dev_lock);

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
	struct xocl_dev_core	*core;
	int			ret = 0;

	sdev = devm_kzalloc(&pdev->dev, sizeof (*sdev), GFP_KERNEL);
	if (!sdev) {
		xocl_err(&pdev->dev, "alloc stream dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	sdev->pdev = pdev;
	core = xocl_get_xdev(pdev);

	cdev_init(&sdev->cdev, &stream_fops);
	sdev->cdev.owner = THIS_MODULE;
	sdev->instance = XOCL_DEV_ID(core->pdev);
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

	INIT_LIST_HEAD(&sdev->ctx_list);
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
	if (err) {
		goto err_drv_reg;
	}

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
