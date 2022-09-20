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
#include "kds_core.h"
#include "xclbin.h"

#define print_ecmd_info(ecmd) \
do {\
	int i;\
	printk("%s: ecmd header 0x%x\n", __func__, ecmd->header);\
	for (i = 0; i < ecmd->count; i++) {\
		printk("%s: ecmd data[%d] 0x%x\n", __func__, i, ecmd->data[i]);\
	}\
} while(0)

int kds_echo = 0;

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
 * Remove the client context and free all the memeory.
 * This function is also unlock the bitstrean for the slot associated with
 * this context.
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client structure
 * @param       cctx:   Client context structure
 *
 */
static void
zocl_remove_client_context(struct drm_zocl_dev *zdev,
			struct kds_client *client, struct kds_client_ctx *cctx)
{
	struct drm_zocl_slot *slot = NULL;
	uuid_t *id = (uuid_t *)cctx->xclbin_id;

	if (!list_empty(&cctx->cu_ctx_list))
		return;

	/* Get the corresponding slot for this xclbin */
	slot = zocl_get_slot(zdev, id);
	if (!slot)
		return;

	/* Unlock this slot specific xclbin */
	zocl_unlock_bitstream(slot, id);

	list_del(&cctx->link);
	if (cctx->xclbin_id)
		vfree(cctx->xclbin_id);
	if (cctx)
		vfree(cctx);
}

/*
 * Create a new client context and lock the bitstrean for the slot
 * associated with this context.
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client structure
 * @param       id:	XCLBIN id
 *
 * @return      newly created context on success, Error code on failure.
 *
 */
static struct kds_client_ctx *
zocl_create_client_context(struct drm_zocl_dev *zdev,
			struct kds_client *client, uuid_t *id)
{
	struct drm_zocl_slot *slot = NULL;
	struct kds_client_ctx *cctx = NULL;
	int ret = 0;

	/* Get the corresponding slot for this xclbin */
	slot = zocl_get_slot(zdev, id);
	if (!slot)
		return NULL;

	/* Lock this slot specific xclbin */
	ret = zocl_lock_bitstream(slot, id);
	if (ret)
		return NULL;

	/* Allocate the new client context and store the xclbin */
	cctx = vzalloc(sizeof(struct kds_client_ctx));
	if (!cctx) {
		(void) zocl_unlock_bitstream(slot, id);
		return NULL;
	}

	cctx->xclbin_id = vzalloc(sizeof(uuid_t));
	if (!cctx->xclbin_id) {
		vfree(cctx);
		(void) zocl_unlock_bitstream(slot, id);
		return NULL;
	}
	uuid_copy(cctx->xclbin_id, id);
	/* Multiple CU context can be active. Initializing CU context list */
	INIT_LIST_HEAD(&cctx->cu_ctx_list);
	list_add_tail(&cctx->link, &client->ctx_list);

	return cctx;
}

/*
 * Check whether there is a active context for this xclbin in this kds client.
 *
 * @param       client:	KDS client structure
 * @param       id:	XCLBIN id
 *
 * @return      existing context on success, NULL on failure.
 *
 */
static struct kds_client_ctx *
zocl_check_exists_context(struct kds_client *client, const uuid_t *id)
{
	struct kds_client_ctx *curr = NULL;

	/* Find whether the xclbin is already loaded and the context is exists
	 */
	list_for_each_entry(curr, &client->ctx_list, link)
		if (uuid_equal(curr->xclbin_id, id))
			break;

	/* Not found any matching context */
	if (&curr->link == &client->ctx_list)
		return NULL;

	return curr;
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

int zocl_add_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl, u32 cu_idx, u32 flags, u32 cu_domain)
{
	int ret = 0;
	struct kds_client_cu_info cu_info = { 0 };
	struct kds_client *client = (struct kds_client *)client_hdl;
	struct kds_client_ctx *cctx = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;

	cctx = vzalloc(sizeof(struct kds_client_ctx));
	if (!cctx)
		return -ENOMEM;

	cctx->xclbin_id = vzalloc(sizeof(uuid_t));
	if (!cctx->xclbin_id) {
		vfree(cctx);
		return -ENOMEM;
	}
	uuid_copy(cctx->xclbin_id, &uuid_null);

	/* Multiple CU context can be active. Initializing CU context list */
	INIT_LIST_HEAD(&cctx->cu_ctx_list);
	
	cu_info.cu_domain = cu_domain;
	cu_info.cu_idx = cu_idx;
	cu_info.flags = flags;

	mutex_lock(&client->lock);
	cu_ctx = kds_alloc_cu_ctx(client, cctx, &cu_info);
	if (!cu_ctx) {
		vfree(cctx->xclbin_id);
		vfree(cctx);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}
	
	list_add_tail(&(cctx->link), &client->ctx_list);
	ret = kds_add_context(&zdev->kds, client, cu_ctx);
	mutex_unlock(&client->lock);
	return ret;
}

