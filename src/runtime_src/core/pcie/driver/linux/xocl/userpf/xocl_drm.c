/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors: Jan Stephan <j.stephan@hzdr.de>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 0, 0)
#include <drm/drm_backport.h>
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) || \
	defined(RHEL_RELEASE_VERSION)
#include <linux/pfn_t.h>
#endif
#include <linux/pagemap.h>
#include <linux/log2.h>
#include <linux/mmzone.h>

#include "version.h"
#include "../lib/libxdma_api.h"
#include "common.h"

#ifndef SZ_4G
#define SZ_4G	_AC(0x100000000, ULL)
#endif

#define XOCL_FILE_PAGE_OFFSET	(SZ_4G / PAGE_SIZE)

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#ifdef _XOCL_DRM_DEBUG
#define DRM_ENTER(fmt, args...)		 \
	printk(KERN_INFO "[DRM] Entering %s:"fmt"\n", __func__, ##args)
#define DRM_DBG(fmt, args...)	       \
	printk(KERN_INFO "[DRM] %s:%d:"fmt"\n", __func__, __LINE__, ##args)
#else
#define DRM_ENTER(fmt, args...)
#define DRM_DBG(fmt, args...)
#endif

extern int kds_mode;

static char driver_date[9];

static void xocl_free_object(struct drm_gem_object *obj)
{
	DRM_ENTER("");
	xocl_drm_free_bo(obj);
}

static int xocl_bo_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	struct drm_xocl_bo *xobj;
	struct mm_struct *mm = current->mm;

	DRM_ENTER("BO map pgoff 0x%lx, size 0x%lx",
		vma->vm_pgoff, vma->vm_end - vma->vm_start);

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	xobj = to_xocl_bo(vma->vm_private_data);

	if (!xobj->pages) {
		XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);
		return -EINVAL;
	}
	/* Clear VM_PFNMAP flag set by drm_gem_mmap()
	 * we have "struct page" for all backing pages for bo
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	/* Clear VM_IO flag set by drm_gem_mmap()
	 * it prevents gdb from accessing mapped buffers
	 */
	vma->vm_flags &= ~VM_IO;
	vma->vm_flags |= VM_MIXEDMAP;
	vma->vm_flags |= mm->def_flags;
	vma->vm_pgoff = 0;

	/* Override pgprot_writecombine() mapping setup by
	 * drm_gem_mmap()
	 * which results in very poor read performance
	 */
	if (vma->vm_flags & (VM_READ | VM_MAYREAD))
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	else
		vma->vm_page_prot = pgprot_writecombine(
			vm_get_page_prot(vma->vm_flags));

	return ret;
}

static int xocl_native_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	unsigned long vsize;
	phys_addr_t res_start;

	/*
	 * HACK -- We assume filp->private_data is pointing to
	 * drm_file data structure.
	 */
	struct drm_file *priv = filp->private_data;
	struct xocl_drm *drm_p = priv->minor->dev->dev_private;
	xdev_handle_t xdev = drm_p->xdev;

	if (vma->vm_pgoff > MAX_CUS) {
		userpf_err(xdev, "invalid native mmap offset: 0x%lx",
			vma->vm_pgoff);
		return -EINVAL;
	}

	vsize = vma->vm_end - vma->vm_start;
	res_start = pci_resource_start(XDEV(xdev)->pdev, XDEV(xdev)->bar_idx);

	if (vma->vm_pgoff == 0) {
		if (vsize > XDEV(xdev)->bar_size) {
			userpf_err(xdev,
				"bad size (0x%lx) for native BAR mmap", vsize);
			return -EINVAL;
		}
	} else {
		int ret;
		u32 cu_addr;
		u32 cu_idx = vma->vm_pgoff - 1;

		if (vsize > 64 * 1024) {
			userpf_err(xdev,
				"bad size (0x%lx) for native CU mmap", vsize);
			return -EINVAL;
		}
		if (kds_mode)
			ret = xocl_cu_map_addr(xdev, cu_idx, priv, &cu_addr);
		else
			ret = xocl_exec_cu_map_addr(xdev, cu_idx, priv, &cu_addr);
		if (ret != 0)
			return ret;
		res_start += cu_addr;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;

	ret = io_remap_pfn_range(vma, vma->vm_start,
				 res_start >> PAGE_SHIFT,
				 vsize, vma->vm_page_prot);
	if (ret != 0) {
		userpf_err(xdev, "io_remap_pfn_range failed: %d", ret);
		return ret;
	}

	userpf_info(xdev, "successful native mmap @0x%lx with size 0x%lx",
		vma->vm_pgoff >> PAGE_SHIFT, vsize);
	return ret;
}

