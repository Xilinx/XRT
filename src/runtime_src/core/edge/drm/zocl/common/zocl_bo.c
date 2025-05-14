/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/pagemap.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0))
	#include <linux/iosys-map.h>
	#define ZOCL_MAP_TYPE iosys_map
	#define ZOCL_MAP_IS_NULL iosys_map_is_null
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0))
	#include <linux/dma-buf-map.h>
	#define ZOCL_MAP_TYPE dma_buf_map
	#define ZOCL_MAP_IS_NULL dma_buf_map_is_null
#endif
#include <asm/io.h>
#include "xrt_drv.h"
#include "zocl_drv.h"
#include "xclbin.h"
#include "zocl_bo.h"

static inline void __user *to_user_ptr(u64 address)
{
	return (void __user *)(uintptr_t)address;
}

void zocl_describe(const struct drm_zocl_bo *obj)
{
	size_t size_in_kb = obj->cma_base.base.size / 1024;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	size_t physical_addr = obj->cma_base.dma_addr;
#else
	size_t physical_addr = obj->cma_base.paddr;
#endif

	DRM_DEBUG("%px: H[0x%zxKB] D[0x%zx]\n",
			obj,
			size_in_kb,
			physical_addr);
}

static inline void
zocl_bo_describe(const struct drm_zocl_bo *bo, uint64_t *size, uint64_t *paddr)
{
	if (!bo->mm_node) {
		*size = (uint64_t)bo->cma_base.base.size;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		*paddr = (uint64_t)bo->cma_base.dma_addr;
#else
		*paddr = (uint64_t)bo->cma_base.paddr;
#endif
	} else {
		*size = (uint64_t)bo->gem_base.size;
		*paddr = (uint64_t)bo->mm_node->start;
	}
}

int zocl_iommu_map_bo(struct drm_device *dev, struct drm_zocl_bo *bo)
{
	int prot = IOMMU_READ | IOMMU_WRITE;
	struct drm_zocl_dev *zdev = dev->dev_private;
	size_t bo_size = bo->gem_base.size;
	ssize_t err;

	/* Create scatter gather list from user's pages */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 9, 0)
	bo->sgt = drm_prime_pages_to_sg(bo->pages, bo_size >> PAGE_SHIFT);
#else
	bo->sgt = drm_prime_pages_to_sg(dev, bo->pages, bo_size >> PAGE_SHIFT);
#endif
	if (IS_ERR(bo->sgt)) {
		bo->uaddr = 0;
		return PTR_ERR(bo->sgt);
	}

	/* MAP user's VA to pages table into IOMMU */
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
                err = iommu_map_sg(zdev->domain, bo->uaddr, bo->sgt->sgl,
                        bo->sgt->nents, prot, GFP_KERNEL);
        #else
                err = iommu_map_sg(zdev->domain, bo->uaddr, bo->sgt->sgl,
                        bo->sgt->nents, prot);
        #endif
	if (err < 0) {
		/* If IOMMU map failed forget user's VA */
		bo->uaddr = 0;
		DRM_ERROR("Failed to map buffer through IOMMU: %zd\n", err);
		return err;
	}
	return 0;
}

int zocl_iommu_unmap_bo(struct drm_device *dev, struct drm_zocl_bo *bo)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	/* If IOMMU map had failed before bo->uaddr will be zero */
	if (bo->uaddr)
		iommu_unmap(zdev->domain, bo->uaddr, bo->gem_base.size);
	return 0;
}

