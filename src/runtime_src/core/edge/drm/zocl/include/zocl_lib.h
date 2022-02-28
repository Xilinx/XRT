/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2016-2021 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Max Zhen <maxz@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_LIB_H_
#define _ZOCL_LIB_H_

#include <linux/platform_device.h>

#define zocl_err(dev, fmt, args...)	dev_err(dev, "%s: "fmt, __func__, ##args)
#define zocl_info(dev, fmt, args...)	dev_info(dev, "%s: "fmt, __func__, ##args)
#define zocl_dbg(dev, fmt, args...)	dev_dbg(dev, "%s: "fmt, __func__, ##args)

void __iomem *zlib_map_res(struct device *dev, struct resource *res, u64 *startp, size_t *szp);
void __iomem *zlib_map_res_by_id(struct platform_device *pdev, int id, u64 *startp, size_t *szp);
void __iomem *zlib_map_res_by_name(struct platform_device *pdev,
				   const char *name, u64 *startp, size_t *szp);
void __iomem *zlib_map_phandle_res_by_name(struct platform_device *pdev,
					   const char *name, u64 *startp, size_t *szp);
int zlib_create_subdev(struct device *dev, const char *devname, struct resource *res, size_t nres,
		       void *info, size_t info_size, struct platform_device **pdevp);
void zlib_destroy_subdev(struct platform_device *pdev);

static inline void fill_irq_res(struct resource *res, u32 irq, char *name)
{
	res->start = irq;
	res->end = irq;
	res->flags = IORESOURCE_IRQ;
	res->name = name;
}

static inline void fill_iomem_res(struct resource *res, resource_size_t start,
				  resource_size_t size, char *name)
{
	res->start = start;
	res->end = start + size - 1;
	res->flags = IORESOURCE_MEM;
	res->name = name;
}

static inline void fill_reg_res(struct resource *res, resource_size_t start, char *name)
{
	fill_iomem_res(res, start, sizeof(u32), name);
}

#endif /* _ZOCL_LIB_H_ */
