/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
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

#ifndef _ZOCL_DRV_H_
#define _ZOCL_DRV_H_
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <drm/drm_gem_cma_helper.h>
#include <linux/version.h>
#include "zocl_ioctl.h"
#include "zocl_ert.h"
#include "zocl_util.h"
#include "zocl_bo.h"
#include "zocl_dma.h"

#if defined(CONFIG_ARM64)
#define ZOCL_PLATFORM_ARM64   1
#else
#define ZOCL_PLATFORM_ARM64   0
#endif

/* Ensure compatibility with newer kernels and backported Red Hat kernels. */
/* The y2k38 bug fix was introduced with Kernel 3.17 and backported to Red Hat
 * 7.2. 
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
	#define ZOCL_TIMESPEC struct timespec64
	#define ZOCL_GETTIME ktime_get_real_ts64
	#define ZOCL_USEC tv_nsec / NSEC_PER_USEC
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,2)
		#define ZOCL_TIMESPEC struct timespec64
		#define ZOCL_GETTIME ktime_get_real_ts64
		#define ZOCL_USEC tv_nsec / NSEC_PER_USEC
	#else
		#define ZOCL_TIMESPEC struct timeval
		#define ZOCL_GETTIME do_gettimeofday
		#define ZOCL_USEC tv_usec
	#endif
#else
	#define ZOCL_TIMESPEC struct timeval
	#define ZOCL_GETTIME do_gettimeofday
	#define ZOCL_USEC tv_usec
#endif

/* drm_gem_object_put_unlocked was introduced with Kernel 4.12 and backported to
 * Red Hat 7.5
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
	#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put_unlocked
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,5)
		#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put_unlocked
	#else
		#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_unreference_unlocked
	#endif
#else
	#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_unreference_unlocked
#endif

/* drm_dev_put was introduced with Kernel 4.15 and backported to Red Hat 7.6. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
	#define ZOCL_DRM_DEV_PUT drm_dev_put
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
		#define ZOCL_DRM_DEV_PUT drm_dev_put	
	#else
		#define ZOCL_DRM_DEV_PUT drm_dev_unref
	#endif
#else
	#define ZOCL_DRM_DEV_PUT drm_dev_unref
#endif

/* access_ok lost its first parameter with Linux 5.0. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	#define ZOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(ADDR, SIZE)
#else
	#define ZOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(TYPE, ADDR, SIZE)
#endif

struct drm_zocl_exec_metadata {
	enum drm_zocl_execbuf_state state;
	unsigned int                index;
};

struct drm_zocl_bo {
	union {
		struct drm_gem_cma_object       cma_base;
		struct {
			struct drm_gem_object         gem_base;
			struct page                 **pages;
			struct sg_table              *sgt;
			void                         *vmapping;
			uint64_t                      uaddr;
		};
	};
	struct drm_mm_node            *mm_node;
	struct drm_zocl_exec_metadata  metadata;
	unsigned int                   bank;
	uint32_t                       flags;
};

struct drm_zocl_copy_bo {
	uint32_t dst_handle;
	uint32_t src_handle;
	uint64_t size;
	uint64_t dst_offset;
	uint64_t src_offset;
};

static inline struct drm_gem_object *
zocl_gem_object_lookup(struct drm_device *dev,
		struct drm_file   *filp,
		u32                handle)
{
	return drm_gem_object_lookup(filp, handle);
}

static inline struct
drm_zocl_bo *to_zocl_bo(struct drm_gem_object *bo)
{
	return (struct drm_zocl_bo *) bo;
}

static inline bool
zocl_bo_userptr(const struct drm_zocl_bo *bo)
{
	return (bo->flags & ZOCL_BO_FLAGS_USERPTR);
}

static inline bool
zocl_bo_execbuf(const struct drm_zocl_bo *bo)
{
	return (bo->flags & ZOCL_BO_FLAGS_EXECBUF);
}


int zocl_create_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_userptr_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_get_hbo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sync_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_map_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_info_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_pwrite_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_pread_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_execbuf_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_read_axlf_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sk_getcmd_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sk_create_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_sk_report_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_info_cu_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp);
int zocl_copy_bo_async(struct drm_device *, struct drm_file *,
	zocl_dma_handle_t *, struct drm_zocl_copy_bo *);

void zocl_describe(const struct drm_zocl_bo *obj);

void zocl_free_userptr_bo(struct drm_gem_object *obj);
void zocl_free_host_bo(struct drm_gem_object *obj);
int zocl_iommu_map_bo(struct drm_device *dev, struct drm_zocl_bo *bo);
int zocl_iommu_unmap_bo(struct drm_device *dev, struct drm_zocl_bo *bo);
int zocl_load_pdi(struct drm_device *ddev, void *data);

int zocl_init_sysfs(struct device *dev);
void zocl_fini_sysfs(struct device *dev);
void zocl_free_sections(struct drm_zocl_dev *zdev);
void zocl_free_bo(struct drm_gem_object *obj);
void zocl_update_mem_stat(struct drm_zocl_dev *zdev, u64 size,
		int count, uint32_t bank);
void zocl_init_mem(struct drm_zocl_dev *zdev, struct mem_topology *mtopo);
void zocl_clear_mem(struct drm_zocl_dev *zdev);

int get_apt_index(struct drm_zocl_dev *zdev, phys_addr_t addr);

#endif