static struct drm_zocl_bo *zocl_create_userprt_bo(struct drm_device *dev,
		uint64_t unaligned_size)
{
	size_t size = PAGE_ALIGN(unaligned_size);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	struct drm_gem_dma_object *cma_obj;
#else
	struct drm_gem_cma_object *cma_obj;
#endif
	int err = 0;

	if (!size)
		return ERR_PTR(-EINVAL);

	cma_obj = kzalloc(sizeof(*cma_obj), GFP_KERNEL);
	if (!cma_obj) {
		DRM_DEBUG("cma object create failed\n");
		return ERR_PTR(-ENOMEM);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	cma_obj->base.funcs = &zocl_gem_object_funcs;
#endif
	err = drm_gem_object_init(dev, &cma_obj->base, size);
	if (err) {
		DRM_DEBUG("drm gem object initial failed\n");
		goto out1;
	}

	cma_obj->sgt   = NULL;
	cma_obj->vaddr = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cma_obj->dma_addr = 0x0;
#else
	cma_obj->paddr = 0x0;
#endif

	return to_zocl_bo(&cma_obj->base);
out1:
	kfree(cma_obj);
	return NULL;
}

void zocl_free_userptr_bo(struct drm_gem_object *gem_obj)
{
	/* Do all drm_gem_cma_free_object(bo->base) do, execpt free vaddr */
	struct drm_zocl_bo *zocl_bo = to_zocl_bo(gem_obj);

	DRM_DEBUG("%s: obj 0x%px", __func__, zocl_bo);
	if (zocl_bo->cma_base.sgt)
		sg_free_table(zocl_bo->cma_base.sgt);

	drm_gem_object_release(gem_obj);

	kfree(&zocl_bo->cma_base);
}

static struct drm_zocl_bo *
zocl_create_cma_mem(struct drm_device *dev, size_t size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	struct drm_gem_dma_object *cma_obj;
#else
	struct drm_gem_cma_object *cma_obj;
#endif
	struct drm_zocl_bo *bo;

	/* Allocate from CMA buffer */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cma_obj = drm_gem_dma_create(dev, size);
#else
	cma_obj = drm_gem_cma_create(dev, size);
#endif
	if (IS_ERR(cma_obj))
		return ERR_PTR(-ENOMEM);

	bo = to_zocl_bo(&cma_obj->base);

	return bo;
}

/*
 * This function allocates memory from the range allocator.
 * Also try to allocate memory from the similer memory types.
 *
 * @param       zdev:   zocl device structure
 * @param       size:	requested memory size
 * @param       mem:	requested zocl memory structure
 *
 * @return	bo pointer on success, error code on failure
 */
static struct drm_zocl_bo *
zocl_create_range_mem(struct drm_device *dev, size_t size, struct zocl_mem *mem)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct drm_zocl_bo *bo = NULL;
	struct zocl_mem *head_mem = mem;
	int err = -ENOMEM;

	mutex_lock(&zdev->mm_lock);
	do {
		if (mem->zm_type == ZOCL_MEM_TYPE_CMA) {
			struct drm_zocl_bo *cma_bo =
				zocl_create_cma_mem(dev, size);
			if (!IS_ERR(cma_bo)) {
				/* Get the memory from CMA memory region */
				cma_bo->flags |= ZOCL_BO_FLAGS_CMA;
				return cma_bo;
			}
			DRM_WARN("Memory allocated from CMA region"
				" whereas requested for reserved memory region\n");
		}
		else {
                        bo = kzalloc(sizeof(struct drm_zocl_bo), GFP_KERNEL);
                        if (IS_ERR(bo))
                                return ERR_PTR(-ENOMEM);

                        #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
                                bo->gem_base.funcs = &zocl_gem_object_funcs;
                        #endif

                        err = drm_gem_object_init(dev, &bo->gem_base, size);
                        if (err) {
                                kfree(bo);
                                return ERR_PTR(err);
                        }

                        bo->mm_node = kzalloc(sizeof(struct drm_mm_node),GFP_KERNEL);

	                if (IS_ERR(bo->mm_node)) {
		                drm_gem_object_release(&bo->gem_base);
		                kfree(bo);
		                return ERR_PTR(-ENOMEM);
	                }

                        err = drm_mm_insert_node_in_range(zdev->zm_drm_mm,
				bo->mm_node, size, PAGE_SIZE, 0,
				mem->zm_base_addr,
				mem->zm_base_addr + mem->zm_size, 0);
			if (!err) {
				/* Got memory from this Range memory manager */
				break;
			}
		}

		/* No memory left to this memory manager.
		 * Try to allocate from similer memory manger link list
		 */
		mem = list_entry(mem->zm_list.next, typeof(*mem), zm_list);

	} while (&mem->zm_list != &head_mem->zm_list);

	if (err) {
		DRM_ERROR("Fail to allocate BO: size %ld\n",
				(long)size);
		mutex_unlock(&zdev->mm_lock);
		kfree(bo->mm_node);
		drm_gem_object_release(&bo->gem_base);
		kfree(bo);
		return ERR_PTR(-ENOMEM);
	}

	mutex_unlock(&zdev->mm_lock);

	/*
	 * Set up a kernel mapping for direct BO access.
	 * We don't have to fail BO allocation if we can
	 * not establish the kernel mapping. We just can not
	 * access BO directly from kernel.
	 */
	bo->vmapping = memremap(bo->mm_node->start, size, MEMREMAP_WC);

	err = drm_gem_create_mmap_offset(&bo->gem_base);
	if (err) {
		DRM_ERROR("Fail to create BO mmap offset.\n");
		zocl_free_bo(&bo->gem_base);
		return ERR_PTR(err);
	}

	return bo;
}

/*
 * This function returns zocl memory for the given memory index.
 * Memory index is a pair of slot id and bank id.
 *
 * @param       zdev:    	zocl device structure
 * @param       mem_index: 	memory index
 *
 * @return	memory pointer on success, NULL on failure
 */
static struct zocl_mem *
zocl_get_mem_by_mem_index(struct drm_zocl_dev *zdev, u32 mem_index)
{
	struct zocl_mem *curr_mem = NULL;
	list_for_each_entry(curr_mem, &zdev->zm_list_head, link)
		if (curr_mem->zm_mem_idx == mem_index)
			return curr_mem;

	return NULL;
}

/* This function returns zocl memory for the given slot based on a
 * specific memory topology
 *
 * @param       zdev:		zocl device structure
 * @param       md:		memory topology structure
 * @param       slot_idx:	slot index
 *
 * @return	memory pointer on success, NULL on failure
 */
static struct zocl_mem *
zocl_get_memp_by_mem_data(struct drm_zocl_dev *zdev,
		     struct mem_data *md, u32 slot_idx)
{
	struct zocl_mem *memp = NULL;

	/* Create a link list for similar memory manager for this slot */
	list_for_each_entry(memp, &zdev->zm_list_head, link) {
		if (GET_SLOT_INDEX(memp->zm_mem_idx) != slot_idx)
		    continue;

		if ((memp->zm_base_addr == md->m_base_address) &&
		    (memp->zm_size == md->m_size * 1024))
			return memp;
	}

	return NULL;
}

