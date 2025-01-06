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
 *    Min Ma       <min.ma@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/iommu.h>
#include <linux/pagemap.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include "zocl_drv.h"
#include "zocl_sk.h"
#include "zocl_aie.h"
#include "zocl_bo.h"
#include "zocl_xclbin.h"
#include "zocl_error.h"
#include "zocl_ert_intc.h"

#define ZOCL_DRIVER_NAME        "zocl"
#define ZOCL_DRIVER_DESC        "Zynq BO manager"

static char driver_date[9];

/* This should be the same as DRM_FILE_PAGE_OFFSET_START in drm_gem.c */
#if defined(CONFIG_ARM64)
#define ZOCL_FILE_PAGE_OFFSET   0x00100000
#else
#define ZOCL_FILE_PAGE_OFFSET   0x00010000
#endif

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif


int enable_xgq_ert = 1;
module_param(enable_xgq_ert, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(enable_xgq_ert, "0 = legacy ERT mode, 1 = XGQ ERT mode (default)");

extern struct platform_driver zocl_ctrl_ert_driver;

static const struct vm_operations_struct reg_physical_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys,
#endif
};


#if KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE
static int
match_name(struct device *dev, const void *data)
#else
static int
match_name(struct device *dev, void *data)
#endif
{
	const char *name = data;
	/*
	 * check if given name is substring inside dev.
	 * the dev_name is like: 20300030000.ert_hw
	 */
	return strstr(dev_name(dev), name) != NULL;
}

/**
 * zocl_pr_slot_init : PR Slot specific initialization
 *
 * @zdev: zocl device struct
 * @pdev: platform device struct
 *
 * Returns 0 if initialization successfully.
 * Returns -EINVAL if failed.
 */

static int zocl_pr_slot_init(struct drm_zocl_dev *zdev,
		                       struct platform_device *pdev)
{
	struct drm_zocl_slot *zocl_slot = NULL;
	int ret = 0;
	int i = 0;

	zdev->num_pr_slot = 0;
	/* TODO : Need to update this function based on the device tree */
	if (ZOCL_PLATFORM_ARM64) {
		u64 pr_num;
		if (!of_property_read_u64(pdev->dev.of_node,
					  "xlnx,pr-num-support", &pr_num))
			zdev->num_pr_slot = (int)pr_num;
	} 

	/* If there is no information available for number of slot available
	 * for this device then consider it for a single slot device for
	 * backward compartability.
	 */
	if (zdev->num_pr_slot == 0)
		zdev->num_pr_slot = 1;

	for (i = 0; i < zdev->num_pr_slot; i++) {
		zocl_slot = (struct drm_zocl_slot *)
			kzalloc(sizeof(struct drm_zocl_slot), GFP_KERNEL);
		if (!zocl_slot)
			return -ENOMEM;

		/* Initial xclbin */
		ret = zocl_xclbin_init(zocl_slot);
		if (ret)
			return ret;

		mutex_init(&zocl_slot->slot_xclbin_lock);
		mutex_init(&zocl_slot->aie_lock);

		if (ZOCL_PLATFORM_ARM64) {
			zocl_slot->pr_isolation_freeze = 0x0;
			zocl_slot->pr_isolation_unfreeze = 0x3;
			if (of_property_read_u64(pdev->dev.of_node,
					"xlnx,pr-isolation-addr",
					&zocl_slot->pr_isolation_addr))
				zocl_slot->pr_isolation_addr = 0;
			if (of_property_read_bool(pdev->dev.of_node,
						  "xlnx,pr-decoupler")) {
				zocl_slot->pr_isolation_freeze = 0x1;
				zocl_slot->pr_isolation_unfreeze = 0x0;
			}
		} 

		DRM_INFO("PR[%d] Isolation addr 0x%llx", i,
			 zocl_slot->pr_isolation_addr);

		zocl_slot->partial_overlay_id = -1;
		zocl_slot->slot_idx = i;
		zocl_slot->slot_type = ZOCL_SLOT_TYPE_PHY;
		zocl_slot->hwctx_ref_cnt = 0;

		zdev->pr_slot[i] = zocl_slot;
	}

	for (i = zdev->num_pr_slot; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = (struct drm_zocl_slot *)
			kzalloc(sizeof(struct drm_zocl_slot), GFP_KERNEL);
		if (!zocl_slot)
			return -ENOMEM;

		/* Initial xclbin */
		ret = zocl_xclbin_init(zocl_slot);
		if (ret)
			return ret;

		mutex_init(&zocl_slot->slot_xclbin_lock);
		mutex_init(&zocl_slot->aie_lock);

		zocl_slot->slot_idx = i;
		zocl_slot->slot_type = ZOCL_SLOT_TYPE_VIRT;

		zdev->pr_slot[i] = zocl_slot;
	}

	zdev->full_overlay_id  = -1;

	return 0;
}

/**
 * zocl_pr_slot_fini : PR Slot specific Cleanup
 *
 * @zdev: zocl device struct
 *
 */
static void zocl_pr_slot_fini(struct drm_zocl_dev *zdev)
{
	struct drm_zocl_slot *zocl_slot = NULL;
	int i = 0;

	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (zocl_slot) {
			zocl_free_sections(zdev, zocl_slot);
			zocl_cleanup_aie(zocl_slot);
			mutex_destroy(&zocl_slot->slot_xclbin_lock);
			mutex_destroy(&zocl_slot->aie_lock);
			zocl_xclbin_fini(zdev, zocl_slot);
			kfree(zocl_slot);
			zdev->pr_slot[i] = NULL;
		}
	}
}

