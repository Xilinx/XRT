// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Sub-device Driver
 *
 * Copyright (C) 2021-2022 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xocl_drv.h"
#include "xrt_cu.h"
#include "cu_xgq.h"

#define XSCU_INFO(xcu, fmt, arg...) \
	xocl_info(&xcu->pdev->dev, fmt "\n", ##arg)
#define XSCU_WARN(xcu, fmt, arg...) \
	xocl_warn(&xcu->pdev->dev, fmt "\n", ##arg)
#define XSCU_ERR(xcu, fmt, arg...) \
	xocl_err(&xcu->pdev->dev, fmt "\n", ##arg)
#define XSCU_DBG(xcu, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

#define IRQ_DISABLED 0
struct xocl_cu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
	DECLARE_BITMAP(flag, 1);
	spinlock_t		 lock;
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
	u32 debug = 0;

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
	u32 interval = 0;

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
	int threshold = 0;

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

	return sprintf(buf, "SCU[%d]\n", info->cu_idx);
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
crc_buf_show(struct file *filp, struct kobject *kobj,
	     struct bin_attribute *attr, char *buf,
	     loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct xocl_cu *cu = (struct xocl_cu *)dev_get_drvdata(dev);
	struct xrt_cu *xcu = NULL;

	if (!cu)
		return 0;

	xcu = &cu->base;
	return xrt_cu_circ_consume_all(xcu, buf, count);
}

static struct attribute *scu_attrs[] = {
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
	NULL,
};

static struct bin_attribute scu_crc_buf_attr = {
	.attr = {
		.name = "scu_crc_buf",
		.mode = 0444
	},
	.read = crc_buf_show,
	.write = NULL,
	.size = 0,
};

static struct bin_attribute *scu_bin_attrs[] = {
	&scu_crc_buf_attr,
	NULL,
};

static const struct attribute_group scu_attrgroup = {
	.attrs = scu_attrs,
	.bin_attrs = scu_bin_attrs,
};

static int scu_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu *xcu = NULL;
	struct xrt_cu_info *info = NULL;
	uint32_t subdev_inst_idx = 0;
	int err = 0;

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
		XSCU_ERR(xcu, "Unknown Instance index");
		return -EINVAL;
	}

	/* Store subdevice instance index with this SCU info.
	 * This will be required to destroy this subdevice.
	 */
	info->inst_idx = subdev_inst_idx;

	memcpy(&xcu->base.info, info, sizeof(struct xrt_cu_info));

	xcu->base.info.model = XCU_XGQ;

	err = xocl_kds_add_scu(xdev, &xcu->base);
	if (err) {
		XSCU_ERR(xcu, "Not able to add CU %p to KDS", xcu);
		goto err;
	}
	err = xrt_cu_xgq_init(&xcu->base, 1 /* slow path */);
	if (err) {
		XSCU_ERR(xcu, "Not able to initialize CU %p", xcu);
		goto err2;
	}

	rwlock_init(&xcu->attr_rwlock);
	if (sysfs_create_group(&pdev->dev.kobj, &scu_attrgroup))
		XSCU_ERR(xcu, "Not able to create SCU sysfs group");

	platform_set_drvdata(pdev, xcu);

	return 0;

err2:
	xrt_cu_xgq_fini(&xcu->base);
err:
	(void) xocl_kds_del_scu(xdev, &xcu->base);
	return err;
}

static int __scu_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xrt_cu_info *info = NULL;
	struct xocl_cu *xcu = NULL;

	xcu = platform_get_drvdata(pdev);
	if (!xcu)
		return -EINVAL;

	(void) sysfs_remove_group(&pdev->dev.kobj, &scu_attrgroup);
	write_lock(&xcu->attr_rwlock);
	info = &xcu->base.info;
	write_unlock(&xcu->attr_rwlock);

	xrt_cu_xgq_fini(&xcu->base);
	(void) xocl_kds_del_scu(xdev, &xcu->base);

	if (xcu->base.res)
		vfree(xcu->base.res);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void scu_remove(struct platform_device *pdev)
{
	__scu_remove(pdev);
}
#else
#define scu_remove __scu_remove
#endif

static struct platform_device_id scu_id_table[] = {
	{ XOCL_DEVNAME(XOCL_SCU), 0 },
	{ },
};

static struct platform_driver scu_driver = {
	.probe		= scu_probe,
	.remove		= scu_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_SCU),
	},
	.id_table	= scu_id_table,
};

int __init xocl_init_scu(void)
{
	return platform_driver_register(&scu_driver);
}

void xocl_fini_scu(void)
{
	platform_driver_unregister(&scu_driver);
}
