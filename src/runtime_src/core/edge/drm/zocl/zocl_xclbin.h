/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_XCLBIN_H_
#define _ZOCL_XCLBIN_H_

struct zocl_xclbin {
	u64		zx_last_bitstream;
	int		zx_refcnt;
	void		*zx_uuid;
};

int zocl_xclbin_init(struct drm_zocl_dev *zdev);
void zocl_xclbin_fini(struct drm_zocl_dev *zdev);

int zocl_xclbin_set_uuid(struct drm_zocl_dev *zdev, void *uuid);
void *zocl_xclbin_get_uuid(struct drm_zocl_dev *zdev);
int zocl_xclbin_ctx(struct drm_zocl_dev *zdev, struct drm_zocl_ctx *ctx,
	struct sched_client_ctx *client);
int zocl_xclbin_release(struct drm_zocl_dev *zdev);

int zocl_xclbin_refcount(struct drm_zocl_dev *zdev);
int zocl_xclbin_read_axlf(struct drm_zocl_dev *zdev,
	struct drm_zocl_axlf *axlf_obj);
int zocl_xclbin_load_pdi(struct drm_zocl_dev *zdev, void *data);

bool zocl_xclbin_accel_adapter(int kds_mask);
bool zocl_xclbin_legacy_intr(struct drm_zocl_dev *zdev);
u32  zocl_xclbin_intr_id(struct drm_zocl_dev *zdev, u32 idx);
bool zocl_xclbin_cus_support_intr(struct drm_zocl_dev *zdev);

#endif /* _ZOCL_XCLBIN_H_ */
