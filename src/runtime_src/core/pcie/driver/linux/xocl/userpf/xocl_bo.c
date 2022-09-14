/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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
#include "common.h"

#ifdef _XOCL_BO_DEBUG
#define	BO_ENTER(fmt, args...)		\
	printk(KERN_INFO "[BO] Entering %s:"fmt"\n", __func__, ##args)
#define	BO_DEBUG(fmt, args...)		\
	printk(KERN_INFO "[BO] %s:%d:"fmt"\n", __func__, __LINE__, ##args)
#else
#define BO_ENTER(fmt, args...)
#define	BO_DEBUG(fmt, args...)
#endif

#define	INVALID_BO_PADDR	0xffffffffffffffffull

static struct sg_table *alloc_onetime_sg_table(struct page **pages, uint64_t offset, uint64_t size);

#if defined(XOCL_DRM_FREE_MALLOC)
static inline void drm_free_large(void *ptr)
{
	kvfree(ptr);
}

static inline void *drm_malloc_ab(size_t nmemb, size_t size)
{
	return kvmalloc_array(nmemb, size, GFP_KERNEL);
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
	uint64_t paddr;

	paddr = xobj->mm_node ? xobj->mm_node->start : INVALID_BO_PADDR;
	return paddr;
}

void xocl_describe(const struct drm_xocl_bo *xobj)
{
	size_t size_kb = xobj->base.size / 1024;
	size_t physical_addr = xocl_bo_physical_addr(xobj);
	unsigned ddr = xobj->mem_idx;
	unsigned userptr = xocl_bo_userptr(xobj) ? 1 : 0;

	DRM_DEBUG("%p: VA:%p BAR:0x%llx EA:0x%zx SZ:0x%zxKB", xobj,
		xobj->vmapping, xobj->p2p_bar_offset, physical_addr, size_kb);
	DRM_DEBUG("%p: IDX:%u UPTR:%u SGL:%u FLG:%x", xobj, ddr, userptr,
		xobj->sgt ? xobj->sgt->orig_nents : 0, xobj->flags);
}

void xocl_bo_get_usage_stat(struct xocl_drm *drm_p, u32 bo_idx,
	struct drm_xocl_mm_stat *pstat)
{
	if (!drm_p->bo_usage_stat)
		return;
	if (bo_idx >= XOCL_BO_USAGE_TOTAL)
		return;

	pstat->memory_usage = drm_p->bo_usage_stat[bo_idx].memory_usage;
	pstat->bo_count = drm_p->bo_usage_stat[bo_idx].bo_count;
}

static int xocl_bo_update_usage_stat(struct xocl_drm *drm_p, unsigned bo_flag,
	u64 size, int count)
{
	int idx = -1;

	if (!drm_p->bo_usage_stat)
		return -EINVAL;

	switch (bo_flag) {
	case XOCL_BO_NORMAL:
		idx = XOCL_BO_USAGE_NORMAL;
		break;
	case XOCL_BO_USERPTR:
		idx = XOCL_BO_USAGE_USERPTR;
		break;
	case XOCL_BO_P2P:
		idx = XOCL_BO_USAGE_P2P;
		break;
	case XOCL_BO_DEV_ONLY:
		idx = XOCL_BO_USAGE_DEV_ONLY;
		break;
	case XOCL_BO_IMPORT:
		idx = XOCL_BO_USAGE_IMPORT;
		break;
	case XOCL_BO_EXECBUF:
		idx = XOCL_BO_USAGE_EXECBUF;
		break;
	case XOCL_BO_CMA:
		idx = XOCL_BO_USAGE_CMA;
		break;
	default:
		idx = -1;
		break;
	}
	if (idx < 0)
		return -EINVAL;

	drm_p->bo_usage_stat[idx].memory_usage += (count > 0) ? size : -size;
	drm_p->bo_usage_stat[idx].bo_count += count;
	return 0;
}

static void xocl_free_mm_node(struct drm_xocl_bo *xobj)
{
	struct drm_device *ddev = xobj->base.dev;
	struct xocl_drm *drm_p = ddev->dev_private;
	unsigned ddr = xobj->mem_idx;

	mutex_lock(&drm_p->mm_lock);
	BO_ENTER("xobj %p, mm_node %p", xobj, xobj->mm_node);
	if (!xobj->mm_node)
		goto end;

	xocl_mm_update_usage_stat(drm_p, ddr, xobj->base.size, -1);
	xocl_bo_update_usage_stat(drm_p, xobj->flags, xobj->base.size, -1);
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
	struct xocl_dev *xdev = drm_p->xdev;
	int npages = obj->size >> PAGE_SHIFT;

	DRM_DEBUG("Freeing BO %p\n", xobj);

	BO_ENTER("xobj %p pages %p", xobj, xobj->pages);

	if (xocl_bo_p2p(xobj)) {
		xocl_p2p_mem_unmap(xdev, xobj->p2p_bar_offset,
				obj->size);
	}

	if (xobj->vmapping)
		vunmap(xobj->vmapping);
	xobj->vmapping = NULL;

	if (xobj->dmabuf) {
		unmap_mapping_range(xobj->dmabuf->file->f_mapping, 0, 0, 1);
	}

	if (xobj->dma_nsg) {
		dma_unmap_sg(&xdev->core.pdev->dev, xobj->sgt->sgl,
			     xobj->dma_nsg, DMA_BIDIRECTIONAL);
	}

	if (xobj->pages) {
		if (xocl_bo_userptr(xobj)) {
			xocl_release_pages(xobj->pages, npages, 0);
			drm_free_large(xobj->pages);
		} else if (xocl_bo_p2p(xobj) || xocl_bo_import(xobj) || xocl_bo_cma(xobj)) {
			drm_free_large(xobj->pages);
		} else if ((xobj->flags & XOCL_KERN_BUF) || (xobj->flags & XOCL_SGL)) {
			drm_free_large(xobj->pages);
		} else {
			drm_gem_put_pages(obj, xobj->pages, false, false);
		}
	}
	xobj->pages = NULL;

	if (xobj->flags & XOCL_SGL) {
		DRM_DEBUG("Freeing kernel buffer\n");
		kfree(xobj->sgt);

		xobj->sgt = NULL;
		xocl_free_mm_node(xobj);
	} else if (!xocl_bo_import(xobj)) {
		DRM_DEBUG("Freeing regular buffer\n");
		if (xobj->sgt) {
			sg_free_table(xobj->sgt);
			kfree(xobj->sgt);
		}
		xobj->sgt = NULL;
		xocl_free_mm_node(xobj);
	} else {
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
	unsigned flags, unsigned type)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	u16 ddr_count;
	unsigned ddr;
	struct mem_topology *topo = NULL;
	int err = 0;

	if (type == XOCL_BO_EXECBUF || type == XOCL_BO_IMPORT ||
	    type == XOCL_BO_CMA)
		return 0;
	//From "mem_topology" or "feature rom" depending on
	//unified or non-unified dsa
	ddr_count = XOCL_DDR_COUNT(xdev);

	if (ddr_count == 0)
		return -EINVAL;

	ddr = xocl_bo_ddr_idx(flags);
	if (ddr >= ddr_count)
		return -EINVAL;
	err = XOCL_GET_GROUP_TOPOLOGY(xdev, topo);
	if (err)
		return err;

	if (topo) {
		if (XOCL_IS_STREAM(topo, ddr)) {
			userpf_err(xdev, "Bank %d is Stream", ddr);
			err = -EINVAL;
			goto done;
		}
		if (!XOCL_IS_DDR_USED(topo, ddr)) {
			userpf_err(xdev,
				"Bank %d is marked as unused in axlf", ddr);
			err = -EINVAL;
			goto done;
		}
	}
done:
	XOCL_PUT_GROUP_TOPOLOGY(xdev);
	return err;
}

static struct page **xocl_cma_collect_pages(struct xocl_drm *drm_p, uint64_t base_addr, uint64_t start, uint64_t size)
{
	struct xocl_dev *xdev = drm_p->xdev;
	uint64_t entry_sz = 0;
	uint64_t chunk_offset, page_copied = 0, page_offset_start, page_offset_end;
	int64_t addr_offset = 0;
	struct page **pages = NULL;
	uint64_t pages_per_chunk = 0;

	BUG_ON(!start || !size);
	BUG_ON(base_addr > start);

	if (!xdev || !xdev->cma_bank)
		return ERR_PTR(-EINVAL);

	entry_sz = xdev->cma_bank->entry_sz;
	pages_per_chunk = entry_sz >> PAGE_SHIFT;

	addr_offset = start - base_addr;

	if (addr_offset < 0)
		return ERR_PTR(-EINVAL);

	page_offset_start = addr_offset >> PAGE_SHIFT;
	page_offset_end = (addr_offset + size) >> PAGE_SHIFT;

	pages = vzalloc((size >> PAGE_SHIFT) * sizeof(struct page*));


	while (page_offset_start < page_offset_end) {
		uint64_t nr = min(page_offset_end - page_offset_start, (pages_per_chunk - page_offset_start % pages_per_chunk));

		chunk_offset = page_offset_start / pages_per_chunk;
		if (chunk_offset >= xdev->cma_bank->entry_num)
			return ERR_PTR(-ENOMEM);

		DRM_DEBUG("chunk_offset %lld start 0x%llx, end 0x%llx\n", chunk_offset, page_offset_start, page_offset_end);

		memcpy(pages+page_copied, xdev->cma_bank->cma_mem[chunk_offset].pages+(page_offset_start%pages_per_chunk), nr*sizeof(struct page*));
		page_offset_start += nr;
		page_copied += nr;
	}

	if (page_copied != size >> PAGE_SHIFT)
		return ERR_PTR(-ENOMEM);


	return pages;
}

static struct drm_xocl_bo *xocl_create_bo(struct drm_device *dev,
					  uint64_t unaligned_size,
					  unsigned user_flags,
					  unsigned bo_type)
{
	size_t size = PAGE_ALIGN(unaligned_size);
	struct drm_xocl_bo *xobj;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	struct drm_gem_object *obj;
	unsigned memidx = xocl_bo_ddr_idx(user_flags);
	bool xobj_inited = false;
	int err = 0;

	BO_DEBUG("New create bo flags:%x, type %x", user_flags, bo_type);
	if (!size)
		return ERR_PTR(-EINVAL);

	/* Either none or only one DDR should be specified */
	/* Check the bo_type */
	if (check_bo_user_reqs(dev, user_flags, bo_type))
		return ERR_PTR(-EINVAL);

	xobj = kzalloc(sizeof(*xobj), GFP_KERNEL);
	if (!xobj)
		return ERR_PTR(-ENOMEM);

	BO_ENTER("xobj %p", xobj);

	xobj->user_flags = user_flags;
	xobj->flags = bo_type;
	mutex_lock(&drm_p->mm_lock);
	/* Assume there is only 1 HOST bank. We ignore the  memidx
	 * for host bank. This is required for supporting No flag
	 * BO on NoDMA platform. We may remove this logic if there is
	 * more than 1 HOST bank in the future.
	 */
	if (xobj->flags & XOCL_CMA_MEM) {
		if (drm_p->cma_bank_idx < 0) {
			err = -EINVAL;
			goto failed;
		}
		memidx = drm_p->cma_bank_idx;
	}

	if (memidx == drm_p->cma_bank_idx) {
		if (xobj->flags &
		    (XOCL_USER_MEM | XOCL_DRM_IMPORT | XOCL_P2P_MEM)) {
			err = -EINVAL;
			xocl_xdev_err(xdev, "invalid HOST BO req. flag %x",
				xobj->flags);
			goto failed;
		}
		xobj->flags = XOCL_BO_CMA;
	}

	if (xobj->flags == XOCL_BO_EXECBUF)
		xobj->metadata.state = DRM_XOCL_EXECBUF_STATE_ABORT;

	obj = &xobj->base;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0) || defined(RHEL_8_5_GE)
	obj->funcs = &xocl_gem_object_funcs;
#endif

	if (xobj->flags & XOCL_DRM_SHMEM) {
		err = drm_gem_object_init(dev, obj, size);
		if (err)
			goto failed;
	} else
		drm_gem_private_object_init(dev, obj, size);

	xobj_inited = true;

	if (!(xobj->flags & XOCL_DEVICE_MEM) && !(xobj->flags & XOCL_CMA_MEM))
		goto done;

	/* Let's reserve some device memory */
	xobj->mm_node = kzalloc(sizeof(*xobj->mm_node), GFP_KERNEL);
	if (!xobj->mm_node) {
		err = -ENOMEM;
		goto failed;
	}

	/* Attempt to allocate buffer on the requested DDR */
	xocl_xdev_dbg(xdev, "alloc bo from bank%u, flag %x, host bank %d",
		memidx, xobj->flags, drm_p->cma_bank_idx);

	err = xocl_mm_insert_node(drm_p, memidx, xobj->mm_node,
		xobj->base.size);
	if (err)
		goto failed;

	BO_DEBUG("insert mm_node:%p, start:%llx size: %llx",
		xobj->mm_node, xobj->mm_node->start,
		xobj->mm_node->size);
	xocl_mm_update_usage_stat(drm_p, memidx, xobj->base.size, 1);
	xocl_bo_update_usage_stat(drm_p, xobj->flags, xobj->base.size, 1);
	/* Record the DDR we allocated the buffer on */
	xobj->mem_idx = memidx;

done:
	mutex_unlock(&drm_p->mm_lock);

	return xobj;
failed:
	mutex_unlock(&drm_p->mm_lock);

	if (xobj->mm_node)
		kfree(xobj->mm_node);

	if (xobj_inited)
		drm_gem_object_release(&xobj->base);
	kfree(xobj);
	return ERR_PTR(err);
}

struct drm_xocl_bo *xocl_drm_create_bo(struct xocl_drm *drm_p,
					  uint64_t unaligned_size,
					  unsigned user_flags)
{
	unsigned bo_type = xocl_bo_type(user_flags);

	return xocl_create_bo(drm_p->ddev, unaligned_size, user_flags, bo_type);
}

static struct page **xocl_p2p_get_pages(struct xocl_dev *xdev,
	u64 bar_off, u64 size)
{
	struct page *p, **pages;
	int ret;
	uint64_t npages = size >> PAGE_SHIFT;

	pages = drm_malloc_ab(npages, sizeof(struct page *));
	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	ret = xocl_p2p_mem_get_pages(xdev, (ulong)bar_off, (ulong)size,
			pages, npages);
	if (ret) {
		p = ERR_PTR(ret);
		goto fail;
	}

	return pages;
fail:
	kvfree(pages);
	return ERR_CAST(p);
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

struct drm_xocl_bo *
__xocl_create_bo_ioctl(struct drm_device *dev,
		       struct drm_xocl_create_bo *args)
{
	struct drm_xocl_bo *xobj;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	unsigned bo_type = xocl_bo_type(args->flags);
	struct mem_topology *topo = NULL;
	unsigned ddr = 0;
	int ret;

	xobj = xocl_create_bo(dev, args->size, args->flags, bo_type);
	if (IS_ERR(xobj)) {
		DRM_ERROR("object creation failed idx %d, size 0x%llx\n",
			xocl_bo_ddr_idx(args->flags), args->size);
		return xobj;
	}
	BO_ENTER("xobj %p, mm_node %p", xobj, xobj->mm_node);

	ddr = (xobj->flags & XOCL_CMA_MEM) ? drm_p->cma_bank_idx :
		xocl_bo_ddr_idx(args->flags);

	if (xobj->flags == XOCL_BO_P2P) {
		/*
		 * DRM allocate contiguous pages, shift the vmapping with
		 * bar address offset
		 */
		ret = XOCL_GET_GROUP_TOPOLOGY(xdev, topo);
		if (ret)
			goto out_free;

		if (topo) {
			int ret;
			ulong bar_off;

			ret = xocl_p2p_mem_map(xdev,
				topo->m_mem_data[ddr].m_base_address,
				topo->m_mem_data[ddr].m_size * 1024,
				xobj->mm_node->start -
				topo->m_mem_data[ddr].m_base_address,
				xobj->base.size,
				&bar_off);
			if (ret) {
				xocl_xdev_err(xdev, "map P2P failed,ret = %d",
						ret);
			} else
				xobj->p2p_bar_offset = bar_off;
		}

		XOCL_PUT_GROUP_TOPOLOGY(xdev);
	}

	if (xobj->flags & XOCL_PAGE_ALLOC) {
		if (xobj->flags & XOCL_P2P_MEM)
			xobj->pages = xocl_p2p_get_pages(xdev,
				xobj->p2p_bar_offset, xobj->base.size);
		else if (xobj->flags & XOCL_DRM_SHMEM)
			xobj->pages = drm_gem_get_pages(&xobj->base);
		else if (xobj->flags & XOCL_CMA_MEM) {
			uint64_t start_addr;

			ret = XOCL_GET_GROUP_TOPOLOGY(xdev, topo);
			if (ret)
				goto out_free;
			start_addr = topo->m_mem_data[ddr].m_base_address;
			xobj->pages = xocl_cma_collect_pages(drm_p, start_addr, xobj->mm_node->start, xobj->base.size);
			XOCL_PUT_GROUP_TOPOLOGY(xdev);
		}

		if (IS_ERR(xobj->pages)) {
			ret = PTR_ERR(xobj->pages);
			xobj->pages = NULL;
			goto out_free;
		}
		xobj->sgt = alloc_onetime_sg_table(xobj->pages, 0,
			xobj->base.size);
		if (IS_ERR(xobj->sgt)) {
			ret = PTR_ERR(xobj->sgt);
			xobj->sgt = NULL;
			goto out_free;
		}

		if (xobj->flags & XOCL_HOST_MEM) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
			if (xobj->base.size >= GB(4)) {
				DRM_ERROR("cannot support BO size >= 4G\n");
				DRM_ERROR("limited by Linux kernel API\n");
				ret = -EINVAL;
				goto out_free;
			}
#endif
			if (!(xobj->flags & XOCL_CMA_MEM)) {
				xobj->vmapping = vmap(xobj->pages,
					xobj->base.size >> PAGE_SHIFT,
					VM_MAP, PAGE_KERNEL);
				if (!xobj->vmapping) {
					ret = -ENOMEM;
					goto out_free;
				}
			}
		}
	}
	return xobj;

out_free:
	xocl_free_bo(&xobj->base);
	return ERR_PTR(ret);
}

int xocl_create_bo_ioctl(struct drm_device *dev,
			 void *data,
			 struct drm_file *filp)
{
	int ret;
	struct drm_xocl_bo *xobj;
	struct drm_xocl_create_bo *args = data;

	xobj = __xocl_create_bo_ioctl(dev, data);
	if (IS_ERR(xobj))
		return PTR_ERR(xobj);

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

int xocl_userptr_bo_ioctl(
	struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret;
	struct drm_xocl_bo *xobj;
	uint64_t page_count = 0;
	uint64_t page_pinned = 0;
	struct drm_xocl_userptr_bo *args = data;
	unsigned user_flags = args->flags;
	int write = 1;

	if (offset_in_page(args->addr))
		return -EINVAL;

	xobj = xocl_create_bo(dev, args->size, user_flags, XOCL_BO_USERPTR);
	BO_ENTER("xobj %p", xobj);

	if (IS_ERR(xobj)) {
		DRM_ERROR("object creation failed user_flags %d, size 0x%llx\n", user_flags, args->size);
		return PTR_ERR(xobj);
	}

	/* Use the page rounded size to accurately account for num of pages */
	page_count = xobj->base.size >> PAGE_SHIFT;

	xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
	if (!xobj->pages) {
		ret = -ENOMEM;
		goto out1;
	}

	ret = XOCL_ACCESS_OK(VERIFY_WRITE, args->addr, args->size);


	if (!ret) {
		ret = XOCL_ACCESS_OK(VERIFY_READ, args->addr, args->size);
		if (!ret)
			goto out0;
		else
			write = 0;
	}

	while (page_pinned < page_count) {
		/*
		 * We pin at most 1G at a time to workaround
		 * a Linux kernel issue inside get_user_pages_fast().
		 */
		u64 nr = min(page_count - page_pinned,
			(1024ULL * 1024 * 1024) / (1ULL << PAGE_SHIFT));
		if (get_user_pages_fast(
			args->addr + (page_pinned << PAGE_SHIFT),
			nr, write, xobj->pages + page_pinned) != nr) {
			ret = -ENOMEM;
			goto out0;
		}
		page_pinned += nr;
	}

	xobj->sgt = alloc_onetime_sg_table(xobj->pages, 0,
		page_count << PAGE_SHIFT);
	if (IS_ERR(xobj->sgt)) {
		ret = PTR_ERR(xobj->sgt);
		xobj->sgt = NULL;
		goto out0;
	}

	/* TODO: resolve the cache issue */
	xobj->vmapping = vmap(xobj->pages, page_count, VM_MAP, PAGE_KERNEL);

	if (!xobj->vmapping) {
		ret = -ENOMEM;
		goto out1;
	}

	ret = drm_gem_create_mmap_offset(&xobj->base);
	if (ret < 0)
		goto out1;

	ret = drm_gem_handle_create(filp, &xobj->base, &args->handle);
	if (ret)
		goto out1;

	xocl_describe(xobj);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);
	return ret;

out0:
	if (page_pinned)
		xocl_release_pages(xobj->pages, page_pinned, 0);
	drm_free_large(xobj->pages);
	xobj->pages = NULL;
out1:
	xocl_free_bo(&xobj->base);
	DRM_ERROR("handle creation failed\n");
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
	/* The mmap offset was set up at BO allocation time. */
	args->offset = drm_vma_node_offset_addr(&obj->vma_node);
	xocl_describe(to_xocl_bo(obj));
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
	return ret;
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
	struct scatterlist *sg;

	u32 dir = (args->dir == DRM_XOCL_SYNC_BO_TO_DEVICE) ? 1 : 0;
	struct drm_gem_object *gem_obj = xocl_gem_object_lookup(dev, filp,
							       args->handle);
	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	xobj = to_xocl_bo(gem_obj);
	BO_ENTER("xobj %p", xobj);

	if (!xocl_bo_sync_able(xobj->flags)) {
		DRM_ERROR("BO %d doesn't support sync_bo\n", args->handle);
		ret = -EOPNOTSUPP;
		goto out;
	}

	sgt = xobj->sgt;
	sg = sgt->sgl;

	if (xocl_bo_cma(xobj) || xocl_bo_p2p(xobj)) {
		if (dir) {
			dma_sync_single_for_device(&(XDEV(xdev)->pdev->dev), sg_phys(sg),
				sg->length, DMA_TO_DEVICE);
		} else {
			dma_sync_single_for_cpu(&(XDEV(xdev)->pdev->dev), sg_phys(sg),
				sg->length, DMA_FROM_DEVICE);
		}
		goto out;
	}

	//Sarab: If it is a remote BO then why do sync over ARE.
	//We should do sync directly using the other device which this bo locally.
	//So that txfer is: HOST->PCIE->DDR; Else it will be HOST->PCIE->ARE->DDR
	paddr = xocl_bo_physical_addr(xobj);
	if (paddr == 0xffffffffffffffffull) {
		DRM_ERROR("BO %d physical address is invalid.\n", args->handle);
		return -EINVAL;
	}

	if ((args->offset + args->size) > gem_obj->size) {
		DRM_ERROR("BO %d request is out of range.\n", args->handle);
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
			DRM_ERROR("BO %d request err: %ld.\n", args->handle, ret);
			goto out;
		}
	}

	//drm_clflush_sg(sgt);
	channel = xocl_acquire_channel(xdev, dir);
	if (channel < 0) {
		DRM_ERROR("BO %d request cannot find channel.\n", args->handle);
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
	args->flags = xobj->user_flags;

	args->paddr = xocl_bo_physical_addr(xobj);
	xocl_describe(xobj);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return 0;
}

static int xocl_migrate_unmgd(struct xocl_dev *xdev, uint64_t data_ptr, uint64_t paddr, size_t size, bool dir)
{
	int channel;
	struct drm_xocl_unmgd unmgd;
	int ret;
	ssize_t migrated;

	ret = xocl_init_unmgd(&unmgd, data_ptr, size, dir);
	if (ret) {
		userpf_err(xdev, "init unmgd failed %d", ret);
		return ret;
	}

	channel = xocl_acquire_channel(xdev, dir);

	if (channel < 0) {
		userpf_err(xdev, "acquire channel failed");
		ret = -EINVAL;
		goto clear;
	}
	/* Now perform DMA */
	migrated = xocl_migrate_bo(xdev, unmgd.sgt, dir, paddr, channel,
		size);
	if (migrated >= 0)
		ret = (migrated == size) ? 0 : -EIO;

	xocl_release_channel(xdev, dir, channel);
clear:
	xocl_finish_unmgd(&unmgd);
	return ret;
}

int xocl_pwrite_bo_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *filp)
{
	struct drm_xocl_bo *xobj;
	const struct drm_xocl_pwrite_bo *args = data;
	struct drm_gem_object *gem_obj = xocl_gem_object_lookup(dev, filp,
							       args->handle);
	char __user *user_data = to_user_ptr(args->data_ptr);
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int ret = 0;
	char *kaddr;
	uint64_t ep_addr;

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
	if (!xobj->vmapping) {
		ep_addr = xocl_bo_physical_addr(xobj);
		if (ep_addr == INVALID_BO_PADDR) {
			ret = -EINVAL;
			goto out;
		}
		ret = xocl_migrate_unmgd(xdev, args->data_ptr, ep_addr + args->offset,
			args->size, 1);
	} else {
		kaddr = xobj->vmapping;
		kaddr += args->offset;

		ret = copy_from_user(kaddr, user_data, args->size);
	}
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
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int ret = 0;
	char *kaddr;
	uint64_t ep_addr;

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

	if (!xobj->vmapping) {
		ep_addr = xocl_bo_physical_addr(xobj);
		if (ep_addr == INVALID_BO_PADDR) {
			ret = -EINVAL;
			goto out;
		}
		ret = xocl_migrate_unmgd(xdev, args->data_ptr, ep_addr + args->offset,
			args->size, 0);

	} else {
		kaddr = xobj->vmapping;
		kaddr += args->offset;
		ret = copy_to_user(user_data, kaddr, args->size);
	}

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
	if (xocl_bo_import(src_xobj) == xocl_bo_import(dst_xobj)) {
		DRM_ERROR("invalid src or dst BO type, copy_bo aborted");
		DRM_ERROR("expecting one local and one imported BO");
		ret = -EINVAL;
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
		/*
		 * dst is local
		 * reading from remote BO, performance degraded
		 */
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

	if (!xobj->pages)
		return ERR_PTR(-EINVAL);

	return alloc_onetime_sg_table(xobj->pages, 0, xobj->base.size);
}

struct drm_gem_object *xocl_gem_prime_import_sg_table(struct drm_device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	int ret = 0;
	struct drm_xocl_bo *importing_xobj;

	importing_xobj = xocl_create_bo(dev, attach->dmabuf->size, 0, XOCL_BO_IMPORT);

	BO_ENTER("xobj %p", importing_xobj);

	if (IS_ERR(importing_xobj)) {
		DRM_ERROR("object creation failed\n");
		return (struct drm_gem_object *)importing_xobj;
	}

	importing_xobj->sgt = sgt;
	importing_xobj->pages = drm_malloc_ab(attach->dmabuf->size >> PAGE_SHIFT,
		sizeof(*importing_xobj->pages));
	if (!importing_xobj->pages) {
		ret = -ENOMEM;
		goto out_free;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0) || defined(RHEL_8_5_GE)
	ret = drm_prime_sg_to_page_array(sgt, importing_xobj->pages,
			attach->dmabuf->size >> PAGE_SHIFT);
#else
	ret = drm_prime_sg_to_page_addr_arrays(sgt, importing_xobj->pages,
			NULL, attach->dmabuf->size >> PAGE_SHIFT);
#endif
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0) && !defined(RHEL_8_5_GE)
void *xocl_gem_prime_vmap(struct drm_gem_object *obj)
{
	struct drm_xocl_bo *xobj = to_xocl_bo(obj);

	BO_ENTER("xobj %p", xobj);
	return xobj->vmapping;
}

void xocl_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{

}
#else
int xocl_gem_prime_vmap(struct drm_gem_object *obj, struct XOCL_MAP_TYPE *map)
{
        struct drm_xocl_bo *xobj = to_xocl_bo(obj);

        BO_ENTER("xobj %p", xobj);
        XOCL_MAP_SET_VADDR(map, xobj->vmapping);

        return 0;
}

void xocl_gem_prime_vunmap(struct drm_gem_object *obj, struct XOCL_MAP_TYPE *map)
{

}
#endif


int xocl_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct drm_xocl_bo *xobj = to_xocl_bo(obj);
	int ret;

	BO_ENTER("obj %p", obj);
	if (obj->size < vma->vm_end - vma->vm_start)
		return -EINVAL;

	if (!obj->filp)
		return -ENODEV;

	/* Add the fake offset */
	vma->vm_pgoff += drm_vma_node_start(&obj->vma_node);

	ret = obj->filp->f_op->mmap(obj->filp, vma);
	if (ret)
		return ret;
	XOCL_DRM_GEM_OBJECT_GET(obj);

	fput(vma->vm_file);
	if(!IS_ERR_OR_NULL(xobj->dmabuf) && !IS_ERR_OR_NULL(xobj->dmabuf->file)) {
		vma->vm_file = get_file(xobj->dmabuf->file);
		vma->vm_ops = xobj->dmabuf_vm_ops;
	} else if (!IS_ERR_OR_NULL(xobj->base.dma_buf) && !IS_ERR_OR_NULL(xobj->base.dma_buf->file)) {
		vma->vm_file = get_file(xobj->base.dma_buf->file);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0) || defined(RHEL_8_5_GE)
		vma->vm_ops = xobj->base.funcs->vm_ops;
#else
		vma->vm_ops = xobj->base.dev->driver->gem_vm_ops;
#endif
	}

	vma->vm_private_data = obj;
	vma->vm_flags |= VM_MIXEDMAP;

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

