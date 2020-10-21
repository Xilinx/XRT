/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "xrt_cu.h"

struct zocl_cu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
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

static struct attribute *cu_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_cu_stat.attr,
	&dev_attr_cu_info.attr,
	NULL,
};

static const struct attribute_group cu_attrgroup = {
	.attrs = cu_attrs,
};

static int cu_probe(struct platform_device *pdev)
{
	struct zocl_cu *zcu;
	struct resource **res;
	struct xrt_cu_info *info;
	struct drm_zocl_dev *zdev;
	struct kernel_info *krnl_info;
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

	zdev = platform_get_drvdata(to_platform_device(pdev->dev.parent));

	krnl_info = zocl_query_kernel(zdev, info->kname);
	if (!krnl_info) {
		err = -EFAULT;
		goto err1;
	}

	args = vmalloc(sizeof(struct xrt_cu_arg) * krnl_info->anums);
	if (!args) {
		err = -ENOMEM;
		goto err1;
	}

	for (i = 0; i < krnl_info->anums; i++) {
		strcpy(args[i].name, krnl_info->args[i].name);
		args[i].offset = krnl_info->args[i].offset;
		args[i].size = krnl_info->args[i].size;
		args[i].dir = krnl_info->args[i].dir;
	}
	zcu->base.info.num_args = krnl_info->anums;
	zcu->base.info.args = args;

	err = zocl_kds_add_cu(zdev, &zcu->base);
	if (err) {
		DRM_ERROR("Not able to add CU %p to KDS", zcu);
		goto err1;
	}

	switch (info->model) {
	case XCU_HLS:
		err = xrt_cu_hls_init(&zcu->base);
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
	case XCU_FA:
		xrt_cu_fa_fini(&zcu->base);
		break;
	}

	zdev = platform_get_drvdata(to_platform_device(pdev->dev.parent));
	zocl_kds_del_cu(zdev, &zcu->base);

	if (zcu->base.res)
		vfree(zcu->base.res);

	if (info->args)
		vfree(info->args);

	sysfs_remove_group(&pdev->dev.kobj, &cu_attrgroup);

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
