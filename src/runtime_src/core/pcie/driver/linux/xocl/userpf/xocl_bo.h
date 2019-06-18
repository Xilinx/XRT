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



#if 0
#define XOCL_DEVICE_MEM		(1 << 31)
#define XOCL_HOST_MEM		(1 << 30)
#define XOCL_HOST_ALLOC		(1 << 29)
#define XOCL_CONTI_MEM		(1 << 28)
#define XOCL_CACHEABLE		(1 << 27)
#define XOCL_PAGE_ALLOC		(1 << 26)
#define XOCL_DRM_IMPORT		(1 << 25)
#define XOCL_DRM_CACHEABLE	(1 << 24)

#define XOCL_BO_NORMAL		(XOCL_DEVICE_MEM | XOCL_HOST_MEM | XOCL_HOST_ALLOC | XOCL_PAGE_ALLOC)
#define XOCL_BO_USERPTR		(XOCL_DEVICE_MEM | XOCL_HOST_MEM |		     XOCL_PAGE_ALLOC)
#define XOCL_BO_P2P		(XOCL_DEVICE_MEM |  				     XOCL_PAGE_ALLOC)
#define XOCL_BO_DEV_ONLY	(XOCL_DEVICE_MEM)
#define XOCL_BO_IMPORT				  (XOCL_HOST_MEM | 		     XOCL_PAGE_ALLOC)
#define XOCL_BO_EXECBUF				  (XOCL_HOST_MEM | XOCL_HOST_ALLOC | XOCL_PAGE_ALLOC)
#define XOCL_BO_CMA				  (XOCL_HOST_MEM | XOCL_HOST_ALLOC | XOCL_PAGE_ALLOC | XOCL_CONTI_MEM)
#else
#define XOCL_DEVICE_MEM		(1 << 31)
#define XOCL_CMA_MEM		(1 << 30)
#define XOCL_USER_MEM		(1 << 29)
#define XOCL_DRM_MEM		(1 << 28)
#define XOCL_P2P_MEM		(1 << 27)
#define XOCL_DRM_IMPORT		(1 << 26)

#define XOCL_HOST_MEM		(XOCL_CMA_MEM | XOCL_USER_MEM | XOCL_DRM_MEM)
#define XOCL_PAGE_ALLOC		(XOCL_CMA_MEM | XOCL_USER_MEM | XOCL_DRM_MEM | XOCL_P2P_MEM | XOCL_DRM_IMPORT)

#define XOCL_BO_NORMAL		(XOCL_DEVICE_MEM | 				    XOCL_DRM_MEM)
#define XOCL_BO_USERPTR		(XOCL_DEVICE_MEM | 		   XOCL_USER_MEM)
#define XOCL_BO_P2P		(XOCL_DEVICE_MEM |  				                   XOCL_P2P_MEM)
#define XOCL_BO_DEV_ONLY	(XOCL_DEVICE_MEM)
#define XOCL_BO_IMPORT				  		   						 (XOCL_DRM_IMPORT)
#define XOCL_BO_EXECBUF				  				   (XOCL_DRM_MEM)
#define XOCL_BO_CMA				  (XOCL_CMA_MEM)
#endif


#define BO_SYNC_ABLE		(XOCL_DEVICE_MEM | XOCL_HOST_MEM)

#define XOCL_BO_DDR0 (1 << 0)
#define XOCL_BO_DDR1 (1 << 1)
#define XOCL_BO_DDR2 (1 << 2)
#define XOCL_BO_DDR3 (1 << 3)

/*
 * When the BO is imported from an ARE device. This is remote BO to
 * be accessed over ARE
 */
#define XOCL_BO_ARE  (1 << 26)

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

static inline bool xocl_bo_cma(const struct drm_xocl_bo *bo)
{
	return (bo->flags == XOCL_BO_CMA);
}
static inline bool xocl_bo_p2p(const struct drm_xocl_bo *bo)
{
	return (bo->flags == XOCL_BO_P2P);
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
        return user_flags & 0xFFFF;
}

static inline unsigned xocl_bo_type(unsigned user_flags)
{
	unsigned type = user_flags & 0xFFFF0000;
	unsigned bo_type = 0;

	switch (type) {
	case XCL_BO_FLAGS_EXECBUF:
		bo_type = XOCL_BO_EXECBUF;
		break;
#ifdef XOCL_CMA_ALLOC
	case XCL_BO_FLAGS_CMA:
		bo_type = XOCL_BO_CMA;
		break;
#endif
	case XCL_BO_FLAGS_P2P:
		bo_type = XOCL_BO_P2P;
		break;
	case XCL_BO_FLAGS_DEV_ONLY:
		bo_type = XOCL_BO_DEV_ONLY;
		break;
	case XCL_BO_FLAGS_CACHEABLE:
		bo_type = XOCL_BO_NORMAL;
		break;
	default:
		bo_type = XOCL_BO_NORMAL;
		break;
	}

	return bo_type;
}

static inline bool xocl_bo_sync_able(unsigned flags)
{
	return (flags & XOCL_DEVICE_MEM) && (flags & XOCL_HOST_MEM);
}

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

struct sg_table *xocl_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *xocl_gem_prime_import_sg_table(struct drm_device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt);
void *xocl_gem_prime_vmap(struct drm_gem_object *obj);
void xocl_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
int xocl_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
int xocl_copy_import_bo(struct drm_device *dev, struct drm_file *filp,
	struct ert_start_copybo_cmd *cmd);

#endif
