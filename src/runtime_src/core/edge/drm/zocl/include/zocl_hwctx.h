/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *    Bikash Singha <bikash.singha@amd.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_HWCTX_H_
#define _ZOCL_HWCTX_H_

#include "zocl_xclbin.h"


int zocl_create_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_create_hw_ctx *drm_hw_ctx, struct kds_client *client);

int zocl_destroy_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_destroy_hw_ctx *drm_hw_ctx, struct kds_client *client);

#endif
