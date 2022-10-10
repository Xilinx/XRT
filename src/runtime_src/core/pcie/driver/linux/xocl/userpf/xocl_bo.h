/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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

#ifndef _XOCL_BO_H
#define	_XOCL_BO_H

#include <ert.h>
#include "xocl_ioctl.h"
#include "../xocl_drm.h"
#include "xrt_drv.h"

#define XOCL_DEVICE_MEM 	XRT_DRV_BO_DEVICE_MEM
#define XOCL_HOST_MEM		XRT_DRV_BO_HOST_MEM
#define XOCL_DRV_ALLOC		XRT_DRV_BO_DRV_ALLOC
#define XOCL_DRM_SHMEM		XRT_DRV_BO_DRM_SHMEM
#define XOCL_USER_MEM		XRT_DRV_BO_USER_ALLOC
#define XOCL_DRM_IMPORT 	XRT_DRV_BO_DRM_IMPORT
#define XOCL_P2P_MEM		XRT_DRV_BO_P2P
#define XOCL_CMA_MEM		XRT_DRV_BO_CMA
#define XOCL_SGL		XRT_DRV_BO_SGL
#define XOCL_KERN_BUF		XRT_DRV_BO_KERN_BUF

#define XOCL_PAGE_ALLOC 	(XOCL_DRV_ALLOC | XOCL_USER_MEM | XOCL_P2P_MEM | XOCL_DRM_IMPORT | XOCL_CMA_MEM)

#define XOCL_BO_NORMAL		(XOCL_DEVICE_MEM | XOCL_HOST_MEM | XOCL_DRV_ALLOC | XOCL_DRM_SHMEM)
#define XOCL_BO_USERPTR 	(XOCL_DEVICE_MEM | XOCL_HOST_MEM | XOCL_USER_MEM)
#define XOCL_BO_P2P		(XOCL_DEVICE_MEM | XOCL_P2P_MEM)
#define XOCL_BO_DEV_ONLY	(XOCL_DEVICE_MEM)
#define XOCL_BO_IMPORT		(XOCL_HOST_MEM | XOCL_DRM_IMPORT)
#define XOCL_BO_EXECBUF		(XOCL_HOST_MEM | XOCL_DRV_ALLOC | XOCL_DRM_SHMEM)
#define XOCL_BO_CMA		(XOCL_HOST_MEM | XOCL_CMA_MEM)

/*
 * BO Usage stats stored in an array in drm_device.
 * BO types are tracked: P2P, EXECBUF, etc
 * BO usage stats to be shown in sysfs & with xbutil
*/
#define XOCL_BO_USAGE_TOTAL	7
#define XOCL_BO_USAGE_NORMAL	0 //Array indexes
#define XOCL_BO_USAGE_USERPTR	1
#define XOCL_BO_USAGE_P2P	2
#define XOCL_BO_USAGE_DEV_ONLY	3
#define XOCL_BO_USAGE_IMPORT	4
#define XOCL_BO_USAGE_EXECBUF	5
#define XOCL_BO_USAGE_CMA	6

#define XOCL_BO_DDR0 (1 << 0)
#define XOCL_BO_DDR1 (1 << 1)
#define XOCL_BO_DDR2 (1 << 2)
#define XOCL_BO_DDR3 (1 << 3)

/*
 * When the BO is imported from an ARE device. This is remote BO to
 * be accessed over ARE
 */
#define XOCL_BO_ARE  (1 << 26)

// Linux 5.18 uses iosys-map instead of dma-buf-map
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	#define XOCL_MAP_TYPE iosys_map
	#define XOCL_MAP_SET_VADDR iosys_map_set_vaddr
	#define XOCL_MAP_IS_NULL iosys_map_is_null
#else
	#define XOCL_MAP_TYPE dma_buf_map
	#define XOCL_MAP_SET_VADDR dma_buf_map_set_vaddr
	#define XOCL_MAP_IS_NULL dma_buf_map_is_null