#if 0
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
#endif

int xocl_pwrite_unmgd_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp)
{
	const struct drm_xocl_pwrite_unmgd *args = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int ret = 0;

	if (args->address_space != 0) {
		userpf_err(xdev, "invalid addr space");
		return -EFAULT;
	}

	if (args->size == 0)
		return 0;

	/* currently we are not able to return error because
	 * it is unclear that what addresses are valid other than
	 * ddr area. we should revisit this sometime.
	 * if (!xocl_validate_paddr(xdev, args->paddr, args->size)) {
	 *	userpf_err(xdev, "invalid paddr: 0x%llx, size:0x%llx",
	 *		args->paddr, args->size);
	 *	return -EINVAL;
	 * }
	 */


	ret = xocl_migrate_unmgd(xdev, args->data_ptr, args->paddr, args->size, 1);

	return ret;
}

int xocl_pread_unmgd_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *filp)
{
	const struct drm_xocl_pwrite_unmgd *args = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int ret = 0;

	if (args->address_space != 0) {
		userpf_err(xdev, "invalid addr space");
		return -EFAULT;
	}

	if (args->size == 0)
		return 0;

	/* currently we are not able to return error because
	 * it is unclear that what addresses are valid other than
	 * ddr area. we should revisit this sometime.
	 * if (!xocl_validate_paddr(xdev, args->paddr, args->size)) {
	 *	userpf_err(xdev, "invalid paddr: 0x%llx, size:0x%llx",
	 *		args->paddr, args->size);
	 *	return -EINVAL;
	 * }
	 */

	ret = xocl_migrate_unmgd(xdev, args->data_ptr, args->paddr, args->size, 0);

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
	for (i = 0; i < args->mm_channel_count; i++)
		xocl_mm_get_usage_stat(drm_p, i, args->mm + i);

	args->dma_channel_count = xocl_get_chan_count(xdev);
	if (args->dma_channel_count > 8)
		args->dma_channel_count = 8;

	for (i = 0; i < args->dma_channel_count; i++) {
		args->h2c[i] = xocl_get_chan_stat(xdev, i, 1);
		args->c2h[i] = xocl_get_chan_stat(xdev, i, 0);
	}

	return 0;
}

