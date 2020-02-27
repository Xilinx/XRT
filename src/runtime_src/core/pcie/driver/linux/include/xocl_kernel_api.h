/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Amit Kumar <akum@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _XOCL_KERNEL_API_H
#define	_XOCL_KERNEL_API_H

#include "xocl_ioctl.h"

/**
 * struct drm_xocl_userptr_bo - Create buffer object with user's pointer
 * used with DRM_IOCTL_XOCL_USERPTR_BO ioctl
 *
 * @addr:       Address of buffer allocated by user
 * @size:       Requested size of the buffer object
 * @handle:     bo handle returned by the driver
 * @flags:      DRM_XOCL_BO_XXX flags
 * @type:       The type of bo
 */
struct drm_xocl_kptr_bo {
	uint64_t addr;
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
	uint32_t type;
};

/**
 * struct drm_xocl_userptr_bo - Create buffer object with user's pointer
 * used with DRM_IOCTL_XOCL_USERPTR_BO ioctl
 *
 * @sgl:       Address of buffer allocated by user
 * @size:       Requested size of the buffer object
 * @handle:     bo handle returned by the driver
 * @flags:      DRM_XOCL_BO_XXX flags
 * @type:       The type of bo
 */
struct drm_xocl_sgl_bo {
	uint64_t sgl;
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
	uint32_t type;
};

int xocl_create_bo_ifc(struct drm_xocl_create_bo *args);
int xocl_map_bo_ifc(struct drm_xocl_map_bo *args);
int xocl_sync_bo_ifc(struct drm_xocl_sync_bo *args);
int xocl_execbuf_ifc(struct drm_xocl_execbuf *args);
int xocl_info_bo_ifc(struct drm_xocl_info_bo *args);
int xocl_create_kmem_bo_ifc(struct drm_xocl_kptr_bo *args);
int xocl_remap_kmem_bo_ifc(struct drm_xocl_kptr_bo *args);
int xocl_create_sgl_bo_ifc(struct drm_xocl_sgl_bo *args);
int xocl_remap_sgl_bo_ifc(struct drm_xocl_sgl_bo *args);
void xocl_delete_bo_ifc(uint32_t bo_handle);
void __iomem *xocl_get_bo_kernel_vaddr(uint32_t bo_handle);


#endif
