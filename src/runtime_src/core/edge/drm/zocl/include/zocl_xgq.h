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

#ifndef _ZOCL_XGQ_H_
#define _ZOCL_XGQ_H_

#include <linux/platform_device.h>
#include "xgq_cmd_common.h"
#include "xgq_cmd_vmr.h"

/*
 * Callback function for processing cmd. Cmd buffer is allocated by zocl_xgq and should be
 * freed by callback provider when it is not needed.
 */
typedef void (*zxgq_cmd_handler)(struct platform_device *pdev, struct xgq_cmd_sq_hdr *cmd);

struct zocl_xgq_init_args {
	struct platform_device *zxia_pdev;
	u32			zxia_irq;
	struct platform_device *zxia_intc_pdev;
	void __iomem		*zxia_ring;
	size_t			zxia_ring_size;
	size_t			zxia_ring_slot_size;
	void __iomem		*zxia_xgq_ip;
	void __iomem		*zxia_cq_prod_int;
	zxgq_cmd_handler	zxia_cmd_handler;
	bool			zxia_simple_cmd_hdr;
};

void *zxgq_init(struct zocl_xgq_init_args *arg);
void zxgq_fini(void *zxgq_hdl);
void zxgq_send_response(void *zxgq_hdl, struct xgq_com_queue_entry *resp);

#endif /* _ZOCL_XGQ_H_ */
