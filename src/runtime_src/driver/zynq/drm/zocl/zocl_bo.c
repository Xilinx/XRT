/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
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

#include <linux/dma-mapping.h>
#include <linux/pagemap.h>
#include <linux/iommu.h>
#include <asm/io.h>
#include "zocl_drv.h"

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

	DRM_INFO("zocl_free_userptr_bo: obj 0x%p", zocl_bo);
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
	} else {
		cma_obj = drm_gem_cma_create(dev, size);
		if (IS_ERR(cma_obj))
			return ERR_PTR(-ENOMEM);

		bo = to_zocl_bo(&cma_obj->base);
	}

	if (user_flags & XCL_BO_FLAGS_EXECBUF) {
		bo->flags = XCL_BO_FLAGS_EXECBUF;
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

	if ((args->flags & XCL_BO_FLAGS_COHERENT) ||
			(args->flags & XCL_BO_FLAGS_CMA))
		return -EINVAL;

	args->flags |= XCL_BO_FLAGS_SVM;
	if (!(args->flags & XCL_BO_FLAGS_SVM))
		return -EINVAL;

	bo = zocl_create_bo(dev, args->size, args->flags);
	bo->flags |= XCL_BO_FLAGS_SVM;

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
	drm_gem_object_unreference_unlocked(&bo->gem_base);
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

	/* Remove all flags, except EXECBUF and CACHEABLE. */
	args->flags &= XCL_BO_FLAGS_EXECBUF | XCL_BO_FLAGS_CACHEABLE;

	if (zdev->domain)
		return zocl_create_svm_bo(dev, data, filp);

	/* This is not good. But force to use CMA flags here. */
	/* Remove this only when XRT use the same flags for xocl and zocl */
	args->flags |= XCL_BO_FLAGS_CMA;

	/* If cacheable is not set, make sure we set COHERENT. */
	if (!(args->flags & XCL_BO_FLAGS_CACHEABLE))
		args->flags |= XCL_BO_FLAGS_COHERENT;

	bo = zocl_create_bo(dev, args->size, args->flags);
	if (IS_ERR(bo)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(bo);
	}

	if (args->flags & XCL_BO_FLAGS_CACHEABLE)
		bo->flags |= XCL_BO_FLAGS_CACHEABLE;
	else
		bo->flags |= XCL_BO_FLAGS_COHERENT;
	bo->flags |= XCL_BO_FLAGS_CMA;

	ret = drm_gem_handle_create(filp, &bo->cma_base.base, &args->handle);
	if (ret) {
		drm_gem_cma_free_object(&bo->cma_base.base);
		DRM_DEBUG("handle creation failed\n");
		return ret;
	}

	zocl_describe(bo);
	drm_gem_object_unreference_unlocked(&bo->cma_base.base);

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

	if (args->flags & XCL_BO_FLAGS_EXECBUF) {
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
		DRM_ERROR("User buffer is not physical contiguous\n");
		ret = -EINVAL;
		goto out0;
	}

	bo->cma_base.vaddr = (void *)args->addr;

	ret = drm_gem_handle_create(filp, &bo->cma_base.base, &args->handle);
	if (ret) {
		ret = -EINVAL;
		DRM_ERROR("Handle creation failed\n");
		goto out0;
	}

	bo->flags |= XCL_BO_FLAGS_USERPTR;

	zocl_describe(bo);
	drm_gem_object_unreference_unlocked(&bo->cma_base.base);

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
	drm_gem_object_unreference_unlocked(gem_obj);
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
	if (bo->flags & XCL_BO_FLAGS_COHERENT) {
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
	drm_gem_object_unreference_unlocked(gem_obj);

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

	args->size = bo->cma_base.base.size;
	args->paddr = bo->cma_base.paddr;
	drm_gem_object_unreference_unlocked(gem_obj);

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

	if (!access_ok(VERIFY_READ, user_data, args->size)) {
		ret = -EFAULT;
		goto out;
	}

	kaddr = drm_gem_cma_prime_vmap(gem_obj);
	kaddr += args->offset;

	ret = copy_from_user(kaddr, user_data, args->size);
out:
	drm_gem_object_unreference_unlocked(gem_obj);

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

	if (!access_ok(VERIFY_WRITE, user_data, args->size)) {
		ret = EFAULT;
		goto out;
	}

	kaddr = drm_gem_cma_prime_vmap(gem_obj);
	kaddr += args->offset;

	ret = copy_to_user(user_data, kaddr, args->size);

out:
	drm_gem_object_unreference_unlocked(gem_obj);

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

	bo->flags |= XCL_BO_FLAGS_HOST_BO;
	bo->flags |= XCL_BO_FLAGS_CMA;

	ret = drm_gem_handle_create(filp, &bo->cma_base.base, &args->handle);
	if (ret) {
		drm_gem_cma_free_object(&bo->cma_base.base);
		DRM_DEBUG("handle creation failed\n");
		return ret;
	}

	zocl_describe(bo);
	drm_gem_object_unreference_unlocked(&bo->cma_base.base);

	return ret;
error:
	drm_gem_object_put_unlocked(&cma_obj->base);
	return ret;
}

void zocl_free_host_bo(struct drm_gem_object *gem_obj)
{
	struct drm_zocl_bo *zocl_bo = to_zocl_bo(gem_obj);

	DRM_INFO("zocl_free_host_bo: obj 0x%p", zocl_bo);

	memunmap(zocl_bo->cma_base.vaddr);

	drm_gem_object_release(gem_obj);

	kfree(&zocl_bo->cma_base);
}