static struct drm_zocl_bo *
zocl_create_bo(struct drm_device *dev, uint64_t unaligned_size, u32 user_flags)
{
	size_t size = PAGE_ALIGN(unaligned_size);
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct drm_zocl_bo *bo = NULL;
	int err = 0;

	if (!size)
		return ERR_PTR(-EINVAL);

	if (zdev->domain) {
		bo = kzalloc(sizeof(*bo), GFP_KERNEL);
		if (!bo)
			return ERR_PTR(-ENOMEM);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
		bo->gem_base.funcs = &zocl_gem_object_funcs;
#endif
		err = drm_gem_object_init(dev, &bo->gem_base, size);
		if (err < 0)
			goto free;
	} else if (user_flags & ZOCL_BO_FLAGS_CMA) {
		bo = zocl_create_cma_mem(dev, size);
	} else {
		/* We are allocating from a separate mem Index, i.e. PL-DDR or LPDDR */
		unsigned int mem_index = GET_MEM_INDEX(user_flags);
		struct zocl_mem *mem = zocl_get_mem_by_mem_index(zdev, mem_index);
		if (mem == NULL)
			return ERR_PTR(-ENOMEM);

		if (!mem->zm_used || mem->zm_type != ZOCL_MEM_TYPE_RANGE_ALLOC)
			return ERR_PTR(-EINVAL);

		bo = zocl_create_range_mem(dev, size, mem);
	}

	if (user_flags & ZOCL_BO_FLAGS_EXECBUF) {
		bo->flags = ZOCL_BO_FLAGS_EXECBUF;
		bo->metadata.state = DRM_ZOCL_EXECBUF_STATE_ABORT;
	}

	return bo;
free:
	kfree(bo);
	return ERR_PTR(err);
}
/*
 * Allocate a sg_table for this GEM object.
 * Note: Both the table's contents, and the sg_table itself must be freed by
 *       the caller.
 * Returns a pointer to the newly allocated sg_table, or an ERR_PTR() error.
 */
struct sg_table *zocl_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
        struct drm_zocl_bo *zocl_obj = to_zocl_bo(obj);
	if (zocl_obj && !(zocl_obj->mm_node)) {
		return drm_gem_dma_get_sg_table(&zocl_obj->cma_base);
	}
        struct drm_device *drm = obj->dev;
        struct sg_table *sgt;
        int ret;
	unsigned long dma_attrs = DMA_ATTR_WRITE_COMBINE;

        sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
        if (!sgt)
                return ERR_PTR(-ENOMEM);

        ret = dma_get_sgtable_attrs(drm->dev, sgt, zocl_obj->vmapping,
                                    zocl_obj->mm_node->start, obj->size,
                                    dma_attrs);
        if (ret) {
                DRM_ERROR("failed to allocate sgt, %d\n", ret);
                kfree(sgt);
                return ERR_PTR(ret);
        }

        return sgt;
}

static struct drm_zocl_bo *
zocl_create_svm_bo(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_create_bo *args = data;
	struct drm_zocl_bo *bo = NULL;
	size_t bo_size = 0;
	int ret = 0;

	if ((args->flags & ZOCL_BO_FLAGS_COHERENT) ||
			(args->flags & ZOCL_BO_FLAGS_CMA))
		return ERR_PTR(-EINVAL);

	args->flags |= ZOCL_BO_FLAGS_SVM;
	if (!(args->flags & ZOCL_BO_FLAGS_SVM))
		return ERR_PTR(-EINVAL);

	bo = zocl_create_bo(dev, args->size, args->flags);
	bo->flags |= ZOCL_BO_FLAGS_SVM;
	bo->mem_index = GET_MEM_INDEX(args->flags);

	if (IS_ERR(bo)) {
		DRM_DEBUG("object creation failed\n");
		return bo;
	}
	bo->pages = drm_gem_get_pages(&bo->gem_base);
	if (IS_ERR(bo->pages)) {
		ret = PTR_ERR(bo->pages);
		goto out_free;
	}

	bo_size = bo->gem_base.size;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 9, 0)
	bo->sgt = drm_prime_pages_to_sg(bo->pages, bo_size >> PAGE_SHIFT);
#else
	bo->sgt = drm_prime_pages_to_sg(dev, bo->pages, bo_size >> PAGE_SHIFT);
#endif
	if (IS_ERR(bo->sgt))
		goto out_free;

	bo->vmapping = vmap(bo->pages, bo->gem_base.size >> PAGE_SHIFT, VM_MAP,
			pgprot_writecombine(PAGE_KERNEL));

	if (!bo->vmapping) {
		ret = -ENOMEM;
		goto out_free;
	}

	ret = drm_gem_create_mmap_offset(&bo->gem_base);
	if (ret < 0)
		goto out_free;

	ret = drm_gem_handle_create(filp, &bo->gem_base, &args->handle);
	if (ret < 0)
		goto out_free;

	zocl_describe(bo);
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&bo->gem_base);

	/* Update memory usage statistics */
	zocl_update_mem_stat(dev->dev_private, args->size, 1, bo->mem_index);

	return bo;

out_free:
	zocl_free_bo(&bo->gem_base);
	return ERR_PTR(ret);
}

