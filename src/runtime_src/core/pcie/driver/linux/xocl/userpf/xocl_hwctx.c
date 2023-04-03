// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo User Function Driver
 *
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
 *
 * Authors: saifuddi@amd.com
 */

#include "common.h"

/*
 * Get the slot id for this hw context.
 */
int xocl_get_slot_id_by_hw_ctx_id(struct xocl_dev *xdev,
		struct drm_file *filp, uint32_t hw_ctx_id)
{
        struct kds_client_hw_ctx *hw_ctx = NULL;
        struct kds_client *client = filp->driver_priv;

		if (xdev->is_legacy_ctx)
			return DEFAULT_PL_SLOT;

        mutex_lock(&client->lock);
        hw_ctx = kds_get_hw_ctx_by_id(client, hw_ctx_id);
        if (!hw_ctx) {
                userpf_err(xdev, "No valid HW context is open");
                mutex_unlock(&client->lock);
                return -EINVAL;
        }
	mutex_unlock(&client->lock);

	return hw_ctx->slot_idx;
}

int xocl_create_hw_context(struct xocl_dev *xdev, struct drm_file *filp,
		struct drm_xocl_create_hw_ctx *hw_ctx_args, uint32_t slot_id)
{
	struct kds_client *client = filp->driver_priv;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	uuid_t *xclbin_id = NULL;
	int ret = 0;

	if (!client)
		return -EINVAL;

	ret = XOCL_GET_XCLBIN_ID(xdev, xclbin_id, slot_id);
	if (ret)
		return ret;

	mutex_lock(&client->lock);

	hw_ctx = kds_alloc_hw_ctx(client, xclbin_id, slot_id);
	if (!hw_ctx) {
		ret = -EINVAL;
		goto error_out;
	}

	/* Lock the bitstream. Unlock the same in destroy context */
	ret = xocl_icap_lock_bitstream(xdev, xclbin_id, slot_id);
	if (ret) {
		kds_free_hw_ctx(client, hw_ctx);
		ret = -EINVAL;
		goto error_out;
	}

	hw_ctx_args->hw_context = hw_ctx->hw_ctx_idx;

error_out:
	mutex_unlock(&client->lock);
	XOCL_PUT_XCLBIN_ID(xdev, slot_id);
	return ret;
}

int xocl_destroy_hw_context(struct xocl_dev *xdev, struct drm_file *filp,
		struct drm_xocl_destroy_hw_ctx *hw_ctx_args)
{
        struct kds_client_hw_ctx *hw_ctx = NULL;
	struct kds_client *client = filp->driver_priv;
        int ret = 0;

        mutex_lock(&client->lock);

        hw_ctx = kds_get_hw_ctx_by_id(client, hw_ctx_args->hw_context);
        if (!hw_ctx) {
                userpf_err(xdev, "No valid HW context is open");
		mutex_unlock(&client->lock);
                return -EINVAL;
        }

	/* Unlock the bitstream for this HW context if no reference is there */
	(void)xocl_icap_unlock_bitstream(xdev, hw_ctx->xclbin_id, hw_ctx->slot_idx);

	ret = kds_free_hw_ctx(client, hw_ctx);

	mutex_unlock(&client->lock);

        return ret;
}

static int
xocl_cu_ctx_to_info(struct xocl_dev *xdev, struct drm_xocl_open_cu_ctx *cu_args,
	struct kds_client_hw_ctx *hw_ctx, struct kds_client_cu_info *cu_info)
{
        uint32_t slot_hndl = hw_ctx->slot_idx;
        struct kds_sched *kds = &XDEV(xdev)->kds;
        char *kname_p = cu_args->cu_name;
        struct xrt_cu *xcu = NULL;
        char iname[CU_NAME_MAX_LEN];
        char kname[CU_NAME_MAX_LEN];
        int i = 0;

        strcpy(kname, strsep(&kname_p, ":"));
        strcpy(iname, strsep(&kname_p, ":"));

        /* Retrieve the CU index from the given slot */
        for (i = 0; i < MAX_CUS; i++) {
                xcu = kds->cu_mgmt.xcus[i];
                if (!xcu)
                        continue;

                if ((xcu->info.slot_idx == slot_hndl) &&
                                (!strcmp(xcu->info.kname, kname)) &&
                                (!strcmp(xcu->info.iname, iname))) {
                        cu_info->cu_domain = DOMAIN_PL;
                        cu_info->cu_idx = i;
                        goto done;
                }
        }

