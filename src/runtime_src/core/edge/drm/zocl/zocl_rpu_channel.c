/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Lizhi Hou <lizhih@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include "zocl_drv.h"

#define ZRPU_CHANNEL_NAME "zocl_rpu_channel"

#define ZCHAN2PDEV(chan)		((chan)->zrc_pdev)
#define ZCHAN2DEV(chan)			(&ZCHAN2PDEV(chan)->dev)
#define zchan_err(chan, fmt, args...)	zocl_err(ZCHAN2DEV(chan), fmt"\n", ##args)
#define zchan_info(chan, fmt, args...)	zocl_info(ZCHAN2DEV(chan), fmt"\n", ##args)
#define zchan_dbg(chan, fmt, args...)	zocl_dbg(ZCHAN2DEV(chan), fmt"\n", ##args)

/* reserve 4k shared memory for RPU outband communication */
#define ZRPU_CHANNEL_READY		0
#define ZRPU_CHANNEL_XGQ_OFF		4

/* hardcode XGQ buffer from offset 4K */
#define ZRPU_CHANNEL_XGQ_BUFFER		4096

struct zocl_rpu_channel {
	struct platform_device *zrc_pdev;
	void __iomem *mem_base;
	u64 mem_start;
	size_t mem_size;
};

static inline void reg_write(void __iomem *base, u64 off, u32 val)
{
	iowrite32(val, base + off);
}

static inline u32 reg_read(void __iomem *base, u64 off)
{
	return ioread32(base + off);
}

static void __iomem *zchan_map_res(struct zocl_rpu_channel *chan,
				   struct resource *res, u64 *startp, size_t *szp)
{
	void __iomem *map = devm_ioremap(ZCHAN2DEV(chan), res->start, res->end - res->start + 1);

	if (IS_ERR(map)) {
		zchan_err(chan, "Failed to map channel resource: %ld", PTR_ERR(map));
		return NULL;
	}

	if (startp)
		*startp = res->start;
	if (szp)
		*szp = res->end - res->start + 1;
	return map;
}

static void __iomem *zchan_map_res_by_name(struct zocl_rpu_channel *chan, const char *name,
					  u64 *startp, size_t *szp)
{
	int ret = -EINVAL;
	struct resource res = {};
	struct device_node *np = NULL;

	np = of_parse_phandle(ZCHAN2PDEV(chan)->dev.of_node, name, 0);
	if (np)
		ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		zchan_err(chan, "failed to find channel resource (%s): %d", name, ret);
		return NULL;
	}
	zchan_info(chan, "Found channel resource (%s): %pR", name, &res);

	return zchan_map_res(chan, &res, startp, szp);
}

static ssize_t ready_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct zocl_rpu_channel *chan = (struct zocl_rpu_channel *)dev_get_drvdata(dev);
	u32 val;

	if (kstrtou32(buf, 10, &val) < 0 || val != 1) {
		zchan_err(chan, "invalid input %d\n", val);
		return -EINVAL;
	}

	reg_write(chan->mem_base, ZRPU_CHANNEL_READY, 1);

	return count;
}
static DEVICE_ATTR_WO(ready);

static struct attribute *zrpu_channel_attrs[] = {
	&dev_attr_ready.attr,
	NULL,
};

static const struct attribute_group zrpu_channel_attrgroup = {
	.attrs = zrpu_channel_attrs,
};

static const struct of_device_id zocl_rpu_channel_of_match[] = {
	{ .compatible = "xlnx,rpu-channel", },
	{ /* end of table */ },
};

static int zrpu_channel_probe(struct platform_device *pdev)
{
	struct zocl_rpu_channel *chan;
	int ret;

	chan = devm_kzalloc(&pdev->dev, sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;

	chan->zrc_pdev = pdev;
	platform_set_drvdata(pdev, chan);

	chan->mem_base = zchan_map_res_by_name(chan, "xlnx,xgq_buffer",
					       &chan->mem_start, &chan->mem_size);
	if (!chan->mem_base) {
		zchan_err(chan, "failed to find channel buffer");
		return -EINVAL;
	}
	reg_write(chan->mem_base, ZRPU_CHANNEL_XGQ_OFF, ZRPU_CHANNEL_XGQ_BUFFER);

	ret = sysfs_create_group(&pdev->dev.kobj, &zrpu_channel_attrgroup);
	if (ret) {
		zchan_err(chan, "failed to create sysfs: %d", ret);
		return ret;
	}

	return 0;
};

static int zrpu_channel_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &zrpu_channel_attrgroup);

	return 0;
};

struct platform_driver zocl_rpu_channel_driver = {
	.driver = {
		.name = ZRPU_CHANNEL_NAME,
		.of_match_table = zocl_rpu_channel_of_match,
	},
	.probe = zrpu_channel_probe,
	.remove = zrpu_channel_remove,
};
