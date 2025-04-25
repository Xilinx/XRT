// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "zocl_drv.h"
#include "kds_ert_table.h"
#include "zocl_util.h"
#include "zocl_hwctx.h"
#include "zocl_aie.h"
#include <drm/drm_print.h>

int zocl_create_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_create_hw_ctx *drm_hw_ctx, struct drm_file *filp, int slot_id)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct drm_zocl_slot *slot = NULL;
    struct kds_client *client = filp->driver_priv;
    int ret = 0;

    if (!client) {
        DRM_ERROR("%s: Invalid client", __func__);
        return -EINVAL;
    }

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
    //increase the count for this slot. decrease the count for destroy hw ctx
    slot->hwctx_ref_cnt++;

error_out:
    mutex_unlock(&client->lock);
    return ret;
}

int zocl_destroy_hw_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_destroy_hw_ctx *drm_hw_ctx, struct drm_file *filp)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct drm_zocl_slot *slot = NULL;
    struct kds_client *client = filp->driver_priv;
    int ret = 0;
    u32 s_id = -1;

    if (!client) {
        DRM_ERROR("%s: Invalid client", __func__);
        return -EINVAL;
    }

    mutex_lock(&client->lock);
    kds_hw_ctx = kds_get_hw_ctx_by_id(client, drm_hw_ctx->hw_context);
    if (!kds_hw_ctx) {
        DRM_ERROR("%s: No valid hw context is open", __func__);
        ret = -EINVAL;
        goto error_out;
    }

    slot = zdev->pr_slot[kds_hw_ctx->slot_idx];
    ret = zocl_unlock_bitstream(slot, slot->slot_xclbin->zx_uuid);
    if (ret) {
        DRM_ERROR("%s: Unlocking the bistream failed", __func__);
        goto error_out;
    }

    s_id = kds_hw_ctx->slot_idx;
    ret = kds_free_hw_ctx(client, kds_hw_ctx);
    if (--slot->hwctx_ref_cnt == 0) {
        zocl_destroy_aie(slot);
        zdev->slot_mask &= ~(1 << s_id);
        DRM_DEBUG("Released the slot %d", s_id);
    }

error_out:
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

int zocl_open_cu_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_open_cu_ctx *drm_cu_ctx, struct drm_file *filp)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct kds_client_cu_ctx *kds_cu_ctx = NULL;
    struct kds_client *client = filp->driver_priv;
    struct kds_client_cu_info kds_cu_info = {};
    int ret = 0;

    if (!client) {
        DRM_ERROR("%s: Invalid client", __func__);
        return -EINVAL;
    }

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

int zocl_close_cu_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_close_cu_ctx *drm_cu_ctx, struct drm_file *filp)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct kds_client_cu_ctx *kds_cu_ctx = NULL;
    struct kds_client *client = filp->driver_priv;
    struct kds_client_cu_info kds_cu_info = {};
    int ret = 0;

    if (!client) {
        DRM_ERROR("%s: Invalid client", __func__);
        return -EINVAL;
    }

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

/**
 * Callback function for async dma operation. This will also clean the
 * command memory.
 *
 * @arg:    kds command pointer
 * @ret:    return value of the dma operation.
 */
static void zocl_hwctx_kds_dma_complete(void *arg, int ret)
{
    struct kds_command *xcmd = (struct kds_command *)arg;
    zocl_dma_handle_t *dma_handle = (zocl_dma_handle_t *)xcmd->priv;

    xcmd->status = KDS_COMPLETED;
    if (ret)
        xcmd->status = KDS_ERROR;
    xcmd->cb.notify_host(xcmd, xcmd->status);
    xcmd->cb.free(xcmd);

    kfree(dma_handle);
}

/**
 * Copy the user space command to kds command. Also register the callback
 * function for the DMA operation.
 *
 * @zdev:   zocl device structure
 * @flip:   DRM file private data
 * @ecmd:   ERT command structure
 * @xcmd:   KDS command structure
 *
 * @return      0 on success, Error code on failure.
 */
