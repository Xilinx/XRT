/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/sched/signal.h>
#include "zocl_drv.h"
#include "zocl_util.h"
#include "zocl_xclbin.h"
#include "zocl_kds.h"
#include "kds_core.h"
#include "kds_ert_table.h"
#include "xclbin.h"

/*
 * Callback function for async dma operation. This will also clean the
 * command memory.
 *
 * @param       arg:	kds command pointer
 * @param       ret:    return value of the dma operation.
 *
 */
static void zocl_kds_dma_complete(void *arg, int ret)
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

/*
 * Copy the user space command to kds command. Also register the callback
 * function for the DMA operation.
 *
 * @param	zdev:   zocl device structure
 * @param       flip:	DRM file private data
 * @param       ecmd:	ERT command structure
 * @param       xcmd:   KDS command structure
 *
 * @return      0 on success, Error code on failure.
 */
static int copybo_ecmd2xcmd(struct drm_zocl_dev *zdev, struct drm_file *filp,
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
	dma_handle->dma_func = zocl_kds_dma_complete;
	dma_handle->dma_arg = xcmd;
	xcmd->priv = dma_handle;

	ret = zocl_copy_bo_async(dev, filp, dma_handle, &args);
	return ret;
}

/*
 * Initialize the CU info using the user input.
 *
 * @param       args:	Userspace ioctl arguments
 * @param       cu_info: KDS client CU info structure
 *
 */
static inline void
zocl_ctx_to_info(struct drm_zocl_ctx *args, struct kds_client_cu_info *cu_info)
{
        cu_info->cu_domain = get_domain(args->cu_index);
        cu_info->cu_idx = get_domain_idx(args->cu_index);

        /* Ignore ZOCL_CTX_SHARED bit if ZOCL_CTX_EXCLUSIVE bit is set */
        if (args->flags & ZOCL_CTX_EXCLUSIVE)
                cu_info->flags = CU_CTX_EXCLUSIVE;
        else
                cu_info->flags = CU_CTX_SHARED;
}

/*
 * Create a new context if no active context present for this xclbin and add
 * it to the kds.
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client structure
 * @param       args:	Userspace ioctl arguments
 *
 * @return      0 on success, error core on failure.
 *
 */
static int
zocl_add_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		 struct drm_zocl_ctx *args)
{
	void *uuid_ptr = (void *)(uintptr_t)args->uuid_ptr;
	struct kds_client_ctx *cctx = NULL;
	struct kds_client_cu_info cu_info = { 0 };
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	uuid_t *id = NULL;
	int ret = 0;

	id = vmalloc(sizeof(uuid_t));
	if (!id)
		return -ENOMEM;

	ret = copy_from_user(id, uuid_ptr, sizeof(uuid_t));
	if (ret) {
		vfree(id);
		return ret;
	}

	mutex_lock(&client->lock);

	cctx = zocl_check_exists_context(client, id);
	if (cctx == NULL) {
		/* No existing context found. Create a new context to this client */
		cctx = zocl_create_client_context(zdev, client, id);
		if (cctx == NULL)
			goto out;
	}

	/* Bitstream is locked. No one could load a new one
	 * until this client close all of the contexts.
	 */

	zocl_ctx_to_info(args, &cu_info);
	cu_ctx = kds_alloc_cu_ctx(client, cctx, &cu_info);
	if (!cu_ctx) {
		zocl_remove_client_context(zdev, client, cctx);
		ret = -EINVAL;
		goto out;
	}

        /* For legacy context case there are only one hw context possible i.e. 0
         */
        hw_ctx = kds_get_hw_ctx_by_id(client, DEFAULT_HW_CTX_ID);
        if (!hw_ctx) {
                DRM_ERROR("No valid HW context is open");
		zocl_remove_client_context(zdev, client, cctx);
                return -EINVAL;
        }

        cu_ctx->hw_ctx = hw_ctx;

	ret = kds_add_context(&zdev->kds, client, cu_ctx);
	if (ret) {
		kds_free_cu_ctx(client, cu_ctx);
		zocl_remove_client_context(zdev, client, cctx);
		goto out;
	}

out:
	mutex_unlock(&client->lock);
	vfree(id);
	return ret;
}

/*
 * Detele an existing context and remove it from the kds.
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client structure
 * @param       args:	userspace arguments
 *
 * @return      0 on success, error core on failure.
 *
 */