static int xocl_mmap(struct file *filp, struct vm_area_struct *vma)
{
	/*
	 * If the offset is > than 4G, then let GEM handle that and do what
	 * it thinks is best, we will only handle offsets less than 4G.
	 */
	if (likely(vma->vm_pgoff >= XOCL_FILE_PAGE_OFFSET))
		return xocl_bo_mmap(filp, vma);

	/*
	 * Native BAR or CU mmap handling.
	 * When pgoff is 0, we perform mmap of the PCIE BAR.
	 * When pgoff is non-zero, we treat it as CU index + 1 and perform
	 * mmap of that particular CU register space.
	 */
	return xocl_native_mmap(filp, vma);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
vm_fault_t xocl_gem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
int xocl_gem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
#else
int xocl_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
#endif
	struct drm_xocl_bo *xobj = to_xocl_bo(vma->vm_private_data);
	loff_t num_pages;
	unsigned int page_offset;
	int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	unsigned long vmf_address = vmf->address;
#else
	unsigned long vmf_address = (unsigned long)vmf->virtual_address;
#endif
	page_offset = (vmf_address - vma->vm_start) >> PAGE_SHIFT;


	if (!xobj->pages)
		return VM_FAULT_SIGBUS;

	num_pages = DIV_ROUND_UP(xobj->base.size, PAGE_SIZE);
	if (page_offset > num_pages)
		return VM_FAULT_SIGBUS;

	if (xocl_bo_p2p(xobj)) {
#ifdef RHEL_RELEASE_VERSION
		pfn_t pfn;
		pfn = phys_to_pfn_t(page_to_phys(xobj->pages[page_offset]), PFN_MAP|PFN_DEV);
#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 2)
		ret = vm_insert_mixed(vma, vmf_address, pfn);
#else
		ret = vmf_insert_mixed(vma, vmf_address, pfn);
#endif
#else
		ret = vm_insert_page(vma, vmf_address, xobj->pages[page_offset]);
#endif
	} else if (xocl_bo_cma(xobj)) {
/*  vm_insert_page does not allow driver to insert anonymous pages.
 *  Instead, we call vm_insert_mixed.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || defined(RHEL_RELEASE_VERSION)
		pfn_t pfn;
		pfn = phys_to_pfn_t(page_to_phys(xobj->pages[page_offset]), PFN_MAP);
#if defined(RHEL_RELEASE_VERSION)
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 2)
		ret = vmf_insert_mixed(vma, vmf_address, pfn);
#else
		ret = vm_insert_mixed(vma, vmf_address, pfn);
#endif
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
                ret = vmf_insert_mixed(vma, vmf_address, pfn);
#else
                ret = vm_insert_mixed(vma, vmf_address, pfn);
#endif
#endif
#else
		ret = vm_insert_mixed(vma, vmf_address, page_to_pfn(xobj->pages[page_offset]));
#endif
	} else {
		ret = vm_insert_page(vma, vmf_address, xobj->pages[page_offset]);
	}

	/**
	 * vmf_*** functions returning VM_FAULT_XXX values.(all positive values)
	 * vm_*** functions returning 0 on success and errno on failure. (Zero or negative)
	 */
	if (ret > 0) {
		/* Comes here only in vmf_*** case. */
		return ret;
	}

	/**
	 *  Comes here only in vm_*** case.
	 *  When two threads enter this function at the same time, the first thread will
	 *  successfully insert the page. The second thread will call the insert page,
	 *  but gets back -EBUSY (-16) since the page has already been inserted. So should
	 *  treat -EBUSY as success
	 */
	switch (ret) {
	case -EBUSY:
	case -EAGAIN:
	case 0:
	case -ERESTARTSYS:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

static int xocl_client_open(struct drm_device *dev, struct drm_file *filp)
{
	struct xocl_drm	*drm_p;
	int	ret = 0;

	DRM_ENTER("");

	/* We do not allow users to open PRIMARY node, /dev/dri/cardX node.
	 * Users should only open RENDER, /dev/dri/renderX node
	 */
	if (drm_is_primary_client(filp))
		return -EPERM;

	drm_p = xocl_drvinst_open(dev);
	if (!drm_p) {
		return -ENXIO;
	}

	if (kds_mode == 1)
		ret = xocl_create_client(drm_p->xdev, &filp->driver_priv);
	else
		ret = xocl_exec_create_client(drm_p->xdev, &filp->driver_priv);
	if (ret) {
		xocl_drvinst_close(drm_p);
		goto failed;
	}

	return 0;

failed:
	return ret;
}

static void xocl_client_release(struct drm_device *dev, struct drm_file *filp)
{
	struct xocl_drm	*drm_p = dev->dev_private;

	if (kds_mode == 1)
		xocl_destroy_client(drm_p->xdev, &filp->driver_priv);
	else
		xocl_exec_destroy_client(drm_p->xdev, &filp->driver_priv);
	xocl_p2p_mem_reclaim(drm_p->xdev);
	xocl_drvinst_close(drm_p);
}

static uint xocl_poll(struct file *filp, poll_table *wait)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct xocl_drm	*drm_p = dev->dev_private;

	BUG_ON(!priv->driver_priv);

	DRM_ENTER("");
	if (kds_mode == 1)
		return xocl_poll_client(filp, wait, priv->driver_priv);
	else
		return xocl_exec_poll_client(drm_p->xdev, filp, wait, priv->driver_priv);
}

static const struct drm_ioctl_desc xocl_ioctls[] = {
	DRM_IOCTL_DEF_DRV(XOCL_CREATE_BO, xocl_create_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_USERPTR_BO, xocl_userptr_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_MAP_BO, xocl_map_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_SYNC_BO, xocl_sync_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_INFO_BO, xocl_info_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_PWRITE_BO, xocl_pwrite_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_PREAD_BO, xocl_pread_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_CTX, xocl_ctx_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_INFO, xocl_info_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_READ_AXLF, xocl_read_axlf_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_PWRITE_UNMGD, xocl_pwrite_unmgd_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_PREAD_UNMGD, xocl_pread_unmgd_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_USAGE_STAT, xocl_usage_stat_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_USER_INTR, xocl_user_intr_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_EXECBUF, xocl_execbuf_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_COPY_BO, xocl_copy_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_HOT_RESET, xocl_hot_reset_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_RECLOCK, xocl_reclock_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_ALLOC_CMA, xocl_alloc_cma_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_FREE_CMA, xocl_free_cma_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),

/* LINUX KERNEL-SPACE IOCTLS - The following entries are meant to be
 * accessible only from Linux Kernel and need be grouped to at the end
 * of this array.
 * New IOCTLS meant for Userspace access needs to be defined above these
 * comments.
 **/
#define NUM_KERNEL_IOCTLS 4
	DRM_IOCTL_DEF_DRV(XOCL_KINFO_BO, xocl_kinfo_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_MAP_KERN_MEM, xocl_map_kern_mem_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_EXECBUF_CB, xocl_execbuf_callback_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_SYNC_BO_CB, xocl_sync_bo_callback_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
};

static long xocl_drm_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long arg)
{
	return drm_ioctl(filp, cmd, arg);
}

static const struct file_operations xocl_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= xocl_mmap,
	.poll		= xocl_poll,
	.read		= drm_read,
	.unlocked_ioctl = xocl_drm_ioctl,
	.release	= drm_release,
};

static const struct vm_operations_struct xocl_vm_ops = {
	.fault = xocl_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static struct drm_driver mm_drm_driver = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
#if defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 3)
        .driver_features                = DRIVER_GEM | DRIVER_RENDER,
#else
	.driver_features		= DRIVER_GEM | DRIVER_PRIME |
						DRIVER_RENDER,
#endif
#else
        .driver_features                = DRIVER_GEM | DRIVER_PRIME |
                                                DRIVER_RENDER,
#endif
#else
	.driver_features		= DRIVER_GEM | DRIVER_RENDER,
#endif

	.postclose			= xocl_client_release,
	.open				= xocl_client_open,

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	.gem_free_object_unlocked       = xocl_free_object,
#else
	.gem_free_object		= xocl_free_object,
#endif
	.gem_vm_ops			= &xocl_vm_ops,

	.ioctls				= xocl_ioctls,
	.num_ioctls			= (ARRAY_SIZE(xocl_ioctls)-NUM_KERNEL_IOCTLS),
	.fops				= &xocl_driver_fops,

	.gem_prime_get_sg_table		= xocl_gem_prime_get_sg_table,
	.gem_prime_import_sg_table	= xocl_gem_prime_import_sg_table,
	.gem_prime_vmap			= xocl_gem_prime_vmap,
	.gem_prime_vunmap		= xocl_gem_prime_vunmap,
	.gem_prime_mmap			= xocl_gem_prime_mmap,

	.prime_handle_to_fd		= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle		= drm_gem_prime_fd_to_handle,
	.gem_prime_import		= drm_gem_prime_import,
	.gem_prime_export		= drm_gem_prime_export,
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)))
	.set_busid			= drm_pci_set_busid,