/**
 * Initialize the aparture and allocate memory for it.
 *
 * @param	zdev: device structure
 *
 * @return	0 on success, Error code on failure.
 */
static int zocl_aperture_init(struct drm_zocl_dev *zdev)
{
	struct addr_aperture *apts = NULL;
	int i = 0;

	zdev->cu_subdev.apertures = kcalloc(MAX_APT_NUM,
				sizeof(struct addr_aperture), GFP_KERNEL);
	if (!zdev->cu_subdev.apertures) {
		DRM_ERROR("Out of memory for Aperture\n");
		return -ENOMEM;
	}

	apts = zdev->cu_subdev.apertures;
	/* Consider this magic number as uninitialized aperture identity */
	for (i = 0; i < MAX_APT_NUM; ++i)
		apts[i].addr = EMPTY_APT_VALUE;

	zdev->cu_subdev.num_apts = 0;
	mutex_init(&zdev->cu_subdev.lock);

	return 0;
}

/**
 * Cleanup the aparture
 *
 * @param	zdev: device structure
 *
 */
static void zocl_aperture_fini(struct drm_zocl_dev *zdev)
{
	/* Free aperture memory */
	if (zdev->cu_subdev.apertures)
		kfree(zdev->cu_subdev.apertures);

	zdev->cu_subdev.apertures = NULL;
	zdev->cu_subdev.num_apts = 0;
	mutex_destroy(&zdev->cu_subdev.lock);
}


/**
 * get_reserved_mem_region - Get reserved memory region
 *
 * @dev: device struct
 * @res: resource struct
 *
 * Returns 0 is get reserved memory resion successfully.
 * Returns -EINVAL if not found.
 */
static int get_reserved_mem_region(struct device *dev, struct resource *res)
{
	struct device_node *np = NULL;
	int ret;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np)
		return -EINVAL;

	ret = of_address_to_resource(np, 0, res);
	if (ret)
		return -EINVAL;

	return 0;
}

/**
 * Find platform device by name
 *
 * @param	name: device name
 *
 * Returns a platform device. Returns NULL if not found.
 */
struct platform_device *zocl_find_pdev(char *name)
{
	struct device *dev;
	struct platform_device *pdev;

	dev = bus_find_device(&platform_bus_type, NULL, (void *)name,
	    match_name);
	if (!dev)
		return NULL;

	pdev = container_of(dev, struct platform_device, dev);
	return pdev;
}

/**
 * Set scheduler CU index in aperture
 *
 * @param	zdev:	zocl device struct
 * @param	apt_idx: aperture index in the IP_LAYOUT ordering
 * @param	cu_idx: CU index in the scheduler ordering
 *
 */
void update_cu_idx_in_apt(struct drm_zocl_dev *zdev, int apt_idx, int cu_idx)
{
	struct addr_aperture *apts = zdev->cu_subdev.apertures;

	mutex_lock(&zdev->cu_subdev.lock);
	apts[apt_idx].cu_idx = cu_idx;
	mutex_unlock(&zdev->cu_subdev.lock);
}

/**
 * Get the index of the geiven phys address,
 *		   if it is the start of an aperture
 *
 * @param	zdev: zocl device struct
 * @param	addr: physical address of the aperture
 *
 * Returns the index if aperture was found, -EINVAL if not found.
 *
 */
int get_apt_index_by_addr(struct drm_zocl_dev *zdev, phys_addr_t addr)
{
	struct addr_aperture *apts = zdev->cu_subdev.apertures;
	int i;

	mutex_lock(&zdev->cu_subdev.lock);
	/* Haven't consider search efficiency yet. */
	for (i = 0; i < zdev->cu_subdev.num_apts; ++i)
		if (apts[i].addr == addr)
			break;
	mutex_unlock(&zdev->cu_subdev.lock);

	return (i == zdev->cu_subdev.num_apts) ? -EINVAL : i;
}

/**
 * Get the index of the geiven phys address,
 *		   if it is the start of an aperture
 *
 * @param	zdev:	zocl device struct
 * @param	cu_idx: CU index
 *
 * Returns the index if aperture was found, -EINVAL if not found.
 *
 */
int get_apt_index_by_cu_idx(struct drm_zocl_dev *zdev, int cu_idx)
{
	struct addr_aperture *apts = zdev->cu_subdev.apertures;
	int i;

	WARN_ON(cu_idx >= MAX_CU_NUM);
	if (cu_idx >= MAX_CU_NUM)
		return -EINVAL;

	mutex_lock(&zdev->cu_subdev.lock);
	/* Haven't consider search efficiency yet. */
	for (i = 0; i < zdev->cu_subdev.num_apts; ++i)
		if (apts[i].cu_idx == cu_idx)
			break;
	mutex_unlock(&zdev->cu_subdev.lock);

	return (i == zdev->cu_subdev.num_apts) ? -EINVAL : i;
}

/**
 * Create a new CU subdevice. And try to attach to the driver. This will force
 * cu probe to call.
 *
 * @param	zdev: zocl Device Instance
 * @param	info: CU related information
 *
 * @return	0 on success, Error code on failure.
 */
int subdev_create_cu(struct device *dev, struct xrt_cu_info *info,
		     struct platform_device **pdevp)
{
	struct platform_device *pldev;
	struct resource res = {};
	int ret;

