// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.


#ifndef _ZOCL_HWCTX_H_
#define _ZOCL_HWCTX_H_

#include "zocl_xclbin.h"


int zocl_create_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_create_hw_ctx *drm_hw_ctx, struct drm_file *filp, int slot_id);

int zocl_destroy_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_destroy_hw_ctx *drm_hw_ctx, struct drm_file *filp);

int zocl_open_cu_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_open_cu_ctx *drm_cu_ctx, struct drm_file *filp);

int zocl_close_cu_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_close_cu_ctx *drm_cu_ctx, struct drm_file *filp);

int zocl_hw_ctx_execbuf(struct drm_zocl_dev *zdev, struct drm_zocl_hw_ctx_execbuf *drm_execbuf, struct drm_file *filp);

int zocl_open_graph_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_open_graph_ctx *drm_graph_ctx, struct drm_file *filp);

int zocl_close_graph_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_close_graph_ctx *drm_graph_ctx, struct drm_file *filp);

#endif