#endif
	.name				= XOCL_MODULE_NAME,
	.desc				= XOCL_DRIVER_DESC,
	.date				= driver_date,
};

void *xocl_drm_init(xdev_handle_t xdev_hdl)
{
	struct xocl_drm		*drm_p = NULL;
	struct drm_device	*ddev = NULL;
	int			year, mon, day;
	int			ret = 0;
	bool			drm_registered = false;

	sscanf(XRT_DRIVER_VERSION, "%d.%d.%d",
		&mm_drm_driver.major,
		&mm_drm_driver.minor,
		&mm_drm_driver.patchlevel);
	sscanf(xrt_build_version_date, "%d-%d-%d ", &year, &mon, &day);
	snprintf(driver_date, sizeof(driver_date),
		"%d%02d%02d", year, mon, day);

	ddev = drm_dev_alloc(&mm_drm_driver, &XDEV(xdev_hdl)->pdev->dev);
	if (!ddev) {
		xocl_xdev_err(xdev_hdl, "alloc drm dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	drm_p = xocl_drvinst_alloc(&XDEV(xdev_hdl)->pdev->dev, sizeof(*drm_p));
	if (!drm_p) {
		xocl_xdev_err(xdev_hdl, "alloc drm inst failed");
		ret = -ENOMEM;
		goto failed;
	}
	drm_p->xdev = xdev_hdl;

	ddev->pdev = XDEV(xdev_hdl)->pdev;

	ret = drm_dev_register(ddev, 0);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "register drm dev failed 0x%x", ret);
		goto failed;
	}
	drm_registered = true;

	drm_p->ddev = ddev;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 4, 0)
	ret = drm_dev_set_unique(ddev, dev_name(ddev->dev));
	if (ret) {
		xocl_xdev_err(xdev_hdl, "set unique name failed 0x%x", ret);
		goto failed;
	}