int
zocl_create_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret = 0;
	struct drm_zocl_create_bo *args = data;
	struct drm_zocl_bo *bo = NULL;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct zocl_mem *mem = NULL;
	unsigned int mem_index = 0;
	uint32_t user_flags = args->flags;

	args->flags = zocl_convert_bo_uflags(args->flags);

	if (zdev->domain) {
		bo = zocl_create_svm_bo(dev, data, filp);
		if (IS_ERR(bo))
			return PTR_ERR(bo);
		bo->user_flags = user_flags;
		return 0;
	}

	mem_index = GET_MEM_INDEX(args->flags);
	mem = zocl_get_mem_by_mem_index(zdev, mem_index);

	/* Always allocate EXECBUF from CMA */
	if (args->flags & ZOCL_BO_FLAGS_EXECBUF)
		args->flags |= ZOCL_BO_FLAGS_CMA;
	else {
		/*
		 * For specified valid DDR bank, we only mark CMA flags
		 * if the bank type is CMA, non-CMA type bank will use
		 * PL-DDR; For any other cases (invalid bank index), we
		 * allocate from CMA by default.
		 */
		if (mem && mem->zm_used) {
			if (mem->zm_type == ZOCL_MEM_TYPE_CMA)
				args->flags |= ZOCL_BO_FLAGS_CMA;
		} else {
			DRM_WARN("Allocating BO from CMA for invalid or unused memory index[%d]\n",
					mem_index);
			args->flags |= ZOCL_BO_FLAGS_CMA;
		}
	}

	if (!(args->flags & ZOCL_BO_FLAGS_CACHEABLE)) {
		/* If cacheable is not set, make sure we set COHERENT. */
		args->flags |= ZOCL_BO_FLAGS_COHERENT;
	} else if (!(args->flags & ZOCL_BO_FLAGS_CMA)) {
		/*
		 * We do not support allocating cacheable BO from PL-DDR or
		 * LPDDR
		 */
		DRM_WARN("Cache is not supported and turned off for PL-DDR or LPDDR\n");
		args->flags &= ~ZOCL_BO_FLAGS_CACHEABLE;
	}

	bo = zocl_create_bo(dev, args->size, args->flags);
	if (IS_ERR(bo)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(bo);
	}

	bo->mem_index = mem_index;
	if (args->flags & ZOCL_BO_FLAGS_CACHEABLE)
		bo->flags |= ZOCL_BO_FLAGS_CACHEABLE;
	else
		bo->flags |= ZOCL_BO_FLAGS_COHERENT;

	if (args->flags & ZOCL_BO_FLAGS_CMA) {
		bo->flags |= ZOCL_BO_FLAGS_CMA;
		ret = drm_gem_handle_create(filp, &bo->cma_base.base,
		    &args->handle);
		if (ret) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
			drm_gem_dma_object_free(&bo->cma_base.base);
#else
			drm_gem_cma_free_object(&bo->cma_base.base);
#endif
			DRM_DEBUG("handle creation failed\n");
			return ret;
		}
	} else {
		ret = drm_gem_handle_create(filp, &bo->gem_base,
		    &args->handle);
		if (ret) {
			zocl_free_bo(&bo->gem_base);
			DRM_DEBUG("handle create failed\n");
			return ret;
		}
	}

	bo->user_flags = user_flags;
	zocl_describe(bo);
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&bo->cma_base.base);

	/*
	 * Update memory usage statistics.
	 *
	 * Note: We can not use args->size here because it is
	 *       the required size while gem object records the
	 *       actual size allocated.
	 */
	zocl_update_mem_stat(zdev, bo->gem_base.size, 1, bo->mem_index);

	return ret;
}

int
zocl_userptr_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret = 0;
	struct drm_zocl_bo *bo = NULL;
	unsigned int page_count = 0;
	struct drm_zocl_userptr_bo *args = data;
	struct page **pages = NULL;
	unsigned int sg_count = 0;
	uint32_t user_flags = args->flags;

	if (offset_in_page(args->addr)) {
		DRM_ERROR("User ptr not PAGE aligned\n");
		return -EINVAL;
	}

	if (user_flags & ZOCL_BO_FLAGS_EXECBUF) {
		DRM_ERROR("Exec buf could not be a user buffer\n");
		return -EINVAL;
	}

	bo = zocl_create_userprt_bo(dev, args->size);
	if (IS_ERR(bo)) {
		DRM_ERROR("Object creation failed\n");
		return PTR_ERR(bo);
	}

	/* For accurately account for number of pages */
	page_count = bo->cma_base.base.size >> PAGE_SHIFT;

	pages = kvmalloc_array(page_count, sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		goto out1;
	}

	ret = get_user_pages_fast(args->addr, page_count, 1, pages);
	if (ret != page_count) {
		DRM_ERROR("Unable to get user pages\n");
		ret = -ENOMEM;
		goto out0;
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 9, 0)
	bo->cma_base.sgt = drm_prime_pages_to_sg(pages, page_count);
#else
	bo->cma_base.sgt = drm_prime_pages_to_sg(dev, pages, page_count);
#endif
	if (IS_ERR(bo->cma_base.sgt)) {
		ret = PTR_ERR(bo->cma_base.sgt);
		goto out0;
	}

	sg_count = dma_map_sg(dev->dev, bo->cma_base.sgt->sgl,
				bo->cma_base.sgt->nents, 0);
	if (sg_count <= 0) {
		DRM_ERROR("Map SG list failed\n");
		ret = -ENOMEM;
		goto out0;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	bo->cma_base.dma_addr = sg_dma_address((bo->cma_base.sgt)->sgl);
#else
	bo->cma_base.paddr = sg_dma_address((bo->cma_base.sgt)->sgl);
#endif

	/* Physical address must be continuous */
	if (sg_count != 1) {
		DRM_WARN("User buffer is not physical contiguous\n");
		ret = -EINVAL;
		goto out0;
	}

	bo->cma_base.vaddr = (void *)(uintptr_t)args->addr;

	ret = drm_gem_handle_create(filp, &bo->cma_base.base, &args->handle);
	if (ret) {
		ret = -EINVAL;
		DRM_ERROR("Handle creation failed\n");
		goto out0;
	}

	bo->flags |= ZOCL_BO_FLAGS_USERPTR;

	bo->user_flags = user_flags;
	zocl_describe(bo);
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&bo->cma_base.base);

	kvfree(pages);

	return ret;

out0:
	kvfree(pages);
out1:
	zocl_free_userptr_bo(&bo->cma_base.base);
	DRM_DEBUG("handle creation failed\n");
	return ret;
}