static int
zocl_del_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		 struct drm_zocl_ctx *args)
{
	void *uuid_ptr = (void *)(uintptr_t)args->uuid_ptr;
	uuid_t *id = NULL;
	struct kds_client_ctx *cctx = NULL;
        struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_cu_info cu_info = { 0 };
	int ret = 0;

	id = vmalloc(sizeof(uuid_t));
	if (!id)
		return -ENOMEM;

	ret = copy_from_user(id, uuid_ptr, sizeof(uuid_t));
	if (ret) {
		vfree(id);
		return ret;
	}

	mutex_lock(&client->lock);
	cctx = zocl_check_exists_context(client, id);
	if (cctx == NULL) {
		ret = -EINVAL;
		goto out;
	}

	zocl_ctx_to_info(args, &cu_info);
	cu_ctx = kds_get_cu_ctx(client, cctx, &cu_info);
        if (!cu_ctx) {
		ret = -EINVAL;
		goto out;
	}

	ret = kds_del_context(&zdev->kds, client, cu_ctx);
	if (ret)
		goto out;

        ret = kds_free_cu_ctx(client, cu_ctx);
        if (ret)
                goto out;

	/* Delete the current client context */
	zocl_remove_client_context(zdev, client, cctx);

out:
	mutex_unlock(&client->lock);
	vfree(id);
	return ret;
}

static int
zocl_add_graph_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	void *uuid_ptr = (void *)(uintptr_t)args->uuid_ptr;
	xuid_t *xclbin_id = NULL;
	uuid_t *ctx_id = NULL;
	u32 gid = args->graph_id;
	u32 flags = args->flags;
	int ret = 0;
	struct drm_zocl_slot *slot = NULL;

	ctx_id = vmalloc(sizeof(uuid_t));
	if (!ctx_id)
		return -ENOMEM;

	ret = copy_from_user(ctx_id, uuid_ptr, sizeof(uuid_t));
	if (ret)
		goto out;

	/* Get the corresponding slot for this xclbin
	 */
	slot = zocl_get_slot(zdev, ctx_id);
	if (!slot)
		return -EINVAL;

	mutex_lock(&slot->slot_xclbin_lock);
	xclbin_id = (xuid_t *)zocl_xclbin_get_uuid(slot);
	mutex_unlock(&slot->slot_xclbin_lock);

	mutex_lock(&client->lock);
	if (!uuid_equal(ctx_id, xclbin_id)) {
		DRM_ERROR("try to allocate Graph CTX with wrong xclbin %pUB",
		    ctx_id);
		ret = -EINVAL;
		goto out;
	}

	ret = zocl_aie_kds_add_graph_context(zdev, gid, flags, client);
out:
	mutex_unlock(&client->lock);
	vfree(ctx_id);
	return ret;
}

static int
zocl_del_graph_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	u32 gid = args->graph_id;
	int ret = 0;

	mutex_lock(&client->lock);
	ret = zocl_aie_kds_del_graph_context(zdev, gid, client);
	mutex_unlock(&client->lock);

	return 0;
}

static int
zocl_add_aie_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	u32 flags = args->flags;

	return zocl_aie_kds_add_context(zdev, flags, client);
}

static int
zocl_del_aie_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	return zocl_aie_kds_del_context(zdev, client);
}

static int
zocl_open_ucu(struct drm_zocl_dev *zdev, struct kds_client *client,
	      struct drm_zocl_ctx *args)
{
	struct kds_sched  *kds = NULL;
	u32 cu_idx = args->cu_index;

	kds = &zdev->kds;
	return kds_open_ucu(kds, client, cu_idx);
}

/*
 * This is an entry point for context ioctl. This function calls the
 * appropoate function based on the requested operation from the userspace.
 *
 * @param	zdev:   zocl device structure
 * @param       data:	userspace arguments
 * @param       flip:	DRM file private data
 *
 * @return      0 on success, error core on failure.
 *
 */
