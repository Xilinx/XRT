/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_DRV_H_
#define _ZOCL_DRV_H_
#include <drm/drm.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#include <drm/drm_gem_dma_helper.h>
#else
#include <drm/drm_gem_cma_helper.h>
#endif
#include <linux/poll.h>
#include "zocl_util.h"
#include "zocl_ioctl.h"
#include "zocl_ert.h"
#include "zocl_bo.h"
#include "zocl_dma.h"
#include "zocl_ospi_versal.h"
#include "xrt_cu.h"

#if defined(CONFIG_ARM64)
#define ZOCL_PLATFORM_ARM64   1
#else
#define ZOCL_PLATFORM_ARM64   0
#endif

#ifndef XRT_DRIVER_VERSION
#define XRT_DRIVER_VERSION ""
#endif

#ifndef XRT_HASH
#define XRT_HASH ""
#endif

#ifndef XRT_HASH_DATE
#define XRT_HASH_DATE ""
#endif

/* Ensure compatibility with newer kernels and backported Red Hat kernels. */
/* The y2k38 bug fix was introduced with Kernel 3.17 and backported to Red Hat
 * 7.2.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	#define ZOCL_TIMESPEC struct timespec64
	#define ZOCL_GETTIME ktime_get_real_ts64
	#define ZOCL_USEC tv_nsec / NSEC_PER_USEC
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 2)
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put_unlocked
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5)
		#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED \
			drm_gem_object_put_unlocked
	#else
		#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED \
			drm_gem_object_unreference_unlocked
	#endif
#else
	#define ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED \
		drm_gem_object_unreference_unlocked
#endif

/* drm_dev_put was introduced with Kernel 4.15 and backported to Red Hat 7.6. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	#define ZOCL_DRM_DEV_PUT drm_dev_put
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6)
		#define ZOCL_DRM_DEV_PUT drm_dev_put
	#else
		#define ZOCL_DRM_DEV_PUT drm_dev_unref
	#endif
#else
	#define ZOCL_DRM_DEV_PUT drm_dev_unref
#endif

/* access_ok lost its first parameter with Linux 5.0. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	#define ZOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(ADDR, SIZE)
#else
	#define ZOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(TYPE, ADDR, SIZE)
#endif

struct drm_zocl_exec_metadata {
	enum drm_zocl_execbuf_state state;
	unsigned int                index;
};

struct zocl_drv_private {
	void		       *ops;
};

struct drm_zocl_bo {
	union {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		struct drm_gem_dma_object       cma_base;
#else
		struct drm_gem_cma_object       cma_base;
#endif
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
	unsigned int                   mem_index;
	uint32_t                       flags;
	unsigned int                   user_flags;
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

static inline struct kernel_info *
zocl_query_kernel(struct drm_zocl_slot *slot, const char *name)
{
	struct kernel_info *kernel;
	int off = 0;

	while (off < slot->ksize) {
		kernel = (struct kernel_info *)(slot->kernels + off);
		if (!strcmp(kernel->name, name))
			break;
		off += sizeof(struct kernel_info);
		off += sizeof(struct argument_info) * kernel->anums;
	}

	if (off < slot->ksize)
		return kernel;

	return NULL;
}

static inline int
zocl_kds_add_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	return kds_add_cu(&zdev->kds, xcu);
}

static inline int
zocl_kds_add_scu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	BUG_ON(!zdev);
	return kds_add_scu(&zdev->kds, xcu);
}

static inline int
zocl_kds_del_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	return kds_del_cu(&zdev->kds, xcu);
}

static inline int
zocl_kds_del_scu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	return kds_del_scu(&zdev->kds, xcu);
}


static inline int
zocl_kds_set_cu_read_range(struct drm_zocl_dev *zdev, u32 cu_idx, u32 start, u32 size)
{
	return kds_set_cu_read_range(&zdev->kds, cu_idx, start, size);
}

int zocl_copy_bo_async(struct drm_device *dev, struct drm_file *fipl,
		zocl_dma_handle_t *handle, struct drm_zocl_copy_bo *bo);

bool zocl_can_dma_performed(struct drm_device *dev, struct drm_file *filp,
	struct drm_zocl_copy_bo *args, uint64_t *dst_paddr,
	uint64_t *src_paddr);
int zocl_dma_channel_instance(zocl_dma_handle_t *dma_handle,
			      struct drm_zocl_dev *zdev);

void zocl_describe(const struct drm_zocl_bo *obj);

void zocl_free_userptr_bo(struct drm_gem_object *obj);
void zocl_free_host_bo(struct drm_gem_object *obj);
int zocl_iommu_map_bo(struct drm_device *dev, struct drm_zocl_bo *bo);
int zocl_iommu_unmap_bo(struct drm_device *dev, struct drm_zocl_bo *bo);

int zocl_init_sysfs(struct device *dev);
void zocl_fini_sysfs(struct device *dev);
void zocl_free_sections(struct drm_zocl_dev *dev, struct drm_zocl_slot *slot);
void zocl_free_bo(struct drm_gem_object *obj);
void zocl_drm_free_bo(struct drm_zocl_bo *bo);
struct drm_gem_object *zocl_gem_create_object(struct drm_device *dev, size_t size);
struct drm_zocl_bo *zocl_drm_create_bo(struct drm_device *dev,
		uint64_t unaligned_size, unsigned user_flags);
void zocl_update_mem_stat(struct drm_zocl_dev *zdev, u64 size,
		int count, uint32_t bank);
void zocl_init_mem(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot);
void zocl_clear_mem(struct drm_zocl_dev *zdev);
void zocl_clear_mem_slot(struct drm_zocl_dev *zdev, u32 slot_idx);
int zocl_cleanup_aie(struct drm_zocl_slot *slot);
void zocl_destroy_aie(struct drm_zocl_slot *slot);
int zocl_create_aie(struct drm_zocl_slot *slot, struct axlf *axlf, char __user *xclbin,
		void *aie_res, uint8_t hw_gen, uint32_t partition_id);
int zocl_init_aie(struct drm_zocl_slot* slot);
int zocl_aie_request_part_fd(struct drm_zocl_dev *zdev, void *data,struct drm_file* filp );
int zocl_aie_reset(struct drm_zocl_dev *zdev, void* data, struct drm_file* file);
int zocl_aie_freqscale(struct drm_zocl_dev *zdev, void *data, struct drm_file* filp);
int zocl_aie_kds_add_graph_context(struct drm_zocl_dev *zdev, u32 gid,
	        u32 ctx_code, struct kds_client *client);
int zocl_aie_kds_del_graph_context(struct drm_zocl_dev *zdev, u32 gid,
	        struct kds_client *client);
void zocl_aie_kds_del_graph_context_all(struct kds_client *client);
int zocl_aie_kds_add_context(struct drm_zocl_dev *zdev, u32 ctx_code,
	struct kds_client *client);
int zocl_aie_kds_del_context(struct drm_zocl_dev *zdev,
	struct kds_client *client);
int zocl_add_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl, u32 cu_idx, u32 flags, u32 cu_domain);
int zocl_del_context_kernel(struct drm_zocl_dev *zdev, void *client_hdl, u32 cu_idx, u32 cu_domain);

int zocl_inject_error(struct drm_zocl_dev *zdev, void *data,
		struct drm_file *filp);
int zocl_init_error(struct drm_zocl_dev *zdev);
void zocl_fini_error(struct drm_zocl_dev *zdev);
int zocl_insert_error_record(struct drm_zocl_dev *zdev, xrtErrorCode err_code);
/* zocl_kds.c */
int zocl_init_sched(struct drm_zocl_dev *zdev);
void zocl_fini_sched(struct drm_zocl_dev *zdev);
int zocl_create_client(struct device *dev, void **client_hdl);
void zocl_destroy_client(void *client_hdl);
uint zocl_poll_client(struct file *filp, poll_table *wait);
int zocl_command_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp);
int zocl_context_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp);
struct platform_device *zocl_find_pdev(char *name);

