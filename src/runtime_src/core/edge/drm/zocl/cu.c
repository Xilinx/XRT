/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "xrt_cu.h"
#include "zocl_ert_intc.h"

#define IRQ_DISABLED 0

struct zocl_cu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
	u32			 irq;
	char			*irq_name;
	DECLARE_BITMAP(flag, 1);
	spinlock_t		 lock;
	/*
	 * This RW lock is to protect the cu sysfs nodes exported
	 * by zocl driver.
	 */
	rwlock_t		 attr_rwlock;
};

static ssize_t debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;

	return sprintf(buf, "%d\n", xcu->debug);
}

static ssize_t debug_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;
	u32 debug;

	if (kstrtou32(buf, 10, &debug) == -EINVAL)
		return -EINVAL;

	xcu->debug = debug;

	return count;
}
static DEVICE_ATTR_RW(debug);

static ssize_t
name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "CU[%d]\n", info->cu_idx);
}
static DEVICE_ATTR_RO(name);

static ssize_t
base_paddr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "0x%llx\n", info->addr);
}
static DEVICE_ATTR_RO(base_paddr);

static ssize_t
size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "%ld\n", info->size);
}
static DEVICE_ATTR_RO(size);

static ssize_t
read_range_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	u32 start = 0;
	u32 end = 0;

	mutex_lock(&cu->base.read_regs.xcr_lock);
	start = cu->base.read_regs.xcr_start;
	end = cu->base.read_regs.xcr_end;
	mutex_unlock(&cu->base.read_regs.xcr_lock);
	return sprintf(buf, "0x%x 0x%x\n", start, end);
}
static DEVICE_ATTR_RO(read_range);

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
stats_begin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t sz = 0;

	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);

	read_lock(&cu->attr_rwlock);
	sz = show_stats_begin(&cu->base, buf);
	read_unlock(&cu->attr_rwlock);

	return sz;
}
static DEVICE_ATTR_RO(stats_begin);

static ssize_t
stats_end_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t sz = 0;

	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);

	read_lock(&cu->attr_rwlock);
	sz = show_stats_end(&cu->base, buf);
	read_unlock(&cu->attr_rwlock);

	return sz;
}
static DEVICE_ATTR_RO(stats_end);

static ssize_t
stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t sz = 0;

	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);

	read_lock(&cu->attr_rwlock);
	sz = show_formatted_cu_stat(&cu->base, buf);
	read_unlock(&cu->attr_rwlock);

	return sz;
}
static DEVICE_ATTR_RO(stat);

static ssize_t
crc_buf_show(struct file *filp, struct kobject *kobj,
	     struct bin_attribute *attr, char *buf,
	     loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct zocl_cu *cu = (struct zocl_cu *)dev_get_drvdata(dev);
	struct xrt_cu *xcu;

	if (!cu)
		return 0;

	xcu = &cu->base;
	return xrt_cu_circ_consume_all(xcu, buf, count);
}

static ssize_t poll_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;

	return sprintf(buf, "%d\n", xcu->poll_threshold);
}

static ssize_t poll_threshold_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;
	u32 threshold;

	if (kstrtou32(buf, 10, &threshold) == -EINVAL)
		return -EINVAL;

	xcu->poll_threshold = threshold;

	return count;
}
static DEVICE_ATTR_RW(poll_threshold);

static struct attribute *cu_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_cu_stat.attr,
	&dev_attr_cu_info.attr,
	&dev_attr_stats_begin.attr,
	&dev_attr_stats_end.attr,
	&dev_attr_stat.attr,
	&dev_attr_poll_threshold.attr,
	&dev_attr_name.attr,
	&dev_attr_base_paddr.attr,
	&dev_attr_size.attr,
	&dev_attr_read_range.attr,
	NULL,
};

static struct bin_attribute crc_buf_attr = {
	.attr = {
		.name = "crc_buf",
		.mode = 0444
	},
	.read = crc_buf_show,
	.write = NULL,
	.size = 0,
};

static struct bin_attribute *cu_bin_attrs[] = {
	&crc_buf_attr,
	NULL,
};