int zocl_del_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl, u32 cu_idx, u32 cu_domain)
{
	int ret = 0;
	struct kds_client_cu_info cu_info = { 0 };
	struct kds_client *client = (struct kds_client *)client_hdl;
	struct kds_client_ctx *cctx = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;

	mutex_lock(&client->lock);
	cctx = zocl_check_exists_context(client, &uuid_null);
	if (cctx == NULL) {
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

        cu_info.cu_domain = cu_domain;
        cu_info.cu_idx = cu_idx;
	
	cu_ctx = kds_get_cu_ctx(client, cctx, &cu_info);
        if (!cu_ctx) {
		mutex_unlock(&client->lock);
                return -EINVAL;
	}

	ret = kds_del_context(&zdev->kds, client, cu_ctx);
        if (ret) {
		mutex_unlock(&client->lock);
                return ret;
	}

        ret = kds_free_cu_ctx(client, cu_ctx);
        if (ret) {
		mutex_unlock(&client->lock);
                return -EINVAL;
	}

	list_del(&cctx->link);
	mutex_unlock(&client->lock);

	if (cctx->xclbin_id)
		vfree(cctx->xclbin_id);
	if (cctx)
		vfree(cctx);
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

static void notify_execbuf(struct kds_command *xcmd, int status)
{
	struct kds_client *client = xcmd->client;
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->execbuf;

	if (status == KDS_COMPLETED)
		ecmd->state = ERT_CMD_STATE_COMPLETED;
	else if (status == KDS_ERROR)
		ecmd->state = ERT_CMD_STATE_ERROR;
	else if (status == KDS_TIMEOUT)
		ecmd->state = ERT_CMD_STATE_TIMEOUT;
	else if (status == KDS_ABORT)
		ecmd->state = ERT_CMD_STATE_ABORT;

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
		client_stat_inc(client, c_cnt[xcmd->cu_idx]);

	atomic_inc(&client->event);
	wake_up_interruptible(&client->waitq);
}

/* This function returns the corresponding context associated to the given CU
 *
 * @param	zdev:   zocl device structure
 * @param       client:	KDS client context
 * @param       cu_idx:	CU index
 *
 * @return      context on success, error core on failure.
 */
static struct kds_client_ctx *
zocl_get_cu_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		    int cu_idx)
{
	struct kds_sched *kds = &zdev->kds;
	struct drm_zocl_slot *slot = NULL;
	struct kds_cu_mgmt *cu_mgmt = NULL;
	u32 slot_idx = 0xFFFF;

	if (!kds)
		return NULL;

	cu_mgmt = &kds->cu_mgmt;
	if (cu_mgmt) {
		struct xrt_cu *xcu = cu_mgmt->xcus[cu_idx];

		/* Found the cu Index. Extract slot id out of it */
		if (xcu)
			slot_idx = xcu->info.slot_idx;
	}

	slot = zdev->pr_slot[slot_idx];
	if (slot) {
		struct kds_client_ctx *curr;
		mutex_lock(&slot->slot_xclbin_lock);
		list_for_each_entry(curr, &client->ctx_list, link) {
			if (uuid_equal(curr->xclbin_id,
				       zocl_xclbin_get_uuid(slot))) {
				curr->slot_idx = slot->slot_idx;
				break;
			}
		}
		mutex_unlock(&slot->slot_xclbin_lock);

		/* check matching context */
		if (&curr->link != &client->ctx_list) {
			/* Found one context */
			return curr;
		}
	}

	/* No match found. Invalid Context */
	return NULL;

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

uint zocl_poll_client(struct file *filp, poll_table *wait)
{
	struct drm_file *priv = filp->private_data;
	struct kds_client *client = (struct kds_client *)priv->driver_priv;
	int event = 0;

	poll_wait(filp, &client->waitq, wait);

	event = atomic_dec_if_positive(&client->event);
	if (event == -1)
		return 0;

	return POLLIN;
}

/*
 * Create a new client and initialize it with KDS.
 *
 * @param	zdev:		zocl device structure
 * @param       priv[output]:	new client pointer
 *
 * @return      0 on success, error core on failure.
 */
int zocl_create_client(struct device *dev, void **client_hdl)
{
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct kds_client *client = NULL;
	struct kds_sched  *kds = NULL;
	int ret = 0;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	if (!zdev)
		return -EINVAL;

	kds = &zdev->kds;
	client->dev = dev;
	ret = kds_init_client(kds, client);
	if (ret) {
		kfree(client);
		goto out;
	}

	/* Multiple context can be active. Initializing context list */
	INIT_LIST_HEAD(&client->ctx_list);

	INIT_LIST_HEAD(&client->graph_list);
	spin_lock_init(&client->graph_list_lock);
	*client_hdl = client;

out:
	zocl_info(dev, "created KDS client for pid(%d), ret: %d\n",
		  pid_nr(task_tgid(current)), ret);
	return ret;
}

/*
 * Destroy the given client and delete it from the KDS.
 *
 * @param	zdev:	zocl device structure
 * @param       priv:	client pointer
 *
 */
void zocl_destroy_client(void *client_hdl)
{
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct kds_client *client = (struct kds_client *)client_hdl;
	struct kds_sched  *kds = NULL;
	struct kds_client_ctx *curr = NULL;
	struct kds_client_ctx *tmp = NULL;
	struct drm_zocl_slot *slot = NULL;
	int pid = pid_nr(client->pid);

	if (!zdev) {
		zocl_info(client->dev, "client exits pid(%d)\n", pid);
		kfree(client);
		return;
	}

	kds = &zdev->kds;

	/* kds_fini_client should released resources hold by the client.
	 * release xclbin_id and unlock bitstream if needed.
	 */
	zocl_aie_kds_del_graph_context_all(client);
	kds_fini_client(kds, client);

	/* Delete all the existing context associated to this device for this
	 * client.
	 */
	list_for_each_entry_safe(curr, tmp, &client->ctx_list, link) {
		/* Get the corresponding slot for this xclbin */
		slot = zocl_get_slot(zdev, curr->xclbin_id);
		if (!slot)
			continue;

		/* Unlock this slot specific xclbin */
		zocl_unlock_bitstream(slot, curr->xclbin_id);
		vfree(curr->xclbin_id);
		list_del(&curr->link);
		vfree(curr);
	}

	zocl_info(client->dev, "client exits pid(%d)\n", pid);
	kfree(client);
}

int zocl_init_sched(struct drm_zocl_dev *zdev)
{
	return kds_init_sched(&zdev->kds);
}

void zocl_fini_sched(struct drm_zocl_dev *zdev)
{
	struct drm_zocl_bo *bo = NULL;

	bo = zdev->kds.cmdmem.bo;
	if (bo)
		zocl_drm_free_bo(bo);
	zdev->kds.cmdmem.bo = NULL;

	kds_fini_sched(&zdev->kds);
}

static void zocl_detect_fa_cmdmem(struct drm_zocl_dev *zdev,
				  struct drm_zocl_slot *slot)
{
	struct ip_layout    *ip_layout = NULL;
	struct drm_zocl_bo *bo = NULL;
	struct drm_zocl_create_bo args = { 0 };
	int i = 0;
	uint64_t size = 0;
	uint64_t base_addr = 0;
	void __iomem *vaddr = NULL;
	ulong bar_paddr = 0;

	/* Detect Fast adapter */
	ip_layout = slot->ip;
	if (!ip_layout)
		return;

	for (i = 0; i < ip_layout->m_count; ++i) {
		struct ip_data *ip = &ip_layout->m_ip_data[i];
		u32 prot;

		if (ip->m_type != IP_KERNEL)
			continue;

		prot = (ip->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT;
		if (prot != FAST_ADAPTER)
			continue;

		break;
	}

	if (i == ip_layout->m_count)
		return;

	/* TODO: logic to dynamicly select size */
	size = 4096;

	args.size = size;
	args.flags = ZOCL_BO_FLAGS_CMA;
	bo = zocl_drm_create_bo(zdev->ddev, size, args.flags);
	if (IS_ERR(bo))
		return;

	
	bar_paddr = (uint64_t)bo->cma_base.paddr;	
	base_addr = (uint64_t)bo->cma_base.paddr;	
	vaddr = bo->cma_base.vaddr;	

	zdev->kds.cmdmem.bo = bo;
	zdev->kds.cmdmem.bar_paddr = bar_paddr;
	zdev->kds.cmdmem.dev_paddr = base_addr;
	zdev->kds.cmdmem.vaddr = vaddr;
	zdev->kds.cmdmem.size = size;
}

int zocl_kds_update(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot,
		    struct drm_zocl_kds *cfg)
{
	struct drm_zocl_bo *bo = NULL;
	int i;

	if (zdev->kds.cmdmem.bo) {
		bo = zdev->kds.cmdmem.bo;
		zocl_drm_free_bo(bo);
		zdev->kds.cmdmem.bo = NULL;
		zdev->kds.cmdmem.bar_paddr = 0;
		zdev->kds.cmdmem.dev_paddr = 0;
		zdev->kds.cmdmem.vaddr = 0;
		zdev->kds.cmdmem.size = 0;
	}

	zocl_detect_fa_cmdmem(zdev, slot);

	// Default supporting interrupt mode
	zdev->kds.cu_intr_cap = 1;

	for (i = 0; i < MAX_CUS; i++) {
		struct xrt_cu *xcu;
		int apt_idx;

		xcu = zdev->kds.cu_mgmt.xcus[i];
		if (!xcu)
			continue;

		apt_idx = get_apt_index_by_addr(zdev, xcu->info.addr); 
		if (apt_idx < 0) {
			DRM_ERROR("CU address %llx is not found in XCLBIN\n",
			    xcu->info.addr);
			return apt_idx;
		}
		update_cu_idx_in_apt(zdev, apt_idx, i);
	}

	// Check for polling mode and enable CU interrupt if polling_mode is false
	if (cfg->polling)
		zdev->kds.cu_intr = 0;
	else
		zdev->kds.cu_intr = 1;

	return kds_cfg_update(&zdev->kds);
}

int zocl_kds_reset(struct drm_zocl_dev *zdev)
{
	kds_reset(&zdev->kds);
	return 0;
}
