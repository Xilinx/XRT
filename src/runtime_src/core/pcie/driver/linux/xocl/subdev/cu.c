// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Sub-device Driver
 *
 * Copyright (C) 2020-2022 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xocl_drv.h"
#include "xrt_cu.h"
#include "cu_xgq.h"

#define XCU_INFO(xcu, fmt, arg...) \
	xocl_info(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_WARN(xcu, fmt, arg...) \
	xocl_warn(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_ERR(xcu, fmt, arg...) \
	xocl_err(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_DBG(xcu, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

#define IRQ_DISABLED 0
struct xocl_cu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
	DECLARE_BITMAP(flag, 1);
	spinlock_t		 lock;
	/*
	 * This RW lock is to protect the cu sysfs nodes exported
	 * by xocl driver.
	 */
	rwlock_t		 attr_rwlock;
};

static ssize_t debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;

	return sprintf(buf, "%d\n", xcu->debug);
}

static ssize_t debug_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;
	u32 debug;

	if (kstrtou32(buf, 10, &debug) == -EINVAL)
		return -EINVAL;

	xcu->debug = debug;

	return count;
}
static DEVICE_ATTR_RW(debug);

static ssize_t
cu_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);

	return show_cu_stat(&cu->base, buf);
}
static DEVICE_ATTR_RO(cu_stat);

static ssize_t
cu_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);

	return show_cu_info(&cu->base, buf);
}
static DEVICE_ATTR_RO(cu_info);

static ssize_t
poll_interval_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", cu->base.interval_min);
}

static ssize_t
poll_interval_store(struct device *dev, struct device_attribute *attr,
	    const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	u32 interval;

	if (kstrtou32(buf, 10, &interval) == -EINVAL)
		return -EINVAL;

	cu->base.interval_min = interval;
	cu->base.interval_max = interval + 3;

	return count;
}
static DEVICE_ATTR_RW(poll_interval);

static ssize_t
busy_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", cu->base.busy_threshold);
}

static ssize_t
busy_threshold_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	int threshold;

	if (kstrtos32(buf, 10, &threshold) == -EINVAL)
		return -EINVAL;

	cu->base.busy_threshold = threshold;

	return count;
}
static DEVICE_ATTR_RW(busy_threshold);

static ssize_t
name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "CU[%d]\n", info->cu_idx);
}
static DEVICE_ATTR_RO(name);

static ssize_t
base_paddr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "0x%llx\n", info->addr);
}
static DEVICE_ATTR_RO(base_paddr);

static ssize_t
size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "%ld\n", info->size);
}
static DEVICE_ATTR_RO(size);

static ssize_t 
stats_begin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t sz = 0;

	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);

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
	struct xocl_cu *cu = platform_get_drvdata(pdev);

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
	struct xocl_cu *cu = platform_get_drvdata(pdev);

	read_lock(&cu->attr_rwlock);
	sz = show_formatted_cu_stat(&cu->base, buf);
	read_unlock(&cu->attr_rwlock);

	return sz;
}
static DEVICE_ATTR_RO(stat);

static ssize_t
is_ucu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	int is_ucu;

	is_ucu = test_bit(0, cu->base.is_ucu);
	return sprintf(buf, "%d\n", is_ucu);
}
static DEVICE_ATTR_RO(is_ucu);

static ssize_t
crc_buf_show(struct file *filp, struct kobject *kobj,
	     struct bin_attribute *attr, char *buf,
	     loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct xocl_cu *cu = (struct xocl_cu *)dev_get_drvdata(dev);
	struct xrt_cu *xcu;

	if (!cu)
		return 0;

	xcu = &cu->base;
	return xrt_cu_circ_consume_all(xcu, buf, count);
}

static ssize_t
read_range_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
	u32 start = 0;
	u32 end = 0;

	mutex_lock(&cu->base.read_regs.xcr_lock);
	start = cu->base.read_regs.xcr_start;
	end = cu->base.read_regs.xcr_end;
	mutex_unlock(&cu->base.read_regs.xcr_lock);
	return sprintf(buf, "0x%x 0x%x\n", start, end);
}
static DEVICE_ATTR_RO(read_range);

static struct attribute *cu_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_cu_stat.attr,
	&dev_attr_cu_info.attr,
	&dev_attr_poll_interval.attr,
	&dev_attr_busy_threshold.attr,
	&dev_attr_name.attr,
	&dev_attr_base_paddr.attr,
	&dev_attr_size.attr,
	&dev_attr_stats_begin.attr,
	&dev_attr_stats_end.attr,
	&dev_attr_stat.attr,
	&dev_attr_is_ucu.attr,
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

