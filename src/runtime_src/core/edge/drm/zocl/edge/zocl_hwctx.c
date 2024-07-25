// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.


#include "zocl_util.h"
#include "zocl_hwctx.h"
#include <drm/drm_print.h>

int zocl_create_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_create_hw_ctx *drm_hw_ctx, struct kds_client *client, int slot_id)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct drm_zocl_slot *slot = NULL;
    slot = zdev->pr_slot[slot_id];
    int ret = 0;

    if (!client)
        return -EINVAL;

    mutex_lock(&client->lock);
    kds_hw_ctx = kds_alloc_hw_ctx(client, slot->slot_xclbin->zx_uuid, slot->slot_idx);
    if (!kds_hw_ctx) {
        DRM_ERROR("%s: Failed to allocate memory for new hw ctx", __func__);
        ret = -EINVAL;
        goto error_out;
    }

    //lock the bitstream. Unloack the bitstream for destory hw ctx
    ret = zocl_lock_bitstream(slot, slot->slot_xclbin->zx_uuid);
    if (ret) {
        DRM_ERROR("%s: Locking the bistream failed", __func__);
        kds_free_hw_ctx(client, kds_hw_ctx);
        ret = -EINVAL;
        goto error_out;
    }
    drm_hw_ctx->hw_context = kds_hw_ctx->hw_ctx_idx;

error_out:
    mutex_unlock(&client->lock);
    return ret;
}

int zocl_destroy_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_destroy_hw_ctx *drm_hw_ctx, struct kds_client *client)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct drm_zocl_slot *slot = NULL;
    int ret = 0;

    mutex_lock(&client->lock);
    kds_hw_ctx = kds_get_hw_ctx_by_id(client, drm_hw_ctx->hw_context);
    if (!kds_hw_ctx) {
        DRM_ERROR("%s: No valid hw context is open", __func__);
        mutex_unlock(&client->lock);
        return -EINVAL;
    }

    slot = zdev->pr_slot[kds_hw_ctx->slot_idx];
    ret = zocl_unlock_bitstream(slot, slot->slot_xclbin->zx_uuid);
    if (ret) {
        DRM_ERROR("%s: Unlocking the bistream failed", __func__);
        return -EINVAL;
    }
    ret = kds_free_hw_ctx(client, kds_hw_ctx);
    mutex_unlock(&client->lock);
    return ret;
}