#endif

	mutex_init(&drm_p->mm_lock);
	ddev->dev_private = drm_p;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	hash_init(drm_p->mm_range);
#endif

	xocl_drvinst_set_filedev(drm_p, ddev);
	return drm_p;

failed:
	if (drm_registered)
		drm_dev_unregister(ddev);
	if (ddev)
		XOCL_DRM_DEV_PUT(ddev);
	if (drm_p)
		xocl_drvinst_release(drm_p, NULL);

	return NULL;
}

void xocl_drm_fini(struct xocl_drm *drm_p)
{
	void *hdl;

	xocl_drvinst_release(drm_p, &hdl);

	xocl_cleanup_mem(drm_p);
	drm_put_dev(drm_p->ddev);
	mutex_destroy(&drm_p->mm_lock);

	xocl_drvinst_free(hdl);
}

void xocl_mm_get_usage_stat(struct xocl_drm *drm_p, u32 ddr,
	struct drm_xocl_mm_stat *pstat)
{
	if (!drm_p->mm_usage_stat)
		return;

	pstat->memory_usage = drm_p->mm_usage_stat[ddr] ?
		drm_p->mm_usage_stat[ddr]->memory_usage : 0;
	pstat->bo_count = drm_p->mm_usage_stat[ddr] ?
		drm_p->mm_usage_stat[ddr]->bo_count : 0;
}

