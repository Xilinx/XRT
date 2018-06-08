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

#include <linux/iommu.h>
#include <linux/pagemap.h>
#include "zoclsvm_drv.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 0)
static inline void drm_free_large(void *ptr)
{
	kvfree(ptr);
}

static inline void *drm_malloc_ab(size_t nmemb, size_t size)
{
	return kvmalloc_array(nmemb, sizeof(struct page *), GFP_KERNEL);
}
#endif

static inline char __user *to_user_ptr(u64 address)
{
	return (char __user *)(uintptr_t)address;
}

void zoclsvm_describe(const struct drm_zoclsvm_bo *obj)
{
	size_t size_in_kb = obj->base.size / 1024;

	DRM_INFO("%p: S[0x%zxKB] H[%p]\n",
		  obj,
		  size_in_kb,
		  obj->vmapping);
}

int zoclsvm_iommu_map_bo(struct drm_device *dev, struct drm_zoclsvm_bo *bo)
{
	int prot = IOMMU_READ | IOMMU_WRITE;
	struct drm_zoclsvm_dev *zdev = dev->dev_private;
	ssize_t err;

	/* Create scatter gather list from user's pages */
	bo->sgt = drm_prime_pages_to_sg(bo->pages, bo->base.size >> PAGE_SHIFT);
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

int zoclsvm_iommu_unmap_bo(struct drm_device *dev, struct drm_zoclsvm_bo *bo)
{
	struct drm_zoclsvm_dev *zdev = dev->dev_private;
	/* If IOMMU map had failed before bo->uaddr will be zero */
	if (bo->uaddr)
		iommu_unmap(zdev->domain, bo->uaddr, bo->base.size);
	return 0;
}


void zoclsvm_free_bo(struct drm_gem_object *obj)
{
	struct drm_zoclsvm_bo *bo = to_zoclsvm_bo(obj);
	int npages = obj->size >> PAGE_SHIFT;
	DRM_INFO("Freeing BO %p\n", bo);

	drm_gem_object_release(obj);

	if (bo->vmapping)
		vunmap(bo->vmapping);
	bo->vmapping = NULL;

	zoclsvm_iommu_unmap_bo(obj->dev, bo);
	if (bo->pages) {
		if (zoclsvm_bo_userptr(bo)) {
			release_pages(bo->pages, npages, 0);
			drm_free_large(bo->pages);
		}
		else
			drm_gem_put_pages(obj, bo->pages, false, false);
	}
	if (bo->sgt)
		sg_free_table(bo->sgt);
	bo->sgt = NULL;
	bo->pages = NULL;
	kfree(bo);
}

static struct drm_zoclsvm_bo *zoclsvm_create_bo(struct drm_device *dev,
						uint64_t unaligned_size, unsigned user_flags)
{
	size_t size = PAGE_ALIGN(unaligned_size);
	struct drm_zoclsvm_bo *bo;
	int err;

	DRM_DEBUG("%s:%s:%d: %zd\n", __FILE__, __func__, __LINE__, size);

	if (!size)
		return ERR_PTR(-EINVAL);

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (!bo)
		return ERR_PTR(-ENOMEM);

	err = drm_gem_object_init(dev, &bo->base, size);
	if (err < 0)
		goto free;

  if (user_flags & DRM_ZOCL_BO_FLAGS_EXECBUF) {
    bo->flags = DRM_ZOCL_BO_FLAGS_EXECBUF;
    bo->metadata.state = DRM_ZOCL_EXECBUF_STATE_ABORT;
  }

	return bo;
free:
	kfree(bo);
	return ERR_PTR(err);
}

int zoclsvm_create_bo_ioctl(struct drm_device *dev,
			    void *data,
			    struct drm_file *filp)
{
	int ret = 0;
	struct drm_zocl_create_bo *args = data;
	struct drm_zoclsvm_bo *bo;

  // Remove all flags, except EXECBUF. 
  args->flags &= DRM_ZOCL_BO_FLAGS_EXECBUF;

	if ((args->flags & DRM_ZOCL_BO_FLAGS_COHERENT) ||
	    (args->flags & DRM_ZOCL_BO_FLAGS_CMA))
		return -EINVAL;

  // TODO: SHIM should pass the correct flags
  args->flags |= DRM_ZOCL_BO_FLAGS_SVM;
	if (!(args->flags & DRM_ZOCL_BO_FLAGS_SVM))
		return -EINVAL;

	bo = zoclsvm_create_bo(dev, args->size, args->flags);
	bo->flags |= DRM_ZOCL_BO_FLAGS_SVM;

	DRM_DEBUG("%s:%s:%d: %p\n", __FILE__, __func__, __LINE__, bo);

	if (IS_ERR(bo)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(bo);
	}
	bo->pages = drm_gem_get_pages(&bo->base);
	if (IS_ERR(bo->pages)) {
		ret = PTR_ERR(bo->pages);
		goto out_free;
	}

	bo->sgt = drm_prime_pages_to_sg(bo->pages, bo->base.size >> PAGE_SHIFT);
	if (IS_ERR(bo->sgt))
		goto out_free;

	bo->vmapping = vmap(bo->pages, bo->base.size >> PAGE_SHIFT, VM_MAP,
			    pgprot_writecombine(PAGE_KERNEL));

	if (!bo->vmapping) {
		ret = -ENOMEM;
		goto out_free;
	}

	ret = drm_gem_create_mmap_offset(&bo->base);
	if (ret < 0)
		goto out_free;

	ret = drm_gem_handle_create(filp, &bo->base, &args->handle);
	if (ret < 0)
		goto out_free;

	zoclsvm_describe(bo);
	drm_gem_object_unreference_unlocked(&bo->base);
	return ret;

out_free:
	zoclsvm_free_bo(&bo->base);
	return ret;
}

int zoclsvm_userptr_bo_ioctl(struct drm_device *dev,
			      void *data,
			      struct drm_file *filp)
{
	int ret;
	struct drm_zocl_userptr_bo *args = data;
	struct drm_zoclsvm_bo *bo;

	if (offset_in_page(args->addr | args->size))
		return -EINVAL;

  if (args->flags & DRM_ZOCL_BO_FLAGS_EXECBUF)
    return -EINVAL;

	if ((args->flags & DRM_ZOCL_BO_FLAGS_COHERENT) ||
	    (args->flags & DRM_ZOCL_BO_FLAGS_CMA))
		return -EINVAL;

	if (!(args->flags & DRM_ZOCL_BO_FLAGS_SVM))
		return -EINVAL;

	bo = zoclsvm_create_bo(dev, args->size, args->flags);

	DRM_DEBUG("%s:%s:%d: %p\n", __FILE__, __func__, __LINE__, bo);

	if (IS_ERR(bo)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(bo);
	}

	bo->pages = drm_malloc_ab(args->size >> PAGE_SHIFT, sizeof(*bo->pages));
	if (!bo->pages) {
		ret = -ENOMEM;
		goto out1;
	}
	ret = get_user_pages_fast(args->addr, args->size >> PAGE_SHIFT, 1, bo->pages);

	if (ret != (args->size >> PAGE_SHIFT))
		goto out0;

	bo->vmapping = vmap(bo->pages, bo->base.size >> PAGE_SHIFT, VM_MAP,
			      pgprot_writecombine(PAGE_KERNEL));

	if (!bo->vmapping) {
		ret = -ENOMEM;
		goto out1;
	}

	bo->uaddr = args->addr;
	bo->flags |= DRM_ZOCL_BO_FLAGS_USERPTR;

	ret = zoclsvm_iommu_map_bo(dev, bo);
	if (ret)
		goto out1;

	ret = drm_gem_handle_create(filp, &bo->base, &args->handle);
	if (ret)
		goto out1;

	zoclsvm_describe(bo);
	drm_gem_object_unreference_unlocked(&bo->base);
	return ret;

out0:
	drm_free_large(bo->pages);
	bo->pages = NULL;
out1:
	zoclsvm_free_bo(&bo->base);
	DRM_DEBUG("handle creation failed\n");
	return ret;
}


int zoclsvm_map_bo_ioctl(struct drm_device *dev,
			 void *data,
			 struct drm_file *filp)
{
	struct drm_gem_object *gem_obj;
	int err = 0;
	struct drm_zocl_map_bo *args = data;

	gem_obj = drm_gem_object_lookup(filp, args->handle);
	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}

	if (zoclsvm_bo_userptr(to_zoclsvm_bo(gem_obj))) {
		err = -EPERM;
		goto out;
	}
	/* The mmap offset was set up at BO allocation time. */
	args->offset = drm_vma_node_offset_addr(&gem_obj->vma_node);

	zoclsvm_describe(to_zoclsvm_bo(gem_obj));
out:
	drm_gem_object_unreference_unlocked(gem_obj);
	return err;
}