struct drm_zocl_bo *zocl_drm_create_bo(struct drm_device *dev,
					  uint64_t unaligned_size,
					  unsigned user_flags)
{
	return zocl_create_bo(dev, unaligned_size, user_flags);
}

int zocl_map_bo_ioctl(struct drm_device *dev,
		void *data,
		struct drm_file *filp)
{
	int ret = 0;
	struct drm_zocl_map_bo *args = data;
	struct drm_gem_object *gem_obj = NULL;

	gem_obj = zocl_gem_object_lookup(dev, filp, args->handle);
	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}

	if (zocl_bo_userptr(to_zocl_bo(gem_obj))) {
		ret = -EPERM;
		goto out;
	}

	/* The mmap offset was set up at BO allocation time. */
	args->offset = drm_vma_node_offset_addr(&gem_obj->vma_node);
	zocl_describe(to_zocl_bo(gem_obj));

out:
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
	return ret;
}

int zocl_sync_bo_ioctl(struct drm_device *dev,
		void *data,
		struct drm_file *filp)
{
	const struct drm_zocl_sync_bo	*args = data;
	struct drm_gem_object		*gem_obj;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	struct drm_gem_dma_object	*cma_obj;
#else
	struct drm_gem_cma_object	*cma_obj;
#endif
	struct drm_zocl_bo		*bo;
	dma_addr_t			bus_addr;
	int				rc = 0;

	gem_obj = zocl_gem_object_lookup(dev, filp, args->handle);
	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}

	if ((args->offset > gem_obj->size) || (args->size > gem_obj->size) ||
			((args->offset + args->size) > gem_obj->size)) {
		rc = -EINVAL;
		goto out;
	}

	bo = to_zocl_bo(gem_obj);
	if (bo->flags & ZOCL_BO_FLAGS_COHERENT && !(bo->flags & ZOCL_BO_FLAGS_CMA)) {
		/* The CMA buf is coherent, we don't need to do anything */
		rc = 0;
		goto out;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cma_obj = to_drm_gem_dma_obj(gem_obj);
	bus_addr = cma_obj->dma_addr;
#else
	cma_obj = to_drm_gem_cma_obj(gem_obj);
	bus_addr = cma_obj->paddr;
#endif

	/* only invalidate the range of addresses requested by the user */
	bus_addr += args->offset;

	/**
	 * NOTE: We a little bit abuse the dma_sync_single_* API here because
	 *       it is documented as for the DMA buffer mapped by dma_map_*
	 *       API. The buffer we are syncing here is mapped through
	 *       remap_pfn_range(). But so far this is our best choice
	 *       and it works.
	 */
	if (args->dir == DRM_ZOCL_SYNC_BO_TO_DEVICE) {
		dma_sync_single_for_device(dev->dev, bus_addr, args->size,
		    DMA_TO_DEVICE);
	} else if (args->dir == DRM_ZOCL_SYNC_BO_FROM_DEVICE) {
		dma_sync_single_for_cpu(dev->dev, bus_addr, args->size,
		    DMA_FROM_DEVICE);
	} else
		rc = -EINVAL;

out:
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return rc;
}

bool
zocl_can_dma_performed(struct drm_device *dev, struct drm_file *filp,
	struct drm_zocl_copy_bo *args, uint64_t *dst_paddr, uint64_t *src_paddr)
{
	struct drm_gem_object  *dst_gem_obj, *src_gem_obj;
	struct drm_zocl_bo     *dst_bo, *src_bo;
	uint64_t               dst_size, src_size;
	int                    unsupported_flags = 0;
	bool                   rc = true;

	dst_gem_obj = zocl_gem_object_lookup(dev, filp, args->dst_handle);
	if (!dst_gem_obj) {
		DRM_ERROR("Failed to look up GEM dst handle %d\n",
		    args->dst_handle);
		return false;
	}

	src_gem_obj = zocl_gem_object_lookup(dev, filp, args->src_handle);
	if (!src_gem_obj) {
		DRM_ERROR("Failed to look up GEM src handle %d\n",
		    args->src_handle);
		rc = false;
		goto out;
	}

	dst_bo = to_zocl_bo(dst_gem_obj);
	src_bo = to_zocl_bo(src_gem_obj);
	unsupported_flags = (ZOCL_BO_FLAGS_USERPTR | ZOCL_BO_FLAGS_HOST_BO |
		ZOCL_BO_FLAGS_SVM);
	if ((dst_bo->flags & unsupported_flags) ||
	    (src_bo->flags & unsupported_flags)) {
		DRM_ERROR("Failed: Not supported dst flags 0x%x and "
		    "src flags 0x%x\n", dst_bo->flags, src_bo->flags);
		rc = false;
		goto out;
	}

	zocl_bo_describe(dst_bo, &dst_size, dst_paddr);
	zocl_bo_describe(src_bo, &src_size, src_paddr);

	/*
	 * pre check before requesting DMA memory copy.
	 *    dst_offset + size <= dst_size
	 *    src_offset + size <= src_size`
	 */
	if (args->size == 0) {
		DRM_ERROR("Failed: request size cannot be ZERO!");
		rc = false;
		goto out;
	}
	if (args->dst_offset + args->size > dst_size) {
		DRM_ERROR("Failed: dst_offset + size out of boundary");
		rc = false;
		goto out;
	}
	if (args->src_offset + args->size > src_size) {
		DRM_ERROR("Failed: src_offset + size out of boundary");
		rc = false;
		goto out;
	}

out:
	if (dst_gem_obj)
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(dst_gem_obj);

	if (src_gem_obj)
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(src_gem_obj);

	return rc;
}

