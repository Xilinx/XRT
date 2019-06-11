/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2017 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Sarabjeet Singh <sarabjeet.singh@xilinx.com>
 *    Jan Stephan <j.stephan@hzdr.de>
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

#include <linux/bitops.h>
#include <linux/swap.h>
#include <linux/dma-buf.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#ifdef XOCL_CMA_ALLOC
#include <linux/cma.h>
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include "common.h"

#ifdef _XOCL_BO_DEBUG
#define	BO_ENTER(fmt, args...)		\
	printk(KERN_INFO "[BO] Entering %s:"fmt"\n", __func__, ##args)
#define	BO_DEBUG(fmt, args...)		\
	printk(KERN_INFO "[BO] %s:%d:"fmt"\n", __func__,__LINE__, ##args)
#else
#define BO_ENTER(fmt, args...)
#define	BO_DEBUG(fmt, args...)
#endif

#define	INVALID_BO_PADDR	0xffffffffffffffffull

#if defined(XOCL_DRM_FREE_MALLOC)
static inline void drm_free_large(void *ptr)
{
	kvfree(ptr);
}

static inline void *drm_malloc_ab(size_t nmemb, size_t size)
{
	return kvmalloc_array(nmemb, sizeof(struct page *), GFP_KERNEL);
}
#endif

static inline void xocl_release_pages(struct page **pages, int nr, bool cold)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	release_pages(pages, nr);
#else
	release_pages(pages, nr, cold);
#endif
}


static inline void __user *to_user_ptr(u64 address)
{
	return (void __user *)(uintptr_t)address;
}

static size_t xocl_bo_physical_addr(const struct drm_xocl_bo *xobj)
{
	uint64_t paddr = xobj->mm_node ? xobj->mm_node->start : INVALID_BO_PADDR;

	//Sarab: Need to check for number of hops & size of DDRs
	if (xobj->type & XOCL_BO_ARE)
		paddr |= XOCL_ARE_HOP;
	return paddr;
}

void xocl_describe(const struct drm_xocl_bo *xobj)
{
	size_t size_in_kb = xobj->base.size / 1024;
	size_t physical_addr = xocl_bo_physical_addr(xobj);
	unsigned ddr = xocl_bo_ddr_idx(xobj->flags);
	unsigned userptr = xocl_bo_userptr(xobj) ? 1 : 0;

	DRM_DEBUG("%p: H[%p] SIZE[0x%zxKB] D[0x%zx] DDR[%u] UPTR[%u] SGLCOUNT[%u]\n",
		  xobj, xobj->vmapping ? xobj->vmapping : xobj->bar_vmapping, size_in_kb,
			physical_addr, ddr, userptr, xobj->sgt->orig_nents);
}

static void xocl_free_mm_node(struct drm_xocl_bo *xobj)
{
	struct drm_device *ddev = xobj->base.dev;
	struct xocl_drm *drm_p = ddev->dev_private;
	unsigned ddr = xocl_bo_ddr_idx(xobj->flags);

	mutex_lock(&drm_p->mm_lock);
	BO_ENTER("xobj %p, mm_node %p", xobj, xobj->mm_node);
	if (!xobj->mm_node)
		goto end;

	xocl_mm_update_usage_stat(drm_p, ddr, xobj->base.size, -1);
	BO_DEBUG("remove mm_node:%p, start:%llx size: %llx", xobj->mm_node,
		xobj->mm_node->start, xobj->mm_node->size);
	drm_mm_remove_node(xobj->mm_node);
	kfree(xobj->mm_node);
	xobj->mm_node = NULL;
end:
	mutex_unlock(&drm_p->mm_lock);
}

static void xocl_free_bo(struct drm_gem_object *obj)
{
	struct drm_xocl_bo *xobj = to_xocl_bo(obj);
	struct drm_device *ddev = xobj->base.dev;
	struct xocl_drm *drm_p = ddev->dev_private;
	struct xocl_dev *xdev= drm_p->xdev;
	int npages = obj->size >> PAGE_SHIFT;
	DRM_DEBUG("Freeing BO %p\n", xobj);

	BO_ENTER("xobj %p pages %p", xobj, xobj->pages);
	if (xobj->vmapping)
		vunmap(xobj->vmapping);
	xobj->vmapping = NULL;

	if (xobj->dmabuf) {
		unmap_mapping_range(xobj->dmabuf->file->f_mapping, 0, 0, 1);
	}

	if (xobj->dma_nsg) {
		pci_unmap_sg(xdev->core.pdev, xobj->sgt->sgl, xobj->dma_nsg,
			PCI_DMA_BIDIRECTIONAL);
	}

	if (xobj->pages) {
		if (xocl_bo_userptr(xobj)) {
			xocl_release_pages(xobj->pages, npages, 0);
			drm_free_large(xobj->pages);
		}
#ifdef XOCL_CMA_ALLOC
		else if (xocl_bo_cma(xobj)) {
			if (xobj->pages[0])
				cma_release(xdev->cma_blk, xobj->pages[0], npages);
			drm_free_large(xobj->pages);
		}
#endif
		else if(xocl_bo_p2p(xobj) || xocl_bo_import(xobj)){
			drm_free_large(xobj->pages);
		}
		else {
			drm_gem_put_pages(obj, xobj->pages, false, false);
		}
	}
	xobj->pages = NULL;

	if (!xocl_bo_import(xobj)) {
		DRM_DEBUG("Freeing regular buffer\n");
		if (xobj->sgt) {
			sg_free_table(xobj->sgt);
			kfree(xobj->sgt);
		}
		xobj->sgt = NULL;
		xocl_free_mm_node(xobj);
	}
	else {
		DRM_DEBUG("Freeing imported buffer\n");
		if (obj->import_attach) {
			DRM_DEBUG("Unnmapping attached dma buf\n");
			dma_buf_unmap_attachment(obj->import_attach,
				xobj->sgt, DMA_TO_DEVICE);
			drm_prime_gem_destroy(obj, NULL);
		}
	}

	//If it is imported BO then we do not delete SG Table
	//And if is imported from ARE device then we do not free the mm_node as well

	//Sarab: Call detach here........
	//to let the exporting device know that importing device do not need it anymore..
	//else free_bo i.e this function is not called for exporting device
	//as it assumes that the exported buffer is still being used
	//dmabuf->ops->release(dmabuf);
	//The drm_driver.gem_free_object callback is responsible for cleaning up the dma_buf attachment and references acquired at import time.

	/* This crashes machine.. Using above code instead
	 * drm_prime_gem_destroy calls detach function..
	 struct dma_buf *imported_dma_buf = obj->dma_buf;
	 if (imported_dma_buf->ops->detach)
	 imported_dma_buf->ops->detach(imported_dma_buf, obj->import_attach);
	*/

	drm_gem_object_release(obj);
	kfree(xobj);
}

void xocl_drm_free_bo(struct drm_gem_object *obj)
{
	xocl_free_bo(obj);
}

static inline int check_bo_user_reqs(const struct drm_device *dev,
	unsigned flags , unsigned type)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev= drm_p->xdev;
	u16 ddr_count;
	unsigned ddr;

	if (type == DRM_XOCL_BO_EXECBUF || type == DRM_XOCL_BO_IMPORT)
		return 0;
#ifdef XOCL_CMA_ALLOC
	if (type == DRM_XOCL_BO_CMA)
		return 0;
#else
	if (type == DRM_XOCL_BO_CMA)
		return -EINVAL;
#endif

	//From "mem_topology" or "feature rom" depending on
	//unified or non-unified dsa
	ddr_count = XOCL_DDR_COUNT(xdev);

	if(ddr_count == 0)
		return -EINVAL;
	ddr = xocl_bo_ddr_idx(flags);
	if (ddr >= ddr_count)
		return -EINVAL;

	if (XOCL_MEM_TOPOLOGY(xdev)) {
		if (XOCL_IS_STREAM(XOCL_MEM_TOPOLOGY(xdev), ddr)) {
			userpf_err(xdev, "Bank %d is Stream", ddr);
			return -EINVAL;
		}
		if (!XOCL_IS_DDR_USED(xdev, ddr)) {
			userpf_err(xdev,
				"Bank %d is marked as unused in axlf", ddr);
			return -EINVAL;
		}
	}
	return 0;
}