	pldev = platform_device_alloc("CU", PLATFORM_DEVID_AUTO);
	if (!pldev) {
		DRM_ERROR("Failed to alloc device CU\n");
		return -ENOMEM;
	}

	/*
	 * Only on U30 and some Versal platforms, the res_start is not zero.
	 * But the situations are different,
	 *
	 *     On U30, CUs are assigned to lower 4GB address space.
	 * Host doesn't know the CU base address, which is 0x80000000,
	 * but zocl get it from device tree and store it in res_start.
	 *
	 *     On Versal, CUs are assigned to above 4GB address space.
	 * Host knows the CU base address from xclbin.
	 * But on some specific shells, zocl will get base address in
	 * device tree. "Or" operation still works in this case.
	 */
	res.start = info->addr | zocl_get_zdev()->res_start;
	res.end = res.start + info->size - 1;
	res.flags = IORESOURCE_MEM;
	ret = platform_device_add_resources(pldev, &res, 1);
	if (ret) {
		DRM_ERROR("Failed to add resource\n");
		goto err;
	}

	ret = platform_device_add_data(pldev, info, sizeof(*info));
	if (ret) {
		DRM_ERROR("Failed to add data\n");
		goto err;
	}

	pldev->dev.parent = dev;

	ret = platform_device_add(pldev);
	if (ret) {
		DRM_ERROR("Failed to add device\n");
		goto err;
	}

	/*
	 * force probe to avoid dependence issue. if probing
	 * failed, it could be the driver is not registered.
	 */
	ret = device_attach(&pldev->dev);
	if (ret != 1) {
		ret = -EINVAL;
		DRM_ERROR("Failed to probe device\n");
		goto err1;
	}

	*pdevp = pldev;

	return 0;
err1:
	platform_device_del(pldev);
err:
	platform_device_put(pldev);
	return ret;
}

/* This function destroy and remove the platform-level devices
 * for all the CUs.
 *
 * @param	zdev: zocl Device Instance
 *
 */
void subdev_destroy_cu(struct drm_zocl_dev *zdev)
{
	int i;

	mutex_lock(&zdev->cu_subdev.lock);
	for (i = 0; i < MAX_CU_NUM; ++i) {
		if (!zdev->cu_subdev.cu_pldev[i])
			continue;

		/* Remove the platform-level device */
		platform_device_del(zdev->cu_subdev.cu_pldev[i]);
		/* Destroy the platform device */
		platform_device_put(zdev->cu_subdev.cu_pldev[i]);
		zdev->cu_subdev.cu_pldev[i] = NULL;
	}
	mutex_unlock(&zdev->cu_subdev.lock);
}

/**
 * Create a new SCU subdevice. And try to attach to the driver. This will force
 * cu probe to call.
 *
 * @param	zdev: zocl Device Instance
 * @param	info: SCU related information
 *
 * @return	0 on success, Error code on failure.
 */
int subdev_create_scu(struct device *dev, struct xrt_cu_info *info,
		      struct platform_device **pdevp)
{
	struct platform_device *pldev;
	int ret;

	pldev = platform_device_alloc("SCU", PLATFORM_DEVID_AUTO);
	if (!pldev) {
		DRM_ERROR("Failed to alloc device SCU\n");
		return -ENOMEM;
	}

	ret = platform_device_add_data(pldev, info, sizeof(*info));
	if (ret) {
		DRM_ERROR("Failed to add data\n");
		goto err;
	}

	pldev->dev.parent = dev;

	ret = platform_device_add(pldev);
	if (ret) {
		DRM_ERROR("Failed to add device\n");
		goto err;
	}

	/*
	 * force probe to avoid dependence issue. if probing
	 * failed, it could be the driver is not registered.
	 */
	ret = device_attach(&pldev->dev);
	if (ret != 1) {
		ret = -EINVAL;
		DRM_ERROR("Failed to probe device\n");
		goto err1;
	}
	*pdevp = pldev;

	return 0;
err1:
	platform_device_del(pldev);
err:
	platform_device_put(pldev);
	return ret;
}

/**
 * zocl_gem_create_object - Create drm_zocl_bo object instead of DRM CMA object.
 *
 * @dev: DRM device struct
 * @size: This size was not use, just to match function prototype.
 *
 */
struct drm_gem_object *
zocl_gem_create_object(struct drm_device *dev, size_t size)
{
	struct drm_zocl_bo *bo = kzalloc(sizeof(struct drm_zocl_bo), GFP_KERNEL);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	bo->gem_base.funcs = &zocl_gem_object_funcs;
#endif
	return (&bo->gem_base);
}

/* This callback function release GEM buffer objects and free memory associated
 * with it. This function is also responsable for free up the memory for BOs.
 *
 * @param	obj:	GEM buffer object
 *
 */
