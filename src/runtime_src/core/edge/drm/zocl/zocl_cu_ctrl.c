/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "kds_core.h"
#include "xrt_cu.h"

#define CU_EXCLU_MASK		0x80000000

struct zocl_cu_ctrl {
	struct kds_ctrl		 core;
	struct drm_zocl_dev	*zdev;
	struct xrt_cu		*xcus[MAX_CUS];
	struct mutex		 lock;
	u32			 cu_refs[MAX_CUS];
	int			 num_cus;
	int			 num_clients;
	int			 configured;
};

struct client_cu_priv {
	DECLARE_BITMAP(cu_bitmap, MAX_CUS);
};

static int get_cu_by_addr(struct zocl_cu_ctrl *zcuc, u32 addr)
{
	int i;

	/* Do not use this search in critical path */
	for (i = 0; i < zcuc->num_cus; ++i) {
		if (zcuc->xcus[i]->info.addr == addr)
			break;
	}

	return i;
}

static inline int cu_mask_to_cu_idx(struct kds_command *xcmd)
{
	/* TODO: balance the CU usage if multiple bits are set */

	/* assume there is alwasy one CU */
	return 0;
}

static void
cu_ctrl_config(struct zocl_cu_ctrl *zcuc, struct kds_command *xcmd)
{
	u32 *cus_addr = (u32 *)xcmd->info;
	size_t num_cus = xcmd->isize / sizeof(u32);
	struct xrt_cu *tmp;
	int i, j;
	int apt_idx;

	mutex_lock(&zcuc->lock);
	/* I don't care if the configure command claim less number of cus */
	if (num_cus > zcuc->num_cus)
		goto error;

	/* If the configure command is sent by xclLoadXclbin(), the command
	 * content should be the same and it is okay to let it go through.
	 *
	 * But it still has chance that user would manually construct a config
	 * command, which could be wrong.
	 *
	 * So, do not allow reconfigure. This is still not totally safe, since
	 * configure command and load xclbin are not atomic.
	 *
	 * The configured flag would be reset once the last one client finished.
	 */
	if (zcuc->configured) {
		DRM_INFO("CU controller already configured\n");
		goto done;
	}

	/* Now we need to make CU index right */
	for (i = 0; i < num_cus; i++) {
		j = get_cu_by_addr(zcuc, cus_addr[i]);
		if (j == zcuc->num_cus)
			goto error;

		/* Ordering CU index */
		if (j != i) {
			tmp = zcuc->xcus[i];
			zcuc->xcus[i] = zcuc->xcus[j];
			zcuc->xcus[j] = tmp;
		}
		zcuc->xcus[i]->info.cu_idx = i;

		/* TODO: replace aperture list. Before that, keep this to make
		 * aperture work.
		 */
		apt_idx = get_apt_index_by_addr(zcuc->zdev, cus_addr[i]);
		if (apt_idx < 0) {
			DRM_ERROR("CU address %x is not found in XCLBIN\n",
				  cus_addr[i]);
			goto error;
		}
		update_cu_idx_in_apt(zcuc->zdev, apt_idx, i);
	}

	zcuc->configured = 1;

done:
	mutex_unlock(&zcuc->lock);
	/* TODO: Does it need a queue for configure commands? */
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	xcmd->cb.free(xcmd);
	return;

error:
	mutex_unlock(&zcuc->lock);
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	xcmd->cb.free(xcmd);
}

static void
cu_ctrl_dispatch(struct zocl_cu_ctrl *zcuc, struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	struct client_cu_priv *cu_priv;
	int cu_idx;
	int inst_idx;

	/* Select CU */
	cu_idx = cu_mask_to_cu_idx(xcmd);

	/* Check if selected CU is in the context */
	cu_priv = client->ctrl_priv[KDS_CU];
	if (!test_bit(cu_idx, cu_priv->cu_bitmap)) {
		xcmd->cb.notify_host(xcmd, KDS_ERROR);
		xcmd->cb.free(xcmd);
		return;
	}

	inst_idx = zcuc->xcus[cu_idx]->info.inst_idx;
	(void) zocl_cu_submit_xcmd(zcuc->zdev, inst_idx, xcmd);
}

static void
cu_ctrl_submit(struct kds_ctrl *ctrl, struct kds_command *xcmd)
{
	struct zocl_cu_ctrl *zcuc = (struct zocl_cu_ctrl *)ctrl;

	/* Priority from hight to low */
	if (xcmd->opcode != OP_CONFIG_CTRL)
		cu_ctrl_dispatch(zcuc, xcmd);
	else
		cu_ctrl_config(zcuc, xcmd);
}

static int
cu_ctrl_add_ctx(struct kds_ctrl *ctrl, struct kds_client *client,
		struct kds_ctx_info *info)
{
	struct zocl_cu_ctrl *zcuc = (struct zocl_cu_ctrl *)ctrl;
	struct client_cu_priv *cu_priv;
	int cu_idx = info->cu_idx;
	bool shared;
	int ret = 0;

