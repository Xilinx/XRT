/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_SK_H_
#define _ZOCL_SK_H_

#define	MAX_SOFT_KERNEL		128

#define	ZOCL_SCU_FLAGS_RELEASE	1

struct soft_cu {
	void			*sc_vregs;
	struct drm_gem_object	*gem_obj;

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
void zocl_fini_soft_kernel(struct drm_device *drm);

#endif