void zocl_free_bo(struct drm_gem_object *obj)
{
	struct drm_zocl_bo *zocl_obj;
	struct drm_zocl_dev *zdev;
	int npages;

	if (IS_ERR(obj) || !obj)
		return;

	DRM_DEBUG("Freeing BO\n");
	zocl_obj = to_zocl_bo(obj);
	zdev = obj->dev->dev_private;

	if (!zdev->domain) {
		zocl_describe(zocl_obj);
		if (zocl_obj->flags & ZOCL_BO_FLAGS_USERPTR)
			zocl_free_userptr_bo(obj);
		else if (zocl_obj->flags & ZOCL_BO_FLAGS_HOST_BO)
			zocl_free_host_bo(obj);
		else if (!zocl_obj->mm_node) {
			/* Update memory usage statistics */
			zocl_update_mem_stat(zdev, obj->size, -1,
			    zocl_obj->mem_index);
			/* free resources associated with a CMA GEM object */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
			drm_gem_dma_object_free(obj);
#else
			drm_gem_cma_free_object(obj);
#endif

		} else {
			if (zocl_obj->mm_node) {
				mutex_lock(&zdev->mm_lock);
				drm_mm_remove_node(zocl_obj->mm_node);
				mutex_unlock(&zdev->mm_lock);
				kfree(zocl_obj->mm_node);
				if (zocl_obj->vmapping) {
					memunmap(zocl_obj->vmapping);
					zocl_obj->vmapping = NULL;
				}
				zocl_update_mem_stat(zdev, obj->size, -1,
				    zocl_obj->mem_index);
			}
			/* release GEM buffer object resources */
			drm_gem_object_release(obj);
			kfree(zocl_obj);
		}

		return;
	}

	npages = obj->size >> PAGE_SHIFT;
	/* release GEM buffer object resources */
	drm_gem_object_release(obj);

	if (zocl_obj->vmapping)
		vunmap(zocl_obj->vmapping);
	zocl_obj->vmapping = NULL;

	zocl_iommu_unmap_bo(obj->dev, zocl_obj);
	if (zocl_obj->pages) {
		if (zocl_bo_userptr(zocl_obj)) {
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
			release_pages(zocl_obj->pages, npages);
#else
			release_pages(zocl_obj->pages, npages, 0);
#endif
			kvfree(zocl_obj->pages);
		} else {
			drm_gem_put_pages(obj, zocl_obj->pages, false, false);

			/* Update memory usage statistics */
			zocl_update_mem_stat(zdev, obj->size, -1,
			    zocl_obj->mem_index);
		}
	}
	if (zocl_obj->sgt)
		sg_free_table(zocl_obj->sgt);
	zocl_obj->sgt = NULL;
	zocl_obj->pages = NULL;
	kfree(zocl_obj);
}

/* This function memory map for GEM objects.
 *
 * @param	flip:	file data structure
 * @param	vma:	struct to a virtual memory area
 *
 * @return	0 on success, Error code on failure.
 */
static int
zocl_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	struct drm_gem_dma_object *dma_obj = NULL;
#else
	struct drm_gem_cma_object *cma_obj = NULL;
#endif
	struct drm_gem_object *gem_obj;
	struct drm_zocl_bo *bo;
	dma_addr_t paddr;
	pgprot_t prot;
	int rc;

	/**
	 * drm_gem_mmap may modify the vma prot as non-cacheable.
	 * We need to preserve this field and resume it in case
	 * the BO is cacheable.
	 */
	prot = vma->vm_page_prot;

	rc = drm_gem_mmap(filp, vma);

	if (rc)
		return rc;

	/**
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(),
	 * and set the vm_pgoff (used as a fake buffer offset by DRM)
	 * to 0 as we want to map the whole buffer.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
        vma->vm_flags &= ~VM_PFNMAP;
#else
        vm_flags_clear(vma, VM_PFNMAP);
#endif
	vma->vm_pgoff = 0;

	gem_obj = vma->vm_private_data;
	bo = to_zocl_bo(gem_obj);

	if (bo->flags & ZOCL_BO_FLAGS_CACHEABLE)
		/**
		 * Resume the protection field from mmap(). Most likely
		 * it will be cacheable. If there is a case that mmap()
		 * protection field explicitly tells us not to map with
		 * cache enabled, we should comply with it and overwrite
		 * the cacheable BO property.
		 */
		vma->vm_page_prot = prot;

	if (!bo->mm_node) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		dma_obj = to_drm_gem_dma_obj(gem_obj);
		paddr = dma_obj->dma_addr;
#else
		cma_obj = to_drm_gem_cma_obj(gem_obj);
		paddr = cma_obj->paddr;
#endif
	} else
		paddr = bo->mm_node->start;

	if (bo->mm_node ||
	    (!bo->mm_node &&
	    bo->flags & ZOCL_BO_FLAGS_CACHEABLE)) {
		/* Map PL-DDR and cacheable CMA */
		rc = remap_pfn_range(vma, vma->vm_start,
		    paddr >> PAGE_SHIFT, vma->vm_end - vma->vm_start,
		    vma->vm_page_prot);
	} else {
		/* Map non-cacheable CMA */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		rc = dma_mmap_wc(dma_obj->base.dev->dev, vma, dma_obj->vaddr,
		    paddr, vma->vm_end - vma->vm_start);
#else
		rc = dma_mmap_wc(cma_obj->base.dev->dev, vma, cma_obj->vaddr,
		    paddr, vma->vm_end - vma->vm_start);
#endif
	}

	if (rc)
		drm_gem_vm_close(vma);

	return rc;
}

/* This function map two types of kernel address to user space.
 * The first type is pysical registers of a hardware IP, like CUs.
 * The second type is GEM buffer.
 *
 * @param	flip:	file data structure
 * @param	vma:	struct to a virtual memory area
 *
 * @return	0 on success, Error code on failure.
 */
