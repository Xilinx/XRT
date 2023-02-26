/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style CMA backed memory manager for ZynQ based OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _XCL_ZOCL_HWCTX_H_
#define _XCL_ZOCL_HWCTX_H_

int zocl_hw_ctx_execbuf_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_create_hw_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_destroy_hw_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_open_cu_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_close_cu_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_open_aie_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_close_aie_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_open_graph_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_close_graph_ctx_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
#endif