	if (cu_idx >= zcuc->num_cus) {
		DRM_ERROR("CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	cu_priv = client->ctrl_priv[KDS_CU];
	if (test_and_set_bit(cu_idx, cu_priv->cu_bitmap)) {
		DRM_ERROR("CU(%d) has been added", cu_idx);
		return -EINVAL;
	}

	info->flags &= ~CU_CTX_OP_MASK;
	shared = (info->flags != CU_CTX_EXCLUSIVE);

	mutex_lock(&zcuc->lock);

	if (zcuc->cu_refs[cu_idx] & CU_EXCLU_MASK) {
		DRM_ERROR("CU(%d) has been exclusively reserved", cu_idx);
		ret = -EBUSY;
		goto err;
	}

	if (!shared && zcuc->cu_refs[cu_idx]) {
		DRM_ERROR("CU(%d) has been shared", cu_idx);
		ret = -EBUSY;
		goto err;
	}

	if (!shared)
		zcuc->cu_refs[cu_idx] |= CU_EXCLU_MASK;
	else
		++zcuc->cu_refs[cu_idx];

	mutex_unlock(&zcuc->lock);

	return 0;
err:
	clear_bit(cu_idx, cu_priv->cu_bitmap);
	return ret;
}

static int
cu_ctrl_del_ctx(struct kds_ctrl *ctrl, struct kds_client *client,
		struct kds_ctx_info *info)
{
	struct zocl_cu_ctrl *zcuc = (struct zocl_cu_ctrl *)ctrl;
	struct client_cu_priv *cu_priv;
	int cu_idx = info->cu_idx;

	if (cu_idx >= zcuc->num_cus) {
		DRM_ERROR("CU(%d) not found", cu_idx);
		return -EINVAL;
	}

	cu_priv = client->ctrl_priv[KDS_CU];
	if (!test_and_clear_bit(cu_idx, cu_priv->cu_bitmap)) {
		DRM_ERROR("CU(%d) has never been reserved", cu_idx);
		return -EINVAL;
	}

	mutex_lock(&zcuc->lock);
	if (zcuc->cu_refs[cu_idx] & CU_EXCLU_MASK)
		zcuc->cu_refs[cu_idx] = 0;
	else
		--zcuc->cu_refs[cu_idx];
	mutex_unlock(&zcuc->lock);

	return 0;
}

static int
cu_ctrl_control_ctx(struct kds_ctrl *ctrl, struct kds_client *client,
		    struct kds_ctx_info *info)
{
	struct zocl_cu_ctrl *zcuc = (struct zocl_cu_ctrl *)ctrl;
	struct client_cu_priv *cu_priv;
	u32 op;

	op = info->flags & CU_CTX_OP_MASK;
	switch (op) {
	case CU_CTX_OP_INIT:
		cu_priv = kzalloc(sizeof(*cu_priv), GFP_KERNEL);
		if (!cu_priv)
			return -ENOMEM;
		client->ctrl_priv[KDS_CU] = cu_priv;
		mutex_lock(&zcuc->lock);
		++zcuc->num_clients;
		mutex_unlock(&zcuc->lock);
		break;
	case CU_CTX_OP_FINI:
		kfree(client->ctrl_priv[KDS_CU]);
		client->ctrl_priv[KDS_CU] = NULL;
		mutex_lock(&zcuc->lock);
		--zcuc->num_clients;
		mutex_unlock(&zcuc->lock);
		break;
	case CU_CTX_OP_ADD:
		return cu_ctrl_add_ctx(ctrl, client, info);
	case CU_CTX_OP_DEL:
		return cu_ctrl_del_ctx(ctrl, client, info);
	}

	/* TODO: Still has space to improve. Since not all of the clients would
	 * need to use CU controller.
	 *
	 * But right now, the scope of a configuration is unclear.
	 * Or maybe the configuration could be per client?
	 * Or maybe config command would be removed?
	 *
	 * Anyway, for now, allow reconfigure when the last client exit.
	 */
	mutex_lock(&zcuc->lock);
	if (!zcuc->num_clients)
		zcuc->configured = 0;
	mutex_unlock(&zcuc->lock);

	return 0;
}

int cu_ctrl_add_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	struct zocl_cu_ctrl *zcuc;
	int i;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	if (!zcuc)
		return -EINVAL;

	if (zcuc->num_cus >= MAX_CUS)
		return -ENOMEM;

	for (i = 0; i < MAX_CUS; i++) {
		if (zcuc->xcus[i] != NULL)
			continue;

		zcuc->xcus[i] = xcu;
		++zcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		DRM_ERROR("Could not find a slot for CU %p\n", xcu);
		return -ENOSPC;
	}

	return 0;
}

int cu_ctrl_remove_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	struct zocl_cu_ctrl *zcuc;
	int i;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	if (!zcuc)
		return -EINVAL;

	if (zcuc->num_cus == 0)
		return -EINVAL;

	for (i = 0; i < MAX_CUS; i++) {
		if (zcuc->xcus[i] != xcu)
			continue;

		zcuc->xcus[i] = NULL;
		--zcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		DRM_ERROR("Could not find CU %p\n", xcu);
		return -EINVAL;
	}

	return 0;
}

int cu_ctrl_init(struct drm_zocl_dev *zdev)
{
	struct zocl_cu_ctrl *zcuc;

	zcuc = kzalloc(sizeof(*zcuc), GFP_KERNEL);
	if (!zcuc)
		return -ENOMEM;

	mutex_init(&zcuc->lock);
	zcuc->zdev = zdev;

	zcuc->core.control_ctx = cu_ctrl_control_ctx;
	zcuc->core.submit = cu_ctrl_submit;

	zocl_kds_setctrl(zdev, KDS_CU, (struct kds_ctrl *)zcuc);

	return 0;
}

void cu_ctrl_fini(struct drm_zocl_dev *zdev)
{
	struct zocl_cu_ctrl *zcuc;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	if (!zcuc)
		return;

	mutex_destroy(&zcuc->lock);
	kfree(zcuc);

	zocl_kds_setctrl(zdev, KDS_CU, NULL);
}
