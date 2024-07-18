/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * MPSoC based OpenCL accelerators Compute Units.
 * 
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *    Bikash Singha <bikash.singha@amd.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_util.h"
#include "zocl_hwctx.h"
#include <linux/kernel.h>

int zocl_create_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_create_hw_ctx *drm_hw_ctx)
{
    //TODO: put the logic
    printk("+++ %s: %d, created the hw ctx now returning", __func__, __LINE__);
    return 0;
}

int zocl_destroy_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_destroy_hw_ctx *drm_hw_ctx)
{
    //TODO: put the logic
    printk("+++ %s: %d, destroyed the hw ctx now returning", __func__, __LINE__);
    return 0;
}
