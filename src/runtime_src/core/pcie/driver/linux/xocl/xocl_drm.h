/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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

#ifndef _XOCL_DRM_H
#define	_XOCL_DRM_H

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#include <linux/hashtable.h>
#endif

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

struct xocl_cma_memory {
	uint64_t		paddr;
	struct page		**pages;
	void 			*vaddr;
	uint64_t		size;
	struct sg_table 	*sgt;
};

struct xocl_cma_bank {
	uint64_t		entry_sz;
	uint64_t		entry_num;
	struct xocl_cma_memory	cma_mem[1];
};

struct xocl_drm {
	xdev_handle_t		xdev;
	/* memory management */
	struct drm_device       *ddev;
	/* Memory manager array, one per DDR channel, protected by mm_lock */
	struct drm_mm           **mm;
	struct mutex            mm_lock;
	struct drm_xocl_mm_stat **mm_usage_stat;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	DECLARE_HASHTABLE(mm_range, 6);
#endif
	struct xocl_cma_bank  *cma_bank;
};

struct drm_xocl_bo {
	/* drm base object */
	struct drm_gem_object base;
	struct drm_mm_node    *mm_node;
	struct drm_xocl_exec_metadata metadata;
	struct page           **pages;
	struct sg_table       *sgt;
	void                  *vmapping;
	u64                   p2p_bar_offset;
	struct dma_buf        *dmabuf;
	const struct vm_operations_struct *dmabuf_vm_ops;
	unsigned              dma_nsg;
	unsigned              flags;
	unsigned              mem_idx;
};

struct drm_xocl_unmgd {
	struct page         **pages;
	struct sg_table      *sgt;
	unsigned int          npages;
	unsigned              flags;
};

struct drm_xocl_bo *xocl_drm_create_bo(struct xocl_drm *drm_p,
	uint64_t unaligned_size, unsigned user_flags);
void xocl_drm_free_bo(struct drm_gem_object *obj);


void xocl_mm_get_usage_stat(struct xocl_drm *drm_p, u32 ddr,
        struct drm_xocl_mm_stat *pstat);
void xocl_mm_update_usage_stat(struct xocl_drm *drm_p, u32 ddr,
        u64 size, int count);

int xocl_mm_insert_node(struct xocl_drm *drm_p, u32 ddr,
                struct drm_mm_node *node, u64 size);
void *xocl_drm_init(xdev_handle_t xdev);
void xocl_drm_fini(struct xocl_drm *drm_p);
uint32_t xocl_get_shared_ddr(struct xocl_drm *drm_p, struct mem_data *m_data);
int xocl_init_mem(struct xocl_drm *drm_p);
int xocl_cleanup_mem(struct xocl_drm *drm_p);

bool is_cma_bank(struct xocl_drm *drm_p, uint32_t memidx);
int xocl_cma_bank_alloc(struct xocl_drm *drm_p, struct drm_xocl_alloc_cma_info *cma_info);
void xocl_cma_bank_free(struct xocl_drm *drm_p);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
vm_fault_t xocl_gem_fault(struct vm_fault *vmf);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
int xocl_gem_fault(struct vm_fault *vmf);
#else
int xocl_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
#endif

static inline struct drm_xocl_bo *to_xocl_bo(struct drm_gem_object *bo)
{
	return (struct drm_xocl_bo *)bo;
}

int xocl_init_unmgd(struct drm_xocl_unmgd *unmgd, uint64_t data_ptr,
		        uint64_t size, u32 write);
void xocl_finish_unmgd(struct drm_xocl_unmgd *unmgd);

#endif