static int zocl_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file     *priv = filp->private_data;
	struct drm_device   *dev = priv->minor->dev;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct addr_aperture *apts = zdev->cu_subdev.apertures;
	struct drm_zocl_bo  *bo = NULL;
	unsigned long        vsize;
	phys_addr_t          phy_addr;
	int apt_idx;
	int rc;

	/* A GEM buffer object has a fake mmap offset start from page offset
	 * DRM_FILE_PAGE_OFFSET_START. See drm_gem_init().
	 * ZOCL_FILE_PAGE_OFFSET should equal to DRM_FILE_PAGE_OFFSET_START.
	 * ZOCL_FILE_PAGE_OFFSET is 4GB for 64 bits system.
	 */
	if (likely(vma->vm_pgoff >= ZOCL_FILE_PAGE_OFFSET)) {
		if (!zdev->domain)
			return zocl_gem_mmap(filp, vma);

		/* Map user's pages into his VM */
		rc = drm_gem_mmap(filp, vma);
		if (rc)
			return rc;
		/* vma->vm_private_data is set by drm_gem_mmap */
		bo = to_zocl_bo(vma->vm_private_data);

		bo->uaddr = vma->vm_start;
		/* Map user's VA into IOMMU */
		rc = zocl_iommu_map_bo(dev, bo);
		if (rc)
			return rc;
                #if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
                        vma->vm_flags &= ~VM_PFNMAP;
                        vma->vm_flags |= VM_MIXEDMAP;
                #else
                        vm_flags_clear(vma, VM_PFNMAP);
                        vm_flags_set(vma, VM_MIXEDMAP);
                #endif
		/* Reset the fake offset used to identify the BO */
		vma->vm_pgoff = 0;
		return 0;
	}


	/* Only allow user to map register ranges in apertures list.
	 * Could not map from the middle of an aperture.
	 */
	apt_idx = vma->vm_pgoff;
	if (apt_idx < 0 || apt_idx >= zdev->cu_subdev.num_apts) {
		DRM_ERROR("The offset is not in the apertures list\n");
		return -EINVAL;
	}
	phy_addr = apts[apt_idx].addr;
	vma->vm_pgoff = phy_addr >> PAGE_SHIFT;

	vsize = vma->vm_end - vma->vm_start;
	if (vsize > zdev->cu_subdev.apertures[apt_idx].size)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
        #if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
        	vma->vm_flags |= VM_IO;
	        vma->vm_flags |= VM_RESERVED;
        #else
                vm_flags_set(vma, VM_IO | VM_RESERVED);
        #endif
	vma->vm_ops = &reg_physical_vm_ops;
	rc = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				vsize, vma->vm_page_prot);

	return rc;
}

/**
 * Registering callback for fault handler.
 *
 * @param	vmf:	vm page fault instance
 *
 * @return	VM_FAULT_NOPAGE on success, Error code on failure.
 */
static vm_fault_t zocl_bo_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_zocl_bo *bo = to_zocl_bo(obj);
	struct drm_zocl_dev *zdev = obj->dev->dev_private;
	struct page *page;
	pgoff_t offset;
	int err;

	if (!zdev->domain)
		return 0;

	if (!bo->pages)
		return VM_FAULT_SIGBUS;

	offset = ((unsigned long)vmf->address - vma->vm_start) >> PAGE_SHIFT;
	page = bo->pages[offset];

	err = vm_insert_page(vma, (unsigned long)vmf->address, page);
	switch (err) {
	case -EAGAIN:
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
	case -EBUSY:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	}
	return VM_FAULT_SIGBUS;
}

/**
 * Driver callback when a new &struct drm_file is opened.
 * This function will create a new client for this device
 *
 * @param	dev:	DRM device structure
 * @param	flip:	DRM file private data
 *
 * @return	0 on success, Error code on failure.
 */
static int zocl_client_open(struct drm_device *dev, struct drm_file *filp)
{
	return zocl_create_client(dev->dev, &filp->driver_priv);
}

/**
 * Driver callback when a new &struct drm_file is closed.
 * This function will cleanup driver-private data structures
 * allocated in @open and destroy the client.
 *
 * @param	dev:	DRM device structure
 * @param	flip:	DRM file private data
 *
 * @return	0 on success, Error code on failure.
 */
static void zocl_client_release(struct drm_device *dev, struct drm_file *filp)
{
	return zocl_destroy_client(filp->driver_priv);
}

/**
 * Register a poll callback function for this driver
 *
 * @param	flip:	file data structure
 * @param	wait:	poll table
 *
 * @return	POLLIN on success, 0 on failure.
 */
static unsigned int zocl_poll(struct file *filp, poll_table *wait)
{
	return zocl_poll_client(filp, wait);
}

/**
 * Initialize iommu domain for this device
 *
 * @param	zdev: zocl Device Instance
 * @param	pdev: Platform Device Instance
 *
 * @return	0 on success, Error code on failure.
 */
static int zocl_iommu_init(struct drm_zocl_dev *zdev,
		struct platform_device *pdev)
{
	struct iommu_domain_geometry *geometry;
	u64 start, end;
	int ret;

	zdev->domain = iommu_domain_alloc(&platform_bus_type);
	if (!zdev->domain)
		return -ENOMEM;

	ret = iommu_attach_device(zdev->domain, &pdev->dev);
	if (ret) {
		DRM_INFO("IOMMU attach device failed. ret(%d)\n", ret);
		iommu_domain_free(zdev->domain);
		zdev->domain = NULL;
		return ret;
	}

