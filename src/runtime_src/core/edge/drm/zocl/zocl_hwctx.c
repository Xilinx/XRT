/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "zocl_xclbin.h"
#include "zynq_hwctx.h"


/*
 * Create a new context if no active context present for this xclbin and add
 * it to the kds.
 *
 * @param       zdev:		zocl device structure
 * @param       flip:		DRM file private data
 * @param       hwctx_args:	Userspace ioctl hw context arguments
 * @param       slot_id:	Slot id for which this hw context will create
 *
 * @return      0 on success, error core on failure.
 *
 */
static int
zocl_create_hw_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_create_hw_ctx *hwctx_args, uint32_t slot_id)
{
	struct kds_client *client = filp->driver_priv;
        struct kds_client_hw_ctx *hw_ctx = NULL;
	struct drm_zocl_slot *slot = zdev->pr_slot[slot_id];
        uuid_t *uuid = NULL;
        int ret = 0;

	uuid = (uuid_t *)zocl_xclbin_get_uuid_lock(slot);
	if (!uuid) {
		DRM_ERROR("No valid slot %d available", slot_id);
		return -EINVAL;
	}

	mutex_lock(&client->lock);
	hw_ctx = kds_alloc_hw_ctx(client, uuid, slot_id);
	if (!hw_ctx) {
		ret = -EINVAL;
		goto out;
	}

	/* Lock this slot specific xclbin */
	ret = zocl_lock_bitstream(slot, uuid);
	if (ret) {
		kds_free_hw_ctx(client, hw_ctx);
		ret = -EINVAL;
		goto out;
	}

	hwctx_args->hw_context = hw_ctx->hw_ctx_idx;

out:
        mutex_unlock(&client->lock);
        return ret;
}

/*
 * This function will create a hw context for a specific slot.
 * Also it will lock that slot until destroy this hw context.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      hw context id on success, error core on failure.
 */
int
zocl_create_hw_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_create_hw_ctx *hwctx_args =
		(struct drm_zocl_create_hw_ctx *)data;
        struct kds_client *client = filp->driver_priv;
        uint32_t slot_id = 0;
        int ret = 0;

	ret = zocl_xclbin_read_axlf(zdev, hwctx_args->axlf_obj, client);
        if (ret)
                return ret;

        // SAIF TODO
	// xdev->is_legacy_ctx = false;

        /* Create the HW Context and lock the bitstream */
        slot_id = hwctx_args->axlf_obj->za_slot_id;
        return zocl_create_hw_context(zdev, filp, hwctx_args, slot_id);
}

/*
 * This function will destroy a specific hw context.
 * Also it will unlock that slot if no further reference exists.
 *
 * @param       zdev:		ZOCL device structure
 * @param       flip:		DRM file private data
 * @param       hwctx_args:	Userspace ioctl hw context arguments
 *
 * @return      0 on success, error core on failure.
 */
static int
zocl_destroy_hw_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_destroy_hw_ctx *hwctx_args)
{
        struct kds_client_hw_ctx *hw_ctx = NULL;
        struct kds_client *client = filp->driver_priv;
	struct drm_zocl_slot *slot = NULL;
	uuid_t *uuid = NULL;
        int ret = 0;

        mutex_lock(&client->lock);

        hw_ctx = kds_get_hw_ctx_by_id(client, hwctx_args->hw_context);
        if (!hw_ctx) {
                DRM_ERROR("No valid HW context is open");
                ret = -EINVAL;
		goto out;
        }

	slot = zdev->pr_slot[hw_ctx->slot_idx];
	uuid = (uuid_t *)zocl_xclbin_get_uuid_lock(slot);
	if (!uuid) {
                DRM_ERROR("No valid xclbin exists");
                ret = -EINVAL;
		goto out;
	}

        /* Unlock this slot specific xclbin */
        zocl_unlock_bitstream(slot, uuid);

        ret = kds_free_hw_ctx(client, hw_ctx);

out:
        mutex_unlock(&client->lock);
        return ret;
}