static struct drm_xocl_bo *xocl_create_bo(struct drm_device *dev,
					  uint64_t unaligned_size,
					  unsigned user_flags,
					  unsigned user_type)
{
	size_t size = PAGE_ALIGN(unaligned_size);
	struct drm_xocl_bo *xobj;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	unsigned ddr = xocl_bo_ddr_idx(user_flags);
	u16 ddr_count = 0;
	bool xobj_inited = false;
	int err = 0;

	BO_DEBUG("New create bo flags:%u type:%u", user_flags, user_type);
	if (!size)
		return ERR_PTR(-EINVAL);

	/* Either none or only one DDR should be specified */
	/* Check the type */
	if (check_bo_user_reqs(dev, user_flags, user_type))
		return ERR_PTR(-EINVAL);

	xobj = kzalloc(sizeof(*xobj), GFP_KERNEL);
	if (!xobj)
		return ERR_PTR(-ENOMEM);

	BO_ENTER("xobj %p", xobj);
	err = drm_gem_object_init(dev, &xobj->base, size);
	if (err)
		goto failed;
	xobj_inited = true;

	if (user_type == DRM_XOCL_BO_EXECBUF) {
		xobj->type = XOCL_BO_EXECBUF;
		xobj->metadata.state = DRM_XOCL_EXECBUF_STATE_ABORT;
		return xobj;
	}
	if (user_type == DRM_XOCL_BO_IMPORT) {
		xobj->type = XOCL_BO_IMPORT;
		return xobj;
	}

	if (user_type & DRM_XOCL_BO_P2P){
		xobj->type = XOCL_BO_P2P;
	}
#ifdef XOCL_CMA_ALLOC
	if (user_type == DRM_XOCL_BO_CMA) {
		xobj->type = XOCL_BO_CMA;
		return xobj;
	}
#endif

	xobj->mm_node = kzalloc(sizeof(*xobj->mm_node), GFP_KERNEL);
	if (!xobj->mm_node) {
		err = -ENOMEM;
		goto failed;
	}

	ddr_count = XOCL_DDR_COUNT(xdev);

	mutex_lock(&drm_p->mm_lock);
	/* Attempt to allocate buffer on the requested DDR */
	xocl_xdev_dbg(xdev, "alloc bo from bank%u", ddr);
	err = xocl_mm_insert_node(drm_p, ddr, xobj->mm_node,
		xobj->base.size);
	BO_DEBUG("insert mm_node:%p, start:%llx size: %llx",
		xobj->mm_node, xobj->mm_node->start,
		xobj->mm_node->size);
	if (err)
		goto failed;

	xocl_mm_update_usage_stat(drm_p, ddr, xobj->base.size, 1);
	mutex_unlock(&drm_p->mm_lock);
	/* Record the DDR we allocated the buffer on */
	//xobj->flags |= (1 << ddr);
	xobj->flags = ddr;

	return xobj;
failed:
	mutex_unlock(&drm_p->mm_lock);
	if (xobj->mm_node)
		kfree(xobj->mm_node);
	if (xobj_inited)
		drm_gem_object_release(&xobj->base);
	if (xobj)
		kfree(xobj);
	return ERR_PTR(err);
}

