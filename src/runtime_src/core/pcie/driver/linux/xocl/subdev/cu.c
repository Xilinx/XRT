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

#define XCU_INFO(xcu, fmt, arg...) \
	xocl_info(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_WARN(xcu, fmt, arg...) \
	xocl_warn(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_ERR(xcu, fmt, arg...) \
	xocl_err(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_DBG(xcu, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

struct xocl_cu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
};

static ssize_t debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu *cu = platform_get_drvdata(pdev);
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
	struct xocl_cu *cu = platform_get_drvdata(pdev);
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

static struct attribute *cu_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_cu_stat.attr,
	&dev_attr_cu_info.attr,
	&dev_attr_poll_interval.attr,
	&dev_attr_busy_threshold.attr,
	&dev_attr_name.attr,
	&dev_attr_base_paddr.attr,
	&dev_attr_size.attr,
	NULL,
};

static const struct attribute_group cu_attrgroup = {
	.attrs = cu_attrs,
};

irqreturn_t cu_isr(int irq, void *arg)
{
	struct xocl_cu *xcu = arg;

	xrt_cu_clear_intr(&xcu->base);

	up(&xcu->base.sem_cu);

	return IRQ_HANDLED;
}

static int cu_add_args(struct xocl_cu *xcu, struct kernel_info *kinfo)
{
	struct xrt_cu_arg *args = NULL;
	int i;

	/* If there is no detail kernel information, maybe it is a manualy
	 * created xclbin. Print warning and let it go through
	 */
	if (!kinfo) {
		XCU_WARN(xcu, "CU %s metadata not found, xclbin maybe corrupted",
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

static void cu_del_args(struct xocl_cu *xcu)
{
	if (xcu->base.info.args)
		vfree(xcu->base.info.args);
}

static int cu_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu *xcu;
	struct resource **res;
	struct xrt_cu_info *info;
	struct kernel_info *krnl_info;
	struct xrt_cu_arg *args = NULL;
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

	info = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	BUG_ON(!info);
	memcpy(&xcu->base.info, info, sizeof(struct xrt_cu_info));

	switch (info->protocol) {
	case CTRL_HS:
	case CTRL_CHAIN:
		xcu->base.info.model = XCU_HLS;
		break;
	case CTRL_FA:
		xcu->base.info.model = XCU_FA;
		break;
	default:
		return -EINVAL;
	}

	if (!xcu->base.info.is_m2m) {
		krnl_info = xocl_query_kernel(xdev, info->kname);
		err = cu_add_args(xcu, krnl_info);
		if (err)
			goto err;
	} else {
		/* M2M CU has 3 arguments */
		args = vmalloc(sizeof(struct xrt_cu_arg) * 3);

		strcpy(args[0].name, "src_addr");
		args[0].offset = 0x10;
		args[0].size = 8;
		args[0].dir = DIR_INPUT;

		strcpy(args[1].name, "dst_addr");
		args[1].offset = 0x1C;
		args[1].size = 8;
		args[1].dir = DIR_INPUT;

		strcpy(args[2].name, "size");
		args[2].offset = 0x28;
		args[2].size = 4;
		args[2].dir = DIR_INPUT;

		xcu->base.info.num_args = 3;
		xcu->base.info.args = args;
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
	case XCU_PLRAM:
		err = xrt_cu_plram_init(&xcu->base);
		break;
	case XCU_FA:
		err = xrt_cu_fa_init(&xcu->base);
		break;
	default:
		err = -EINVAL;
	}
	if (err) {
		XCU_ERR(xcu, "Not able to initial CU %p", xcu);
		goto err2;
	}

	/* If mb_scheduler is enable, the intc subdevic would not be created.
	 * In this case, the err would be -ENODEV. Don't print error message.
	 */
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

	if (sysfs_create_group(&pdev->dev.kobj, &cu_attrgroup))
		XCU_ERR(xcu, "Not able to create CU sysfs group");

	platform_set_drvdata(pdev, xcu);

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
	info = &xcu->base.info;

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
	case XCU_PLRAM:
		xrt_cu_plram_fini(&xcu->base);
		break;
	case XCU_FA:
		xrt_cu_fa_fini(&xcu->base);
		break;
	}

	(void) xocl_kds_del_cu(xdev, &xcu->base);

	if (xcu->base.res)
		vfree(xcu->base.res);

	cu_del_args(xcu);

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