/*
 * This function will destroy the specified hw context for a specific slot.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_destroy_hw_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_destroy_hw_ctx *hwctx_args =
		(struct drm_zocl_destroy_hw_ctx *)data;

	if (!hwctx_args)
		return -EINVAL;

	return zocl_destroy_hw_context(zdev, filp, hwctx_args);
}

static inline int
zocl_open_cu_ctx_to_info(struct drm_zocl_dev *zdev, struct drm_zocl_open_cu_ctx *cu_args,
        struct kds_client_hw_ctx *hw_ctx, struct kds_client_cu_info *cu_info)
{
        uint32_t slot_hndl = hw_ctx->slot_idx;
        struct kds_sched *kds = &zdev->kds;
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
        if (cu_args->flags == ZOCL_CTX_EXCLUSIVE)
                cu_info->flags = CU_CTX_EXCLUSIVE;
        else
                cu_info->flags = CU_CTX_SHARED;

        return 0;
}

/*
 * This function will destroy a specific hw context.
 * Also it will unlock that slot if no further reference exists.
 *
 * @param       zdev:		ZOCL device structure
 * @param       flip:		DRM file private data
 * @param       hwctx_args:	Userspace ioctl hw context arguments
 *
 * @return      0 on success, error core on failure.
 */
static int
zocl_open_cu_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_open_cu_ctx *cuctx_args)
{
        struct kds_client *client = filp->driver_priv;
        struct kds_client_hw_ctx *hw_ctx = NULL;
        struct kds_client_cu_ctx *cu_ctx = NULL;
        struct kds_client_cu_info cu_info = {};
        int ret = 0;

        mutex_lock(&client->lock);

        hw_ctx = kds_get_hw_ctx_by_id(client, cuctx_args->hw_context);
        if (!hw_ctx) {
                DRM_ERROR("No valid HW context is open");
                ret = -EINVAL;
                goto out;
        }

        /* Bitstream is locked. No one could load a new one
         * until this HW context is closed.
         */
        ret = zocl_open_cu_ctx_to_info(zdev, cuctx_args, hw_ctx, &cu_info);
        if (ret) {
                DRM_ERROR("No valid CU info available for this request");
                goto out;
        }

        /* Allocate a free CU context for the given CU index */
        cu_ctx = kds_alloc_cu_hw_ctx(client, hw_ctx, &cu_info);
        if (!cu_ctx) {
                DRM_ERROR("Allocation of CU context failed");
                ret = -EINVAL;
                goto out;
        }

        ret = kds_add_context(&zdev->kds, client, cu_ctx);
        if (ret) {
                kds_free_cu_ctx(client, cu_ctx);
                goto out;
        }

        // Return the encoded cu index along with cu domain
        cuctx_args->cu_index = set_domain(cu_ctx->cu_domain,
                                           cu_ctx->cu_idx);
out:
        mutex_unlock(&client->lock);
        return ret;
}

/*
 * This function will open a CU context under a hw context.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_open_cu_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_open_cu_ctx *cuctx_args =
		(struct drm_zocl_open_cu_ctx *)data;

	if (!cuctx_args)
		return -EINVAL;

	return zocl_open_cu_context(zdev, filp, cuctx_args);
}

/*
 * Initialize the CU info using the user input.
 *
 * @param       args:   Userspace ioctl arguments
 * @param       cu_info: KDS client CU info structure
 *
 */
static inline void
zocl_close_cu_ctx_to_info(struct drm_zocl_close_cu_ctx *args,
				struct kds_client_cu_info *cu_info)
{
	/* Extract the CU information */
        cu_info->cu_domain = get_domain(args->cu_index);
        cu_info->cu_idx = get_domain_idx(args->cu_index);
}

static int
zocl_close_cu_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_close_cu_ctx *cuctx_args)
{
        struct kds_client *client = filp->driver_priv;
        struct kds_client_hw_ctx *hw_ctx = NULL;
        struct kds_client_cu_ctx *cu_ctx = NULL;
        struct kds_client_cu_info cu_info = {};
        int ret = 0;

        mutex_lock(&client->lock);

        hw_ctx = kds_get_hw_ctx_by_id(client, cuctx_args->hw_context);
        if (!hw_ctx) {
                DRM_ERROR("No valid HW context is open");
                ret = -EINVAL;
                goto out;
        }

        zocl_close_cu_ctx_to_info(cuctx_args, &cu_info);

        /* Get the corresponding CU Context */
        cu_ctx = kds_get_cu_hw_ctx(client, hw_ctx, &cu_info);
        if (!cu_ctx) {
                DRM_ERROR("No CU context is open");
                ret = -EINVAL;
                goto out;
        }

        ret = kds_del_context(&zdev->kds, client, cu_ctx);
        if (ret)
                goto out;

        ret = kds_free_cu_ctx(client, cu_ctx);

out:
        mutex_unlock(&client->lock);
        return ret;
}