	geometry = &zdev->domain->geometry;
	start = geometry->aperture_start;
	end = geometry->aperture_end;

	DRM_INFO("IOMMU aperture initialized (%#llx-%#llx)\n",
				start, end);

	return 0;
}

const struct vm_operations_struct zocl_bo_vm_ops = {
	.fault = zocl_bo_fault,
	.open  = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
/* This was removed in 6.8 */
#define DRM_UNLOCKED 0
#endif

static const struct drm_ioctl_desc zocl_ioctls[] = {
	DRM_IOCTL_DEF_DRV(ZOCL_CREATE_BO, zocl_create_bo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_USERPTR_BO, zocl_userptr_bo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_GET_HOST_BO, zocl_get_hbo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_MAP_BO, zocl_map_bo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SYNC_BO, zocl_sync_bo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_INFO_BO, zocl_info_bo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_PWRITE_BO, zocl_pwrite_bo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_PREAD_BO, zocl_pread_bo_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_EXECBUF, zocl_execbuf_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_HW_CTX_EXECBUF, zocl_hw_ctx_execbuf_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_READ_AXLF, zocl_read_axlf_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_CREATE_HW_CTX, zocl_create_hw_ctx_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_DESTROY_HW_CTX, zocl_destroy_hw_ctx_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_OPEN_CU_CTX, zocl_open_cu_ctx_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_CLOSE_CU_CTX, zocl_close_cu_ctx_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_OPEN_GRAPH_CTX, zocl_open_graph_ctx_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_CLOSE_GRAPH_CTX, zocl_close_graph_ctx_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SK_GETCMD, zocl_sk_getcmd_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SK_CREATE, zocl_sk_create_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SK_REPORT, zocl_sk_report_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_INFO_CU, zocl_info_cu_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_CTX, zocl_ctx_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_ERROR_INJECT, zocl_error_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_AIE_FD, zocl_aie_fd_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_AIE_RESET, zocl_aie_reset_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_AIE_GETCMD, zocl_aie_getcmd_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_AIE_PUTCMD, zocl_aie_putcmd_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_AIE_FREQSCALE, zocl_aie_freqscale_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SET_CU_READONLY_RANGE, zocl_set_cu_read_only_range_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
};

static const struct file_operations zocl_driver_fops = {
	.owner          = THIS_MODULE,
	.open           = drm_open,
	.mmap           = zocl_mmap,
	.poll           = zocl_poll,
	.read           = drm_read,
	.unlocked_ioctl = drm_ioctl,
	.release        = drm_release,
};

static struct drm_driver zocl_driver = {
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
	.driver_features           = DRIVER_GEM | DRIVER_PRIME | DRIVER_RENDER,
#else
	.driver_features           = DRIVER_GEM | DRIVER_RENDER,
#endif
	.open                      = zocl_client_open,
	.postclose                 = zocl_client_release,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	.gem_free_object_unlocked  = zocl_free_bo,
#else
	.gem_free_object           = zocl_free_bo,
#endif

	.gem_vm_ops                = &zocl_bo_vm_ops,
	.gem_prime_get_sg_table    = drm_gem_cma_prime_get_sg_table,
	.gem_prime_vmap            = drm_gem_cma_prime_vmap,
	.gem_prime_vunmap          = drm_gem_cma_prime_vunmap,
	.gem_prime_export          = drm_gem_prime_export,
#endif

	.gem_create_object         = zocl_gem_create_object,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
	.prime_handle_to_fd        = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle        = drm_gem_prime_fd_to_handle,
        .gem_prime_mmap            = drm_gem_prime_mmap,
#endif
	.gem_prime_import          = drm_gem_prime_import,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	.gem_prime_import_sg_table = drm_gem_dma_prime_import_sg_table,
#else
	.gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
#endif
	.ioctls                    = zocl_ioctls,
	.num_ioctls                = ARRAY_SIZE(zocl_ioctls),
	.fops                      = &zocl_driver_fops,
	.name                      = ZOCL_DRIVER_NAME,
	.desc                      = ZOCL_DRIVER_DESC,
	.date                      = driver_date,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
const struct drm_gem_object_funcs zocl_gem_object_funcs = {
	.free = zocl_free_bo,
	.vm_ops = &zocl_bo_vm_ops,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	.get_sg_table = zocl_gem_prime_get_sg_table,
	.vmap = drm_gem_dma_object_vmap,
#else
	.get_sg_table = drm_gem_cma_get_sg_table,
	.vmap = drm_gem_cma_vmap,
#endif
	.export = drm_gem_prime_export,
};

const struct drm_gem_object_funcs zocl_cma_default_funcs = {
	.free = zocl_free_bo,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	.get_sg_table = drm_gem_dma_object_get_sg_table,
#else
	.get_sg_table = drm_gem_cma_get_sg_table,
#endif
	.vm_ops = &zocl_bo_vm_ops,
};
#endif
static const struct zdev_data zdev_data_mpsoc = {
	.fpga_driver_name = "pcap",
	.fpga_driver_new_name = "pcap"
};

static const struct zdev_data zdev_data_versal = {
	.fpga_driver_name = "versal_fpga",
	.fpga_driver_new_name = "versal-fpga"
};

static const struct of_device_id zocl_drm_of_match[] = {
	{ .compatible = "xlnx,zocl", .data = &zdev_data_mpsoc},
	{ .compatible = "xlnx,zoclsvm", .data = &zdev_data_mpsoc},
	{ .compatible = "xlnx,zocl-ert", .data = &zdev_data_mpsoc},
	{ .compatible = "xlnx,zocl-versal", .data = &zdev_data_versal},
	{ /* end of table */ },
};
MODULE_DEVICE_TABLE(of, zocl_drm_of_match);

/*
 *
 * Initialization of Xilinx openCL DRM platform device.
 *
 * @param        pdev: Platform Device Instance
 *
 * @return       0 on success, Error code on failure.
 *
 */
static int zocl_drm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct drm_device *drm;
	struct drm_zocl_dev	*zdev;
	struct platform_device *subdev;
	struct resource res_mem;
	struct resource *res;
	struct device_node *fnode;
	int index;
	int irq;
	int ret;
	int year, mon, day;

	id = of_match_node(zocl_drm_of_match, pdev->dev.of_node);
	if (!id)
		return -EINVAL;
	DRM_INFO("Probing for %s\n", id->compatible);

	/* Create zocl device and initial */
	zdev = devm_kzalloc(&pdev->dev, sizeof(*zdev), GFP_KERNEL);
	if (!zdev)
		return -ENOMEM;

	zdev->zdev_data_info = id->data;
	INIT_LIST_HEAD(&zdev->ctx_list);
	zdev->slot_mask = 0;

	/* Record and get IRQ number */
	for (index = 0; index < MAX_CU_NUM; index++) {
		irq = platform_get_irq(pdev, index);
		if (irq < 0)
			break;
		DRM_DEBUG("CU(%d) IRQ %d\n", index, irq);
		zdev->cu_subdev.irq[index] = irq;
	}
	zdev->cu_subdev.cu_num = index;
	if (zdev->cu_subdev.cu_num) {
		ret = zocl_ert_create_intc(&pdev->dev, zdev->cu_subdev.irq,
					   zdev->cu_subdev.cu_num, 0,
					   ERT_CU_INTC_DEV_NAME, &zdev->cu_intc);
		if (ret)
			DRM_ERROR("Failed to create cu intc device, ret %d\n", ret);
	}

	/* set to 0xFFFFFFFF(32bit) or 0xFFFFFFFFFFFFFFFF(64bit) */
	zdev->host_mem = (phys_addr_t) -1;
	zdev->host_mem_len = 0;
	/* Work around for CR-1119382 issue.
	 * ZOCL driver is crashing if it accessing the device tree node */
	if (ZOCL_PLATFORM_ARM64) {
		/* If reserved memory region are not found, just keep going */
		ret = get_reserved_mem_region(&pdev->dev, &res_mem);
		if (!ret) {
			DRM_INFO("Reserved memory for host at 0x%lx, size 0x%lx\n",
				 (unsigned long)res_mem.start,
				 (unsigned long)resource_size(&res_mem));
			zdev->host_mem = res_mem.start;
			zdev->host_mem_len = resource_size(&res_mem);
		}
	}
	mutex_init(&zdev->mm_lock);
	INIT_LIST_HEAD(&zdev->zm_list_head);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	/* Platform did not initialize dma_mask, try to set 64-bit DMA
	 * first */
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		/* If setting 64-bit DMA mask
		 * fails, fall back to 32-bit
		 * DMA mask */
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret) {
			DRM_ERROR("DMA configuration failed: 0x%x\n", ret);
			return ret;
		}
	}
#endif
	subdev = zocl_find_pdev("ert_hw");
	if (subdev) {
		DRM_INFO("ert_hw found: 0x%llx\n", (uint64_t)(uintptr_t)subdev);
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (res) {
			zdev->res_start = res->start;
		}

		zdev->res_start = res->start;
		zdev->ert = (struct zocl_ert_dev *)platform_get_drvdata(subdev);
		//ert_hw is present only for PCIe + PS devices (ex: U30,VCK5000
		//Dont enable new kds for those devices
	}

	/* Work around for CR-1119382 issue.
	 * ZOCL driver is crashing if it accessing the device tree node */
	if (ZOCL_PLATFORM_ARM64) {
		/* For Non PR platform, there is not need to have FPGA manager
		 * For PR platform, the FPGA manager is required. No good way to
		 * determin if it is a PR platform at probe.
		 */
		fnode = of_find_node_by_name(NULL,
				     zdev->zdev_data_info->fpga_driver_name);
		if(!fnode) {
			fnode = of_find_node_by_name(NULL,
				     zdev->zdev_data_info->fpga_driver_new_name);
		}

		if (fnode) {
			zdev->fpga_mgr = of_fpga_mgr_get(fnode);
			if (IS_ERR(zdev->fpga_mgr))
				zdev->fpga_mgr = NULL;
			DRM_INFO("FPGA programming device %s founded.\n",
				 zdev->zdev_data_info->fpga_driver_name);
			of_node_put(fnode);
		}
	}

	/* Initialize Aperture */
	ret = zocl_aperture_init(zdev);
	if (ret)
		goto err_apt;

	/* Initialzie Slot */
	ret = zocl_pr_slot_init(zdev, pdev);
	if (ret)
		goto err_drm;

	/* Initialzie IOMMU */
	if (iommu_present(&platform_bus_type)) {
		/*
		 * Note: we ignore the return value of zocl_iommu_init().
		 * In the case of failing to initialize iommu, zocl
		 * driver will keep working with iommu disabled.
		 */
		(void) zocl_iommu_init(zdev, pdev);
	}

	platform_set_drvdata(pdev, zdev);

	/* Work around for CR-1119382 issue.
	 * ZOCL driver is crashing if it accessing the device tree node */
	if (ZOCL_PLATFORM_ARM64) {
		sscanf(XRT_DRIVER_VERSION, "%d.%d.%d",
		       &zocl_driver.major,
		       &zocl_driver.minor,
		       &zocl_driver.patchlevel);
		sscanf(XRT_DATE, "%d-%d-%d ", &year, &mon, &day);
		//e.g HASH_DATE ==> Wed, 4 Nov 2020 08:46:44 -0800
		//e.g XRT_DATE ==> 2020-11-04
		snprintf(driver_date, sizeof(driver_date),
			 "%d%02d%02d", year, mon, day);
	}
	/* Create and register DRM device */
	drm = drm_dev_alloc(&zocl_driver, &pdev->dev);
	if (IS_ERR(drm)) {
		ret = PTR_ERR(drm);
		goto err_drm;
	}

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_sysfs;

	/* During attach, we don't request dma channel */
	zdev->zdev_dma_chan = NULL;

	/* doen with zdev initialization */
	drm->dev_private = zdev;
	zdev->ddev       = drm;

	ret = zocl_init_error(zdev);
	if (ret)
		goto err_sysfs;

	/* Initial sysfs */
	rwlock_init(&zdev->attr_rwlock);
	ret = zocl_init_sysfs(drm->dev);
	if (ret)
		goto err_err;

	/* Now initial kds */
	ret = zocl_init_sched(zdev);
	if (ret)
		goto err_sched;

	return 0;

/* error out in exact reverse order of init */
err_sched:
	zocl_fini_sysfs(drm->dev);
err_err:
	zocl_fini_error(zdev);
err_sysfs:
	ZOCL_DRM_DEV_PUT(drm);
err_drm:
	zocl_pr_slot_fini(zdev);
err_apt:
	zocl_aperture_fini(zdev);
	return ret;
}