static int get_bo_paddr(struct xocl_dev *xdev, struct drm_file *filp,
	uint32_t bo_hdl, size_t off, size_t size, uint64_t *paddrp)
{
	struct drm_device *ddev = filp->minor->dev;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;

	obj = xocl_gem_object_lookup(ddev, filp, bo_hdl);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO 0x%x\n", bo_hdl);
		return -ENOENT;
	}

	xobj = to_xocl_bo(obj);
	if (!xobj->mm_node) {
		/* Not a local BO */
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
		return -EADDRNOTAVAIL;
	}

	if (obj->size <= off || obj->size < off + size) {
		userpf_err(xdev, "Failed to get paddr for BO 0x%x\n", bo_hdl);
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
		return -EINVAL;
	}

	*paddrp = xobj->mm_node->start + off;
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(obj);
	return 0;
}

int xocl_copy_bo_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	struct drm_xocl_copy_bo *args = data;
	uint64_t dst_paddr, src_paddr;
	int ret_src, ret_dst;

	/* Look up gem obj */
	ret_src = get_bo_paddr(xdev, filp, args->src_handle, args->src_offset,
	    args->size, &src_paddr);
	if (ret_src != 0 && ret_src != -EADDRNOTAVAIL)
		return ret_src;

	ret_dst = get_bo_paddr(xdev, filp, args->dst_handle, args->dst_offset,
	    args->size, &dst_paddr);
	if (ret_dst != 0 && ret_dst != -EADDRNOTAVAIL)
		return ret_dst;

	/* We need at least one local BO for copy */
	if (ret_src == -EADDRNOTAVAIL && ret_dst == -EADDRNOTAVAIL) {
		return -EINVAL;
	} else if (ret_src == -EADDRNOTAVAIL || ret_dst == -EADDRNOTAVAIL) {
		struct ert_start_copybo_cmd scmd;

		/* One of them is not local BO, perform P2P copy */
		ert_fill_copybo_cmd(&scmd, args->src_handle, args->dst_handle,
		    args->src_offset, args->dst_offset, args->size);
		return xocl_copy_import_bo(dev, filp, &scmd);
	}

	return xocl_m2m_copy_bo(xdev, src_paddr, dst_paddr, args->src_handle,
	    args->dst_handle, args->size);
}

