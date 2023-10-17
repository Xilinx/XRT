/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
 *    Jeff Lin  <jeffli@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_SK_H_
#define _ZOCL_SK_H_

#include <drm/drm_file.h>
#include "ps_kernel.h"

#define	MAX_SOFT_KERNEL		128

#define	ZOCL_SCU_FLAGS_RELEASE	1

#define SK_CRASHED		-1
#define SK_ERROR		-2
#define SK_NOTEXIST		-3
#define SK_DONE			1
#define SK_RUNNING		2

struct scu_image {
	uint32_t		si_start;	/* start instance # */
	uint32_t		si_end;		/* end instance # */
	int			si_bohdl;	/* BO handle */
	struct drm_zocl_bo	*si_bo;		/* BO to hold the image */
	char		        scu_name[PS_KERNEL_NAME_LENGTH];	/* Soft Kernel Name */
};

struct soft_krnl {
	struct list_head	sk_cmd_list;
	struct mutex		sk_lock;

	/*
	 * sk_ncus is a counter represents how many
	 * compute units are configured.
	 */
	uint32_t		sk_ncus;

	int			sk_meta_bohdl[MAX_PR_SLOT_NUM];	/* metadata BO handle */
	struct drm_zocl_bo	*sk_meta_bo[MAX_PR_SLOT_NUM];		/* BO to hold metadata */
	uint32_t		sk_nimg[MAX_PR_SLOT_NUM];
	struct scu_image	*sk_img[MAX_PR_SLOT_NUM];
	wait_queue_head_t	sk_wait_queue;
};

struct soft_krnl_cmd {
	struct list_head	skc_list;
	uint32_t		skc_opcode;
	struct config_sk_image_uuid	*skc_packet;
};

int zocl_init_soft_kernel(struct drm_zocl_dev *zdev);
void zocl_fini_soft_kernel(struct drm_zocl_dev *zdev);
extern struct platform_device *zert_get_scu_pdev(struct platform_device *pdev, u32 cu_idx);
extern int zocl_scu_create_sk(struct platform_device *pdev, u32 pid, u32 parent_pid, struct drm_file *filp, int *boHandle);
extern int zocl_scu_wait_cmd_sk(struct platform_device *pdev);
extern int zocl_scu_wait_ready(struct platform_device *pdev);
extern void zocl_scu_sk_ready(struct platform_device *pdev);
extern void zocl_scu_sk_crash(struct platform_device *pdev);
extern void zocl_scu_sk_shutdown(struct platform_device *pdev);

#endif
