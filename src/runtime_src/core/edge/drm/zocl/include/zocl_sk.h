/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
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

#define SK_CRASHED		-1
#define SK_ERROR		-2
#define SK_NOTEXIST		-3
#define SK_DONE			1
#define SK_RUNNING		2

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
	uint64_t		usage;

	/*
	 * soft cu pid and parent pid. This can be used to identify if the
	 * soft cu is still running or not. The parent should never crash
	 */
	uint32_t		sc_pid;
	uint32_t		sc_parent_pid;
};

struct scu_image {
	uint32_t		si_start;	/* start instance # */
	uint32_t		si_end;		/* end instance # */
	int			si_bohdl;	/* BO handle */
	struct drm_zocl_bo	*si_bo;		/* BO to hold the image */
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

	uint32_t		sk_nimg;
	struct scu_image	*sk_img;
	wait_queue_head_t	sk_wait_queue;
};

struct soft_krnl_cmd {
	struct list_head	skc_list;
	uint32_t		skc_opcode;
	struct config_sk_image	*skc_packet;
};

int zocl_init_soft_kernel(struct drm_device *drm);
void zocl_fini_soft_kernel(struct drm_device *drm);

#endif