static int copybo_hwctx_ecmd2xcmd(struct drm_zocl_dev *zdev, struct drm_file *filp,
			    struct ert_start_copybo_cmd *ecmd,
			    struct kds_command *xcmd)
{
    struct drm_device *dev = zdev->ddev;
    zocl_dma_handle_t *dma_handle;
    struct drm_zocl_copy_bo args = {
        .dst_handle = ecmd->dst_bo_hdl,
        .src_handle = ecmd->src_bo_hdl,
        .size = ert_copybo_size(ecmd),
        .dst_offset = ert_copybo_dst_offset(ecmd),
        .src_offset = ert_copybo_src_offset(ecmd),
    };
    int ret = 0;

    dma_handle = kmalloc(sizeof(zocl_dma_handle_t), GFP_KERNEL);
    if (!dma_handle)
        return -ENOMEM;

    memset(dma_handle, 0, sizeof(zocl_dma_handle_t));

    ret = zocl_dma_channel_instance(dma_handle, zdev);
    if (ret)
        return ret;

    /* We must set up callback for async dma operations. */
    dma_handle->dma_func = zocl_hwctx_kds_dma_complete;
    dma_handle->dma_arg = xcmd;
    xcmd->priv = dma_handle;

    return zocl_copy_bo_async(dev, filp, dma_handle, &args);
}

static void notify_hwctx_execbuf(struct kds_command *xcmd, enum kds_status status)
{
    struct kds_client *client = xcmd->client;
    struct ert_packet *ecmd = (struct ert_packet *)xcmd->execbuf;

    ecmd->state = kds_ert_table[status];

    if (xcmd->timestamp_enabled) {
        /* Only start kernel command supports timestamps */
        struct ert_start_kernel_cmd *scmd;
        struct cu_cmd_state_timestamps *ts;

        scmd = (struct ert_start_kernel_cmd *)ecmd;
        ts = ert_start_kernel_timestamps(scmd);
        ts->skc_timestamps[ERT_CMD_STATE_NEW] = xcmd->timestamp[KDS_NEW];
        ts->skc_timestamps[ERT_CMD_STATE_QUEUED] = xcmd->timestamp[KDS_QUEUED];
        ts->skc_timestamps[ERT_CMD_STATE_RUNNING] = xcmd->timestamp[KDS_RUNNING];
        ts->skc_timestamps[ecmd->state] = xcmd->timestamp[status];
    }

    ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(xcmd->gem_obj);

    if (xcmd->cu_idx >= 0)
        client_stat_inc(client, xcmd->hw_ctx_id, c_cnt[xcmd->cu_idx]);

    atomic_inc(&client->event);
    wake_up_interruptible(&client->waitq);
}

static struct kds_client_cu_ctx *
zocl_get_hw_cu_ctx(struct kds_client_hw_ctx *kds_hw_ctx, int cu_idx)
{
    struct kds_client_cu_ctx *kds_cu_ctx = NULL;
    bool found = false;

    list_for_each_entry(kds_cu_ctx, &kds_hw_ctx->cu_ctx_list, link) {
        if (kds_cu_ctx->cu_idx == cu_idx) {
            found = true;
            break;
        }
    }

    if (found)
        return kds_cu_ctx;
    return NULL;
}

static int
check_for_open_hw_cu_ctx(struct drm_zocl_dev *zdev, struct kds_client *client, struct kds_command *xcmd)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    int first_cu_idx = -EINVAL;
    u32 mask = 0;
    int i, j;
    int ret = 0;

    /* i for iterate masks, j for iterate bits */
    for (i = 0; i < xcmd->num_mask; ++i) {
        if (xcmd->cu_mask[i] == 0)
            continue;

        mask = xcmd->cu_mask[i];
        for (j = 0; mask > 0; ++j) {
            if (!(mask & 0x1)) {
                mask >>= 1;
                continue;
            }

            first_cu_idx = i * sizeof(u32) + j;
            goto out;
        }
    }

out:
    if (first_cu_idx < 0)
        return -EINVAL;

    mutex_lock(&client->lock);
    kds_hw_ctx = kds_get_hw_ctx_by_id(client, xcmd->hw_ctx_id);
    if (!kds_hw_ctx) {
        mutex_unlock(&client->lock);
        return -EINVAL;
    }

    if (zocl_get_hw_cu_ctx(kds_hw_ctx, first_cu_idx) != NULL)
        ret = 0;
    else
        ret = -EINVAL;

    mutex_unlock(&client->lock);
    return ret;
}

