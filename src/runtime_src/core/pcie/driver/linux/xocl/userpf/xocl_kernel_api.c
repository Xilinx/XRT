/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Amit Kumar <akum@xilinx.com>
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
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 0, 0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include "common.h"
#include "../xocl_drv.h"
#include "xocl_kernel_api.h"

//TO-DO: Remove these
#ifdef _XOCL_BO_DEBUG
#define BO_ENTER(fmt, args...)          \
        printk(KERN_INFO "[BO] Entering %s:"fmt"\n", __func__, ##args)
#define BO_DEBUG(fmt, args...)          \
        printk(KERN_INFO "[BO] %s:%d:"fmt"\n", __func__, __LINE__, ##args)
#else
#define BO_ENTER(fmt, args...)
#define BO_DEBUG(fmt, args...)
#endif

extern void xocl_describe(const struct drm_xocl_bo *xobj);

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

struct xocl_drm_dev_info uapp_drm_context;

int xocl_create_bo_ifc(struct drm_xocl_create_bo *args)
{
	return xocl_create_bo_ioctl(uapp_drm_context.dev, args, 
							uapp_drm_context.file);
}

EXPORT_SYMBOL_GPL(xocl_create_bo_ifc);

int xocl_map_bo_ifc(struct drm_xocl_map_bo *args)
{
	return xocl_map_bo_ioctl(uapp_drm_context.dev, args, 
							uapp_drm_context.file);
}
EXPORT_SYMBOL_GPL(xocl_map_bo_ifc);

int xocl_sync_bo_ifc(struct drm_xocl_sync_bo *args)
{
	return xocl_sync_bo_ioctl(uapp_drm_context.dev, args, 
							uapp_drm_context.file);
}
EXPORT_SYMBOL_GPL(xocl_sync_bo_ifc);

int xocl_info_bo_ifc(struct drm_xocl_info_bo *args)
{
	return xocl_info_bo_ioctl(uapp_drm_context.dev, args, 
							uapp_drm_context.file);
}
EXPORT_SYMBOL_GPL(xocl_info_bo_ifc);

int xocl_execbuf_ifc(struct drm_xocl_execbuf *args)
{
	return xocl_execbuf_ioctl(uapp_drm_context.dev, args, 
							uapp_drm_context.file);
}
EXPORT_SYMBOL_GPL(xocl_execbuf_ifc);

int xocl_create_kmem_bo_ifc(struct drm_xocl_kptr_bo *args)
{
	int ret, i;
	struct drm_xocl_bo *xobj;
	struct xocl_drm *drm_p = uapp_drm_context.dev->dev_private;
	uint64_t page_count = 0;

	if (offset_in_page(args->addr))
		return -EINVAL;

	xobj = xocl_drm_create_bo(drm_p, args->size, 
					(args->flags | XCL_BO_FLAGS_KERNPTR));
	BO_ENTER("xobj %p", xobj);

	if (IS_ERR(xobj)) {
		DRM_ERROR("object creation failed\n");
		return PTR_ERR(xobj);
	}

	/* Use the page rounded size to accurately account for num of pages */
	page_count = xobj->base.size >> PAGE_SHIFT;

	xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
	if (!xobj->pages) {
		ret = -ENOMEM;
		goto out1;
	}

	for (i=0; i<page_count; i++)
	{
		xobj->pages[i] = virt_to_page(args->addr+i*PAGE_SIZE);
	}

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


	ret = drm_gem_handle_create(uapp_drm_context.file, &xobj->base, 
								&args->handle);
	if (ret)
		goto out1;

	xocl_describe(xobj);
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);
	return ret;

out0:
	drm_free_large(xobj->pages);
	xobj->pages = NULL;
out1:
	xocl_drm_free_bo(&xobj->base);
	DRM_DEBUG("handle creation failed\n");
	return ret;	
}
EXPORT_SYMBOL_GPL(xocl_create_kmem_bo_ifc);

int xocl_remap_kmem_bo_ifc(struct drm_xocl_kptr_bo *args)
{
	int i;
        int ret;
        unsigned int page_count;

        struct drm_xocl_bo *xobj;
        struct drm_gem_object *gem_obj = xocl_gem_object_lookup(
							uapp_drm_context.dev, 
							uapp_drm_context.file,
                                                        args->handle);
        if (!gem_obj) {
                DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
                return -ENOENT;
        }

	xobj = to_xocl_bo(gem_obj);

        if (xobj->pages) {
                drm_free_large(xobj->pages);
                xobj->pages = NULL;
        }

        if (xobj->sgt) {
                sg_free_table(xobj->sgt);
                kfree(xobj->sgt);
        }

        /* Use the page rounded size so we can accurately account for number of pages */
        page_count = xobj->base.size >> PAGE_SHIFT;

        xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
        if (!xobj->pages) {
                ret = -ENOMEM;
                goto out1;
        }
	
	//pr_info("%s: %d %p", __func__, page_count, xobj->pages);

        for (i=0; i<page_count; i++)
        {
                xobj->pages[i] = virt_to_page(args->addr+i*PAGE_SIZE);
        }

        xobj->sgt = drm_prime_pages_to_sg(xobj->pages, page_count);
        if (IS_ERR(xobj->sgt)) {
                ret = PTR_ERR(xobj->sgt);
                goto out0;
        }

        /* TODO: resolve the cache issue */
        xobj->vmapping = vmap(xobj->pages, page_count, VM_MAP, PAGE_KERNEL);

	 if (!xobj->vmapping) {
                ret = -ENOMEM;
        }

        return ret;
out0:
        drm_free_large(xobj->pages);
        xobj->pages = NULL;
out1:
        return ret;
}
EXPORT_SYMBOL_GPL(xocl_remap_kmem_bo_ifc);

int xocl_create_sgl_bo_ifc(struct drm_xocl_sgl_bo *args)
{
        int i;
        int ret;
        struct drm_xocl_bo *xobj;
	struct xocl_drm *drm_p = uapp_drm_context.dev->dev_private;
        unsigned int page_count;

        struct scatterlist *sg;
        int nents;

	xobj = xocl_drm_create_bo(drm_p, args->size, 
					(args->flags | XCL_BO_FLAGS_KERNPTR));
        BO_ENTER("xobj %p", xobj);

        if (IS_ERR(xobj)) {
                DRM_DEBUG("object creation failed\n");
                return PTR_ERR(xobj);
        }

	if (args->sgl) {
        	nents = sg_nents((struct scatterlist *)args->sgl);
        	/* Use the page rounded size so we can accurately account for 
		number of pages */
        	page_count = xobj->base.size >> PAGE_SHIFT;

		/* error out if SGL being mapped is bigger than BO size*/
		if (nents > page_count)
			return -EINVAL;

        	xobj->sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
                if (!xobj->sgt)
                        return -ENOMEM;

                xobj->sgt->sgl = (struct scatterlist *)args->sgl;
                xobj->sgt->nents = xobj->sgt->orig_nents = nents;

                xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
                if (!xobj->pages) {
                        ret = -ENOMEM;
                        goto out1;
       		}
	
                for_each_sg((struct scatterlist *)args->sgl, sg, nents, i) {
                        xobj->pages[i] = sg_page(sg);
                }

                /* TODO: resolve the cache issue */
                xobj->vmapping = vmap(xobj->pages, nents, VM_MAP, PAGE_KERNEL);
		
		if (!xobj->vmapping) {
                        ret = -ENOMEM;
                        goto out0;
        	}
        }
	else{
		xobj->sgt = NULL;
		xobj->pages = NULL;
		xobj->vmapping = NULL;
	}

        ret = drm_gem_handle_create(uapp_drm_context.file, &xobj->base, 
								&args->handle);
        if (ret)
                goto out1;

        xocl_describe(xobj);
        drm_gem_object_unreference_unlocked(&xobj->base);

        return ret;
out0:
        drm_free_large(xobj->pages);
        xobj->pages = NULL;
out1:
	xocl_drm_free_bo(&xobj->base);
        DRM_DEBUG("handle creation failed\n");
        return ret;
}
EXPORT_SYMBOL_GPL(xocl_create_sgl_bo_ifc);

int xocl_remap_sgl_bo_ifc(struct drm_xocl_sgl_bo *args)
{
        int i;
        int ret=0;
	unsigned int page_count;
	struct scatterlist *sg;
	int nents = sg_nents((struct scatterlist *)args->sgl);

	struct drm_xocl_bo *xobj;
	struct drm_gem_object *gem_obj = xocl_gem_object_lookup(
							uapp_drm_context.dev, 
							uapp_drm_context.file,
							args->handle);
	if (!gem_obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	xobj = to_xocl_bo(gem_obj);

        /* Use the page rounded size so we can accurately account for number of pages */
        page_count = xobj->base.size >> PAGE_SHIFT;

	/* error out if SGL being mapped is bigger than BO size*/
	if (nents > page_count)
		return -EINVAL;

	if (!xobj->sgt) {
		xobj->sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (!xobj->sgt)
			return -ENOMEM;

	}

	xobj->sgt->sgl = (struct scatterlist *)args->sgl;
	xobj->sgt->nents = xobj->sgt->orig_nents = nents;

	if (!xobj->pages) {
		page_count = nents;
		xobj->pages = drm_malloc_ab(page_count, sizeof(*xobj->pages));
		if (!xobj->pages)
			return -ENOMEM;
	}

	for_each_sg((struct scatterlist *)args->sgl, sg, nents, i) {
		xobj->pages[i] = sg_page(sg);
	}

	/* TODO: resolve the cache issue */
	xobj->vmapping = vmap(xobj->pages, nents, VM_MAP, PAGE_KERNEL);

	if (!xobj->vmapping) {
		ret = -ENOMEM;
                goto out0;
	}

	return ret;
out0:
	drm_free_large(xobj->pages);
	xobj->pages = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(xocl_remap_sgl_bo_ifc);

void __iomem *xocl_get_bo_kernel_vaddr(uint32_t bo_handle)
{
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;

	obj = xocl_gem_object_lookup(uapp_drm_context.dev, 
					uapp_drm_context.file, bo_handle);
	xobj = to_xocl_bo(obj);

	if (!obj) {
		DRM_ERROR("Failed to look up GEM BO %d\n", bo_handle);
		return NULL;
	}

	if (xobj->flags & XOCL_P2P_MEM)
		return page_to_virt(xobj->pages[0]);
	else
		return xobj->vmapping;
}
EXPORT_SYMBOL_GPL(xocl_get_bo_kernel_vaddr);