struct drm_xocl_bo *xocl_drm_create_bo(struct xocl_drm *drm_p,
					  uint64_t unaligned_size,
					  unsigned user_flags,
					  unsigned user_type)
{
	return xocl_create_bo(drm_p->ddev, unaligned_size, user_flags,
			user_type);
}

static struct page** xocl_p2p_get_pages(void *bar_vaddr, int npages)
{
	struct page *p, **pages;
	int i;
	uint64_t page_offset_enum = 0;

	pages = drm_malloc_ab(npages, sizeof(struct page *));

	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	for(i=0; i<npages;i++){
		p = virt_to_page(bar_vaddr+page_offset_enum);
		pages[i] = p;

		if (IS_ERR(p))
			goto fail;

		page_offset_enum += PAGE_SIZE;
	}

	return pages;
fail:
	kvfree(pages);
	return ERR_CAST(p);
}

int xocl_create_bo_ioctl(struct drm_device *dev,
			 void *data,
			 struct drm_file *filp)
{
	int ret;
	struct drm_xocl_bo *xobj;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
#ifdef XOCL_CMA_ALLOC
	unsigned int page_count;
	int j;
	struct page *cpages;
#endif
	struct drm_xocl_create_bo *args = data;
	unsigned ddr = args->flags;
	unsigned bar_mapped = (args->type & DRM_XOCL_BO_P2P) ? 1 : 0;

	if(bar_mapped){
		if(!xdev->p2p_bar_addr){
			xocl_xdev_err(xdev, "No P2P mem region available, "
					"Can't create p2p BO");
			return -EINVAL;
		}
	}

	xobj = xocl_create_bo(dev, args->size, args->flags, args->type);

	BO_ENTER("xobj %p, mm_node %p", xobj, xobj->mm_node);
	if (IS_ERR(xobj)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(xobj);
	}

	if(bar_mapped){
		ddr = xocl_bo_ddr_idx(xobj->flags);
		/*
		 * DRM allocate contiguous pages, shift the vmapping with
		 * bar address offset
		 */
		xobj->bar_vmapping = xdev->p2p_bar_addr +
			drm_p->mm_p2p_off[ddr] + xobj->mm_node->start -
			XOCL_MEM_TOPOLOGY(xdev)->m_mem_data[ddr].m_base_address;
	}

#ifdef XOCL_CMA_ALLOC
	if (args->type == DRM_XOCL_BO_CMA) {
		page_count = xobj->base.size >> PAGE_SHIFT;
		xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
		if (!xobj->pages) {
			ret = -ENOMEM;
			goto out_free;
		}
		cpages = cma_alloc(xdev->cma_blk, page_count, 0, GFP_KERNEL);
		if (!cpages) {
			ret = -ENOMEM;
			goto out_free;
		}
		for (j = 0; j < page_count; j++)
			xobj->pages[j] = cpages++;
	}
	else {
		xobj->pages = drm_gem_get_pages(&xobj->base);
	}
#else