/*
 * This function will close a CU context under a hw context.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_close_cu_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_close_cu_ctx *cuctx_args =
			(struct drm_zocl_close_cu_ctx *)data;

	if (!cuctx_args)
		return -EINVAL;

	return zocl_close_cu_context(zdev, filp, cuctx_args);
}

static int
zocl_open_aie_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_open_aie_ctx *aiectx_args)
{
	return 0;
#if 0
        struct kds_client *client = filp->driver_priv;
        struct kds_client_hw_ctx *hw_ctx = NULL;
        struct zocl_aie_ctx_node *aie_ctx = NULL;
        struct kds_client_cu_info cu_info = {};
        int ret = 0;

        mutex_lock(&client->lock);

        hw_ctx = kds_get_hw_ctx_by_id(client, aiectx_args->hw_context);
        if (!hw_ctx) {
                DRM_ERROR("No valid HW context is open");
                ret = -EINVAL;
                goto out;
        }

        /* Bitstream is locked. No one could load a new one
         * until this HW context is closed.
         */
        ret = zocl_open_cu_ctx_to_info(zdev, cuctx_args, hw_ctx, &cu_info);
        if (ret) {
                DRM_ERROR("No valid CU info available for this request");
                goto out;
        }

        /* Allocate a free CU context for the given CU index */
        cu_ctx = kds_alloc_cu_hw_ctx(client, hw_ctx, &cu_info);
        if (!cu_ctx) {
                DRM_ERROR("Allocation of CU context failed");
                ret = -EINVAL;
                goto out;
        }

        ret = kds_add_context(&zdev->kds, client, cu_ctx);
        if (ret) {
                kds_free_cu_ctx(client, cu_ctx);
                goto out;
        }

        // Return the encoded cu index along with cu domain
        cuctx_args->cu_index = set_domain(cu_ctx->cu_domain,
                                           cu_ctx->cu_idx);
out:
        mutex_unlock(&client->lock);
        return ret;
#endif
}

/*
 * This function will open a AIE context under a hw context.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_open_aie_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_open_aie_ctx *aiectx_args =
		(struct drm_zocl_open_aie_ctx *)data;

	if (!aiectx_args)
		return -EINVAL;

	return zocl_open_aie_context(zdev, filp, aiectx_args);
}

static int
zocl_close_aie_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_close_aie_ctx *aiectx_args)
{
	return 0;
}

/*
 * This function will close a AIE context under a hw context.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_close_aie_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_close_aie_ctx *aiectx_args =
			(struct drm_zocl_close_aie_ctx *)data;

	if (!aiectx_args)
		return -EINVAL;

	return zocl_close_aie_context(zdev, filp, aiectx_args);
}

static int
zocl_open_graph_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_open_graph_ctx *aiectx_args)
{
	return 0;
}

/*
 * This function will open a CU context under a hw context.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_open_graph_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_open_graph_ctx *graphctx_args =
		(struct drm_zocl_open_graph_ctx *)data;

	if (!graphctx_args)
		return -EINVAL;

	return zocl_open_graph_context(zdev, filp, graphctx_args);
}

static int
zocl_close_graph_context(struct drm_zocl_dev *zdev, struct drm_file *filp,
                struct drm_zocl_close_graph_ctx *aiectx_args)
{
	return 0;
}

/*
 * This function will close a GRAPH context under a hw context.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_close_graph_ctx_ioctl(struct drm_device *ddev, void *data,
			 struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
        struct drm_zocl_close_graph_ctx *graphctx_args =
			(struct drm_zocl_close_graph_ctx *)data;

	if (!graphctx_args)
		return -EINVAL;

	return zocl_close_graph_context(zdev, filp, graphctx_args);
}

/*
 * This function will destroy the specified hw context for a specific slot.
 *
 * @param       ddev:   drm device structure
 * @param       data:   userspace arguments
 * @param       flip:   DRM file private data
 *
 * @return      0 on success, error core on failure.
 */
int
zocl_hw_ctx_execbuf_ioctl(struct drm_device *ddev, void *data,
			  struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ddev->dev_private;

	return zocl_command_ioctl(zdev, data, filp);
}