int zocl_dma_channel_instance(zocl_dma_handle_t *dma_handle,
			      struct drm_zocl_dev *zdev)
{
	dma_cap_mask_t dma_mask;

	if (!dma_handle->dma_chan && ZOCL_PLATFORM_ARM64) {
		/* If zdev_dma_chan is NULL, we haven't initialized it yet. */
		if (!zdev->zdev_dma_chan) {
			dma_cap_zero(dma_mask);
			dma_cap_set(DMA_MEMCPY, dma_mask);
			zdev->zdev_dma_chan =
			    dma_request_channel(dma_mask, 0, NULL);
			if (!zdev->zdev_dma_chan) {
				DRM_WARN("no DMA Channel available.\n");
				return -EBUSY;
			}
		}
		dma_handle->dma_chan = zdev->zdev_dma_chan;
	}

	return dma_handle->dma_chan ? 0 : -EINVAL;
}

int zocl_copy_bo_async(struct drm_device *dev,
		struct drm_file *filp,
		zocl_dma_handle_t *dma_handle,
		struct drm_zocl_copy_bo *args)
{
	uint64_t dst_paddr, src_paddr;
	int rc = 0;
	bool ret = false;

	if (dma_handle->dma_func == NULL) {
		DRM_ERROR("Failed: no callback dma_func for async dma");
		return -EINVAL;
	}

	ret = zocl_can_dma_performed(dev, filp, args, &dst_paddr, &src_paddr);
	if (!ret) {
		DRM_ERROR("Failed: Cannot perform DMA due to previous Errors");
		return -EINVAL;
	}

	dst_paddr += args->dst_offset;
	src_paddr += args->src_offset;

	rc = zocl_dma_memcpy_pre(dma_handle, (dma_addr_t)dst_paddr,
	    (dma_addr_t)src_paddr, (size_t)args->size);
	if (!rc)
		zocl_dma_start(dma_handle);

	return rc;
}

int zocl_info_bo_ioctl(struct drm_device *dev,
		void *data,
		struct drm_file *filp)
{
	const struct drm_zocl_bo *bo;
	struct drm_zocl_info_bo *args = data;
	struct drm_gem_object *gem_obj = zocl_gem_object_lookup(dev, filp,
			args->handle);

	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}

	bo = to_zocl_bo(gem_obj);
	zocl_bo_describe(bo, &args->size, &args->paddr);
	args->flags = bo->user_flags;

	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return 0;
}

static int zocl_bo_rdwr_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *filp, bool is_read)
{
	const struct drm_zocl_pwrite_bo *args = data;
	struct drm_gem_object *gem_obj = zocl_gem_object_lookup(dev, filp,
								args->handle);
	char __user *user_data = to_user_ptr(args->data_ptr);
	struct drm_zocl_bo *bo;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0))
	struct ZOCL_MAP_TYPE map;
#endif
	int ret = 0;
	char *kaddr;

	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}

	if ((args->offset > gem_obj->size) || (args->size > gem_obj->size)
	    || ((args->offset + args->size) > gem_obj->size)) {
		ret = -EINVAL;
		goto out;
	}

	if (args->size == 0) {
		ret = 0;
		goto out;
	}
	if (!ZOCL_ACCESS_OK(VERIFY_READ, user_data, args->size)) {
		ret = -EFAULT;
		goto out;
	}

	bo = to_zocl_bo(gem_obj);
	if (bo->flags & ZOCL_BO_FLAGS_CMA) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		ret = drm_gem_dma_object_vmap(gem_obj, &map);
#else
		ret = drm_gem_cma_vmap(gem_obj, &map);
#endif
		if(ret || ZOCL_MAP_IS_NULL(&map))
			kaddr = NULL;
		else
			kaddr = map.is_iomem ? map.vaddr_iomem : map.vaddr;
#else
		kaddr = drm_gem_cma_prime_vmap(gem_obj);
#endif
	}
	else
		kaddr = bo->vmapping;
	if (!kaddr) {
		DRM_ERROR("Fail to map BO %d\n", args->handle);
		ret = -EFAULT;
		goto out;
	}

	kaddr += args->offset;

	if (is_read)
		ret = copy_to_user(user_data, kaddr, args->size);
	else
		ret = copy_from_user(kaddr, user_data, args->size);

out:
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return ret;

}

int zocl_pwrite_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	return (zocl_bo_rdwr_ioctl(dev, data, filp, false));
}

