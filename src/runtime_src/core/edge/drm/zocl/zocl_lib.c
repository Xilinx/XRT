/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Max Zhen <maxz@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "zocl_lib.h"

void __iomem *zlib_map_res(struct device *dev, struct resource *res, u64 *startp, size_t *szp)
{
	void __iomem *map = devm_ioremap(dev, res->start, res->end - res->start + 1);

	if (IS_ERR(map)) {
		zocl_err(dev, "Failed to map resource: %ld", PTR_ERR(map));
		return NULL;
	}

	if (startp)
		*startp = res->start;
	if (szp)
		*szp = res->end - res->start + 1;
	return map;
}

void __iomem *zlib_map_res_by_id(struct platform_device *pdev, int id, u64 *startp, size_t *szp)
{
	struct device *dev = &pdev->dev;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, id);

	if (!res) {
		zocl_err(dev, "failed to find resource ID (%d)", id);
		return NULL;
	}
	zocl_info(dev, "Found resource (%d): %pR", id, res);

	return zlib_map_res(dev, res, startp, szp);
}

void __iomem *zlib_map_res_by_name(struct platform_device *pdev,
				   const char *name, u64 *startp, size_t *szp)
{
	struct device *dev = &pdev->dev;
	struct resource *res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);

	if (!res) {
		zocl_err(dev, "failed to find resource (%s)", name);
		return NULL;
	}
	zocl_info(dev, "Found resource (%s): %pR", name, res);

	return zlib_map_res(dev, res, startp, szp);
}

void __iomem *zlib_map_phandle_res_by_name(struct platform_device *pdev,
					   const char *name, u64 *startp, size_t *szp)
{
	int ret = -EINVAL;
	struct resource res = {};
	struct device_node *np = NULL;
	struct device *dev = &pdev->dev;

	np = of_parse_phandle(dev->of_node, name, 0);
	if (np)
		ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		zocl_err(dev, "failed to find resource (%s): %d", name, ret);
		return NULL;
	}
	zocl_info(dev, "Found resource (%s): %pR", name, &res);

	return zlib_map_res(dev, &res, startp, szp);
}

int zlib_create_subdev(struct device *dev, const char *devname, struct resource *res, size_t nres,
		       void *info, size_t info_size, struct platform_device **pdevp)
{
	struct platform_device *pldev;
	int ret;

	pldev = platform_device_alloc(devname, PLATFORM_DEVID_AUTO);
	if (!pldev) {
		zocl_err(dev, "Failed to alloc %s device", devname);
		return -ENOMEM;
	}

	ret = platform_device_add_resources(pldev, res, nres);
	if (ret) {
		zocl_err(dev, "Failed to add resource for %s device", devname);
		goto err;
	}

	if (info) {
		ret = platform_device_add_data(pldev, info, info_size);
		if (ret) {
			zocl_err(dev, "Failed to add data for %s device", devname);
			goto err;
		}
	}

	pldev->dev.parent = dev;

	ret = platform_device_add(pldev);
	if (ret) {
		zocl_err(dev, "Failed to create %s device", devname);
		goto err;
	}

	ret = device_attach(&pldev->dev);
	if (ret != 1) {
		ret = -EINVAL;
		zocl_err(dev, "Failed to attach driver to %s device", devname);
		goto err1;
	}

	*pdevp = pldev;

	return 0;

err1:
	platform_device_del(pldev);
err:
	platform_device_put(pldev);
	return ret;
}

void zlib_destroy_subdev(struct platform_device *pdev)
{
	if (!pdev)
		return;

	platform_device_del(pdev);
	platform_device_put(pdev);
}

