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

int zocl_create_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_create_hw_ctx *drm_hw_ctx, struct kds_client *client)
{
    printk("+++ %s: creating the hw ctx", __func__);

    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct drm_zocl_slot *slot = NULL;
    slot = zdev->pr_slot[0]; //hardcoding the slot for now
    int ret = 0;

    if (!client)
        return -EINVAL;

    mutex_lock(&client->lock);
    kds_hw_ctx = kds_alloc_hw_ctx(client, slot->slot_xclbin->zx_uuid, slot->slot_idx);
    if (!kds_hw_ctx) {
        ret = -EINVAL;
        goto error_out;
    }

    //lock the bitstream. Unloack the bitstream for destory hw ctx
    ret = zocl_lock_bitstream(slot, slot->slot_xclbin->zx_uuid);
    if (ret) {
        kds_free_hw_ctx(client, kds_hw_ctx);
        ret = -EINVAL;
        goto error_out;
    }
    drm_hw_ctx->hw_context = kds_hw_ctx->hw_ctx_idx;

error_out:
    mutex_unlock(&client->lock);
    printk("+++ %s: created the hw_context", __func__);
    return ret;
}

int zocl_destroy_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_destroy_hw_ctx *drm_hw_ctx, struct kds_client *client)
{
    printk("+++ %s: destroying the hw ctx", __func__);
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct drm_zocl_slot *slot = NULL;
    int ret = 0;

    mutex_lock(&client->lock);
    kds_hw_ctx = kds_get_hw_ctx_by_id(client, drm_hw_ctx->hw_context);
    if (!kds_hw_ctx) {
        printk("+++ %s: No valid hw context is open", __func__);
        mutex_unlock(&client->lock);
        return -EINVAL;
    }

    slot = zdev->pr_slot[0];
    ret = zocl_unlock_bitstream(slot, slot->slot_xclbin->zx_uuid);
    if (ret) {
        printk("+++ %s: unlocking the bistream failed with rcode=%d", __func__, ret);
        return -EINVAL;
    }
    ret = kds_free_hw_ctx(client, kds_hw_ctx);
    mutex_unlock(&client->lock);
    printk("+++ %s: destroyed the hw context", __func__);
    return ret;
}