int zocl_hw_ctx_execbuf(struct drm_zocl_dev *zdev, struct drm_zocl_hw_ctx_execbuf *drm_hw_ctx_execbuf, struct drm_file *filp)
{
    struct drm_gem_object *gem_obj = NULL;
    struct drm_device *dev = zdev->ddev;
    struct kds_client *client = filp->driver_priv;
    struct drm_zocl_bo *zocl_bo = NULL;
    struct ert_packet *ecmd = NULL;
    struct kds_command *xcmd = NULL;
    int ret = 0;

    if (zdev->kds.bad_state) {
        DRM_ERROR("%s: KDS is in bad state", __func__);
        return -EDEADLK;
    }
    gem_obj = zocl_gem_object_lookup(dev, filp, drm_hw_ctx_execbuf->exec_bo_handle);
    if (!gem_obj) {
        DRM_ERROR("%s: Look up GEM BO %d failed", __func__, drm_hw_ctx_execbuf->exec_bo_handle);
        ret = -EINVAL;
        goto out;
    }

    zocl_bo = to_zocl_bo(gem_obj);
    if (!zocl_bo_execbuf(zocl_bo)) {
        DRM_ERROR("%s: Command Buffer is not exec buf", __func__);
        ret = -EINVAL;
        goto out;
    }

    ecmd = (struct ert_packet *)zocl_bo->cma_base.vaddr;
    ecmd->state = ERT_CMD_STATE_NEW;

    xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
    if (!xcmd) {
        DRM_ERROR("%s: Failed to alloc xcmd", __func__);
        ret = -ENOMEM;
        goto out;
    }

    xcmd->cb.free = kds_free_command;
    xcmd->cb.notify_host = notify_hwctx_execbuf;
    xcmd->execbuf = (u32 *)ecmd;
    xcmd->gem_obj = gem_obj;
    xcmd->exec_bo_handle = drm_hw_ctx_execbuf->exec_bo_handle;
    xcmd->hw_ctx_id = drm_hw_ctx_execbuf->hw_ctx_id;

    switch (ecmd->opcode) {
        case ERT_CONFIGURE:
            xcmd->status = KDS_COMPLETED;
            xcmd->cb.notify_host(xcmd, xcmd->status);
            goto out1;
        case ERT_START_CU:
            start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
            break;
        case ERT_EXEC_WRITE:
            DRM_WARN_ONCE("ERT_EXEC_WRITE is obsoleted, use ERT_START_KEY_VAL");
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
		    __attribute__ ((fallthrough));
#else
		    __attribute__ ((__fallthrough__));
#endif
        case ERT_START_KEY_VAL:
            start_krnl_kv_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
            break;
        case ERT_START_FA:
            start_fa_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
            break;
        case ERT_START_COPYBO:
            ret = copybo_hwctx_ecmd2xcmd(zdev, filp, to_copybo_pkg(ecmd), xcmd);
            if (ret)
                goto out1;
            goto out;
        case ERT_ABORT:
            abort_ecmd2xcmd(to_abort_pkg(ecmd), xcmd);
            break;
        default:
            DRM_ERROR("%s: Unsupport command", __func__);
            ret = -EINVAL;
            goto out1;
    }

    if (check_for_open_hw_cu_ctx(zdev, client, xcmd) < 0) {
        DRM_ERROR("The client has no opening context\n");
        ret = -EINVAL;
        goto out;
    }

    ret = kds_add_command(&zdev->kds, xcmd);
    goto out;

out1:
    xcmd->cb.free(xcmd);
out:
    if (ret < 0)
        ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
    return ret;
}

static int zocl_add_hw_graph_context(struct drm_zocl_dev *zdev, struct kds_client *client,
        struct kds_client_hw_ctx *kds_hw_ctx, struct drm_zocl_open_graph_ctx *drm_graph_ctx)
{
    struct zocl_hw_graph_ctx *graph_ctx = NULL;
    int ret = 0;

