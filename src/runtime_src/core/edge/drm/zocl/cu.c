/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "xrt_cu.h"

#define IRQ_DISABLED 0

struct zocl_cu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
	u32			 irq;
	char			*irq_name;
	DECLARE_BITMAP(flag, 1);
	spinlock_t		 lock;
};

static ssize_t debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;
#endif
	/* Place holder for now. */
	return 0;
}

static ssize_t debug_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;
#endif

	/* Place holder for now. */
	return count;
}
static DEVICE_ATTR_RW(debug);

static ssize_t
cu_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);

	return show_cu_stat(&cu->base, buf);
}
static DEVICE_ATTR_RO(cu_stat);

static ssize_t
cu_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);

	return show_cu_info(&cu->base, buf);
}
static DEVICE_ATTR_RO(cu_info);

static ssize_t
stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);

	return show_formatted_cu_stat(&cu->base, buf);
}
static DEVICE_ATTR_RO(stat);

static struct attribute *cu_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_cu_stat.attr,
	&dev_attr_cu_info.attr,
	&dev_attr_stat.attr,
	NULL,
};

static const struct attribute_group cu_attrgroup = {
	.attrs = cu_attrs,
};

irqreturn_t cu_isr(int irq, void *arg)
{
	struct zocl_cu *zcu = arg;

	xrt_cu_clear_intr(&zcu->base);

	up(&zcu->base.sem_cu);

	return IRQ_HANDLED;
}

irqreturn_t ucu_isr(int irq, void *arg)
{
	struct zocl_cu *zcu = arg;
	unsigned long flags;

	spin_lock_irqsave(&zcu->lock, flags);
	atomic_inc(&zcu->base.ucu_event);

	/* To handle level interrupt, have to disable this irq line.
	 * We could esaily support edge interrupt if needed.
	 * Like, provide one more gcu->flag to permanently enabl irq.
	 */
	if (!__test_and_set_bit(IRQ_DISABLED, zcu->flag))
		disable_irq_nosync(irq);
	spin_unlock_irqrestore(&zcu->lock, flags);

	wake_up_interruptible(&zcu->base.ucu_waitq);

	return IRQ_HANDLED;
}

static int user_manage_irq(struct xrt_cu *xcu, bool user_manage)
{
	struct zocl_cu *zcu = (struct zocl_cu *)xcu;
	int ret;

	if (xcu->info.intr_enable)
		free_irq(zcu->irq, zcu);

	/* Do not use IRQF_SHARED! */
	if (user_manage)
		ret = request_irq(zcu->irq, ucu_isr, 0, zcu->irq_name, zcu);
	else
		ret = request_irq(zcu->irq, cu_isr, 0, zcu->irq_name, zcu);
	if (ret) {
		DRM_INFO("%s: request_irq() failed\n", __func__);
		return ret;
	}

	if (user_manage) {
		__set_bit(IRQ_DISABLED, zcu->flag);
		spin_lock_init(&zcu->lock);
		disable_irq(zcu->irq);
	}

	return 0;
}

static int configure_irq(struct xrt_cu *xcu, bool enable)
{
	struct zocl_cu *zcu = (struct zocl_cu *)xcu;
	unsigned long flags;

	spin_lock_irqsave(&zcu->lock, flags);
	if (enable) {
		if (__test_and_clear_bit(IRQ_DISABLED, zcu->flag))
			enable_irq(zcu->irq);
	} else {
		if (!__test_and_set_bit(IRQ_DISABLED, zcu->flag))
			disable_irq(zcu->irq);
	}
	spin_unlock_irqrestore(&zcu->lock, flags);

	return 0;
}