int zoclsvm_sync_bo_ioctl(struct drm_device *dev,
			   void *data,
			   struct drm_file *filp)
{
	const struct drm_zocl_sync_bo *args = data;
	struct drm_gem_object *gem_obj = drm_gem_object_lookup(filp,
							       args->handle);
	struct drm_zoclsvm_bo *bo = to_zoclsvm_bo(gem_obj);

	char *kaddr;
	int ret = 0;

	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}

	if ((args->offset > gem_obj->size) || (args->size > gem_obj->size) ||
		((args->offset + args->size) > gem_obj->size)) {
		ret = -EINVAL;
		goto out;
	}

	kaddr = bo->vmapping;

	/* only invalidate the range of addresses requested by the user */
	kaddr += args->offset;

	if (args->dir == DRM_ZOCL_SYNC_BO_TO_DEVICE)
		flush_kernel_vmap_range(kaddr, args->size);
	else if (args->dir == DRM_ZOCL_SYNC_BO_FROM_DEVICE)
		invalidate_kernel_vmap_range(kaddr, args->size);
	else
		ret = -EINVAL;

out:
	drm_gem_object_unreference_unlocked(gem_obj);

	return ret;
}

int zoclsvm_info_bo_ioctl(struct drm_device *dev,
			   void *data,
			   struct drm_file *filp)
{
	const struct drm_zoclsvm_bo *bo;
	struct drm_zocl_info_bo *args = data;
	struct drm_gem_object *gem_obj = drm_gem_object_lookup(filp,
							       args->handle);

	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}

	bo = to_zoclsvm_bo(gem_obj);

	args->size = bo->base.size;
	args->paddr = (uint64_t)bo->vmapping;
	drm_gem_object_unreference_unlocked(gem_obj);

	return 0;
}

