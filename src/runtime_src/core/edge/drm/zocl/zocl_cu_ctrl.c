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

struct zocl_cu_ctrl {
	struct kds_controller    core;
	struct drm_zocl_dev	*zdev;
	struct xrt_cu		*xcus[MAX_CUS];
	int			 num_cus;

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

	/* I don't care if the configure command claim less number of cus */
	if (num_cus > zcuc->num_cus)
		goto error;

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

	/* TODO: Does it need a queue for configure commands? */
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	xcmd->cb.free(xcmd);
	return;

error:
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	xcmd->cb.free(xcmd);
}

static void
cu_ctrl_dispatch(struct zocl_cu_ctrl *zcuc, struct kds_command *xcmd)
{
	int cu_idx;
	int inst_idx;

	/* Select CU */
	cu_idx = cu_mask_to_cu_idx(xcmd);
	inst_idx = zcuc->xcus[cu_idx]->info.inst_idx;
	(void) zocl_cu_submit_xcmd(zcuc->zdev, inst_idx, xcmd);
}

static void
cu_ctrl_submit(struct kds_controller *ctrl, struct kds_command *xcmd)
{
	struct zocl_cu_ctrl *zcuc = (struct zocl_cu_ctrl *)ctrl;

	/* Priority from hight to low */
	if (xcmd->opcode != OP_CONFIG_CTRL)
		cu_ctrl_dispatch(zcuc, xcmd);
	else
		cu_ctrl_config(zcuc, xcmd);
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

	zcuc->zdev = zdev;

	zcuc->core.submit = cu_ctrl_submit;

	zocl_kds_setctrl(zdev, KDS_CU, (struct kds_controller *)zcuc);

	return 0;
}

void cu_ctrl_fini(struct drm_zocl_dev *zdev)
{
	struct zocl_cu_ctrl *zcuc;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	if (!zcuc)
		return;

	kfree(zcuc);

	zocl_kds_setctrl(zdev, KDS_CU, NULL);
}