struct free_sgt_cb {
	struct sg_table *sgt;
	void *orig_func;
	void *orig_data;
};

static void xocl_free_sgt_callback(unsigned long cb_hndl, int err)
{
	struct free_sgt_cb *cb_data = (struct free_sgt_cb *)cb_hndl;
	void (*cb_func)(unsigned long cb_hndl, int err) = cb_data->orig_func;

	sg_free_table(cb_data->sgt);
	kfree(cb_data->sgt);
	if (cb_func)
		cb_func((unsigned long)cb_data->orig_data, err);

}

int xocl_sync_bo_callback_ioctl(struct drm_device *dev,
		       void *data,
		       struct drm_file *filp)
{
	const struct drm_xocl_bo *xobj;
	struct sg_table *sgt;
	u64 paddr = 0;
	ssize_t ret = 0;
	const struct drm_xocl_sync_bo_cb *args = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	struct scatterlist *sg;
	void (*cb_func)(unsigned long cb_hndl, int err) = NULL;
	void *cb_data = NULL;
	bool cb_data_alloced = false;

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
	sg = sgt->sgl;

	if (!xocl_bo_sync_able(xobj->flags)) {
		DRM_ERROR("BO %d doesn't support sync_bo\n", args->handle);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (xocl_bo_cma(xobj) || xocl_bo_p2p(xobj)) {
		if (dir) {
			dma_sync_single_for_device(&(XDEV(xdev)->pdev->dev), sg_phys(sg),
				sg->length, DMA_TO_DEVICE);
		} else {
			dma_sync_single_for_cpu(&(XDEV(xdev)->pdev->dev), sg_phys(sg),
				sg->length, DMA_FROM_DEVICE);
		}
		goto out;
	}

	//Sarab: If it is a remote BO then why do sync over ARE.
	//We should do sync directly using the other device which this bo locally.
	//So that txfer is: HOST->PCIE->DDR; Else it will be HOST->PCIE->ARE->DDR
	paddr = xocl_bo_physical_addr(xobj);

	if (paddr == 0xffffffffffffffffull)
		return -EINVAL;

	if ((args->offset + args->size) > gem_obj->size) {
		ret = -EINVAL;
		goto out;
	}

	/* only invalidate the range of addresses requested by the user */
#if 0
	if (args->dir == DRM_XOCL_SYNC_BO_TO_DEVICE)
		flush_kernel_vmap_range(kaddr, args->size);
	else if (args->dir == DRM_XOCL_SYNC_BO_FROM_DEVICE)
		invalidate_kernel_vmap_range(kaddr, args->size);
	else {
		ret = -EINVAL;
		goto out;
	}
#endif
	paddr += args->offset;

	if (args->offset || (args->size != xobj->base.size)) {
		sgt = alloc_onetime_sg_table(xobj->pages, args->offset, args->size);
		if (IS_ERR(sgt)) {
			ret = PTR_ERR(sgt);
			goto out;
		}
		if (args->cb_data) {
			cb_data = kzalloc(sizeof(struct free_sgt_cb), GFP_KERNEL);
			if (!cb_data) {
				ret = -ENOMEM;
				goto out;
			}
			cb_data_alloced = true;
			cb_func = xocl_free_sgt_callback;
			((struct free_sgt_cb *)cb_data)->sgt = sgt;
			((struct free_sgt_cb *)cb_data)->orig_func = (void *)args->cb_func;
			((struct free_sgt_cb *)cb_data)->orig_data = (void *)args->cb_data;
		}
	} else if (args->cb_data) {
		cb_func = (void (*)(unsigned long cb_hndl, int err))args->cb_func;
		cb_data = (void *)args->cb_data;
	}

	//drm_clflush_sg(sgt);
	//pr_info("%s: %llx, %llx, %d, %llx %llx", __func__, paddr, args->size, dir, (u64)cb_func, (u64)cb_data);

	if (args->cb_data)
		/* Now perform DMA */
		ret = xocl_async_migrate_bo(xdev, sgt, dir, paddr, 0, args->size, cb_func, cb_data);
	else {
		int channel;
		//drm_clflush_sg(sgt);
		channel = xocl_acquire_channel(xdev, dir);

		if (channel < 0) {
			ret = -EINVAL;
			goto clear;
		}
		/* Now perform DMA */
		ret = xocl_async_migrate_bo(xdev, sgt, dir, paddr, channel, args->size, cb_func, cb_data);
		if (ret >= 0)
			ret = (ret == args->size) ? 0 : -EIO;
		xocl_release_channel(xdev, dir, channel);
clear:
		if (args->offset || (args->size != xobj->base.size)) {
			sg_free_table(sgt);
			kfree(sgt);
		}
	}

out:
	if (cb_data_alloced)
		kfree(cb_data);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
	return ret;
}

int xocl_kinfo_bo_ioctl(struct drm_device *dev,
		       void *data,
		       struct drm_file *filp)
{
	const struct drm_xocl_bo *xobj;
	struct drm_xocl_kinfo_bo *args = data;
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

