/*
 * A GEM style SVM device manager for ZynQ based OpenCL accelerators.
 *
 * Copyright (C) 2016 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
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

#ifndef _ZOCLSVM_DRV_H_
#define _ZOCLSVM_DRV_H_
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <linux/version.h>
#include "zocl_ioctl.h"

#define zocl_err(dev, fmt, args...)     \
  dev_err(dev, "%s: "fmt, __func__, ##args)

struct drm_zocl_exec_metadata {
  enum drm_zocl_execbuf_state state;
  unsigned int                index;
};

struct drm_zoclsvm_bo {
	struct drm_gem_object         base;
	struct page                 **pages;
	struct sg_table              *sgt;
	void                         *vmapping;
	uint64_t                      uaddr;
	unsigned int                  flags;
  struct drm_zocl_exec_metadata metadata;
};

struct drm_zoclsvm_dev {
	struct drm_device       *ddev;
	struct fpga_manager     *fpga_mgr;
	struct iommu_domain     *domain;
	void __iomem            *regs;
	phys_addr_t              res_start;
	resource_size_t          res_len;
	unsigned int             irq;
  struct sched_exec_core  *exec;
};

static inline struct drm_gem_object *
zoclsvm_gem_object_lookup(struct drm_device *dev,
							            struct drm_file   *filp,
							            u32                handle)
{
	return drm_gem_object_lookup(filp, handle);
}

static inline struct drm_zoclsvm_bo *to_zoclsvm_bo(struct drm_gem_object *bo)
{
	return (struct drm_zoclsvm_bo *)bo;
}

static inline bool zoclsvm_bo_userptr(const struct drm_zoclsvm_bo *bo)
{
    return (bo->flags & DRM_ZOCL_BO_FLAGS_USERPTR);
}

static inline bool
zoclsvm_bo_execbuf(const struct drm_zoclsvm_bo *bo)
{
  return (bo->flags & DRM_ZOCL_BO_FLAGS_EXECBUF);
}

int zoclsvm_create_bo_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *filp);
int zoclsvm_userptr_bo_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *filp);
int zoclsvm_sync_bo_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *filp);
int zoclsvm_map_bo_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp);
int zoclsvm_info_bo_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *filp);
int zoclsvm_pwrite_bo_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *filp);
int zoclsvm_pread_bo_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp);
int zoclsvm_read_axlf_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp);
int zoclsvm_execbuf_ioctl(struct drm_device *dev, void *data,
                          struct drm_file *filp);
void zoclsvm_describe(const struct drm_zoclsvm_bo *obj);
int zoclsvm_pcap_download_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp);
void zoclsvm_free_bo(struct drm_gem_object *obj);
int zoclsvm_iommu_map_bo(struct drm_device *dev, struct drm_zoclsvm_bo *bo);
int zoclsvm_iommu_unmap_bo(struct drm_device *dev, struct drm_zoclsvm_bo *bo);
#endif