/*
 *
 * Exit Xilinx openCL DRM platform device.
 *
 * @param        pdev: Platform Device Instance
 *
 * @return       0 on success, Error code on failure.
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void zocl_drm_platform_remove(struct platform_device *pdev)
#else
static int zocl_drm_platform_remove(struct platform_device *pdev)
#endif
{
	struct drm_zocl_dev *zdev = platform_get_drvdata(pdev);
	struct drm_device *drm = zdev->ddev;

	/* Cleanup of iommu domain, if exists */
	if (zdev->domain) {
		iommu_detach_device(zdev->domain, drm->dev);
		iommu_domain_free(zdev->domain);
	}

	/* If dma channel has been requested, make sure it is released */
	if (zdev->zdev_dma_chan) {
		dma_release_channel(zdev->zdev_dma_chan);
		zdev->zdev_dma_chan = NULL;
	}

	if (zdev->fpga_mgr)
		fpga_mgr_put(zdev->fpga_mgr);

	zocl_clear_mem(zdev);
	mutex_destroy(&zdev->mm_lock);
	zocl_pr_slot_fini(zdev);
	zdev->slot_mask = 0;
	zocl_ert_destroy_intc(zdev->cu_intc);
	zocl_fini_sysfs(drm->dev);
	zocl_fini_error(zdev);

	zocl_fini_sched(zdev);

	zocl_aperture_fini(zdev);

	drm_dev_unregister(drm);
	ZOCL_DRM_DEV_PUT(drm);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	return 0;