    list_for_each_entry(graph_ctx, &kds_hw_ctx->graph_ctx_list, link) {
        if (graph_ctx->graph_id != drm_graph_ctx->graph_id){
            continue;
        }

        if (graph_ctx->flags == ZOCL_CTX_EXCLUSIVE || drm_graph_ctx->flags == ZOCL_CTX_EXCLUSIVE) {
            DRM_ERROR("%s: graph %d is already opened with exclusive context", __func__, graph_ctx->graph_id);
            ret = -EBUSY;
            goto out;
        }

        if (graph_ctx->flags == ZOCL_CTX_PRIMARY && drm_graph_ctx->flags != ZOCL_CTX_SHARED) {
            DRM_ERROR("%s: graph %d is already opened with primary context", __func__, graph_ctx->graph_id);
            ret = -EBUSY;
            goto out;
        }
    }

    graph_ctx = kzalloc(sizeof(*graph_ctx), GFP_KERNEL);
    if (!graph_ctx) {
        DRM_ERROR("%s: Failed to allocate memory", __func__);
        ret = -ENOMEM;
        goto out;
    }
    graph_ctx->flags = drm_graph_ctx->flags;
    graph_ctx->graph_id = drm_graph_ctx->graph_id;
    graph_ctx->hw_context = drm_graph_ctx->hw_context;

    list_add_tail(&graph_ctx->link, &kds_hw_ctx->graph_ctx_list);
    return 0;

out:
    DRM_ERROR("%s: Failed to to add the graph ctx for graph = %d", __func__, drm_graph_ctx->graph_id);
    return ret;

}

int zocl_open_graph_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_open_graph_ctx *drm_graph_ctx, struct drm_file *filp)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct kds_client *client = filp->driver_priv;
    int ret = 0;

    if (!client) {
        DRM_ERROR("%s: Invalid client", __func__);
        return -EINVAL;
    }
    mutex_lock(&client->lock);
    kds_hw_ctx = kds_get_hw_ctx_by_id(client, drm_graph_ctx->hw_context);
    if (!kds_hw_ctx) {
        DRM_ERROR("%s: No valid hw context is open", __func__);
        ret = -EINVAL;
        goto out;
    }
    ret = zocl_add_hw_graph_context(zdev, client, kds_hw_ctx, drm_graph_ctx);

out:
    mutex_unlock(&client->lock);
    return ret;
}

static int zocl_del_hw_graph_context(struct drm_zocl_dev *zdev, struct kds_client *client,
        struct kds_client_hw_ctx *kds_hw_ctx, struct drm_zocl_close_graph_ctx *drm_graph_ctx)
{
    struct zocl_hw_graph_ctx *graph_ctx = NULL;
    struct list_head *gptr, *next;

    list_for_each_safe(gptr, next, &kds_hw_ctx->graph_ctx_list) {
        graph_ctx = list_entry(gptr, struct zocl_hw_graph_ctx, link);
        if (graph_ctx->graph_id == drm_graph_ctx->graph_id) {
            list_del(gptr);
            kfree(graph_ctx);
            return 0;
        }
    }

    DRM_ERROR("%s Failed to close graph context: graph id %d does not exist", __func__, drm_graph_ctx->graph_id);

    return -EINVAL;
}

int zocl_close_graph_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_close_graph_ctx *drm_graph_ctx, struct drm_file *filp)
{
    struct kds_client_hw_ctx *kds_hw_ctx = NULL;
    struct kds_client *client = filp->driver_priv;
    int ret = 0;

    if (!client) {
        DRM_ERROR("%s: Invalid client", __func__);
        return -EINVAL;
    }
    mutex_lock(&client->lock);

    kds_hw_ctx = kds_get_hw_ctx_by_id(client, drm_graph_ctx->hw_context);
    if (!kds_hw_ctx) {
        DRM_ERROR("%s: No valid hw context is open", __func__);
        ret = -EINVAL;
        goto out;
    }

    ret = zocl_del_hw_graph_context(zdev, client, kds_hw_ctx, drm_graph_ctx);

out:
    mutex_unlock(&client->lock);
    return ret;
}
