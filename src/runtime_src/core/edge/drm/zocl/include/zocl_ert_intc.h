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

#ifndef _ZOCL_ERT_INTC_H_
#define _ZOCL_ERT_INTC_H_

#include <linux/interrupt.h>

#define ERT_CQ_INTC_DEV_NAME		"ZOCL_CQ_INTC"
/* TODO: support CU intc when hardware supports ERT ssv3 */
#define ERT_CU_INTC_DEV_NAME		"ZOCL_CU_INTC"
#define ERT_XGQ_INTC_DEV_NAME		"ZOCL_XGQ_INTC"

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
	struct platform_device *zeih_pdev;
	u32		zeih_irq;
	irq_handler_t	zeih_cb;
	void		*zeih_arg;
	bool		zeih_enabled;
};

struct zocl_ert_intc_drv_data {
	void (*add)(struct platform_device *pdev, u32 id, irq_handler_t handler, void *arg);
	void (*remove)(struct platform_device *pdev, u32 id);
	void (*config)(struct platform_device *pdev, u32 id);
};

#define	ERT_INTC_DRVDATA(pdev)	\
	((struct zocl_ert_intc_drv_data *)platform_get_device_id(pdev)->driver_data)

static inline void
zocl_ert_intc_add(struct platform_device *pdev, u32 id, irq_handler_t cb, void *arg)
{
	ERT_INTC_DRVDATA(pdev)->add(pdev, id, cb, arg);
}

static inline void
zocl_ert_intc_remove(struct platform_device *pdev, u32 id)
{
	ERT_INTC_DRVDATA(pdev)->remove(pdev, id);
}

/* TODO: */
//extern void zocl_ert_intc_config(struct platform_device *pdev, u32 id, bool enabled);

#endif /* _ZOCL_ERT_INTC_H_ */
