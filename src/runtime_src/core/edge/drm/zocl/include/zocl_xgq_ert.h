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

#ifndef _ZOCL_XGQ_ERT_H_
#define _ZOCL_XGQ_ERT_H_

#define CU_XGQ_DEV_NAME		"CU_XGQ"

/* Resources for one cu xgq device */
enum ZERT_CU_XGQ_RES {
	/* Start with IOMEM resource. */
	ZCX_RES_SQ_PROD = 0,
	ZCX_RES_CQ_PROD,
	ZCX_RES_CQ_PROD_INT, /* 0 means CQ PROD reg will trigger interrupt */
	ZCX_RES_RING,
	/* Start of IRQ resource */
	ZCX_RES_IRQ,
	ZCX_NUM_RES
};

extern struct platform_driver zocl_xgq_ert_driver;

#endif /* _ZOCL_XGQ_ERT_H_ */