static irqreturn_t cu_isr(int irq, void *arg)
{
	struct xocl_cu *xcu = arg;

	xrt_cu_circ_produce(&xcu->base, CU_LOG_STAGE_ISR, 0);
	xrt_cu_clear_intr(&xcu->base);

	up(&xcu->base.sem_cu);

	return IRQ_HANDLED;
}

static irqreturn_t ucu_isr(int irq, void *arg)
{
	struct xocl_cu *xcu = arg;
	xdev_handle_t xdev = xocl_get_xdev(xcu->pdev);
	unsigned long flags;

	spin_lock_irqsave(&xcu->lock, flags);
	atomic_inc(&xcu->base.ucu_event);

	if (!__test_and_set_bit(IRQ_DISABLED, xcu->flag))
		xocl_intc_cu_config(xdev, irq, false);
	spin_unlock_irqrestore(&xcu->lock, flags);

	wake_up_interruptible(&xcu->base.ucu_waitq);

	return IRQ_HANDLED;
}

static int user_manage_irq(struct xrt_cu *xrt_cu, bool user_manage)
{
	struct xocl_cu *xcu = (struct xocl_cu *)xrt_cu;
	xdev_handle_t xdev = xocl_get_xdev(xcu->pdev);
	struct xrt_cu_info *info = &xrt_cu->info;
	int ret;

	if (!xrt_cu->info.intr_enable)
		return -EINVAL;

	xocl_intc_cu_config(xdev, info->intr_id, false);
	xocl_intc_cu_request(xdev, info->intr_id, NULL, NULL);
	if (user_manage) {
		ret = xocl_intc_cu_request(xdev, info->intr_id, ucu_isr, xcu);
	} else {
		ret = xocl_intc_cu_request(xdev, info->intr_id, cu_isr, xcu);
	}
	if (ret) {
		XCU_ERR(xcu, "CU register request failed");
		return ret;
	}

	if (user_manage) {
		__set_bit(IRQ_DISABLED, xcu->flag);
		spin_lock_init(&xcu->lock);
		xocl_intc_cu_config(xdev, info->intr_id, false);
	} else {
		xocl_ert_user_enable(xdev);
		xocl_intc_cu_config(xdev, info->intr_id, true);
	}

	return 0;
}

static int configure_irq(struct xrt_cu *xrt_cu, bool enable)
{
	struct xocl_cu *xcu = (struct xocl_cu *)xrt_cu;
	xdev_handle_t xdev = xocl_get_xdev(xcu->pdev);
	struct xrt_cu_info *info = &xrt_cu->info;
	unsigned long flags;

	if (!test_bit(0, xrt_cu->is_ucu)) {
		if (enable) {
			xocl_intc_cu_request(xdev, info->intr_id, cu_isr, xcu);
			xocl_intc_cu_config(xdev, info->intr_id, true);
		} else {
			xocl_intc_cu_config(xdev, info->intr_id, false);
			xocl_intc_cu_request(xdev, info->intr_id, NULL, NULL);
		}

		return 0;
	}

	/* User manage interrupt */
	spin_lock_irqsave(&xcu->lock, flags);
	if (enable) {
		if (__test_and_clear_bit(IRQ_DISABLED, xcu->flag))
			xocl_intc_cu_config(xdev, info->intr_id, true);
	} else {
		if (!__test_and_set_bit(IRQ_DISABLED, xcu->flag))
			xocl_intc_cu_config(xdev, info->intr_id, false);
	}
	spin_unlock_irqrestore(&xcu->lock, flags);

	return 0;
}

