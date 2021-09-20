// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Sub-device Driver
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xocl_drv.h"
#include "xrt_cu.h"

#define XSCU_INFO(xcu, fmt, arg...) \
	xocl_info(&xcu->pdev->dev, fmt "\n", ##arg)
#define XSCU_WARN(xcu, fmt, arg...) \
	xocl_warn(&xcu->pdev->dev, fmt "\n", ##arg)
#define XSCU_ERR(xcu, fmt, arg...) \
	xocl_err(&xcu->pdev->dev, fmt "\n", ##arg)
#define XSCU_DBG(xcu, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

#define IRQ_DISABLED 0
struct xocl_scu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
	DECLARE_BITMAP(flag, 1);
	spinlock_t		 lock;
};

static ssize_t debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &cu->base;

	return sprintf(buf, "%d\n", xcu->debug);
}

static ssize_t debug_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);
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
	struct xocl_scu *cu = platform_get_drvdata(pdev);

	return show_cu_stat(&cu->base, buf);
}
static DEVICE_ATTR_RO(cu_stat);

static ssize_t
cu_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);

	return show_cu_info(&cu->base, buf);
}
static DEVICE_ATTR_RO(cu_info);

static ssize_t
poll_interval_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", cu->base.interval_min);
}

static ssize_t
poll_interval_store(struct device *dev, struct device_attribute *attr,
	    const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);
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
	struct xocl_scu *cu = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", cu->base.busy_threshold);
}

static ssize_t
busy_threshold_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);
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
	struct xocl_scu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "CU[%d]\n", info->cu_idx);
}
static DEVICE_ATTR_RO(name);

static ssize_t
base_paddr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "0x%llx\n", info->addr);
}
static DEVICE_ATTR_RO(base_paddr);

static ssize_t
size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);
	struct xrt_cu_info *info = &cu->base.info;

	return sprintf(buf, "%ld\n", info->size);
}
static DEVICE_ATTR_RO(size);

static ssize_t
stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_scu *cu = platform_get_drvdata(pdev);

	return show_formatted_cu_stat(&cu->base, buf);
}
static DEVICE_ATTR_RO(stat);

static ssize_t
crc_buf_show(struct file *filp, struct kobject *kobj,
	     struct bin_attribute *attr, char *buf,
	     loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct xocl_scu *cu = (struct xocl_scu *)dev_get_drvdata(dev);
	struct xrt_cu *xcu;

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

static int scu_add_args(struct xocl_scu *xcu, struct kernel_info *kinfo)
{
	struct xrt_cu_arg *args = NULL;
	int i;

	/* If there is no detail kernel information, maybe it is a manualy
	 * created xclbin. Print warning and let it go through
	 */
	if (!kinfo) {
		XSCU_WARN(xcu, "CU %s metadata not found, xclbin maybe corrupted",
			 xcu->base.info.iname);
		xcu->base.info.num_args = 0;
		xcu->base.info.args = NULL;
		return 0;
	}

	args = vmalloc(sizeof(struct xrt_cu_arg) * kinfo->anums);
	if (!args)
		return -ENOMEM;

	if (kinfo) {
		for (i = 0; i < kinfo->anums; i++) {
			strcpy(args[i].name, kinfo->args[i].name);
			args[i].offset = kinfo->args[i].offset;
			args[i].size = kinfo->args[i].size;
			args[i].dir = kinfo->args[i].dir;
		}
		xcu->base.info.num_args = kinfo->anums;
		xcu->base.info.args = args;
	}

	return 0;
}

static void scu_del_args(struct xocl_scu *xcu)
{
	if (xcu->base.info.args)
		vfree(xcu->base.info.args);
}

static int scu_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_scu *xcu;
	int err = 0;
	int i;

	/* Not using xocl_drvinst_alloc here. Because it would quickly run out
	 * of memory when there are a lot of cards. Since user cannot open CU
	 * subdevice, the normal way to allocate device is good enough.
	 */
	xcu = devm_kzalloc(&pdev->dev, sizeof(*xcu), GFP_KERNEL);
	if (!xcu)
		return -ENOMEM;

	xcu->pdev = pdev;
	xcu->base.dev = XDEV2DEV(xdev);

	xcu->base.info.model = XCU_HLS;

	err = xocl_kds_add_scu(xdev, &xcu->base);
	if (err) {
		err = 0; //Ignore this error now
		//XSCU_ERR(xcu, "Not able to add CU %p to KDS", xcu);
		goto err;
	}

	if (sysfs_create_group(&pdev->dev.kobj, &scu_attrgroup))
		XSCU_ERR(xcu, "Not able to create SCU sysfs group");

	platform_set_drvdata(pdev, xcu);

	return 0;

err:
	(void) xocl_kds_del_scu(xdev, &xcu->base);
	return err;
}

static int scu_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xrt_cu_info *info;
	struct xocl_scu *xcu;

	xcu = platform_get_drvdata(pdev);
	if (!xcu)
		return -EINVAL;

	(void) sysfs_remove_group(&pdev->dev.kobj, &scu_attrgroup);
	info = &xcu->base.info;

	(void) xocl_kds_del_cu(xdev, &xcu->base);

	if (xcu->base.res)
		vfree(xcu->base.res);

	scu_del_args(xcu);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

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
