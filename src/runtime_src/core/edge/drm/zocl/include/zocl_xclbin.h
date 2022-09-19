/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_XCLBIN_H_
#define _ZOCL_XCLBIN_H_

#include <linux/uuid.h>

struct zocl_xclbin {
	int		zx_refcnt;
	char		*zx_dtbo_path;
	void		*zx_uuid;
};

int zocl_xclbin_init(struct drm_zocl_slot *slot);
void zocl_xclbin_fini(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot);

int zocl_xclbin_set_uuid(struct drm_zocl_slot *slot, void *uuid);
void *zocl_xclbin_get_uuid(struct drm_zocl_slot *slot);
int zocl_xclbin_hold(struct drm_zocl_slot *slot, const uuid_t *id);
int zocl_lock_bitstream(struct drm_zocl_slot *slot, const uuid_t *id);
int zocl_xclbin_release(struct drm_zocl_slot *slot, const uuid_t *id);
int zocl_unlock_bitstream(struct drm_zocl_slot *slot, const uuid_t *id);
struct drm_zocl_slot *zocl_get_slot(struct drm_zocl_dev *zdev,
					uuid_t *id);

int zocl_xclbin_refcount(struct drm_zocl_slot *slot);
int zocl_xclbin_read_axlf(struct drm_zocl_dev *zdev,
	struct drm_zocl_axlf *axlf_obj, struct sched_client_ctx *client);
int zocl_xclbin_load_pdi(struct drm_zocl_dev *zdev, void *data,
			struct drm_zocl_slot *slot);
int zocl_xclbin_load_pskernel(struct drm_zocl_dev *zdev, void *data);
bool zocl_xclbin_accel_adapter(int kds_mask);
int zocl_xclbin_set_dtbo_path(struct drm_zocl_slot *slot, char *dtbo_path,
		uint32_t len);

#endif /* _ZOCL_XCLBIN_H_ */
