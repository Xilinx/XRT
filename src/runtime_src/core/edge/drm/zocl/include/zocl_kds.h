/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_KDS_H_
#define _ZOCL_KDS_H_
void
zocl_remove_client_context(struct drm_zocl_dev *zdev,
			struct kds_client *client, struct kds_client_ctx *cctx);
struct kds_client_ctx *zocl_create_client_context(struct drm_zocl_dev *zdev,
		struct kds_client *client, uuid_t *id);
struct kds_client_ctx *zocl_check_exists_context(struct kds_client *client,
		const uuid_t *id);
struct kds_client_ctx *zocl_get_cu_context(struct drm_zocl_dev *zdev,
		struct kds_client *client, int cu_idx);
int zocl_aie_kds_add_graph_context(struct drm_zocl_dev *zdev, u32 gid,
	        u32 ctx_code, struct kds_client *client);
int zocl_aie_kds_del_graph_context(struct drm_zocl_dev *zdev, u32 gid,
	        struct kds_client *client);
void zocl_aie_kds_del_graph_context_all(struct kds_client *client);
int zocl_aie_kds_add_context(struct drm_zocl_dev *zdev, u32 ctx_code,
	struct kds_client *client);
int zocl_aie_kds_del_context(struct drm_zocl_dev *zdev,
	struct kds_client *client);
int zocl_add_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl,
			    u32 cu_idx, u32 flags, u32 cu_domain);
int zocl_del_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl,
			    u32 cu_idx, u32 cu_domain);

int zocl_init_sched(struct drm_zocl_dev *zdev);
void zocl_fini_sched(struct drm_zocl_dev *zdev);
int zocl_create_client(struct device *dev, void **client_hdl);
void zocl_destroy_client(void *client_hdl);
uint zocl_poll_client(struct file *filp, poll_table *wait);
int zocl_command_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp);
int zocl_context_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp);
int zocl_kds_reset(struct drm_zocl_dev *zdev);

#endif
