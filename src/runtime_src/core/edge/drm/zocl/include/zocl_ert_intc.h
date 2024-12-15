/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Author(s):
 *        Max Zhen <maxz@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_ERT_INTC_H_
#define _ZOCL_ERT_INTC_H_

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/version.h>
#include "zocl_lib.h"

#define ERT_CSR_INTC_DEV_NAME		"ZOCL_CSR_INTC"
#define ERT_XGQ_INTC_DEV_NAME		"ZOCL_XGQ_INTC"
#define ERT_CU_INTC_DEV_NAME		"ZOCL_CU_INTC"

/*
 * Resources for one ERT INTC device.
 */
#define	ZEI_RES_IRQ		"ZOCL_ERT_INTC_IRQ"
#define	ZEI_RES_STATUS		"ZOCL_ERT_INTC_STATUS"

/* 5 registers for CU and CQ status resptectively. Max supported ID is 128. */
struct zocl_ert_intc_status_reg {
	u32		zeisr_enable;
	u32		zeisr_status[4];
};

struct zocl_ert_intc_handler {
	struct platform_device	*zeih_pdev;
	spinlock_t		 zeih_lock;
	u32			 zeih_irq;
	irq_handler_t		 zeih_cb;
	void			*zeih_arg;
	bool			 zeih_enabled;
};

struct zocl_ert_intc_drv_data {
	int (*add)(struct platform_device *pdev, u32 id, irq_handler_t handler, void *arg);
	void (*remove)(struct platform_device *pdev, u32 id);
	void (*config)(struct platform_device *pdev, u32 id, bool enabled);
};

#define	ERT_INTC_DRVDATA(pdev)	\
	((struct zocl_ert_intc_drv_data *)platform_get_device_id(pdev)->driver_data)

static inline int
zocl_ert_intc_add(struct platform_device *pdev, u32 id, irq_handler_t cb, void *arg)
{
	return ERT_INTC_DRVDATA(pdev)->add(pdev, id, cb, arg);
}

static inline void
zocl_ert_intc_remove(struct platform_device *pdev, u32 id)
{
	ERT_INTC_DRVDATA(pdev)->remove(pdev, id);
}

static inline void zocl_ert_intc_config(struct platform_device *pdev, u32 id, bool enabled)
{
	ERT_INTC_DRVDATA(pdev)->config(pdev, id, enabled);
}

static inline int zocl_ert_create_intc(struct device *dev, u32 *irqs, size_t num_irqs,
				       u64 status_reg, const char *dev_name,
				       struct platform_device **pdevp)
{
	/* Total num of res is irqs + status reg. */
	struct resource *res = kzalloc(sizeof(*res) * (num_irqs + 1), GFP_KERNEL);
	int ret = 0;
	size_t i = 0;

	if (!res)
		return -ENOMEM;

	/* Fill in IRQ and status reg resources. */
	for (i = 0; i < num_irqs; i++)
		fill_irq_res(&res[i], irqs[i], ZEI_RES_IRQ);
	fill_iomem_res(&res[i++], status_reg, sizeof(struct zocl_ert_intc_status_reg),
		       ZEI_RES_STATUS);

	ret = zlib_create_subdev(dev, dev_name, res, i, NULL, 0, pdevp);
	kfree(res);

	return ret;
}

static inline void zocl_ert_destroy_intc(struct platform_device *pdev)
{
	zlib_destroy_subdev(pdev);
}

#endif /* _ZOCL_ERT_INTC_H_ */