#endif

static inline bool xocl_bo_userptr(const struct drm_xocl_bo *bo)
{
	return (bo->flags == XOCL_BO_USERPTR);
}

static inline bool xocl_bo_import(const struct drm_xocl_bo *bo)
{
	return (bo->flags == XOCL_BO_IMPORT);
}

static inline bool xocl_bo_execbuf(const struct drm_xocl_bo *bo)
{
	return (bo->flags == XOCL_BO_EXECBUF);
}
static inline bool xocl_bo_p2p(const struct drm_xocl_bo *bo)
{
	return (bo->flags == XOCL_BO_P2P);
}
static inline bool xocl_bo_cma(const struct drm_xocl_bo *bo)
{
	return (bo->flags == XOCL_BO_CMA);
}


static inline struct drm_gem_object *xocl_gem_object_lookup(struct drm_device *dev,
							    struct drm_file *filp,
							    u32 handle)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
	return drm_gem_object_lookup(filp, handle);
#elif defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,4)
	return drm_gem_object_lookup(filp, handle);
#else
	return drm_gem_object_lookup(dev, filp, handle);
#endif
#else
	return drm_gem_object_lookup(dev, filp, handle);
#endif
}

static inline struct drm_xocl_dev *bo_xocl_dev(const struct drm_xocl_bo *bo)
{
	return bo->base.dev->dev_private;
}

static inline unsigned xocl_bo_ddr_idx(unsigned user_flags)
{
        return user_flags & XRT_BO_FLAGS_MEMIDX_MASK;
}

static inline unsigned xocl_bo_type(unsigned user_flags)
{
	unsigned type = (user_flags & ~XRT_BO_FLAGS_MEMIDX_MASK);
	unsigned bo_type = 0;

	switch (type) {
	case XCL_BO_FLAGS_EXECBUF:
		bo_type = XOCL_BO_EXECBUF;
		break;
	case XCL_BO_FLAGS_P2P:
		bo_type = XOCL_BO_P2P;
		break;
	case XCL_BO_FLAGS_DEV_ONLY:
		bo_type = XOCL_BO_DEV_ONLY;
		break;
	case XCL_BO_FLAGS_CACHEABLE:
		bo_type = XOCL_BO_NORMAL;
		break;
	case XCL_BO_FLAGS_HOST_ONLY:
		bo_type = XOCL_BO_CMA;
		break;
	default:
		bo_type = XOCL_BO_NORMAL;
		break;
	}

	return bo_type;
}

static inline bool xocl_bo_sync_able(unsigned bo_flags)
{
	return ((bo_flags & XOCL_DEVICE_MEM) && (bo_flags & XOCL_HOST_MEM)) 
		|| (bo_flags & XOCL_CMA_MEM) || (bo_flags & XOCL_P2P_MEM);
}

void xocl_bo_get_usage_stat(struct xocl_drm *drm_p, u32 bo_idx,
	struct drm_xocl_mm_stat *pstat);
int xocl_create_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_userptr_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_sync_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_map_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_info_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_pwrite_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_pread_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_ctx_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_pwrite_unmgd_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_pread_unmgd_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_usage_stat_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_copy_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);

int xocl_kinfo_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_map_kern_mem_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_execbuf_callback_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_sync_bo_callback_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);

struct sg_table *xocl_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *xocl_gem_prime_import_sg_table(struct drm_device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0) && !defined(RHEL_8_5_GE)
void *xocl_gem_prime_vmap(struct drm_gem_object *obj);
void xocl_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
#else
int xocl_gem_prime_vmap(struct drm_gem_object *obj, struct XOCL_MAP_TYPE *map);
void xocl_gem_prime_vunmap(struct drm_gem_object *obj, struct XOCL_MAP_TYPE *map);
#endif

int xocl_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
int xocl_copy_import_bo(struct drm_device *dev, struct drm_file *filp,
	struct ert_start_copybo_cmd *cmd);

#endif
