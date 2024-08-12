// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.


#include "zocl_util.h"
#include "zocl_hwctx.h"
#include <drm/drm_print.h>

int zocl_create_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_create_hw_ctx *drm_hw_ctx, struct kds_client *client, int slot_id)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct drm_zocl_slot *slot = NULL;
    int ret = 0;

    if (!client)
        return -EINVAL;

    if (slot_id < 0) {
        DRM_ERROR("%s: Invalid slot id =%d", __func__, slot_id);
        return -EINVAL;
    }
    slot = zdev->pr_slot[slot_id];

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

static int zocl_cu_ctx_to_info(struct drm_zocl_dev *zdev, struct drm_zocl_open_cu_ctx *drm_cu_ctx,
    struct kds_client_hw_ctx *kds_hw_ctx, struct kds_client_cu_info *kds_cu_info)
{
    uint32_t slot_hndl = kds_hw_ctx->slot_idx;
    struct kds_sched *kds = &zdev->kds;
    char *kname_p = drm_cu_ctx->cu_name;
    struct xrt_cu *xcu = NULL;
    char iname[CU_NAME_MAX_LEN];
    char kname[CU_NAME_MAX_LEN];
    int i = 0;

    strcpy(kname, strsep(&kname_p, ":"));
    strcpy(iname, strsep(&kname_p, ":"));

    /* Retrive the CU index from the given slot */
    for (i = 0; i < MAX_CUS; i++) {
        xcu = kds->cu_mgmt.xcus[i];
        if (!xcu)
            continue;

        if ((xcu->info.slot_idx == slot_hndl) && (!strcmp(xcu->info.kname, kname)) && (!strcmp(xcu->info.iname, iname))) {
            kds_cu_info->cu_domain = DOMAIN_PL;
            kds_cu_info->cu_idx = i;
            goto done;
        }
    }

    /* Retrive the SCU index from the given slot */
    for (i = 0; i < MAX_CUS; i++) {
        xcu = kds->scu_mgmt.xcus[i];
        if (!xcu)
            continue;

        if ((xcu->info.slot_idx == slot_hndl) && (!strcmp(xcu->info.kname, kname)) && (!strcmp(xcu->info.iname, iname))) {
            kds_cu_info->cu_domain = DOMAIN_PS;
            kds_cu_info->cu_idx = i;
            goto done;
        }
    }
    return -EINVAL;

done:
    kds_cu_info->ctx = (void *)kds_hw_ctx;
    if (drm_cu_ctx->flags == ZOCL_CTX_EXCLUSIVE)
        kds_cu_info->flags = ZOCL_CTX_EXCLUSIVE;
    else
        kds_cu_info->flags = ZOCL_CTX_SHARED;
    return 0;
}

static inline void
zocl_close_cu_ctx_to_info(struct drm_zocl_close_cu_ctx *drm_cu_ctx, struct kds_client_cu_info *kds_cu_info)
{
    kds_cu_info->cu_domain = get_domain(drm_cu_ctx->cu_index);
    kds_cu_info->cu_idx = get_domain_idx(drm_cu_ctx->cu_index);
}

int zocl_open_cu_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_open_cu_ctx *drm_cu_ctx, struct kds_client *client)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct kds_client_cu_ctx *kds_cu_ctx = NULL;
    struct kds_client_cu_info kds_cu_info = {};
    int ret = 0;

    mutex_lock(&client->lock);

    kds_hw_ctx = kds_get_hw_ctx_by_id(client, drm_cu_ctx->hw_context);
    if (!kds_hw_ctx) {
        DRM_ERROR("%s: No valid hw context is open", __func__);
        ret = -EINVAL;
        goto out;
    }

    ret = zocl_cu_ctx_to_info(zdev, drm_cu_ctx, kds_hw_ctx, &kds_cu_info);
    if (ret) {
        DRM_ERROR("%s: No valid CU context found for this hw context", __func__);
        goto out;
    }

    kds_cu_ctx = kds_alloc_cu_hw_ctx(client, kds_hw_ctx, &kds_cu_info);
    if (ret) {
        DRM_ERROR("%s: Allocation of CU context failed", __func__);
        ret = -EINVAL;
        goto out;
    }

    ret = kds_add_context(&zdev->kds, client, kds_cu_ctx);
    if (ret) {
        DRM_ERROR("%s: Failed to add kds context", __func__);
        kds_free_cu_ctx(client, kds_cu_ctx);
        goto out;
    }

    drm_cu_ctx->cu_index = set_domain(kds_cu_ctx->cu_domain, kds_cu_ctx->cu_idx);

out:
    mutex_unlock(&client->lock);
    return ret;
}

int zocl_close_cu_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_close_cu_ctx *drm_cu_ctx, struct kds_client *client)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct kds_client_cu_ctx *kds_cu_ctx = NULL;
    struct kds_client_cu_info kds_cu_info = {};
    int ret = 0;

    mutex_lock(&client->lock);

    kds_hw_ctx = kds_get_hw_ctx_by_id(client, drm_cu_ctx->hw_context);
    if (!kds_hw_ctx) {
        DRM_ERROR("%s: No valid hw context is open", __func__);
        ret = -EINVAL;
        goto out;
    }

    zocl_close_cu_ctx_to_info(drm_cu_ctx, &kds_cu_info);

    kds_cu_ctx = kds_get_cu_hw_ctx(client, kds_hw_ctx, &kds_cu_info);
    if (!kds_cu_ctx) {
        DRM_ERROR("%s: No cu context is open", __func__);
        ret = -EINVAL;
        goto out;
    }

    ret = kds_del_context(&zdev->kds, client, kds_cu_ctx);
    if (ret)
        goto out;

    ret = kds_free_cu_ctx(client, kds_cu_ctx);

out:
    mutex_unlock(&client->lock);
    return ret;
}
