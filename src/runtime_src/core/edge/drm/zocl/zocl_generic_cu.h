/** * Generic compute unit structures.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Min Ma <min.ma@xilinx.com>
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