#endif
}

static struct platform_driver zocl_drm_private_driver = {
	.probe			= zocl_drm_platform_probe,
	.remove			= zocl_drm_platform_remove,
	.driver			= {
		.name	        = "zocl-drm",
		.of_match_table	= zocl_drm_of_match,
	},
};

static struct platform_driver *drivers[] = {
	&zocl_ospi_versal_driver,
	&cu_driver,
	&scu_driver,
	&zocl_csr_intc_driver,
	&zocl_irq_intc_driver,
	&zocl_cu_xgq_driver,
	&zocl_drm_private_driver,
	&zocl_ctrl_ert_driver,
	&zocl_rpu_channel_driver,
};

static int __init zocl_init(void)
{
	int ret = 0, total = ARRAY_SIZE(drivers), i;

	/* HACK: fix ert driver. */
	if (!enable_xgq_ert) {
		for (i = 0; i < total && ret >= 0; i++) {
			if (drivers[i] == &zocl_ctrl_ert_driver) {
				drivers[i] = &zocl_ert_driver;
				break;
			}
		}
	}

	for (i = 0; i < total && ret >= 0; i++)
		ret = platform_driver_register(drivers[i]);
	if (ret >= 0)
		return 0;

	/* Failed to register some drivers, undo. */
	while (--i >= 0)
		platform_driver_unregister(drivers[i]);
	return ret;
}
module_init(zocl_init);

static void __exit zocl_exit(void)
{
	int i = ARRAY_SIZE(drivers);

	while (--i >= 0)
		platform_driver_unregister(drivers[i]);
}
module_exit(zocl_exit);

MODULE_VERSION(XRT_DRIVER_VERSION);

MODULE_DESCRIPTION(ZOCL_DRIVER_DESC);
MODULE_AUTHOR("Sonal Santan <sonal.santan@xilinx.com>");
MODULE_LICENSE("GPL");
