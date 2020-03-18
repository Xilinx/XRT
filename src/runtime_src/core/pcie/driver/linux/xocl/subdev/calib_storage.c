/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Chien-Wei Lan <chienwei@xilinx.com>
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

#include <linux/vmalloc.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

struct calib_cache {
	uint64_t	mem_id;
	char		*data;
};

struct calib_storage {
	struct device		*dev;
	struct mutex		lock;
	struct calib_cache	**cache;
	uint32_t		cache_num;
};

static int calib_storage_save_by_idx(struct platform_device *pdev, uint32_t idx)
{
	struct calib_storage *calib_storage = platform_get_drvdata(pdev);
	int err = 0;

	BUG_ON(!calib_storage->cache);

	calib_storage->cache[idx] = vzalloc(sizeof(struct calib_cache));
	if (!calib_storage->cache[idx]) {
		err = -ENOMEM;
		goto done;
	}

	calib_storage->cache[idx]->data = vzalloc(XOCL_CALIB_CACHE_SIZE);
	if (!calib_storage->cache[idx]->data) {
		err = -ENOMEM;
		goto done;
	}

done:
	if (err) {
		vfree(calib_storage->cache[idx]->data);
		vfree(calib_storage->cache[idx]);
		calib_storage->cache[idx] = NULL;
	}
	return err;
}

static void calib_cache_clean(struct platform_device *pdev)
{
	int i = 0;
	struct calib_storage *calib_storage = platform_get_drvdata(pdev);

	if (!calib_storage)
		return;

	mutex_lock(&calib_storage->lock);

	if (!calib_storage->cache)
		goto done;

	for (; i < calib_storage->cache_num; ++i) {
		if (!calib_storage->cache[i])
			continue;

		vfree(calib_storage->cache[i]->data);
		vfree(calib_storage->cache[i]);
		calib_storage->cache[i] = NULL;
	}
done:
	mutex_unlock(&calib_storage->lock);
}

static int calib_storage_save(struct platform_device *pdev)
{
	struct calib_storage *calib_storage = platform_get_drvdata(pdev);
	int err = 0;
	uint32_t i = 0;

	if (err)
		return err;

	calib_cache_clean(pdev);

	mutex_lock(&calib_storage->lock);

	for (; i < calib_storage->cache_num; ++i)
		err = calib_storage_save_by_idx(pdev, i);

	mutex_unlock(&calib_storage->lock);
	return err;
}

static int calib_storage_restore(struct platform_device *pdev)
{
	struct calib_storage *calib_storage = platform_get_drvdata(pdev);
	int err = 0;
	uint32_t i = 0;

	mutex_lock(&calib_storage->lock);

	BUG_ON(!calib_storage->cache);
	BUG_ON(!calib_storage->cache_num);

	for (; i < calib_storage->cache_num; ++i) {
		if (!calib_storage->cache[i])
			continue;
	}

	mutex_unlock(&calib_storage->lock);
	return err;
}


static struct calib_storage_funcs calib_storage_ops = {
	.save = calib_storage_save,
	.restore = calib_storage_restore,
};

static int calib_storage_probe(struct platform_device *pdev)
{
	struct calib_storage *calib_storage;
	int err = 0;

	calib_storage = devm_kzalloc(&pdev->dev, sizeof(*calib_storage), GFP_KERNEL);
	if (!calib_storage)
		return -ENOMEM;
	
	calib_storage->cache = vzalloc(MAX_M_COUNT*sizeof(struct calib_cache *));
	if (!calib_storage->cache) {
		return -ENOMEM;
	}

	calib_storage->cache_num = MAX_M_COUNT;

	calib_storage->dev = &pdev->dev;

	mutex_init(&calib_storage->lock);
	platform_set_drvdata(pdev, calib_storage);

	return err;
}


static int calib_storage_remove(struct platform_device *pdev)
{
	struct calib_storage *calib_storage = platform_get_drvdata(pdev);

	if (!calib_storage) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	calib_cache_clean(pdev);
	vfree(calib_storage->cache);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, calib_storage);

	return 0;
}

struct xocl_drv_private calib_storage_priv = {
	.ops = &calib_storage_ops,
};

struct platform_device_id calib_storage_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CALIB_STORAGE), (kernel_ulong_t)&calib_storage_priv },
	{ },
};

static struct platform_driver	calib_storage_driver = {
	.probe		= calib_storage_probe,
	.remove		= calib_storage_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CALIB_STORAGE),
	},
	.id_table = calib_storage_id_table,
};

int __init xocl_init_calib_storage(void)
{
	return platform_driver_register(&calib_storage_driver);
}

void xocl_fini_calib_storage(void)
{
	platform_driver_unregister(&calib_storage_driver);
}