	if(bar_mapped){
		xobj->pages = xocl_p2p_get_pages(xobj->bar_vmapping, xobj->base.size >> PAGE_SHIFT);
	} else {
		xobj->pages = drm_gem_get_pages(&xobj->base);
	}

#endif
	if (IS_ERR(xobj->pages)) {
		ret = PTR_ERR(xobj->pages);
		goto out_free;
	}

	xobj->sgt = drm_prime_pages_to_sg(xobj->pages, xobj->base.size >> PAGE_SHIFT);
	if (IS_ERR(xobj->sgt)) {
		ret = PTR_ERR(xobj->sgt);
		goto out_free;
	}

	if(!bar_mapped){
		xobj->vmapping = vmap(xobj->pages, xobj->base.size >> PAGE_SHIFT, VM_MAP, PAGE_KERNEL);
		if (!xobj->vmapping) {
			ret = -ENOMEM;
			goto out_free;
		}
	}

	ret = drm_gem_create_mmap_offset(&xobj->base);
	if (ret < 0)
		goto out_free;
	ret = drm_gem_handle_create(filp, &xobj->base, &args->handle);
	if (ret < 0)
		goto out_free;

	xocl_describe(xobj);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);
	return ret;

out_free:
	xocl_free_bo(&xobj->base);
	return ret;
}

int xocl_userptr_bo_ioctl(struct drm_device *dev,
			      void *data,
			      struct drm_file *filp)
{
	int ret;
	struct drm_xocl_bo *xobj;
	unsigned int page_count;
	struct drm_xocl_userptr_bo *args = data;
	//unsigned ddr = args->flags & XOCL_MEM_BANK_MSK;
	//unsigned ddr = args->flags;

	if (offset_in_page(args->addr))
		return -EINVAL;

	if (args->type & DRM_XOCL_BO_EXECBUF)
		return -EINVAL;

	if (args->type & DRM_XOCL_BO_CMA)
		return -EINVAL;

//	if (args->flags && (hweight_long(ddr) > 1))
//		return -EINVAL;

	xobj = xocl_create_bo(dev, args->size, args->flags, args->type);
	BO_ENTER("xobj %p", xobj);

	if (IS_ERR(xobj)) {
		DRM_DEBUG("object creation failed\n");
		return PTR_ERR(xobj);
	}

	/* Use the page rounded size so we can accurately account for number of pages */
	page_count = xobj->base.size >> PAGE_SHIFT;

	xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
	if (!xobj->pages) {
		ret = -ENOMEM;
		goto out1;
	}
	ret = get_user_pages_fast(args->addr, page_count, 1, xobj->pages);

	if (ret != page_count)
		goto out0;

	xobj->sgt = drm_prime_pages_to_sg(xobj->pages, page_count);
	if (IS_ERR(xobj->sgt)) {
		ret = PTR_ERR(xobj->sgt);
		goto out0;
	}

	/* TODO: resolve the cache issue */
	xobj->vmapping = vmap(xobj->pages, page_count, VM_MAP, PAGE_KERNEL);

	if (!xobj->vmapping) {
		ret = -ENOMEM;
		goto out1;
	}

	ret = drm_gem_handle_create(filp, &xobj->base, &args->handle);
	if (ret)
		goto out1;

	xobj->type |= XOCL_BO_USERPTR;
	xocl_describe(xobj);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);
	return ret;

out0:
	drm_free_large(xobj->pages);
	xobj->pages = NULL;
out1:
	xocl_free_bo(&xobj->base);
	DRM_DEBUG("handle creation failed\n");
	return ret;
}


int xocl_map_bo_ioctl(struct drm_device *dev,
		      void *data,
		      struct drm_file *filp)
{
	int ret = 0;
	struct drm_xocl_map_bo *args = data;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;

	obj = xocl_gem_object_lookup(dev, filp, args->handle);
	xobj = to_xocl_bo(obj);

	if (!obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	BO_ENTER("xobj %p", xobj);
	if (xocl_bo_userptr(xobj)) {
		ret = -EPERM;
		goto out;
	}
	/* The mmap offset was set up at BO allocation time. */
	args->offset = drm_vma_node_offset_addr(&obj->vma_node);
	xocl_describe(to_xocl_bo(obj));
out:
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
	return ret;
}

static struct sg_table *alloc_onetime_sg_table(struct page **pages, uint64_t offset, uint64_t size)
{
	int ret;
	unsigned int nr_pages;
	struct sg_table *sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	pages += (offset >> PAGE_SHIFT);
	offset &= (~PAGE_MASK);
	nr_pages = PAGE_ALIGN(size + offset) >> PAGE_SHIFT;

	ret = sg_alloc_table_from_pages(sgt, pages, nr_pages, offset, size, GFP_KERNEL);
	if (ret)
		goto cleanup;
	return sgt;

cleanup:
	kfree(sgt);
	return ERR_PTR(-ENOMEM);
}

int xocl_sync_bo_ioctl(struct drm_device *dev,
		       void *data,
		       struct drm_file *filp)
{
	const struct drm_xocl_bo *xobj;
	struct sg_table *sgt;
	u64 paddr = 0;
	int channel = 0;
	ssize_t ret = 0;
	const struct drm_xocl_sync_bo *args = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;