static int cu_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu *xcu = NULL;
	struct resource **res = NULL;
	struct xrt_cu_info *info = NULL;
	struct xrt_cu_arg *args = NULL;
	uint32_t subdev_inst_idx = 0;
	int err = 0;
	int i = 0;

	/* Not using xocl_drvinst_alloc here. Because it would quickly run out
	 * of memory when there are a lot of cards. Since user cannot open CU
	 * subdevice, the normal way to allocate device is good enough.
	 */
	xcu = devm_kzalloc(&pdev->dev, sizeof(*xcu), GFP_KERNEL);
	if (!xcu)
		return -ENOMEM;

	xcu->pdev = pdev;
	xcu->base.dev = XDEV2DEV(xdev);

	info = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	BUG_ON(!info);

	subdev_inst_idx = XOCL_SUBDEV_INST_IDX(&pdev->dev);
	if (subdev_inst_idx == INVALID_INST_INDEX) {
		XCU_ERR(xcu, "Unknown Instance index");
		return -EINVAL;
	}

	/* Store subdevice instance index with this CU info.
	 * This will be required to destroy this subdevice.
	 */
	info->inst_idx = subdev_inst_idx;

	memcpy(&xcu->base.info, info, sizeof(struct xrt_cu_info));

	if (xcu->base.info.model == XCU_AUTO) {
		switch (info->protocol) {
		case CTRL_HS:
		case CTRL_CHAIN:
		case CTRL_NONE:
			xcu->base.info.model = XCU_HLS;
			break;
		case CTRL_FA:
			xcu->base.info.model = XCU_FA;
			break;
		default:
			XCU_ERR(xcu, "Unknown protocol");
			return -EINVAL;
		}
	}

	res = vzalloc(sizeof(struct resource *) * xcu->base.info.num_res);
	if (!res) {
		err = -ENOMEM;
		goto err;
	}

	for (i = 0; i < xcu->base.info.num_res; ++i) {
		res[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res[i]) {
			err = -EINVAL;
			goto err1;
		}
	}
	xcu->base.res = res;

	err = xocl_kds_add_cu(xdev, &xcu->base);
	if (err) {
		err = 0; //Ignore this error now
		//XCU_ERR(xcu, "Not able to add CU %p to KDS", xcu);
		goto err1;
	}

	switch (xcu->base.info.model) {
	case XCU_HLS:
		err = xrt_cu_hls_init(&xcu->base);
		break;
	case XCU_FA:
		err = xrt_cu_fa_init(&xcu->base);
		break;
	case XCU_XGQ:
		err = xrt_cu_xgq_init(&xcu->base, 0 /* fast path */);
		break;
	default:
		err = -EINVAL;
	}
	if (err) {
		XCU_ERR(xcu, "Not able to initial CU %p", xcu);
		goto err2;
	}

	if (info->intr_enable) {
		err = xocl_intc_cu_request(xdev, info->intr_id, cu_isr, xcu);
		if (!err)
			XCU_INFO(xcu, "Register CU interrupt id %d", info->intr_id);
		else if (err != -ENODEV)
			XCU_ERR(xcu, "xocl_intc_cu_request failed, err: %d", err);

		err = xocl_intc_cu_config(xdev, info->intr_id, true);
		if (err && err != -ENODEV)
			XCU_ERR(xcu, "xocl_intc_cu_config failed, err: %d", err);
	}

	rwlock_init(&xcu->attr_rwlock);
	if (sysfs_create_group(&pdev->dev.kobj, &cu_attrgroup))
		XCU_ERR(xcu, "Not able to create CU sysfs group");

	platform_set_drvdata(pdev, xcu);

	xcu->base.user_manage_irq = user_manage_irq;
	xcu->base.configure_irq = configure_irq;

	return 0;

err2:
	(void) xocl_kds_del_cu(xdev, &xcu->base);
err1:
	vfree(res);
err:
	vfree(args);
	return err;
}

static int cu_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xrt_cu_info *info;
	struct xocl_cu *xcu;
	int err;

	xcu = platform_get_drvdata(pdev);
	if (!xcu)
		return -EINVAL;

	(void) sysfs_remove_group(&pdev->dev.kobj, &cu_attrgroup);
	write_lock(&xcu->attr_rwlock);
	info = &xcu->base.info;
	write_unlock(&xcu->attr_rwlock);

	if (info->intr_enable) {
		err = xocl_intc_cu_config(xdev, info->intr_id, false);
		if (!err)
			XCU_INFO(xcu, "Unregister CU interrupt id %d", info->intr_id);
		xocl_intc_cu_request(xdev, info->intr_id, NULL, NULL);
	}

	switch (info->model) {
	case XCU_HLS:
		xrt_cu_hls_fini(&xcu->base);
		break;
	case XCU_FA:
		xrt_cu_fa_fini(&xcu->base);
		break;
	case XCU_XGQ:
		xrt_cu_xgq_fini(&xcu->base);
		break;
	}

	(void) xocl_kds_del_cu(xdev, &xcu->base);

	if (xcu->base.res)
		vfree(xcu->base.res);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_device_id cu_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CU), 0 },
	{ },
};

static struct platform_driver cu_driver = {
	.probe		= cu_probe,
	.remove		= cu_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CU),
	},
	.id_table	= cu_id_table,
};

int __init xocl_init_cu(void)
{
	return platform_driver_register(&cu_driver);
}

void xocl_fini_cu(void)
{
	platform_driver_unregister(&cu_driver);
}
