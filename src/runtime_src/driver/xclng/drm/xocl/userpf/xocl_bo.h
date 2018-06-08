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

#include "xocl_ioctl.h"

#define XOCL_BO_USERPTR (1 << 31)
#define XOCL_BO_IMPORT  (1 << 30)
#define XOCL_BO_EXECBUF (1 << 29)
#define XOCL_BO_CMA     (1 << 28)
#define XOCL_BO_P2P     (1 << 27)

#define XOCL_BO_DDR0 (1 << 0)
#define XOCL_BO_DDR1 (1 << 1)
#define XOCL_BO_DDR2 (1 << 2)
#define XOCL_BO_DDR3 (1 << 3)



#define XOCL_MEM_BANK_MSK (0xFFFFFF)
/*
 * When the BO is imported from an ARE device. This is remote BO to
 * be accessed over ARE
 */
#define XOCL_BO_ARE  (1 << 26)

/**
 * struct drm_xocl_exec_metadata - Meta data for exec bo
 *
 * @state: State of exec buffer object
 * @active: Reverse mapping to kds command object managed exclusively by kds
 */
struct drm_xocl_exec_metadata {
	enum drm_xocl_execbuf_state state;
	struct xocl_cmd            *active;
};

struct drm_xocl_bo {
	/* drm base object */
	struct drm_gem_object base;
	struct drm_mm_node   *mm_node;
	struct drm_xocl_exec_metadata metadata;
	struct page         **pages;
	struct sg_table      *sgt;
	void                 *vmapping;
	void                 *bar_vmapping;
	unsigned              flags;
};

struct drm_xocl_unmgd {
	struct page         **pages;
	struct sg_table      *sgt;
	unsigned int          npages;
	unsigned              flags;
};

static inline bool xocl_bo_userptr(const struct drm_xocl_bo *bo)
{
	return (bo->flags & XOCL_BO_USERPTR);
}

static inline bool xocl_bo_import(const struct drm_xocl_bo *bo)
{
	return (bo->flags & XOCL_BO_IMPORT);
}

static inline bool xocl_bo_execbuf(const struct drm_xocl_bo *bo)
{
	return (bo->flags & XOCL_BO_EXECBUF);
}

static inline bool xocl_bo_cma(const struct drm_xocl_bo *bo)
{
	return (bo->flags & XOCL_BO_CMA);
}
static inline bool xocl_bo_p2p(const struct drm_xocl_bo *bo)
{
	return (bo->flags & XOCL_BO_P2P);
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

static inline struct drm_xocl_bo *to_xocl_bo(struct drm_gem_object *bo)
{
	return (struct drm_xocl_bo *)bo;
}

static inline struct drm_xocl_dev *bo_xocl_dev(const struct drm_xocl_bo *bo)
{
	return bo->base.dev->dev_private;
}

static inline unsigned xocl_bo_ddr_idx(unsigned flags)
{
	const unsigned ddr = flags & XOCL_MEM_BANK_MSK;
	if (!ddr)
		return 0xffffffff;
	return __builtin_ctz(ddr);
}

int xocl_create_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_userptr_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_sync_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_copy_bo_ioctl(struct drm_device *dev, void *data,
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

void xocl_free_bo(struct drm_gem_object *obj);
struct sg_table *xocl_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *xocl_gem_prime_import_sg_table(struct drm_device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt);
void *xocl_gem_prime_vmap(struct drm_gem_object *obj);
void xocl_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);

#endif