static const struct attribute_group cu_attrgroup = {
	.attrs = cu_attrs,
	.bin_attrs = cu_bin_attrs,
};

irqreturn_t cu_isr(int irq, void *arg)
{
	struct zocl_cu *zcu = arg;

	xrt_cu_circ_produce(&zcu->base, CU_LOG_STAGE_ISR, 0);
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
	struct platform_device *intc;
	int ret = 0;

	intc = zocl_find_pdev(ERT_CU_INTC_DEV_NAME);
	if (!intc) {
		DRM_INFO("%s: finding platform device - %s failed\n", __func__, ERT_CU_INTC_DEV_NAME);
		return -ENODEV;
	}

	if (xcu->info.intr_enable)
		zocl_ert_intc_remove(intc, xcu->info.intr_id);

	/* Do not use IRQF_SHARED! */
	if (user_manage)
		ret = zocl_ert_intc_add(intc, xcu->info.intr_id, ucu_isr, zcu);
	else
		ret = zocl_ert_intc_add(intc, xcu->info.intr_id, cu_isr, zcu);
	if (ret) {
		DRM_INFO("%s: request_irq() failed\n", __func__);
		return ret;
	}

	if (user_manage) {
		__set_bit(IRQ_DISABLED, zcu->flag);
		spin_lock_init(&zcu->lock);
		zocl_ert_intc_config(intc, xcu->info.intr_id, false); // disable irq
	}

	return 0;
}

static int configure_irq(struct xrt_cu *xcu, bool enable)
{
	struct zocl_cu *zcu = (struct zocl_cu *)xcu;
	struct platform_device *intc;
	unsigned long flags;

	intc = zocl_find_pdev(ERT_CU_INTC_DEV_NAME);
	if (!intc) {
		DRM_INFO("%s: finding platform device - %s failed\n", __func__, ERT_CU_INTC_DEV_NAME);
		return -ENODEV;
	}

	if (enable) {
		if (__test_and_clear_bit(IRQ_DISABLED, zcu->flag))
			zocl_ert_intc_config(intc, xcu->info.intr_id, true); // enable irq
	} else {
		if (!__test_and_set_bit(IRQ_DISABLED, zcu->flag))
			zocl_ert_intc_config(intc, xcu->info.intr_id, false); // disable irq
	}

	return 0;
}

static int cu_probe(struct platform_device *pdev)
{
	struct zocl_cu *zcu;
	struct resource **res;
	struct xrt_cu_info *info;
	struct drm_zocl_dev *zdev;
	struct platform_device *intc;
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
	if (!zdev) {
		err = -EINVAL;
		goto err1;
	}

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
		intc = zocl_find_pdev(ERT_CU_INTC_DEV_NAME);
		if (intc)
			err = zocl_ert_intc_add(intc, info->intr_id, cu_isr, zcu);
		if (!intc || err) {
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

	rwlock_init(&zcu->attr_rwlock);
	err = sysfs_create_group(&pdev->dev.kobj, &cu_attrgroup);
	if (err)
		zocl_err(&pdev->dev, "create CU attrs failed: %d", err);

	zcu->base.user_manage_irq = user_manage_irq;
	zcu->base.configure_irq = configure_irq;

	zocl_info(&pdev->dev, "CU[%d] created", info->inst_idx);
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
	struct platform_device *intc;

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

	if (info->intr_enable) {
		intc = zocl_find_pdev(ERT_CU_INTC_DEV_NAME);
		if (intc)
			zocl_ert_intc_remove(intc, info->intr_id);
	}

	zdev = zocl_get_zdev();
	if(zdev)
		zocl_kds_del_cu(zdev, &zcu->base);

	if (zcu->base.res)
		vfree(zcu->base.res);

	write_lock(&zcu->attr_rwlock);
	sysfs_remove_group(&pdev->dev.kobj, &cu_attrgroup);
	write_unlock(&zcu->attr_rwlock);

	zocl_info(&pdev->dev, "CU[%d] removed", info->inst_idx);
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
