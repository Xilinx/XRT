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

static int cu_submit(struct platform_device *pdev, struct kds_command *xcmd)
{
	struct zocl_cu *xcu = platform_get_drvdata(pdev);

	xrt_cu_submit(&xcu->base, xcmd);

	return 0;
}

static struct zocl_cu_ops cu_ops = {
	.submit = &cu_submit,
};

static struct zocl_drv_private cu_priv = {
	.ops = &cu_ops,
};

static int cu_probe(struct platform_device *pdev)
{
	struct zocl_cu *zcu;
	struct resource **res;
	struct xrt_cu_info *info;
	struct drm_zocl_dev *zdev;
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
	err = cu_ctrl_add_cu(zdev, &zcu->base);
	if (err) {
		DRM_ERROR("Not able to add CU %p to controller", zcu);
		goto err1;
	}

	switch (info->model) {
	case XCU_HLS:
		err = xrt_cu_hls_init(&zcu->base);
		break;
	default:
		err = -EINVAL;
	}
	if (err) {
		DRM_ERROR("Not able to initial CU %p\n", zcu);
		goto err2;
	}

	platform_set_drvdata(pdev, zcu);

	return 0;
err2:
	cu_ctrl_remove_cu(zdev, &zcu->base);
err1:
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
	}

	zdev = platform_get_drvdata(to_platform_device(pdev->dev.parent));
	cu_ctrl_remove_cu(zdev, &zcu->base);

	if (zcu->base.res)
		vfree(zcu->base.res);

	kfree(zcu);

	return 0;
}

static struct platform_device_id cu_id_table[] = {
	{"CU", (kernel_ulong_t)&cu_priv},
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
