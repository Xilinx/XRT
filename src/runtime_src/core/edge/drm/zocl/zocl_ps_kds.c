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
 * Create a new ps kernel context if no active context present for this xclbin and add
 * it to the kds.
 *
 * @param       zdev:   zocl device structure
 * @param       client: KDS client structure
 * @param       cu_idx: CU index for which context need to create
 * @param       flags:  Flags for this context
 * @param       cu_domian: CU domain(PS/PL) for which context need to create
 *
 * @return      0 on success, error core on failure.
 *
 */
int zocl_add_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl,
			    u32 cu_idx, u32 flags, u32 cu_domain)
{
	int ret = 0;
	struct kds_client_cu_info cu_info = { 0 };
	struct kds_client *client = (struct kds_client *)client_hdl;
	struct kds_client_ctx *cctx = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_hw_ctx *hw_ctx = NULL;

	mutex_lock(&client->lock);
	cctx = zocl_check_exists_context(client, &uuid_null);
	if (cctx == NULL) {
		cctx = vzalloc(sizeof(struct kds_client_ctx));
		if (!cctx) {
			mutex_unlock(&client->lock);
			return -ENOMEM;
		}

		cctx->xclbin_id = vzalloc(sizeof(uuid_t));
		if (!cctx->xclbin_id) {
			vfree(cctx);
			mutex_unlock(&client->lock);
			return -ENOMEM;
		}
		uuid_copy(cctx->xclbin_id, &uuid_null);

		/* Multiple CU context can be active. Initializing CU context list */
		INIT_LIST_HEAD(&cctx->cu_ctx_list);

		/* This is required to maintain the command stats per hw context.
		 * for this case zocl hw context is not required. This is only for
		 * backward compartability.
		 */
		client->next_hw_ctx_id = 0;
		hw_ctx = kds_alloc_hw_ctx(client, cctx->xclbin_id, 0 /*slot id */);
		if (!hw_ctx) {
			vfree(cctx->xclbin_id);
			vfree(cctx);
			mutex_unlock(&client->lock);
			return -EINVAL;
		}

		list_add_tail(&(cctx->link), &client->ctx_list);
	}

	cu_info.cu_domain = cu_domain;
	cu_info.cu_idx = cu_idx;
	cu_info.flags = flags;

	cu_ctx = kds_alloc_cu_ctx(client, cctx, &cu_info);
	if (!cu_ctx) {
		vfree(cctx->xclbin_id);
		vfree(cctx);
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

	/* For legacy context case there are only one hw context possible i.e. 0
	 */
	hw_ctx = kds_get_hw_ctx_by_id(client, DEFAULT_HW_CTX_ID);
	if (!hw_ctx) {
		DRM_ERROR("No valid HW context is open");
		mutex_unlock(&client->lock);
		return -EINVAL;
	}

	cu_ctx->hw_ctx = hw_ctx;

	ret = kds_add_context(&zdev->kds, client, cu_ctx);
	mutex_unlock(&client->lock);
	return ret;
}

/*
 * Detele an existing PS context and remove it from the kds.
 *
 * @param       zdev:   zocl device structure
 * @param       client: KDS client structure
 * @param       cu_idx: CU index for which context need to delete
 * @param       cu_domian: CU domain(PS/PL) for which context need to delete
 *
 * @return      0 on success, error core on failure.
 *
 */
int zocl_del_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl,
			    u32 cu_idx, u32 cu_domain)
{
	int ret = 0;
	struct kds_client_cu_info cu_info = { 0 };
	struct kds_client *client = (struct kds_client *)client_hdl;
	struct kds_client_ctx *cctx = NULL;
	struct kds_client_cu_ctx *cu_ctx = NULL;
	struct kds_client_hw_ctx *hw_ctx = NULL;

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

	if (list_empty(&cctx->cu_ctx_list)) {
		/* For legacy context case there are only one hw context possible i.e. 0
		*/
		hw_ctx = kds_get_hw_ctx_by_id(client, DEFAULT_HW_CTX_ID);
		kds_free_hw_ctx(client, hw_ctx);

		list_del(&cctx->link);

		if (cctx->xclbin_id)
			vfree(cctx->xclbin_id);
		if (cctx)
			vfree(cctx);
	}

	mutex_unlock(&client->lock);
	return ret;
}