static inline struct drm_zocl_dev *
zocl_get_zdev(void)
{
	struct platform_device *pdev = zocl_find_pdev("zyxclmm_drm");
	if(!pdev)
		return NULL;
	return platform_get_drvdata(pdev);
}
int get_apt_index_by_addr(struct drm_zocl_dev *zdev, phys_addr_t addr);
int get_apt_index_by_cu_idx(struct drm_zocl_dev *zdev, int cu_idx);
void update_cu_idx_in_apt(struct drm_zocl_dev *zdev, int apt_idx, int cu_idx);

int zocl_kds_reset(struct drm_zocl_dev *zdev);

int subdev_create_cu(struct device *dev, struct xrt_cu_info *info, struct platform_device **pdevp);
void subdev_destroy_cu(struct drm_zocl_dev *zdev);
int subdev_create_scu(struct device *dev, struct xrt_cu_info *info, struct platform_device **pdevp);
void subdev_destroy_scu(struct drm_zocl_dev *zdev);

irqreturn_t cu_isr(int irq, void *arg);
irqreturn_t ucu_isr(int irq, void *arg);
/* Sub device driver */
extern struct platform_driver zocl_cu_xgq_driver;
extern struct platform_driver zocl_csr_intc_driver;
extern struct platform_driver zocl_irq_intc_driver;
extern struct platform_driver zocl_rpu_channel_driver;
extern struct platform_driver cu_driver;
extern struct platform_driver scu_driver;
struct zocl_cu_ops {
	int (*submit)(struct platform_device *pdev, struct kds_command *xcmd);
};

static inline int
zocl_cu_submit_xcmd(struct drm_zocl_dev *zdev, int i, struct kds_command *xcmd)
{
	struct platform_device *pdev;
	struct zocl_drv_private *priv;
	struct zocl_cu_ops *ops;

	pdev = zdev->cu_subdev.cu_pldev[i];
	priv = (void *)platform_get_device_id(pdev)->driver_data;
	ops = (struct zocl_cu_ops *)priv->ops;
	return ops->submit(pdev, xcmd);
}

extern u32 zocl_cu_get_status(struct platform_device *pdev);
extern u32 zocl_scu_get_status(struct platform_device *pdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
extern const struct drm_gem_object_funcs zocl_gem_object_funcs;
extern const struct drm_gem_object_funcs zocl_cma_default_funcs;
#endif
#endif