static int cu_probe(struct platform_device *pdev)
{
	struct zocl_cu *zcu;
	struct resource **res;
	struct xrt_cu_info *info;
	struct drm_zocl_dev *zdev;
	struct xrt_cu_arg *args = NULL;
	int err = 0;
	int i;

	zcu = kzalloc(sizeof(*zcu), GFP_KERNEL);
	if (!zcu)
		return -ENOMEM;

	zcu->pdev = pdev;
	zcu->base.dev = &pdev->dev;

	info = dev_get_platdata(&pdev->dev);
	memcpy(&zcu->base.info, info, sizeof(struct xrt_cu_info));

	res = vzalloc(sizeof(struct resource *) * info->num_res);
	if (!res) {
		err = -ENOMEM;
		goto err;
	}

	for (i = 0; i < info->num_res; ++i) {
		res[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res[i]) {
			err = -EINVAL;
			goto err1;
		}
	}
	zcu->base.res = res;

	zdev = zocl_get_zdev();
	err = zocl_kds_add_cu(zdev, &zcu->base);
	if (err) {
		DRM_ERROR("Not able to add CU %p to KDS", zcu);
		goto err1;
	}

	zcu->irq_name = kzalloc(20, GFP_KERNEL);
	if (!zcu->irq_name)
		return -ENOMEM;

	sprintf(zcu->irq_name, "zocl_cu[%d]", info->intr_id);

	if (info->intr_enable) {
		zcu->irq = zdev->irq[info->intr_id];
		/* Currently requesting irq if it's enable in cu config.
		 * Not disabling it further, even user wants to use 
		 * polling.
		 */
		err = request_irq(zcu->irq, cu_isr, 0,
			  zcu->irq_name, zcu);
		if (err) {
			DRM_WARN("Failed to initial CU interrupt. "
			    "Fall back to polling\n");
			zcu->base.info.intr_enable = 0;
		}
	}

	switch (info->model) {
	case XCU_HLS:
		err = xrt_cu_hls_init(&zcu->base);
		break;
	case XCU_FA:
		err = xrt_cu_fa_init(&zcu->base);
		break;
	default:
		err = -EINVAL;
	}
	if (err) {
		DRM_ERROR("Not able to initial CU %p\n", zcu);
		goto err2;
	}

	platform_set_drvdata(pdev, zcu);

	err = sysfs_create_group(&pdev->dev.kobj, &cu_attrgroup);
	if (err)
		zocl_err(&pdev->dev, "create CU attrs failed: %d", err);

	zcu->base.user_manage_irq = user_manage_irq;
	zcu->base.configure_irq = configure_irq;

	zocl_info(&pdev->dev, "CU[%d] created", info->cu_idx);
	return 0;
err2:
	zocl_kds_del_cu(zdev, &zcu->base);
err1:
	vfree(args);
	vfree(res);
err:
	kfree(zcu);
	return err;
}

static int cu_remove(struct platform_device *pdev)
{
	struct zocl_cu *zcu;
	struct drm_zocl_dev *zdev;
	struct xrt_cu_info *info;

	zcu = platform_get_drvdata(pdev);
	if (!zcu)
		return -EINVAL;

	info = &zcu->base.info;
	switch (info->model) {
	case XCU_HLS:
		xrt_cu_hls_fini(&zcu->base);
		break;
	case XCU_FA:
		xrt_cu_fa_fini(&zcu->base);
		break;
	}

	if (info->intr_enable)
		free_irq(zcu->irq, zcu);

	zdev = zocl_get_zdev();
	zocl_kds_del_cu(zdev, &zcu->base);

	if (zcu->base.res)
		vfree(zcu->base.res);

	sysfs_remove_group(&pdev->dev.kobj, &cu_attrgroup);

	zocl_info(&pdev->dev, "CU[%d] removed", info->cu_idx);
	kfree(zcu->irq_name);
	kfree(zcu);

	return 0;
}

static struct platform_device_id cu_id_table[] = {
	{"CU", 0 },
	{ },
};

struct platform_driver cu_driver = {
	.probe		= cu_probe,
	.remove		= cu_remove,
	.driver		= {
		.name = "cu_drv",
	},
	.id_table	= cu_id_table,
};
u32 zocl_cu_get_status(struct platform_device *pdev)
{
	struct zocl_cu *zcu = platform_get_drvdata(pdev);

	BUG_ON(!zcu);
	return xrt_cu_get_status(&zcu->base);
}