int zocl_pread_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	return (zocl_bo_rdwr_ioctl(dev, data, filp, true));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static struct drm_gem_dma_object *
#else
static struct drm_gem_cma_object *
#endif
zocl_cma_create(struct drm_device *dev, size_t size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	struct drm_gem_dma_object *cma_obj;
#else
	struct drm_gem_cma_object *cma_obj;
#endif
	struct drm_gem_object *gem_obj;
	int ret;

	gem_obj = kzalloc(sizeof(struct drm_zocl_bo), GFP_KERNEL);
	if (!gem_obj) {
		DRM_ERROR("cma_create: alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cma_obj = container_of(gem_obj, struct drm_gem_dma_object, base);
#else
	cma_obj = container_of(gem_obj, struct drm_gem_cma_object, base);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	gem_obj->funcs = &zocl_cma_default_funcs;
#endif

	ret = drm_gem_object_init(dev, gem_obj, size);
	if (ret) {
		DRM_ERROR("cma_create: gem_obj_init failed\n");
		goto error;
	}

	ret = drm_gem_create_mmap_offset(gem_obj);
	if (ret) {
		DRM_ERROR("cma_create: gem_mmap_offset failed\n");
		drm_gem_object_release(gem_obj);
		goto error;
	}

	return cma_obj;

error:
	kfree(cma_obj);
	return ERR_PTR(ret);
}

int zocl_get_hbo_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *filp)
{
	struct drm_zocl_bo *bo;
	struct drm_zocl_host_bo *args = data;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	struct drm_gem_dma_object *cma_obj;
#else
	struct drm_gem_cma_object *cma_obj;
#endif
	struct drm_zocl_dev *zdev = dev->dev_private;
	u64 host_mem_start = zdev->host_mem;
	u64 host_mem_end = zdev->host_mem + zdev->host_mem_len;
	int ret;

	if (args->size == 0) {
		DRM_ERROR("get_hbo: Buffer size must be greater than zero\n");
		return -EINVAL;
	}
	if (!(host_mem_start <= args->paddr &&
	   args->paddr < host_mem_end &&
	   args->size <= host_mem_end - args->paddr)) {
		DRM_ERROR("get_hbo: Buffer at out side of reserved memory region\n");
		return -ENOMEM;
	}
	if (!PAGE_ALIGNED(args->paddr) || !PAGE_ALIGNED(args->size)) {
		/* DRM requirement */
		DRM_ERROR("get_hbo: Buffer paddr & size must be page aligned to page_size. paddr: 0x%llx, size: 0x%lx\n", args->paddr, args->size);
		return -EINVAL;
	}

	cma_obj = zocl_cma_create(dev, args->size);
	if (IS_ERR(cma_obj))
		return -ENOMEM;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	cma_obj->dma_addr = args->paddr;
#else
	cma_obj->paddr = args->paddr;
#endif
	cma_obj->vaddr = memremap(args->paddr, args->size, MEMREMAP_WB);
	if (!cma_obj->vaddr) {
		DRM_ERROR("get_hbo: failed to allocate buffer with size %zu\n",
			  args->size);
		ret = -ENOMEM;
		goto error;
	}

	bo = to_zocl_bo(&cma_obj->base);

	bo->flags |= ZOCL_BO_FLAGS_HOST_BO;
	bo->flags |= ZOCL_BO_FLAGS_CMA;

	ret = drm_gem_handle_create(filp, &bo->cma_base.base, &args->handle);
	if (ret) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		drm_gem_dma_object_free(&bo->cma_base.base);
#else
		drm_gem_cma_free_object(&bo->cma_base.base);
#endif
		DRM_ERROR("get_hbo: gem handle creation failed\n");
		return ret;
	}

	zocl_describe(bo);
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&bo->cma_base.base);

	return ret;
error:
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&cma_obj->base);
	return ret;
}

void zocl_free_host_bo(struct drm_gem_object *gem_obj)
{
	struct drm_zocl_bo *zocl_bo = to_zocl_bo(gem_obj);

	DRM_DEBUG("%s: obj 0x%px", __func__, zocl_bo);

	memunmap(zocl_bo->cma_base.vaddr);

	drm_gem_object_release(gem_obj);

	kfree(&zocl_bo->cma_base);
}

/*
 * Update the memory usage of by BO.
 *
 * count is the number of BOs being allocated/freed. If count > 0, we are
 * allocating 'count' BOs with total size 'size'; If count < 0, we are
 * freeing 'count' BOs with total size 'size'.
 */
void zocl_update_mem_stat(struct drm_zocl_dev *zdev, u64 size, int count,
		uint32_t index)
{
	struct zocl_mem *mem = zocl_get_mem_by_mem_index(zdev, index);
	if (!mem)
		return;

	/*
	 * If the 'bank' passed in is a valid bank and its type is
	 * PL-DDR or LPDDR , we update that bank usage. Otherwise, we go
	 * through our bank list and find the CMA bank to update
	 * its usage.
	 */
	if (mem->zm_type != ZOCL_MEM_TYPE_RANGE_ALLOC) {
		struct zocl_mem *curr_mem = NULL;
		list_for_each_entry(curr_mem, &zdev->zm_list_head, link) {
			if (curr_mem->zm_used &&
			    curr_mem->zm_type == ZOCL_MEM_TYPE_CMA) {
				mem = curr_mem;
				break;
			}
		}
	}

	write_lock(&zdev->attr_rwlock);
	mem->zm_stat.memory_usage +=
	    (count > 0) ?  size : -size;
	mem->zm_stat.bo_count += count;
	write_unlock(&zdev->attr_rwlock);
}

/* This function return True if given region is reserved
 * on device tree. Else return False
 */
static bool check_for_reserved_memory(uint64_t start_addr, size_t size)
{
	struct device_node *mem_np;
	struct device_node *np_it;
	struct resource res_mem;
	int err;

	mem_np = of_find_node_by_name(NULL, "reserved-memory");
	if(!mem_np)
		return false;

	/* Traverse through all the child nodes */
	for (np_it = NULL; (np_it = of_get_next_child(mem_np, np_it)) != NULL;) {
		err = of_address_to_resource(np_it, 0, &res_mem);
		if (!err) {
			/* Check the given address and size fall
			 * in this reserved memory region
			 */
			if (start_addr == res_mem.start &&
					size == resource_size(&res_mem)) {
				of_node_put(mem_np);
				return true;
			}
		}
	}

	of_node_put(mem_np);
	return false;
}


/*
 * Initialize the memory structure in zocl driver based on the memory
 * topology extracted from xclbin.
 *
 * Currently, we could have multiple memory sections but only two type
 * of them could be marked as used. We identify the memory type by its
 * tag. If the tag field contains "MIG", it is PL-DDR. Tag filed LPDDR
 * for higher order LPDDR memory. Other tags e.g. "HP", "HPC", it is
 * CMA memory.
 *
 * PL-DDR and LPDDR are managed by DRM MM Range Allocator;
 * CMA is managed by DRM CMA Allocator.
 *
 * @param       zdev:    zocl device structure
 * @param       slot:  	 slot specific structure
 *
 */