void xocl_mm_update_usage_stat(struct xocl_drm *drm_p, u32 ddr,
	u64 size, int count)
{
	BUG_ON(!drm_p->mm_usage_stat[ddr]);

	drm_p->mm_usage_stat[ddr]->memory_usage += (count > 0) ? size : -size;
	drm_p->mm_usage_stat[ddr]->bo_count += count;
}

int xocl_mm_insert_node_range(struct xocl_drm *drm_p, u32 mem_id,
			      struct drm_mm_node *node, u64 size)
{
	uint64_t start_addr = 0;
	uint64_t end_addr = 0;
	struct mem_topology *grp_topology = NULL;
	int err = 0;
	BUG_ON(!mutex_is_locked(&drm_p->mm_lock));

	err = XOCL_GET_GROUP_TOPOLOGY(drm_p->xdev, grp_topology);
	if (err)
		return 0;

	if (drm_p->mm == NULL)
		return -EINVAL;

	start_addr = grp_topology->m_mem_data[mem_id].m_base_address;
	end_addr = start_addr + grp_topology->m_mem_data[mem_id].m_size * 1024;

#if defined(XOCL_DRM_FREE_MALLOC)
	err = drm_mm_insert_node_in_range(drm_p->mm, node, size, PAGE_SIZE, 0,
					   start_addr, end_addr, 0);
#else
	err = drm_mm_insert_node_in_range(drm_p->mm, node, size, PAGE_SIZE,
					   start_addr, end_addr, 0);
#endif

	XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev);
	return err;

}

int xocl_mm_insert_node(struct xocl_drm *drm_p, u32 ddr,
			struct drm_mm_node *node, u64 size)
{
	BUG_ON(!mutex_is_locked(&drm_p->mm_lock));
	if (drm_p->mm == NULL)
		return -EINVAL;

	return drm_mm_insert_node_generic(drm_p->mm, node, size, PAGE_SIZE,
#if defined(XOCL_DRM_FREE_MALLOC)
		0, 0);
#else
		0, 0, 0);
#endif
}

int xocl_check_topology(struct xocl_drm *drm_p)
{
	struct mem_topology    *group_topology = NULL;
	u16	i;
	int	err = 0;

	err = XOCL_GET_GROUP_TOPOLOGY(drm_p->xdev, group_topology);
	if (err)
		return 0;

	if (group_topology == NULL)
		goto done;

	if (!drm_p->mm_usage_stat)
		goto done;

	for (i = 0; i < group_topology->m_count; i++) {
		if (!group_topology->m_mem_data[i].m_used)
			continue;

		if (XOCL_IS_STREAM(group_topology, i))
			continue;

		if (!drm_p->mm_usage_stat[i])
			continue;

		if (drm_p->mm_usage_stat[i]->bo_count != 0) {
			err = -EPERM;
			xocl_err(drm_p->ddev->dev,
				 "The ddr %d has pre-existing buffer allocations, please exit and re-run.",
				 i);
		}
	}

done:
	XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev);
	return err;
}

