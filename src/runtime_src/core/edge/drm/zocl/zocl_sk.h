/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
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

#ifndef _ZOCL_SK_H_
#define _ZOCL_SK_H_

#define	MAX_SOFT_KERNEL		128

#define	ZOCL_SCU_FLAGS_RELEASE	1

struct soft_cu {
	void			*sc_vregs;

	/*
	 * This semaphore is used for each soft kernel
	 * CU to wait for next command. When new command
	 * for this CU comes in or we are told to abort
	 * a CU, ert will up this semaphore.
	 */
	struct semaphore	sc_sem;

	uint32_t		sc_flags;
};

struct soft_krnl {
	struct list_head	sk_cmd_list;
	struct mutex		sk_lock;
	struct soft_cu		*sk_cu[MAX_SOFT_KERNEL];

	/*
	 * sk_ncus is a counter represents how many
	 * compute units are configured.
	 */
	uint32_t		sk_ncus;

	wait_queue_head_t	sk_wait_queue;
};

struct soft_krnl_cmd {
	struct list_head	skc_list;
	struct ert_packet	*skc_packet;
};

int zocl_init_soft_kernel(struct drm_device *drm);

#endif
