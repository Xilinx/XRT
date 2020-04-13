/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Generic compute unit structures.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_GENERIC_CU_H_
#define _ZOCL_GENERIC_CU_H_

/* Bits in generic_cu_info.flag */
#define GCU_IRQ_DISABLED 0

struct generic_cu {
	struct drm_zocl_dev	*zdev;
	struct generic_cu_info	*info;
	wait_queue_head_t	waitq;
	atomic_t		event;
	unsigned long		flag;
	spinlock_t		lock;
};

struct generic_cu_info {
	const char		*name;
	int			cu_idx;
	int                     irq;
};

int zocl_open_gcu(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
		  struct sched_client_ctx *client);

#endif