uint32_t xocl_get_shared_ddr(struct xocl_drm *drm_p, struct mem_data *m_data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	struct xocl_mm_wrapper *wrapper = NULL;
	uint64_t start_addr = m_data->m_base_address;
	uint64_t sz = m_data->m_size*1024;

	BUG_ON(!drm_p->mm_range);
	BUG_ON(!m_data);

	hash_for_each_possible(drm_p->mm_range, wrapper, node, start_addr) {
		if (!wrapper)
			continue;

		if (wrapper->start_addr == start_addr) {
			if (wrapper->size == sz)
				return wrapper->ddr;
			else
				return 0xffffffff;
		}
	}
#endif
	return 0xffffffff;
}

int xocl_cleanup_mem_nolock(struct xocl_drm *drm_p)
{
	int err;
	struct mem_topology *topology = NULL;
	struct mem_topology *group_topology = NULL;
	u16 i, ddr;
	uint64_t addr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	struct xocl_mm_wrapper *wrapper;
	struct hlist_node *tmp;
#endif

	BUG_ON(!mutex_is_locked(&drm_p->mm_lock));

	err = xocl_check_topology(drm_p);
	if (err)
		return err;

	err = XOCL_GET_MEM_TOPOLOGY(drm_p->xdev, topology);
	if (err)
		goto done;

	if (topology) {
		xocl_p2p_mem_cleanup(drm_p->xdev);
		ddr = topology->m_count;
		for (i = 0; i < ddr; i++) {
			if (!topology->m_mem_data[i].m_used)
				continue;

			if (XOCL_IS_STREAM(topology, i))
				continue;

			if (IS_HOST_MEM(topology->m_mem_data[i].m_tag))
				xocl_addr_translator_disable_remap(drm_p->xdev);

			xocl_info(drm_p->ddev->dev, "Taking down DDR : %d", i);
			addr = topology->m_mem_data[i].m_base_address;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
			hash_for_each_possible_safe(drm_p->mm_range, wrapper,
					tmp, node, addr) {
				if (wrapper->ddr != i)
					continue;
				hash_del(&wrapper->node);
				vfree(wrapper);
			}
#endif
		}
	}
	XOCL_PUT_MEM_TOPOLOGY(drm_p->xdev);

	/* Cleanup stats based on group_topology */
	err = XOCL_GET_GROUP_TOPOLOGY(drm_p->xdev, group_topology);
	if (err)
		goto done;

	if (group_topology) {
		for (i = 0; i < group_topology->m_count; i++) {
			if (!group_topology->m_mem_data[i].m_used)
				continue;

			if (XOCL_IS_STREAM(group_topology, i))
				continue;

			if (drm_p->mm_usage_stat && drm_p->mm_usage_stat[i]) {
				vfree(drm_p->mm_usage_stat[i]);
				drm_p->mm_usage_stat[i] = NULL;
			}
		}
		vfree(drm_p->mm_usage_stat);
		drm_p->mm_usage_stat = NULL;
	}
	XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev);
done:
	if (drm_p->mm) {
		drm_mm_takedown(drm_p->mm);
	vfree(drm_p->mm);
	}
	drm_p->mm = NULL;

	return 0;
}

int xocl_set_cma_bank(struct xocl_drm *drm_p, uint64_t base_addr, size_t ddr_bank_size)
{
	return xocl_addr_translator_enable_remap(drm_p->xdev, base_addr, ddr_bank_size);
}