	if (xobj->flags & XOCL_P2P_MEM)
		args->vaddr = (u64)page_address(xobj->pages[0]);
	else
		args->vaddr = (u64)xobj->vmapping;

	xocl_describe(xobj);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return 0;
}



int xocl_map_kern_mem_ioctl(struct drm_device *dev,
		       void *data,
		       struct drm_file *filp)
{
	int i;
	int ret = 0;
	unsigned int page_count;
	struct scatterlist *sg;
	const struct drm_xocl_map_kern_mem *args = data;

	struct drm_xocl_bo *xobj;
	struct drm_gem_object *gem_obj;

	/* This ioctl should only be called to map to kernel memory */
	if ((args->flags != XCL_BO_FLAGS_KERNBUF) &&
					(args->flags != XCL_BO_FLAGS_SGL)) {
		return -EINVAL;
	}

	gem_obj = xocl_gem_object_lookup(dev, filp, args->handle);
	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	xobj = to_xocl_bo(gem_obj);

	/* This ioctl should only be called to map to kernel memory to BOs
	 * with Device Memory only.
	 */
	if (!(xobj->flags & XOCL_DEVICE_MEM)) {
		ret = -EINVAL;
		goto out1;
	}

	/* The host memory being mapped must be equal to the buffer object's size
	 * on the device
	 */
	//if (args->size != xobj->base.size)
	//       return -EINVAL;