void zocl_init_mem(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot)
{
	struct zocl_mem *memp = NULL;
	struct mem_topology *mtopo = slot->topology;
	uint64_t mm_start_addr = 0;
	uint64_t mm_end_addr = 0;
	int i, j;

	if (!mtopo)
		return;

	mutex_lock(&zdev->mm_lock);
	/* Initialize with max and min possible value */
	mm_start_addr = 0xffffFFFFffffFFFF;
	mm_end_addr = 0;

	for (i = 0; i < mtopo->m_count; i++) {
		struct mem_data *md = &mtopo->m_mem_data[i];

		memp = vzalloc(sizeof(struct zocl_mem));
		if (md->m_type == MEM_STREAMING) {
			memp->zm_type = ZOCL_MEM_TYPE_STREAMING;
			continue;
		}

		memp->zm_base_addr = md->m_base_address;
		/* In mem_topology, size is in KB */
		memp->zm_size = md->m_size * 1024;
		memp->zm_used = md->m_used;
		memp->zm_mem_idx = SET_MEM_INDEX(slot->slot_idx, i);
		/* This list used for multiple tag case */
		INIT_LIST_HEAD(&memp->zm_list);

		list_add_tail(&memp->link, &zdev->zm_list_head);

		if (!check_for_reserved_memory(memp->zm_base_addr,
					       memp->zm_size)) {
			DRM_INFO("Memory %d is not reserved in device tree."
					" Will allocate memory from CMA\n", i);
			memp->zm_type = ZOCL_MEM_TYPE_CMA;
			continue;
		}

                /* Update the start and end address for the memory manager */
                if (memp->zm_base_addr < mm_start_addr)
                        mm_start_addr = memp->zm_base_addr;
                if ((memp->zm_base_addr + memp->zm_size) > mm_end_addr)
                        mm_end_addr = memp->zm_base_addr + memp->zm_size;

		memp->zm_type = ZOCL_MEM_TYPE_RANGE_ALLOC;
	}

	/* Initialize drm memory manager if not yet done */
	if (!zdev->zm_drm_mm) {
		/* Initialize a single drm memory manager for whole memory
		 * available for this device.
		 */
		zdev->zm_drm_mm = vzalloc(sizeof(struct drm_mm));
		drm_mm_init(zdev->zm_drm_mm, mm_start_addr,
			    (mm_end_addr - mm_start_addr));
	}

	/* Create a link list for similar memory manager for this slot */
	for (i = 0; i < mtopo->m_count; i++) {
		struct mem_data *md = &mtopo->m_mem_data[i];
		if (!md->m_used)
			continue;

		memp = zocl_get_memp_by_mem_data(zdev, md, slot->slot_idx);
		if (!memp) {
			DRM_ERROR("Failed to get the memoory\n");
			mutex_unlock(&zdev->mm_lock);
			return;
		}

		for (j = 0; j < mtopo->m_count; j++) {
			struct zocl_mem *tmp_memp;
			if ((i == j) || !mtopo->m_mem_data[j].m_used)
				continue;

			tmp_memp = zocl_get_memp_by_mem_data(zdev,
					&mtopo->m_mem_data[j],
					slot->slot_idx);
			if (strcmp(mtopo->m_mem_data[i].m_tag,
				   mtopo->m_mem_data[j].m_tag) &&
			    list_empty(&tmp_memp->zm_list)) {
				list_add_tail(&memp->zm_list,
					      &tmp_memp->zm_list);
				memp = tmp_memp;
			}
		}
	}

	mutex_unlock(&zdev->mm_lock);
}

/*
 * Clean the memories for a specific slot. Other memory remain unchanged.
 * This will not delete the memory manager.
 *
 * @param       zdev:		zocl device structure
 * @param       slot_idx:	slot index
 *
 */
void zocl_clear_mem_slot(struct drm_zocl_dev *zdev, u32 slot_idx)
{
	struct zocl_mem *curr_mem = NULL;
	struct zocl_mem *next = NULL;

	mutex_lock(&zdev->mm_lock);
	if (list_empty(&zdev->zm_list_head))
		goto done;

	list_for_each_entry_safe(curr_mem, next, &zdev->zm_list_head, link) {
		if (slot_idx != GET_SLOT_INDEX(curr_mem->zm_mem_idx))
			continue;

		list_del(&curr_mem->link);
		vfree(curr_mem);
	}

done:
	mutex_unlock(&zdev->mm_lock);
}

/*
 * Clean all the memories for a specific device. This will also delete
 * the memory manager.
 *
 * @param       zdev:	zocl device structure
 *
 */
void zocl_clear_mem(struct drm_zocl_dev *zdev)
{
	struct zocl_mem *curr_mem = NULL;
	struct zocl_mem *next = NULL;

	mutex_lock(&zdev->mm_lock);

	list_for_each_entry_safe(curr_mem, next, &zdev->zm_list_head, link) {
		list_del(&curr_mem->link);
		vfree(curr_mem);
	}

	/* clean up a drm_mm allocator. Free the memory */
	if (zdev->zm_drm_mm) {
		drm_mm_takedown(zdev->zm_drm_mm);
		vfree(zdev->zm_drm_mm);
	}

	mutex_unlock(&zdev->mm_lock);
}

void zocl_drm_free_bo(struct drm_zocl_bo *bo)
{
	zocl_free_bo(&bo->gem_base);
}