	u32 dir = (args->dir == DRM_XOCL_SYNC_BO_TO_DEVICE) ? 1 : 0;
	struct drm_gem_object *gem_obj = xocl_gem_object_lookup(dev, filp,
							       args->handle);
	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	xobj = to_xocl_bo(gem_obj);
	BO_ENTER("xobj %p", xobj);
	sgt = xobj->sgt;

	if(xocl_bo_p2p(xobj)){
		DRM_DEBUG("P2P_BO doesn't support sync_bo\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	//Sarab: If it is a remote BO then why do sync over ARE.
	//We should do sync directly using the other device which this bo locally.
	//So that txfer is: HOST->PCIE->DDR; Else it will be HOST->PCIE->ARE->DDR
	paddr = xocl_bo_physical_addr(xobj);

	if (paddr == 0xffffffffffffffffull)
		return -EINVAL;

	/* If device is offline (due to error), reject all DMA requests */
	if (xdev->offline)
		return -ENODEV;


	if ((args->offset + args->size) > gem_obj->size) {
		ret = -EINVAL;
		goto out;
	}

	/* only invalidate the range of addresses requested by the user */
	/*
	if (args->dir == DRM_XOCL_SYNC_BO_TO_DEVICE)
		flush_kernel_vmap_range(kaddr, args->size);
	else if (args->dir == DRM_XOCL_SYNC_BO_FROM_DEVICE)
		invalidate_kernel_vmap_range(kaddr, args->size);
	else {
		ret = -EINVAL;
		goto out;
	}
	*/
	paddr += args->offset;

	if (args->offset || (args->size != xobj->base.size)) {
		sgt = alloc_onetime_sg_table(xobj->pages, args->offset, args->size);
		if (IS_ERR(sgt)) {
			ret = PTR_ERR(sgt);
			goto out;
		}
	}

	//drm_clflush_sg(sgt);
	channel = xocl_acquire_channel(xdev, dir);

	if (channel < 0) {
		ret = -EINVAL;
		goto clear;
	}
	/* Now perform DMA */
	ret = xocl_migrate_bo(xdev, sgt, dir, paddr, channel, args->size);
	if (ret >= 0)
		ret = (ret == args->size) ? 0 : -EIO;
	xocl_release_channel(xdev, dir, channel);
clear:
	if (args->offset || (args->size != xobj->base.size)) {
		sg_free_table(sgt);
		kfree(sgt);
	}
out:
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
	return ret;
}

int xocl_info_bo_ioctl(struct drm_device *dev,
		       void *data,
		       struct drm_file *filp)
{
	const struct drm_xocl_bo *xobj;
	struct drm_xocl_info_bo *args = data;
	struct drm_gem_object *gem_obj = xocl_gem_object_lookup(dev, filp,
								args->handle);

	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	xobj = to_xocl_bo(gem_obj);
	BO_ENTER("xobj %p", xobj);

	args->size = xobj->base.size;

	args->paddr = xocl_bo_physical_addr(xobj);
	xocl_describe(xobj);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return 0;
}

int xocl_pwrite_bo_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *filp)
{
	struct drm_xocl_bo *xobj;
	const struct drm_xocl_pwrite_bo *args = data;
	struct drm_gem_object *gem_obj = xocl_gem_object_lookup(dev, filp,
							       args->handle);
	char __user *user_data = to_user_ptr(args->data_ptr);
	int ret = 0;
	void *kaddr;

	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
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

	if (!XOCL_ACCESS_OK(VERIFY_READ, user_data, args->size)) {
		ret = -EFAULT;
		goto out;
	}

	xobj = to_xocl_bo(gem_obj);
	BO_ENTER("xobj %p", xobj);

	if (xocl_bo_userptr(xobj)) {
		ret = -EPERM;
		goto out;
	}

	kaddr = xobj->vmapping ? xobj->vmapping : xobj->bar_vmapping;
	kaddr += args->offset;

	ret = copy_from_user(kaddr, user_data, args->size);
out:
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return ret;
}

int xocl_pread_bo_ioctl(struct drm_device *dev, void *data,
			struct drm_file *filp)
{
	struct drm_xocl_bo *xobj;
	const struct drm_xocl_pread_bo *args = data;
	struct drm_gem_object *gem_obj = xocl_gem_object_lookup(dev, filp,
							       args->handle);
	char __user *user_data = to_user_ptr(args->data_ptr);
	int ret = 0;
	void *kaddr;

	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	if (xocl_bo_userptr(to_xocl_bo(gem_obj))) {
		ret = -EPERM;
		goto out;
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

	if (!XOCL_ACCESS_OK(VERIFY_WRITE, user_data, args->size)) {
		ret = EFAULT;
		goto out;
	}

	xobj = to_xocl_bo(gem_obj);
	BO_ENTER("xobj %p", xobj);
	kaddr = xobj->vmapping ? xobj->vmapping : xobj->bar_vmapping;
	kaddr += args->offset;

	ret = copy_to_user(user_data, kaddr, args->size);

out:
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return ret;
}

int xocl_copy_import_bo(struct drm_device *dev, struct drm_file *filp,
	struct ert_start_copybo_cmd *cmd)
{
	const struct drm_xocl_bo *dst_xobj, *src_xobj;
	const struct drm_xocl_bo *import_xobj, *local_xobj;
	struct sg_table *sgt = NULL;
	struct sg_table *tmp_sgt = NULL;
	int channel = 0;
	ssize_t ret = 0;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	u32 dir = 0;
	struct drm_gem_object *dst_gem_obj = NULL;
	struct drm_gem_object *src_gem_obj = NULL;
	u64 local_pa = 0;
	u64 local_offset = 0;
	u64 import_offset = 0;
	u64 cp_size = ert_copybo_size(cmd);