int zoclsvm_pwrite_bo_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *filp)
{
	const struct drm_zocl_pwrite_bo *args = data;
	struct drm_gem_object *gem_obj = drm_gem_object_lookup(filp,
							       args->handle);
	struct drm_zoclsvm_bo *bo = to_zoclsvm_bo(gem_obj);

	char __user *user_data = to_user_ptr(args->data_ptr);
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

	if (!access_ok(VERIFY_READ, user_data, args->size)) {
		ret = -EFAULT;
		goto out;
	}

	kaddr = bo->vmapping;
	kaddr += args->offset;

	ret = copy_from_user(kaddr, user_data, args->size);
out:
	drm_gem_object_unreference_unlocked(gem_obj);

	return ret;
}

int zoclsvm_pread_bo_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp)
{
	const struct drm_zocl_pread_bo *args = data;
	struct drm_gem_object *gem_obj = drm_gem_object_lookup(filp,
							       args->handle);
	struct drm_zoclsvm_bo *bo = to_zoclsvm_bo(gem_obj);

	char __user *user_data = to_user_ptr(args->data_ptr);
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

	if (!access_ok(VERIFY_WRITE, user_data, args->size)) {
		ret = -EFAULT;
		goto out;
	}

	kaddr = bo->vmapping;
	kaddr += args->offset;

	ret = copy_to_user(user_data, kaddr, args->size);

out:
	drm_gem_object_unreference_unlocked(gem_obj);

	return ret;
}
