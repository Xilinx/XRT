/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
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
#include <linux/iommu.h>
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
	size_t physical_addr = obj->cma_base.paddr;

	DRM_DEBUG("%p: H[0x%zxKB] D[0x%zx]\n",
			obj,
			size_in_kb,
			physical_addr);
}

static inline void
zocl_bo_describe(const struct drm_zocl_bo *bo, uint64_t *size, uint64_t *paddr)
{
	if (bo->flags & (ZOCL_BO_FLAGS_CMA | ZOCL_BO_FLAGS_USERPTR)) {
		*size = (uint64_t)bo->cma_base.base.size;
		*paddr = (uint64_t)bo->cma_base.paddr;
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
	bo->sgt = drm_prime_pages_to_sg(bo->pages, bo_size >> PAGE_SHIFT);
	if (IS_ERR(bo->sgt)) {
		bo->uaddr = 0;
		return PTR_ERR(bo->sgt);
	}

	/* MAP user's VA to pages table into IOMMU */
	err = iommu_map_sg(zdev->domain, bo->uaddr, bo->sgt->sgl,
			bo->sgt->nents, prot);
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
	struct drm_gem_cma_object *cma_obj;
	int err = 0;

	if (!size)
		return ERR_PTR(-EINVAL);

	cma_obj = kzalloc(sizeof(*cma_obj), GFP_KERNEL);
	if (!cma_obj) {
		DRM_DEBUG("cma object create failed\n");
		return ERR_PTR(-ENOMEM);
	}

	err = drm_gem_object_init(dev, &cma_obj->base, size);
	if (err) {
		DRM_DEBUG("drm gem object initial failed\n");
		goto out1;
	}

	cma_obj->sgt   = NULL;
	cma_obj->vaddr = NULL;
	cma_obj->paddr = 0x0;

	return to_zocl_bo(&cma_obj->base);
out1:
	kfree(cma_obj);
	return NULL;
}

void zocl_free_userptr_bo(struct drm_gem_object *gem_obj)
{
	/* Do all drm_gem_cma_free_object(bo->base) do, execpt free vaddr */
	struct drm_zocl_bo *zocl_bo = to_zocl_bo(gem_obj);

	DRM_INFO("%s: obj 0x%p", __func__, zocl_bo);
	if (zocl_bo->cma_base.sgt)
		sg_free_table(zocl_bo->cma_base.sgt);

	drm_gem_object_release(gem_obj);

	kfree(&zocl_bo->cma_base);
}

static struct drm_zocl_bo *
zocl_create_bo(struct drm_device *dev, uint64_t unaligned_size, u32 user_flags)
{
	size_t size = PAGE_ALIGN(unaligned_size);
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct drm_gem_cma_object *cma_obj;
	struct drm_zocl_bo *bo;
	int err;

	if (!size)
		return ERR_PTR(-EINVAL);

	if (zdev->domain) {
		bo = kzalloc(sizeof(*bo), GFP_KERNEL);
		if (!bo)
			return ERR_PTR(-ENOMEM);

		err = drm_gem_object_init(dev, &bo->gem_base, size);
		if (err < 0)
			goto free;
	} else if (user_flags & ZOCL_BO_FLAGS_CMA) {
		/* Allocate from CMA buffer */
		cma_obj = drm_gem_cma_create(dev, size);
		if (IS_ERR(cma_obj))
			return ERR_PTR(-ENOMEM);

		bo = to_zocl_bo(&cma_obj->base);
	} else {
		/* We are allocating from a separate BANK, i.e. PL-DDR */
		unsigned int bank = GET_MEM_BANK(user_flags);

		if (bank >= zdev->num_mem || !zdev->mem[bank].zm_used ||
		    zdev->mem[bank].zm_type != ZOCL_MEM_TYPE_PLDDR)
			return ERR_PTR(-EINVAL);

		bo = kzalloc(sizeof(struct drm_zocl_bo), GFP_KERNEL);
		if (IS_ERR(bo))
			return ERR_PTR(-ENOMEM);

		err = drm_gem_object_init(dev, &bo->gem_base, size);
		if (err) {
			kfree(bo);
			return ERR_PTR(err);
		}

		bo->mm_node = kzalloc(sizeof(struct drm_mm_node),
		    GFP_KERNEL);
		if (IS_ERR(bo->mm_node)) {
			kfree(bo);
			return ERR_PTR(-ENOMEM);
		}

		mutex_lock(&zdev->mm_lock);
		err = drm_mm_insert_node_generic(zdev->mem[bank].zm_mm,
		    bo->mm_node, size, PAGE_SIZE, 0, 0);
		if (err) {
			DRM_ERROR("Fail to allocate BO: size %ld\n",
			    (long)size);
			mutex_unlock(&zdev->mm_lock);
			kfree(bo->mm_node);
			kfree(bo);
			return ERR_PTR(-ENOMEM);
		}
		mutex_unlock(&zdev->mm_lock);

		err = drm_gem_create_mmap_offset(&bo->gem_base);
		if (err) {
			DRM_ERROR("Fail to create BO mmap offset.\n");
			zocl_free_bo(&bo->gem_base);
			return ERR_PTR(err);
		}
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

int
zocl_create_svm_bo(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_create_bo *args = data;
	struct drm_zocl_bo *bo;
	size_t bo_size;
	int ret = 0;

	if ((args->flags & ZOCL_BO_FLAGS_COHERENT) ||
			(args->flags & ZOCL_BO_FLAGS_CMA))
		return -EINVAL;

	args->flags |= ZOCL_BO_FLAGS_SVM;
	if (!(args->flags & ZOCL_BO_FLAGS_SVM))
		return -EINVAL;

	bo = zocl_create_bo(dev, args->size, args->flags);
	bo->flags |= ZOCL_BO_FLAGS_SVM;
	bo->bank = GET_MEM_BANK(args->flags);

	if (IS_ERR(bo)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(bo);
	}
	bo->pages = drm_gem_get_pages(&bo->gem_base);
	if (IS_ERR(bo->pages)) {
		ret = PTR_ERR(bo->pages);
		goto out_free;
	}

	bo_size = bo->gem_base.size;
	bo->sgt = drm_prime_pages_to_sg(bo->pages, bo_size >> PAGE_SHIFT);
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
	zocl_update_mem_stat(dev->dev_private, args->size, 1, bo->bank);

	return ret;

out_free:
	zocl_free_bo(&bo->gem_base);
	return ret;
}

int
zocl_create_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret = 0;
	struct drm_zocl_create_bo *args = data;
	struct drm_zocl_bo *bo;
	struct drm_zocl_dev *zdev = dev->dev_private;
	unsigned int bank;

	args->flags = zocl_convert_bo_uflags(args->flags);

	if (zdev->domain)
		return zocl_create_svm_bo(dev, data, filp);

	bank = GET_MEM_BANK(args->flags);

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
		if (bank < zdev->num_mem && zdev->mem[bank].zm_used) {
			if (zdev->mem[bank].zm_type == ZOCL_MEM_TYPE_CMA)
				args->flags |= ZOCL_BO_FLAGS_CMA;
		} else
			args->flags |= ZOCL_BO_FLAGS_CMA;
	}

	if (!(args->flags & ZOCL_BO_FLAGS_CACHEABLE)) {
		/* If cacheable is not set, make sure we set COHERENT. */
		args->flags |= ZOCL_BO_FLAGS_COHERENT;
	} else if (!(args->flags & ZOCL_BO_FLAGS_CMA)) {
		/* We do not support allocating cacheable BO from PL-DDR. */
		DRM_WARN("Cache is not supported and turned off for PL-DDR.\n");
		args->flags &= ~ZOCL_BO_FLAGS_CACHEABLE;
	}

	bo = zocl_create_bo(dev, args->size, args->flags);
	if (IS_ERR(bo)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(bo);
	}

	bo->bank = bank;
	if (args->flags & ZOCL_BO_FLAGS_CACHEABLE)
		bo->flags |= ZOCL_BO_FLAGS_CACHEABLE;
	else
		bo->flags |= ZOCL_BO_FLAGS_COHERENT;

	if (args->flags & ZOCL_BO_FLAGS_CMA) {
		bo->flags |= ZOCL_BO_FLAGS_CMA;
		ret = drm_gem_handle_create(filp, &bo->cma_base.base,
		    &args->handle);
		if (ret) {
			drm_gem_cma_free_object(&bo->cma_base.base);
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

	zocl_describe(bo);
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&bo->cma_base.base);

	/*
	 * Update memory usage statistics.
	 *
	 * Note: We can not use args->size here because it is
	 *       the required size while gem object records the
	 *       actual size allocated.
	 */
	zocl_update_mem_stat(zdev, bo->gem_base.size, 1, bo->bank);

	return ret;
}

int
zocl_userptr_bo_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret;
	struct drm_zocl_bo *bo;
	unsigned int page_count;
	struct drm_zocl_userptr_bo *args = data;
	struct page **pages;
	unsigned int sg_count;

	if (offset_in_page(args->addr)) {
		DRM_ERROR("User ptr not PAGE aligned\n");
		return -EINVAL;
	}

	if (args->flags & ZOCL_BO_FLAGS_EXECBUF) {
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

	bo->cma_base.sgt = drm_prime_pages_to_sg(pages, page_count);
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

	bo->cma_base.paddr = sg_dma_address((bo->cma_base.sgt)->sgl);

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

int zocl_map_bo_ioctl(struct drm_device *dev,
		void *data,
		struct drm_file *filp)
{
	int ret = 0;
	struct drm_zocl_map_bo *args = data;
	struct drm_gem_object *gem_obj;

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
	struct drm_gem_cma_object	*cma_obj;
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
	if (bo->flags & ZOCL_BO_FLAGS_COHERENT) {
		/* The CMA buf is coherent, we don't need to do anything */
		rc = 0;
		goto out;
	}

	cma_obj = to_drm_gem_cma_obj(gem_obj);
	bus_addr = cma_obj->paddr;

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

	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return 0;
}

int zocl_pwrite_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	const struct drm_zocl_pwrite_bo *args = data;
	struct drm_gem_object *gem_obj = zocl_gem_object_lookup(dev, filp,
			args->handle);
	char __user *user_data = to_user_ptr(args->data_ptr);
	int ret = 0;
	void *kaddr;

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

	kaddr = drm_gem_cma_prime_vmap(gem_obj);
	kaddr += args->offset;

	ret = copy_from_user(kaddr, user_data, args->size);
out:
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return ret;
}

int zocl_pread_bo_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	const struct drm_zocl_pread_bo *args = data;
	struct drm_gem_object *gem_obj = zocl_gem_object_lookup(dev, filp,
			args->handle);
	char __user *user_data = to_user_ptr(args->data_ptr);
	int ret = 0;
	void *kaddr;

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

	if (!ZOCL_ACCESS_OK(VERIFY_WRITE, user_data, args->size)) {
		ret = EFAULT;
		goto out;
	}

	kaddr = drm_gem_cma_prime_vmap(gem_obj);
	kaddr += args->offset;

	ret = copy_to_user(user_data, kaddr, args->size);

out:
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return ret;
}

static struct drm_gem_cma_object *
zocl_cma_create(struct drm_device *dev, size_t size)
{
	struct drm_gem_cma_object *cma_obj;
	struct drm_gem_object *gem_obj;
	int ret;

	gem_obj = kzalloc(sizeof(struct drm_zocl_bo), GFP_KERNEL);
	if (!gem_obj)
		return ERR_PTR(-ENOMEM);
	cma_obj = container_of(gem_obj, struct drm_gem_cma_object, base);

	ret = drm_gem_object_init(dev, gem_obj, size);
	if (ret)
		goto error;

	ret = drm_gem_create_mmap_offset(gem_obj);
	if (ret) {
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
	struct drm_gem_cma_object *cma_obj;
	struct drm_zocl_dev *zdev = dev->dev_private;
	u64 host_mem_start = zdev->host_mem;
	u64 host_mem_end = zdev->host_mem + zdev->host_mem_len;
	int ret;

	if (!(host_mem_start <= args->paddr &&
	      args->paddr + args->size <= host_mem_end)) {
		DRM_ERROR("Buffer at out side of reserved memory region\n");
		return -ENOMEM;
	}

	cma_obj = zocl_cma_create(dev, args->size);
	if (IS_ERR(cma_obj))
		return -ENOMEM;

	cma_obj->paddr = args->paddr;
	cma_obj->vaddr = memremap(args->paddr, args->size, MEMREMAP_WB);
	if (!cma_obj->vaddr) {
		DRM_ERROR("failed to allocate buffer with size %zu\n",
			  args->size);
		ret = -ENOMEM;
		goto error;
	}

	bo = to_zocl_bo(&cma_obj->base);

	bo->flags |= ZOCL_BO_FLAGS_HOST_BO;
	bo->flags |= ZOCL_BO_FLAGS_CMA;

	ret = drm_gem_handle_create(filp, &bo->cma_base.base, &args->handle);
	if (ret) {
		drm_gem_cma_free_object(&bo->cma_base.base);
		DRM_DEBUG("handle creation failed\n");
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

	DRM_INFO("%s: obj 0x%p", __func__, zocl_bo);

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
		uint32_t bank)
{
	int i, update_bank = zdev->num_mem;

	/*
	 * If the 'bank' passed in is a valid bank and its type is
	 * PL-DDR, we update that bank usage. Otherwise, we go
	 * through our bank list and find the CMA bank to update
	 * its usage.
	 */
	if (bank < zdev->num_mem &&
	    zdev->mem[bank].zm_type == ZOCL_MEM_TYPE_PLDDR) {
		update_bank = bank;
	} else {
		for (i = 0; i < zdev->num_mem; i++) {
			if (zdev->mem[i].zm_used &&
			    zdev->mem[i].zm_type == ZOCL_MEM_TYPE_CMA) {
				update_bank = i;
				break;
			}
		}
	}

	if (update_bank == zdev->num_mem)
		return;

	write_lock(&zdev->attr_rwlock);
	zdev->mem[update_bank].zm_stat.memory_usage +=
	    (count > 0) ?  size : -size;
	zdev->mem[update_bank].zm_stat.bo_count += count;
	write_unlock(&zdev->attr_rwlock);
}

/*
 * Initialize the memory structure in zocl driver based on the memory
 * topology extracted from xclbin.
 *
 * Currently, we could have multiple memory sections but only two type
 * of them could be marked as used. We identify the memory type by its
 * tag. If the tag field contains "MIG", it is PL-DDR. Other tags
 * e.g. "HP", "HPC", it is CMA memory.
 *
 * PL-DDR is managed by DRM MM Range Allocator;
 * CMA is managed by DRM CMA Allocator.
 */
void zocl_init_mem(struct drm_zocl_dev *zdev, struct mem_topology *mtopo)
{
	struct zocl_mem *memp;
	int i;

	zdev->num_mem = mtopo->m_count;
	zdev->mem = vzalloc(zdev->num_mem * sizeof(struct zocl_mem));

	for (i = 0; i < zdev->num_mem; i++) {
		struct mem_data *md = &mtopo->m_mem_data[i];

		if (!md->m_used)
			continue;

		memp = &zdev->mem[i];
		if (md->m_type == MEM_STREAMING) {
			memp->zm_type = ZOCL_MEM_TYPE_STREAMING;
			continue;
		}

		memp->zm_base_addr = md->m_base_address;
		/* In mem_topology, size is in KB */
		memp->zm_size = md->m_size * 1024;
		memp->zm_used = 1;

		if (!strstr(md->m_tag, "MIG")) {
			memp->zm_type = ZOCL_MEM_TYPE_CMA;
			continue;
		}

		memp->zm_mm = vzalloc(sizeof(struct drm_mm));
		memp->zm_type = ZOCL_MEM_TYPE_PLDDR;

		drm_mm_init(memp->zm_mm, memp->zm_base_addr, memp->zm_size);
	}
}

void zocl_clear_mem(struct drm_zocl_dev *zdev)
{
	int i;

	if (!zdev->mem)
		return;

	mutex_lock(&zdev->mm_lock);

	for (i = 0; i < zdev->num_mem; i++) {
		struct zocl_mem *md = &zdev->mem[i];

		if (md->zm_mm) {
			drm_mm_takedown(md->zm_mm);
			vfree(md->zm_mm);
		}
	}

	vfree(zdev->mem);
	zdev->mem = NULL;
	zdev->num_mem = 0;

	mutex_unlock(&zdev->mm_lock);
}

struct drm_gem_object *zocl_gem_import(struct drm_device *dev,
				       struct dma_buf *dma_buf)
{
	struct drm_gem_object *gem_obj;
	struct drm_zocl_bo *zocl_bo;

	gem_obj = drm_gem_prime_import(dev, dma_buf);
	if (!IS_ERR(gem_obj)) {
		zocl_bo = to_zocl_bo(gem_obj);
		/* drm_gem_cma_prime_import_sg_table() is used for hook
		 * gem_prime_import_sg_table. It will check if import buffer
		 * is a CMA buffer and create CMA object.
		 */
		zocl_bo->flags |= ZOCL_BO_FLAGS_CMA;
	}

	return gem_obj;
}