	if (cmd->opcode != ERT_START_COPYBO)
		return -EINVAL;

	/* Sanity check against inputs */
	dst_gem_obj = xocl_gem_object_lookup(dev, filp, cmd->dst_bo_hdl);
	if (!dst_gem_obj) {
		DRM_ERROR("invalid destination BO %d\n", cmd->dst_bo_hdl);
		ret = -ENOENT;
		goto out;
	}
	src_gem_obj = xocl_gem_object_lookup(dev, filp, cmd->src_bo_hdl);
	if (!src_gem_obj) {
		DRM_ERROR("invalid source BO %d\n", cmd->src_bo_hdl);
		ret = -ENOENT;
		goto out;
	}
	if (((ert_copybo_src_offset(cmd) + cp_size) > src_gem_obj->size) ||
		((ert_copybo_dst_offset(cmd) + cp_size) > dst_gem_obj->size)) {
		DRM_ERROR("offsize + sizes out of boundary, copy_bo aborted");
		ret = -EINVAL;
		goto out;
	}

	dst_xobj = to_xocl_bo(dst_gem_obj);
	src_xobj = to_xocl_bo(src_gem_obj);
	DRM_DEBUG("dst_xobj %p, src_xobj %p", dst_xobj, src_xobj);
	if(xocl_bo_import(src_xobj) == xocl_bo_import(dst_xobj)){
		DRM_ERROR("invalid src or dst BO type, copy_bo aborted");
		DRM_ERROR("expecting one local and one imported BO");
		ret = -EINVAL;
		goto out;
	}

	/* If device is offline (due to error), reject all DMA requests */
	if (xdev->offline){
		DRM_ERROR("DMA engine is offline, copy_bo aborted");
		ret = -ENODEV;
		goto out;
	}

	if (!xocl_bo_import(src_xobj)) {
		/* src is local */
		local_xobj = src_xobj;
		local_offset = ert_copybo_src_offset(cmd);
		import_xobj = dst_xobj;
		import_offset = ert_copybo_dst_offset(cmd);
		dir = 0;
	} else {
		/* dst is local */
		DRM_ERROR("reading from remote BO, performance degraded");
		local_xobj = dst_xobj;
		local_offset = ert_copybo_dst_offset(cmd);
		import_xobj = src_xobj;
		import_offset = ert_copybo_src_offset(cmd);
		dir = 1;
	}

	local_pa = xocl_bo_physical_addr(local_xobj);
	if (local_pa == INVALID_BO_PADDR) {
		DRM_ERROR("local BO has no dev mem, copy_bo aborted");
		ret = -EINVAL;
		goto out;
	}
	local_pa += local_offset;

	if (import_offset || (cp_size != import_xobj->base.size)) {
		tmp_sgt = alloc_onetime_sg_table(import_xobj->pages,
			import_offset, cp_size);
		if (IS_ERR(tmp_sgt)) {
			DRM_ERROR("failed to alloc tmp sgt, copy_bo aborted");
			ret = PTR_ERR(tmp_sgt);
			goto out;
		}
		sgt = tmp_sgt;
	} else {
		sgt = import_xobj->sgt;
	}

	DRM_DEBUG("sgt=0x%p, dir=%d, pa=0x%llx, size=0x%llx",
		sgt, dir, local_pa, cp_size);

	channel = xocl_acquire_channel(xdev, dir);
	if (channel < 0) {
		DRM_ERROR("DMA channel not available, copy_bo aborted");
		ret = -ENODEV;
		goto out;
	}

