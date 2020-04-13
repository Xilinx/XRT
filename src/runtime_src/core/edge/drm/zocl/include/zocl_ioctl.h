/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style CMA backed memory manager for ZynQ based OpenCL accelerators.
 *
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _XCL_ZOCL_IOCTL_H_
#define _XCL_ZOCL_IOCTL_H_

#include "zynq_ioctl.h"

int zocl_create_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_userptr_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_get_hbo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sync_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_map_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_info_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_pwrite_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_pread_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_execbuf_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_read_axlf_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sk_getcmd_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sk_create_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sk_report_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_info_cu_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);

#endif