	/* Use the page rounded size so we can accurately account for number of pages */
	page_count = xobj->base.size >> PAGE_SHIFT;

	if (args->flags == XCL_BO_FLAGS_SGL) {
		int nents = sg_nents((struct scatterlist *)args->addr);

		//pr_info("%s: 1 bo_type: %x", __func__, xobj->flags);
		/* error out if SGL being mapped is bigger than BO size*/
		if (nents > page_count) {
			ret = -EINVAL;
			goto out1;
		}

		/* SGL_BO starts as a normal BO, which then gets mapped to a SGL
		 * In case its not been mapped yet, allocate a SGT.
		 * In case its been already been mapped, the same SGT can be used map
		 * to the new host SGL.
		 */
		if (!xobj->sgt) {
			xobj->sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
			if (!xobj->sgt) {
				ret = -ENOMEM;
				goto out1;
			}

		}

		xobj->sgt->sgl = (struct scatterlist *)args->addr;
		xobj->sgt->nents = xobj->sgt->orig_nents = nents;

		/* pages need to point to SGL pages, in case partial sync is needed. */
		if (!xobj->pages) {
			page_count = nents;
			xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
			if (!xobj->pages) {
				ret = -ENOMEM;
				goto out1;
			}
		}

		for_each_sg((struct scatterlist *)args->addr, sg, nents, i) {
			xobj->pages[i] = sg_page(sg);
		}
		xobj->flags |= (XOCL_HOST_MEM | XOCL_SGL);
	} else if (args->flags == XCL_BO_FLAGS_KERNBUF) {
		//pr_info("%s: 2 bo_type: %x", __func__, xobj->flags);
		/* If KERNBUF buffer oject is already mapped to a kernel buffer
		 * free up previosly allocated pages and SGT so that they can be
		 * allocatd again.
		 */
		if (xobj->pages) {
			drm_free_large(xobj->pages);
			xobj->pages = NULL;
		}

		if (xobj->sgt) {
			sg_free_table(xobj->sgt);
			kfree(xobj->sgt);
		}

		xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
		if (!xobj->pages) {
			ret = -ENOMEM;
			goto out1;
		}

		for (i = 0; i < page_count; i++)
			xobj->pages[i] = virt_to_page(args->addr+i*PAGE_SIZE);

		xobj->sgt = xocl_prime_pages_to_sg(dev, xobj->pages, page_count);
		if (IS_ERR(xobj->sgt)) {
			ret = PTR_ERR(xobj->sgt);
			goto out0;
		}
		xobj->flags |= (XOCL_HOST_MEM | XOCL_KERN_BUF);
	}

	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);
	return ret;
out0:
	drm_free_large(xobj->pages);
	xobj->pages = NULL;
out1:
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);
	return ret;
}