	/* Now perform the copy via DMA engine */
	ret = xocl_migrate_bo(xdev, sgt, dir, local_pa, channel, cp_size);
	if (ret >= 0)
		ret = (ret == cp_size) ? 0 : -EIO;
	xocl_release_channel(xdev, dir, channel);

out:
	if (tmp_sgt) {
		sg_free_table(tmp_sgt);
		kfree(tmp_sgt);
	}
	if (src_gem_obj)
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(src_gem_obj);
	if (dst_gem_obj)
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(dst_gem_obj);

	return ret;
}


struct sg_table *xocl_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct drm_xocl_bo *xobj = to_xocl_bo(obj);
	BO_ENTER("xobj %p", xobj);
	return drm_prime_pages_to_sg(xobj->pages, xobj->base.size >> PAGE_SHIFT);
}

struct drm_gem_object *xocl_gem_prime_import_sg_table(struct drm_device *dev,
      struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	int ret = 0;
	struct drm_xocl_bo *importing_xobj;

	importing_xobj = xocl_create_bo(dev, attach->dmabuf->size, 0,
		DRM_XOCL_BO_IMPORT);

	BO_ENTER("xobj %p", importing_xobj);

	if (IS_ERR(importing_xobj)) {
		DRM_DEBUG("object creation failed\n");
		return (struct drm_gem_object *)importing_xobj;
	}

	importing_xobj->sgt = sgt;
	importing_xobj->pages = drm_malloc_ab(attach->dmabuf->size >> PAGE_SHIFT,
		sizeof(*importing_xobj->pages));
	if (!importing_xobj->pages) {
		ret = -ENOMEM;
		goto out_free;
	}
	ret = drm_prime_sg_to_page_addr_arrays(sgt, importing_xobj->pages,
	       NULL, attach->dmabuf->size >> PAGE_SHIFT);
	if (ret)
		goto out_free;

	importing_xobj->vmapping = vmap(importing_xobj->pages,
		importing_xobj->base.size >> PAGE_SHIFT, VM_MAP, PAGE_KERNEL);
	if (!importing_xobj->vmapping) {
		ret = -ENOMEM;
		goto out_free;
	}

	ret = drm_gem_create_mmap_offset(&importing_xobj->base);
	if (ret < 0)
		goto out_free;

	xocl_describe(importing_xobj);
	return &importing_xobj->base;

out_free:
        xocl_free_bo(&importing_xobj->base);
        DRM_ERROR("Buffer import failed\n");
        return ERR_PTR(ret);
}

void *xocl_gem_prime_vmap(struct drm_gem_object *obj)
{
	struct drm_xocl_bo *xobj = to_xocl_bo(obj);
	BO_ENTER("xobj %p", xobj);
	return xobj->vmapping;
}

void xocl_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{

}

int xocl_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct drm_xocl_bo *xobj = to_xocl_bo(obj);
	int ret;