int xocl_cleanup_mem(struct xocl_drm *drm_p)
{
	int ret;
	mutex_lock(&drm_p->mm_lock);
	ret = xocl_cleanup_mem_nolock(drm_p);
	mutex_unlock(&drm_p->mm_lock);
	return ret;
}

int xocl_init_mem(struct xocl_drm *drm_p)
{
	size_t length = 0;
	size_t mm_stat_size = 0;
	size_t size = 0, wrapper_size = 0;
	size_t ddr_bank_size;
	struct mem_topology *topo = NULL;
	struct mem_topology *group_topo = NULL;
	struct mem_data *mem_data;
	uint32_t shared;
	struct xocl_mm_wrapper *wrapper = NULL;
	uint64_t mm_start_addr = 0;
	uint64_t mm_end_addr = 0;
	uint64_t reserved1 = 0;
	uint64_t reserved2 = 0;
	uint64_t reserved_start;
	uint64_t reserved_end, host_reserve_size;
	int err = 0;
	int i = -1;

	if (XOCL_DSA_IS_MPSOC(drm_p->xdev)) {
		/* TODO: This is still hardcoding.. */
		reserved1 = 0x80000000;
		reserved2 = 0x1000000;
	}

	err = XOCL_GET_MEM_TOPOLOGY(drm_p->xdev, topo);
	if (err)
		return err;

	if (topo == NULL) {
		err = -ENODEV;
		XOCL_PUT_MEM_TOPOLOGY(drm_p->xdev);
		return err;
	}

	length = topo->m_count * sizeof(struct mem_data);
	wrapper_size = sizeof(struct xocl_mm_wrapper);
	mm_stat_size = sizeof(struct drm_xocl_mm_stat);
	xocl_info(drm_p->ddev->dev, "Topology count = %d, data_length = %ld",
		topo->m_count, length);

	mutex_lock(&drm_p->mm_lock);

	err = xocl_p2p_mem_init(drm_p->xdev);
	if (err && err != -ENODEV) {
		xocl_err(drm_p->ddev->dev,
			"init p2p mem failed, err %d", err);
		goto done;
	}
	err = 0;

	/* Initialize memory stats based on Group topology */
	err = XOCL_GET_GROUP_TOPOLOGY(drm_p->xdev, group_topo);
	if (err) {
		XOCL_PUT_MEM_TOPOLOGY(drm_p->xdev);
		mutex_unlock(&drm_p->mm_lock);
		return err;
	}

	if (group_topo == NULL) {
		err = -ENODEV;
		XOCL_PUT_MEM_TOPOLOGY(drm_p->xdev);
		XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev);
		mutex_unlock(&drm_p->mm_lock);
		return err;
	}

	size = group_topo->m_count * sizeof(void *);
	drm_p->mm_usage_stat = vzalloc(size);
	if (!drm_p->mm_usage_stat) {
		err = -ENOMEM;
		XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev);
		goto done;
	}

	drm_p->cma_bank_idx = -1;
	for (i = 0; i < group_topo->m_count; i++) {
		mem_data = &group_topo->m_mem_data[i];
		ddr_bank_size = mem_data->m_size * 1024;
		xocl_info(drm_p->ddev->dev, "  Memory Bank: %s", mem_data->m_tag);
		xocl_info(drm_p->ddev->dev, "  Base Address:0x%llx",
			mem_data->m_base_address);
		xocl_info(drm_p->ddev->dev, "  Size:0x%lx", ddr_bank_size);
		xocl_info(drm_p->ddev->dev, "  Type:%d", mem_data->m_type);
		xocl_info(drm_p->ddev->dev, "  Used:%d", mem_data->m_used);
		if (IS_HOST_MEM(mem_data->m_tag))
			drm_p->cma_bank_idx = i;

		if (!group_topo->m_mem_data[i].m_used)
			continue;

		if (XOCL_IS_STREAM(group_topo, i))
			continue;

		drm_p->mm_usage_stat[i] = vzalloc(mm_stat_size);
		if (!drm_p->mm_usage_stat[i]) {
			err = -ENOMEM;
			XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev);
			goto done;
		}
	}

	XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev);
	/* Initialize with max and min possible value */
	mm_start_addr = 0xffffFFFFffffFFFF;
	mm_end_addr = 0;

	/* Initialize the used banks and their sizes */
	/* Currently only fixed sizes are supported */
	for (i = 0; i < topo->m_count; i++) {
		mem_data = &topo->m_mem_data[i];
		ddr_bank_size = mem_data->m_size * 1024;

		if (XOCL_IS_P2P_MEM(topo, i)) {
			if (mem_data->m_used) {
				xocl_p2p_mem_map(drm_p->xdev,
				    mem_data->m_base_address,
				    ddr_bank_size, 0, 0, NULL);
			} else {
				xocl_p2p_mem_map(drm_p->xdev, ~0UL,
				     ddr_bank_size, 0, 0, NULL);
			}
		}

		if (!mem_data->m_used)
			continue;

		if (XOCL_IS_STREAM(topo, i))
			continue;

		xocl_info(drm_p->ddev->dev, "Allocating Memory Bank: %s", mem_data->m_tag);
		xocl_info(drm_p->ddev->dev, "  base_addr:0x%llx, total size:0x%lx",
			mem_data->m_base_address, ddr_bank_size);

		/* Update the start and end address for the memory manager */
		if (mem_data->m_base_address < mm_start_addr)
			mm_start_addr = mem_data->m_base_address;
		if ((mem_data->m_base_address + ddr_bank_size) > mm_end_addr)
			mm_end_addr = mem_data->m_base_address + ddr_bank_size;

		if (XOCL_DSA_IS_MPSOC(drm_p->xdev)) {
			reserved_end = mem_data->m_base_address + ddr_bank_size;
			reserved_start = reserved_end - reserved1 - reserved2;
			xocl_info(drm_p->ddev->dev, "  reserved region:0x%llx - 0x%llx",
				reserved_start, reserved_end - 1);
		}

		shared = xocl_get_shared_ddr(drm_p, mem_data);
		if (shared != 0xffffffff) {
			xocl_info(drm_p->ddev->dev, "Found duplicated memory region!");
			continue;
		}

		xocl_info(drm_p->ddev->dev, "Found a new memory region");
		wrapper = vzalloc(wrapper_size);
		if (!wrapper) {
			err = -ENOMEM;
			goto done;
		}

		wrapper->start_addr = mem_data->m_base_address;
		wrapper->size = mem_data->m_size*1024;
		wrapper->ddr = i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
		hash_add(drm_p->mm_range, &wrapper->node, wrapper->start_addr);
#endif

		if (IS_HOST_MEM(mem_data->m_tag)) {
			host_reserve_size = xocl_addr_translator_get_host_mem_size(drm_p->xdev);

			ddr_bank_size = min(ddr_bank_size, (size_t)host_reserve_size);
			err = xocl_set_cma_bank(drm_p, mem_data->m_base_address, ddr_bank_size);
			if (err) {
				xocl_err(drm_p->ddev->dev, "Run host_mem to setup host memory access, request 0x%lx bytes", ddr_bank_size);
				goto done;
			}
		}
	}

    drm_p->mm = vzalloc(sizeof(struct drm_mm));
	if (!drm_p->mm) {
		err = -ENOMEM;
		goto done;
	}

    drm_mm_init(drm_p->mm, mm_start_addr, (mm_end_addr - mm_start_addr));
    xocl_info(drm_p->ddev->dev, "drm_mm_init called for the available memory range");

done:
	if (err)
		xocl_cleanup_mem_nolock(drm_p);
	XOCL_PUT_MEM_TOPOLOGY(drm_p->xdev);
	mutex_unlock(&drm_p->mm_lock);
	xocl_info(drm_p->ddev->dev, "ret %d", err);
	return err;
}