int zocl_context_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp)
{
	struct drm_zocl_ctx *args = data;
	struct kds_client *client = filp->driver_priv;
	int ret = 0;

	switch (args->op) {
	case ZOCL_CTX_OP_ALLOC_CTX:
		ret = zocl_add_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_FREE_CTX:
		ret = zocl_del_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_ALLOC_GRAPH_CTX:
		ret = zocl_add_graph_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_FREE_GRAPH_CTX:
		ret = zocl_del_graph_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_ALLOC_AIE_CTX:
		ret = zocl_add_aie_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_FREE_AIE_CTX:
		ret = zocl_del_aie_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_OPEN_GCU_FD:
		ret = zocl_open_ucu(zdev, client, args);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void notify_execbuf(struct kds_command *xcmd, enum kds_status status)
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

/* Every CU is associated with a slot. And a client can open only one
 * context for a slot. Hence, from the CU we can validate whethe the
 * current context is valid or not.
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client context
 * @param       xcmd:	KDS command structure
 *
 * @return      0 on success, error core on failure.
 */
static int
check_for_open_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		      struct kds_command *xcmd)
{
	int first_cu_idx = -EINVAL;
	u32 mask = 0;
	int i, j;

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

	if (zocl_get_cu_context(zdev, client, first_cu_idx) != NULL)
		return 0;
	else
		return -EINVAL;
}

/*
 * This function will create a kds command using the given parameters from the
 * userspace and add that to the KDS.
 *
 * @param	zdev:   zocl device structure
 * @param       data:	userspace arguments
 * @param       flip:	DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int zocl_command_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp)
{
	struct drm_gem_object *gem_obj = NULL;
	struct drm_device *dev = zdev->ddev;
	struct drm_zocl_execbuf *args = data;
	struct kds_client *client = filp->driver_priv;
	struct drm_zocl_bo *zocl_bo = NULL;
	struct ert_packet *ecmd = NULL;
	struct kds_command *xcmd = NULL;
	int ret = 0;

	if (zdev->kds.bad_state) {
		DRM_ERROR("KDS is in bad state\n");
		return -EDEADLK;
	}

	gem_obj = zocl_gem_object_lookup(dev, filp, args->exec_bo_handle);
	if (!gem_obj) {
		DRM_ERROR("Look up GEM BO %d failed\n", args->exec_bo_handle);
		return -EINVAL;
	}

	zocl_bo = to_zocl_bo(gem_obj);
	if (!zocl_bo_execbuf(zocl_bo)) {
		DRM_ERROR("Command buffer is not exec buf\n");
		return -EINVAL;
	}

	ecmd = (struct ert_packet *)zocl_bo->cma_base.vaddr;

	ecmd->state = ERT_CMD_STATE_NEW;
	/* only the user command knows the real size of the payload.
	 * count is more than enough!
	 */
	xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
	if (!xcmd) {
		DRM_ERROR("Failed to alloc xcmd\n");
		ret = -ENOMEM;
		goto out;
	}
	xcmd->cb.free = kds_free_command;
	xcmd->cb.notify_host = notify_execbuf;
	xcmd->execbuf = (u32 *)ecmd;
	xcmd->gem_obj = gem_obj;
	xcmd->exec_bo_handle = args->exec_bo_handle;
	/* Default hw context. For backward compartability */
	xcmd->hw_ctx_id = 0;

	//print_ecmd_info(ecmd);

	switch(ecmd->opcode) {
	case ERT_CONFIGURE:
		xcmd->status = KDS_COMPLETED;
		xcmd->cb.notify_host(xcmd, xcmd->status);
		goto out1;
	case ERT_START_CU:
		start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_EXEC_WRITE:
		DRM_WARN_ONCE("ERT_EXEC_WRITE is obsoleted, use ERT_START_KEY_VAL\n");
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
		__attribute__ ((fallthrough));
#else
		__attribute__ ((__fallthrough__));
#endif
		/* pass through */
	case ERT_START_KEY_VAL:
		start_krnl_kv_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_FA:
		start_fa_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_COPYBO:
		ret = copybo_ecmd2xcmd(zdev, filp, to_copybo_pkg(ecmd), xcmd);
		if (ret)
			goto out1;
		goto out;
	case ERT_ABORT:
		abort_ecmd2xcmd(to_abort_pkg(ecmd), xcmd);
		break;
	default:
		DRM_ERROR("Unsupport command\n");
		ret = -EINVAL;
		goto out1;
	}

	/* Check whether client has already Open Context for this Command */
	if (check_for_open_context(zdev, client, xcmd) < 0) {
		DRM_ERROR("The client has no opening context\n");
		return -EINVAL;
	}

	/* Now, we could forget execbuf */
	ret = kds_add_command(&zdev->kds, xcmd);
	return ret;

out1:
	xcmd->cb.free(xcmd);
out:
	/* Don't forget to put gem object if error happen */
	if (ret < 0) {
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
	}
	return ret;
}