        /* Retrieve the SCU index from the given slot */
        for (i = 0; i < MAX_CUS; i++) {
                xcu = kds->scu_mgmt.xcus[i];
                if (!xcu)
                        continue;

                if ((xcu->info.slot_idx == slot_hndl) &&
                                (!strcmp(xcu->info.kname, kname)) &&
                                (!strcmp(xcu->info.iname, iname))) {
                        cu_info->cu_domain = DOMAIN_PS;
                        cu_info->cu_idx = i;
                        goto done;
                }
        }

        return -EINVAL;

done:
	cu_info->ctx = (void *)hw_ctx;
        if (cu_args->flags == XOCL_CTX_EXCLUSIVE)
                cu_info->flags = CU_CTX_EXCLUSIVE;
        else
                cu_info->flags = CU_CTX_SHARED;

        return 0;
}

static inline void
xocl_close_cu_ctx_to_info(struct drm_xocl_close_cu_ctx *args,
		   struct kds_client_cu_info *cu_info)
{
	/* Extract the CU information */
	cu_info->cu_domain = get_domain(args->cu_index);
        cu_info->cu_idx = get_domain_idx(args->cu_index);
}

int xocl_open_cu_context(struct xocl_dev *xdev, struct drm_file *filp,
		struct drm_xocl_open_cu_ctx *drm_cu_args)
{
	struct kds_client *client = filp->driver_priv;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_cu_info cu_info = {};
	int ret = 0;

        mutex_lock(&client->lock);

        hw_ctx = kds_get_hw_ctx_by_id(client, drm_cu_args->hw_context);
        if (!hw_ctx) {
                userpf_err(xdev, "No valid HW context is open");
                ret = -EINVAL;
		goto out;
        }

	/* Bitstream is locked. No one could load a new one
	 * until this HW context is closed.
	 */
	ret = xocl_cu_ctx_to_info(xdev, drm_cu_args, hw_ctx, &cu_info);
	if (ret) {
		userpf_err(xdev, "No valid CU ctx found for this HW context");
		goto out;
	}

	/* Allocate a free CU context for the given CU index */
	cu_ctx = kds_alloc_cu_hw_ctx(client, hw_ctx, &cu_info);
	if (!cu_ctx) {
		userpf_err(xdev, "Allocation of CU context failed");
                ret = -EINVAL;
		goto out;
	}

        ret = kds_add_context(&XDEV(xdev)->kds, client, cu_ctx);
        if (ret) {
                kds_free_cu_ctx(client, cu_ctx);
		goto out;
	}

	// Return the encoded cu index along with cu domain
	drm_cu_args->cu_index = set_domain(cu_ctx->cu_domain,
					   cu_ctx->cu_idx);
out:
	mutex_unlock(&client->lock);
        return ret;
}

int xocl_close_cu_context(struct xocl_dev *xdev, struct drm_file *filp,
		struct drm_xocl_close_cu_ctx *drm_cu_args)
{
	struct kds_client *client = filp->driver_priv;
	struct kds_client_hw_ctx *hw_ctx = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;
        struct kds_client_cu_info cu_info = {}; 
        int ret = 0;

        mutex_lock(&client->lock);

        hw_ctx = kds_get_hw_ctx_by_id(client, drm_cu_args->hw_context);
        if (!hw_ctx) {
                userpf_err(xdev, "No valid HW context is open");
                ret = -EINVAL;
                goto out;
        }

	xocl_close_cu_ctx_to_info(drm_cu_args, &cu_info);
	
	/* Get the corresponding CU Context */ 
        cu_ctx = kds_get_cu_hw_ctx(client, hw_ctx, &cu_info);
        if (!cu_ctx) {
                userpf_err(xdev, "No CU context is open");
                ret = -EINVAL;
                goto out;
        }

        ret = kds_del_context(&XDEV(xdev)->kds, client, cu_ctx);
        if (ret)
                goto out;

        ret = kds_free_cu_ctx(client, cu_ctx);

out:
        mutex_unlock(&client->lock);
	return ret;
}

int xocl_hw_ctx_command(struct xocl_dev *xdev, void *data,
			      struct drm_file *filp)
{
	struct drm_xocl_hw_ctx_execbuf *args = data;
	struct drm_xocl_execbuf legacy_args = {};

	/* Update the legacy structure with the appropiate value */
	legacy_args.ctx_id = args->hw_ctx_id;
	legacy_args.exec_bo_handle = args->exec_bo_handle;

	return xocl_command_ioctl(xdev, &legacy_args, filp, false);
}