	BO_ENTER("obj %p", obj);
	if (obj->size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	if (!obj->filp)
		return -ENODEV;

	ret = obj->filp->f_op->mmap(obj->filp, vma);
	if (ret)
		return ret;

	fput(vma->vm_file);
	if (!IS_ERR(xobj->dmabuf)) {
		vma->vm_file = get_file(xobj->dmabuf->file);
		vma->vm_ops = xobj->dmabuf_vm_ops;
		vma->vm_private_data = obj;
		vma->vm_flags |= VM_MIXEDMAP;
	}

	return 0;
}

int xocl_init_unmgd(struct drm_xocl_unmgd *unmgd, uint64_t data_ptr,
	uint64_t size, u32 write)
{
	int ret;
	char __user *user_data = to_user_ptr(data_ptr);

	if (!XOCL_ACCESS_OK((write == 1) ? VERIFY_READ : VERIFY_WRITE, user_data, size))
		return -EFAULT;

	memset(unmgd, 0, sizeof(struct drm_xocl_unmgd));

	unmgd->npages = (((unsigned long)user_data + size + PAGE_SIZE - 1) -
			((unsigned long)user_data & PAGE_MASK)) >> PAGE_SHIFT;

	unmgd->pages = drm_malloc_ab(unmgd->npages, sizeof(*unmgd->pages));
	if (!unmgd->pages)
		return -ENOMEM;

	ret = get_user_pages_fast(data_ptr, unmgd->npages, (write == 0) ? 1 : 0, unmgd->pages);

	if (ret != unmgd->npages)
		goto clear_pages;

	unmgd->sgt = alloc_onetime_sg_table(unmgd->pages, data_ptr & ~PAGE_MASK, size);
	if (IS_ERR(unmgd->sgt)) {
		ret = PTR_ERR(unmgd->sgt);
		goto clear_release;
	}

	return 0;

clear_release:
	xocl_release_pages(unmgd->pages, unmgd->npages, 0);
clear_pages:
	drm_free_large(unmgd->pages);
	unmgd->pages = NULL;
	return ret;
}

void xocl_finish_unmgd(struct drm_xocl_unmgd *unmgd)
{
	if (!unmgd->pages)
		return;
	sg_free_table(unmgd->sgt);
	kfree(unmgd->sgt);
	xocl_release_pages(unmgd->pages, unmgd->npages, 0);
	drm_free_large(unmgd->pages);
	unmgd->pages = NULL;
}

static bool xocl_validate_paddr(struct xocl_dev *xdev, u64 paddr, u64 size)
{
	struct mem_data *mem_data;
	int	i;
	uint64_t addr;
	bool start_check = false;
	bool end_check = false;

	for (i = 0; i < XOCL_MEM_TOPOLOGY(xdev)->m_count; i++) {
		mem_data = &XOCL_MEM_TOPOLOGY(xdev)->m_mem_data[i];
		addr = mem_data->m_base_address;
		start_check = (paddr >= addr);
		end_check = (paddr + size <= addr + mem_data->m_size * 1024);
		if (mem_data->m_used && start_check && end_check)
			return true;
	}

	return false;
}

int xocl_pwrite_unmgd_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp)
{
	int channel;
	struct drm_xocl_unmgd unmgd;
	const struct drm_xocl_pwrite_unmgd *args = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	u32 dir = 1;
	ssize_t ret = 0;

	if (args->address_space != 0) {
		userpf_err(xdev, "invalid addr space");
		return -EFAULT;
	}

	if (args->size == 0)
		return 0;

	if (!xocl_validate_paddr(xdev, args->paddr, args->size)) {
		userpf_err(xdev, "invalid paddr: 0x%llx, size:0x%llx",
			args->paddr, args->size);
		/* currently we are not able to return error because
		 * it is unclear that what addresses are valid other than
		 * ddr area. we should revisit this sometime.
		 * return -EINVAL;
		 */
	}

	ret = xocl_init_unmgd(&unmgd, args->data_ptr, args->size, dir);
	if (ret) {
		userpf_err(xdev, "init unmgd failed %ld", ret);
		return ret;
	}

	channel = xocl_acquire_channel(xdev, dir);
	if (channel < 0) {
		userpf_err(xdev, "acquire channel failed");
		ret = -EINVAL;
		goto clear;
	}
	/* Now perform DMA */
	ret = xocl_migrate_bo(xdev, unmgd.sgt, dir, args->paddr, channel,
		args->size);
	if (ret >= 0)
		ret = (ret == args->size) ? 0 : -EIO;
	xocl_release_channel(xdev, dir, channel);
clear:
	xocl_finish_unmgd(&unmgd);
	return ret;
}

int xocl_pread_unmgd_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *filp)
{
	int channel;
	struct drm_xocl_unmgd unmgd;
	const struct drm_xocl_pwrite_unmgd *args = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	u32 dir = 0;  /* read */
	ssize_t ret = 0;

	if (args->address_space != 0) {
		userpf_err(xdev, "invalid addr space");
		return -EFAULT;
	}

	if (args->size == 0)
		return 0;

	if (!xocl_validate_paddr(xdev, args->paddr, args->size)) {
		userpf_err(xdev, "invalid paddr: 0x%llx, size:0x%llx",
			args->paddr, args->size);
		/* currently we are not able to return error because
		 * it is unclear that what addresses are valid other than
		 * ddr area. we should revisit this sometime.
		 * return -EINVAL;
		 */
	}

	ret = xocl_init_unmgd(&unmgd, args->data_ptr, args->size, dir);
	if (ret) {
		userpf_err(xdev, "init unmgd failed %ld", ret);
		return ret;
	}

	channel = xocl_acquire_channel(xdev, dir);

	if (channel < 0) {
		userpf_err(xdev, "acquire channel failed");
		ret = -EINVAL;
		goto clear;
	}
	/* Now perform DMA */
	ret = xocl_migrate_bo(xdev, unmgd.sgt, dir, args->paddr, channel,
		args->size);
	if (ret >= 0)
		ret = (ret == args->size) ? 0 : -EIO;

	xocl_release_channel(xdev, dir, channel);
clear:
	xocl_finish_unmgd(&unmgd);
	return ret;
}

int xocl_usage_stat_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	struct drm_xocl_usage_stat *args = data;
	int	i;

	args->mm_channel_count = XOCL_DDR_COUNT(xdev);
	if (args->mm_channel_count > 8)
		args->mm_channel_count = 8;
	for (i = 0; i < args->mm_channel_count; i++) {
		xocl_mm_get_usage_stat(drm_p, i, args->mm + i);
	}

	args->dma_channel_count = xocl_get_chan_count(xdev);
	if (args->dma_channel_count > 8)
		args->dma_channel_count = 8;

	for (i = 0; i < args->dma_channel_count; i++) {
		args->h2c[i] = xocl_get_chan_stat(xdev, i, 1);
		args->c2h[i] = xocl_get_chan_stat(xdev, i, 0);
	}

	return 0;
}
