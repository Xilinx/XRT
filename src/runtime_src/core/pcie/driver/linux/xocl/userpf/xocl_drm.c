/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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

static char driver_date[9];

static int xocl_cleanup_mem_nolock(struct xocl_drm *drm_p, uint32_t slot_id);
static int xocl_cleanup_memory_manager(struct xocl_drm *drm_p);
static int xocl_init_drm_memory_manager(struct xocl_drm *drm_p);

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

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0) && !defined(RHEL_9_5_GE)
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
#else
	vm_flags_clear(vma, VM_PFNMAP | VM_IO);
	vm_flags_set(vma, VM_MIXEDMAP | mm->def_flags);
#endif

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

	if (vsize > XDEV(xdev)->bar_size) {
		userpf_err(xdev,
			"bad size (0x%lx) for native BAR mmap", vsize);
		return -EINVAL;
	}

	if (vma->vm_pgoff != 0) {
		int ret;
		u32 cu_addr;
		u32 cu_idx = vma->vm_pgoff - 1;

		ret = xocl_cu_map_addr(xdev, cu_idx, priv, vsize, &cu_addr);
		if (ret != 0)
			return ret;
		res_start += cu_addr;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0) && !defined(RHEL_9_5_GE)
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;
#else
	vm_flags_set(vma, VM_IO | VM_RESERVED);
#endif

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

static bool is_mem_region_valid(struct xocl_drm *drm_p,
		struct mem_data *mem_data)
{
	struct xocl_dev *xdev = drm_p->xdev;
	void *blob;
	int offset;
	const u64 *prop;
	u64 start, end, mem_start, mem_end;
	const char *ipname;
	int found = 0;

	if (!XOCL_DSA_IS_MPSOC(xdev) && !XOCL_DSA_IS_VERSAL(xdev))
		return true;

	/* PLRAM does not have to be accessed by PS */
	if (convert_mem_tag(mem_data->m_tag) == MEM_TAG_PLRAM)
		return true;

	blob = XDEV(xdev)->fdt_blob;
	if (!blob)
		return true;

	for (offset = fdt_next_node(blob, -1, NULL);
	    offset >= 0;
	    offset = fdt_next_node(blob, offset, NULL)) {
		ipname = fdt_get_name(blob, offset, NULL);
		if (ipname && strncmp(ipname, NODE_RESERVED_PSMEM,
		    strlen(NODE_RESERVED_PSMEM)))
			continue;

		found = 1;

		prop = fdt_getprop(blob, offset, PROP_IO_OFFSET, NULL);
		if (!prop)
			continue;

		start = be64_to_cpu(prop[0]);
		end = start + be64_to_cpu(prop[1]);
		mem_start = mem_data->m_base_address;
		mem_end = mem_start + mem_data->m_size * 1024;

		/*
		 * Memory region in mem_topology needs to match or
		 * be inside the PS reserved memory region for U30.
		 * Restriction relaxed for Versal
		 */
		if ((mem_start >= start && mem_end <= end) || (XOCL_DSA_IS_VERSAL(xdev)))
			return true;
	}

	if (!found)
		return true;

	xocl_err(drm_p->ddev->dev,
	    "Topology memory range does not match reserved PS memory\n");

	return false;
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

	/*
	 * It looks vm_insert_mixed() is the newer interface to handle different
	 * types of page. We may consider to only use this interface when the old
	 * kernel support is dropped.
	 */
	if (xocl_bo_p2p(xobj) || xocl_bo_import(xobj)) {
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
	} else if (xocl_bo_cma(xobj) || xocl_bo_userptr(xobj)) {
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

	ret = xocl_create_client(drm_p->xdev, &filp->driver_priv);
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

	xocl_destroy_client(drm_p->xdev, &filp->driver_priv);
	xocl_p2p_mem_reclaim(drm_p->xdev);
	xocl_drvinst_close(drm_p);
}

static uint xocl_poll(struct file *filp, poll_table *wait)
{
	struct drm_file *priv = filp->private_data;

	BUG_ON(!priv->driver_priv);
	DRM_ENTER("");
	return xocl_poll_client(filp, wait, priv->driver_priv);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0) || defined(RHEL_9_5_GE)
/* This was removed in 6.8 */
#define DRM_UNLOCKED 0
#endif

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
	DRM_IOCTL_DEF_DRV(XOCL_CREATE_HW_CTX, xocl_create_hw_ctx_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_DESTROY_HW_CTX, xocl_destroy_hw_ctx_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_OPEN_CU_CTX, xocl_open_cu_ctx_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_CLOSE_CU_CTX, xocl_close_cu_ctx_ioctl,
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
	DRM_IOCTL_DEF_DRV(XOCL_HW_CTX_EXECBUF, xocl_hw_ctx_execbuf_ioctl,
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
	DRM_IOCTL_DEF_DRV(XOCL_SET_CU_READONLY_RANGE, xocl_set_cu_read_only_range_ioctl,
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0) && !defined(RHEL_8_5_GE)
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
		.gem_free_object_unlocked       = xocl_free_object,
	#else
		.gem_free_object		= xocl_free_object,
	#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0) && !defined(RHEL_8_5_GE)
        .gem_vm_ops                     = &xocl_vm_ops,
        .gem_prime_get_sg_table         = xocl_gem_prime_get_sg_table,
        .gem_prime_vmap                 = xocl_gem_prime_vmap,
        .gem_prime_vunmap               = xocl_gem_prime_vunmap,
        .gem_prime_export               = drm_gem_prime_export,
#endif

	.ioctls				= xocl_ioctls,
	.num_ioctls			= (ARRAY_SIZE(xocl_ioctls)-NUM_KERNEL_IOCTLS),
	.fops				= &xocl_driver_fops,

	.gem_prime_import_sg_table	= xocl_gem_prime_import_sg_table,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) && !defined(RHEL_9_4_GE)
	.gem_prime_mmap			= xocl_gem_prime_mmap,
#endif

	.prime_handle_to_fd		= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle		= drm_gem_prime_fd_to_handle,
	.gem_prime_import		= drm_gem_prime_import,
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)))
	.set_busid			= drm_pci_set_busid,
#endif
	.name				= XOCL_MODULE_NAME,
	.desc				= XOCL_DRIVER_DESC,
	.date				= driver_date,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0) || defined(RHEL_8_5_GE)
const struct drm_gem_object_funcs xocl_gem_object_funcs = {
        .free = xocl_free_object,
        .vm_ops = &xocl_vm_ops,
        .get_sg_table = xocl_gem_prime_get_sg_table,
        .vmap = xocl_gem_prime_vmap,
        .vunmap = xocl_gem_prime_vunmap,
        .export = drm_gem_prime_export,
};
#endif

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

	/*
	 * The pdev field was removed from drm_device starting from 5.14 and
	 * should be skipped starting from that version.
	 * https://github.com/torvalds/linux/commit/b347e04452ff6382ace8fba9c81f5bcb63be17a6
	 */
#if defined(RHEL_RELEASE_VERSION)
#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 6)
	ddev->pdev = XDEV(xdev_hdl)->pdev;
#endif
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
	ddev->pdev = XDEV(xdev_hdl)->pdev;
#endif
#endif

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
	INIT_LIST_HEAD(&drm_p->mem_list_head);
	ddev->dev_private = drm_p;

	xocl_drvinst_set_filedev(drm_p, ddev);
	xocl_drvinst_set_offline(drm_p, false);

        ret = xocl_init_drm_memory_manager(drm_p);
	if (ret) {
		xocl_xdev_err(xdev_hdl, "Init DRM Memory manager failed 0x%x", ret);
                goto failed;
	}

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

	xocl_cleanup_mem_all(drm_p);
	mutex_lock(&drm_p->mm_lock);
	xocl_cleanup_memory_manager(drm_p);
	mutex_unlock(&drm_p->mm_lock);

	drm_put_dev(drm_p->ddev);
	mutex_destroy(&drm_p->mm_lock);

	xocl_drvinst_free(hdl);
}

void xocl_mm_get_usage_stat(struct xocl_drm *drm_p, u32 ddr,
	struct drm_xocl_mm_stat *pstat)
{
	struct drm_xocl_mm_stat *mm_stat = &drm_p->mm_usage_stat[ddr];

	if (!pstat) {
        	xocl_err(drm_p->ddev->dev, "Invalid memory %d stats", ddr);
		return;
	}

	pstat->memory_usage = mm_stat->is_used ? mm_stat->memory_usage : 0;
	pstat->bo_count = mm_stat->is_used ? mm_stat->bo_count : 0;
}

void xocl_mm_update_usage_stat(struct xocl_drm *drm_p, u32 ddr,
	u64 size, int count)
{
	struct drm_xocl_mm_stat *mm_stat = &drm_p->mm_usage_stat[ddr];

	if (!mm_stat->is_used) {
        	xocl_dbg(drm_p->ddev->dev, "Invalid memory %d stats", ddr);
		return;
	}

	mm_stat->memory_usage += (count > 0) ? size : -size;
	mm_stat->bo_count += count;
}

static int xocl_mm_insert_node_range_all(struct xocl_drm *drm_p, uint32_t *mem_id,
		struct mem_topology *grp_topology, struct drm_mm_node *dnode, u64 size)
{
	struct mem_data *mem_data = NULL;
	struct mem_data *ps_mem_data = &drm_p->ps_mem_data;
	struct xocl_mm *xocl_mm = drm_p->xocl_mm;
	uint64_t start_addr = 0;
	uint64_t end_addr = 0;
	int ret = 0;
	bool phy_bank_exists = false;
	int i = 0;

	BUG_ON(!xocl_mm && !xocl_mm->mm);

	for (i = 0; i < grp_topology->m_count; i++) {
		mem_data = &grp_topology->m_mem_data[i];
		if ((convert_mem_tag(mem_data->m_tag) == MEM_TAG_HOST) ||
				XOCL_IS_PS_KERNEL_MEM(grp_topology, i))
			continue;

		if (ps_mem_data->m_used) {
			/* Check whether PS memory falls on this Bank or not */
			if ((ps_mem_data->m_base_address >= mem_data->m_base_address) &&
				(ps_mem_data->m_size <= mem_data->m_size * 1024)) {
				start_addr = ps_mem_data->m_base_address;
				end_addr = start_addr + ps_mem_data->m_size * 1024;
			}
			else
				continue;
		}
		else {
			start_addr = mem_data->m_base_address;
			end_addr = start_addr + mem_data->m_size * 1024;
		}
		phy_bank_exists = true;

#if defined(XOCL_DRM_FREE_MALLOC)
		ret = drm_mm_insert_node_in_range(xocl_mm->mm, dnode, size, PAGE_SIZE, 0,
				start_addr, end_addr, 0);
#else
		ret = drm_mm_insert_node_in_range(xocl_mm->mm, dnode, size, PAGE_SIZE,
				start_addr, end_addr, 0);
#endif
		if (!ret) {
			// Memory is allocated to this Bank
			*mem_id = i;
			return 0;
		}
	}

    /* If no physical memory BANKs exists till now then
     * allocate memory from the base address of the memory manager.
     */
    if (!phy_bank_exists && ps_mem_data->m_used) {
        start_addr = ps_mem_data->m_base_address;
        end_addr = start_addr + ps_mem_data->m_size * 1024;

#if defined(XOCL_DRM_FREE_MALLOC)
        ret = drm_mm_insert_node_in_range(xocl_mm->mm, dnode, size, PAGE_SIZE, 0,
                start_addr, end_addr, 0);
#else
        ret = drm_mm_insert_node_in_range(xocl_mm->mm, dnode, size, PAGE_SIZE,
                start_addr, end_addr, 0);
#endif
        if (!ret) {
            // Memory is allocated to this Bank
            *mem_id = 0;
            return 0;
        }
    }

    return ret;
}

static int xocl_mm_insert_node_range(struct xocl_drm *drm_p,
		struct mem_data *mem_data, struct drm_mm_node *node, u64 size)
{
	struct xocl_mm *xocl_mm = drm_p->xocl_mm;
	uint64_t start_addr = 0;
	uint64_t end_addr = 0;
	int ret = 0;

	BUG_ON(!xocl_mm && !xocl_mm->mm);
	start_addr = mem_data->m_base_address;
	end_addr = start_addr + mem_data->m_size * 1024;

#if defined(XOCL_DRM_FREE_MALLOC)
	ret = drm_mm_insert_node_in_range(xocl_mm->mm, node, size, PAGE_SIZE, 0,
					   start_addr, end_addr, 0);
#else
	ret = drm_mm_insert_node_in_range(xocl_mm->mm, node, size, PAGE_SIZE,
					   start_addr, end_addr, 0);
#endif

	return ret;
}

int xocl_mm_insert_node(struct xocl_drm *drm_p, unsigned memidx,
			uint32_t slotidx, struct drm_xocl_bo *xobj, u64 size)
{
	int ret = 0;
	struct drm_mm_node *node = xobj->mm_node;
	struct xocl_mem_stat *curr_mem = NULL;
	struct mem_topology *grp_topology = NULL;

	BUG_ON(!mutex_is_locked(&drm_p->mm_lock));
        if (drm_p->xocl_mm->mm == NULL)
                return -EINVAL;

	ret = XOCL_GET_GROUP_TOPOLOGY(drm_p->xdev, grp_topology, slotidx);
        if (ret)
                return 0;

	if (grp_topology->m_mem_data[memidx].m_type == MEM_PS_KERNEL) {
		/* For PS kernel case the memidx is specified will be dummy.
		 * The memory will be created from the actual Banks. Hence,
		 * memidx will be updated accordingly.
		 */
		ret = xocl_mm_insert_node_range_all(drm_p, &memidx,
				grp_topology, node, size);
	}
	else {
		ret = xocl_mm_insert_node_range(drm_p,
				&grp_topology->m_mem_data[memidx], node, size);
	}

        XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev, slotidx);
	if (!ret) {
		/* Update memory manager stats for whole device */
		xocl_mm_update_usage_stat(drm_p,
				memidx, size, 1);
		/* Update slot specific stats */
		list_for_each_entry(curr_mem, &drm_p->mem_list_head, link) {
			if ((slotidx == curr_mem->slot_idx) &&
					(memidx == curr_mem->mem_idx)) {
				curr_mem->mm_usage_stat.memory_usage += size;
				curr_mem->mm_usage_stat.bo_count += 1;
			}
		}
	}
	/* Record the DDR we allocated the buffer on */
	xobj->mem_idx = memidx;

        return ret;
}

static int xocl_check_slot_topology(struct xocl_drm *drm_p, uint32_t slot_id)
{
	struct xocl_mem_stat *curr_mem = NULL;
	int err = 0;

	if (list_empty(&drm_p->mem_list_head))
		return 0;

	list_for_each_entry(curr_mem, &drm_p->mem_list_head, link) {
		if (slot_id != curr_mem->slot_idx)
			continue;
		if (curr_mem->mm_usage_stat.bo_count != 0) {
			err = -EPERM;
			xocl_err(drm_p->ddev->dev,
				"The ddr %d has pre-existing buffer allocations,"
				" for slot %d, please exit and re-run.",
				 curr_mem->mem_idx, curr_mem->slot_idx);
		}
	}

	return err;
}

int xocl_check_topology(struct xocl_drm *drm_p)
{
	int err = 0;
	uint32_t slot_id = 0;

	if (list_empty(&drm_p->mem_list_head))
		return 0;

	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
		err = xocl_check_slot_topology(drm_p, slot_id);
		if (err)
			break;
	}

	return err;
}

static int xocl_cleanup_mem_nolock(struct xocl_drm *drm_p, uint32_t slot_id)
{
	int err = 0;
	struct xocl_mem_stat *curr_mem = NULL;
	struct xocl_mem_stat *next = NULL;

	BUG_ON(!mutex_is_locked(&drm_p->mm_lock));

	err = xocl_check_slot_topology(drm_p, slot_id);
	if (err)
		return err;

	if (list_empty(&drm_p->mem_list_head))
		goto done;

	list_for_each_entry_safe(curr_mem, next, &drm_p->mem_list_head,
			link) {
		if (slot_id != curr_mem->slot_idx)
			continue;

		list_del(&curr_mem->link);
		vfree(curr_mem);
	}

done:
	return 0;
}

static int xocl_set_cma_bank(struct xocl_drm *drm_p, uint64_t base_addr, size_t ddr_bank_size)
{
	int ret = 0;
	uint64_t *phys_addrs = 0;
	uint64_t entry_num = 0, entry_sz = 0, host_reserve_size = 0;
	struct xocl_dev *xdev = (struct xocl_dev *)drm_p->xdev;

	if (!xdev->cma_bank) {
		xocl_warn(drm_p->ddev->dev, "Could not find reserved HOST mem, Skipped");
		return 0;
	}

	phys_addrs = xdev->cma_bank->phys_addrs;
	entry_num = xdev->cma_bank->entry_num;
	entry_sz = xdev->cma_bank->entry_sz;
	ret = xocl_addr_translator_set_page_table(drm_p->xdev, phys_addrs, entry_sz, entry_num);
	if (ret)
		return ret;

	host_reserve_size = xocl_addr_translator_get_host_mem_size(drm_p->xdev);

	ddr_bank_size = min(ddr_bank_size, (size_t)host_reserve_size);

	return xocl_addr_translator_enable_remap(drm_p->xdev, base_addr, ddr_bank_size);
}

int xocl_cleanup_mem(struct xocl_drm *drm_p, uint32_t slot_id)
{
	int ret;

	mutex_lock(&drm_p->mm_lock);
	ret = xocl_cleanup_mem_nolock(drm_p, slot_id);
	mutex_unlock(&drm_p->mm_lock);

	return ret;
}

int xocl_cleanup_mem_all(struct xocl_drm *drm_p)
{
	int ret = 0;
	uint32_t slot_id = 0;

	mutex_lock(&drm_p->mm_lock);

	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
		ret = xocl_cleanup_mem_nolock(drm_p, slot_id);
		if (ret)
			break;
	}

	mutex_unlock(&drm_p->mm_lock);
	return ret;
}

static int xocl_cleanup_drm_memory_manager(struct xocl_mm *xocl_mm)
{
	if (!xocl_mm)
		return 0;

	if (xocl_mm->bo_usage_stat)
		vfree(xocl_mm->bo_usage_stat);

	if (xocl_mm->mm) {
		drm_mm_takedown(xocl_mm->mm);
		vfree(xocl_mm->mm);
	}

	vfree(xocl_mm);

	return 0;
}

static int xocl_init_drm_mm(struct xocl_drm *drm_p, struct xocl_mm *xocl_mm)
{
	int err = 0;
	int i = 0;

	BUG_ON(!mutex_is_locked(&drm_p->mm_lock));

	if (!xocl_mm)
		return -EINVAL;

	/* Initialize memory status for the memory manager */
	xocl_mm->bo_usage_stat = vzalloc(XOCL_BO_USAGE_TOTAL *
			sizeof(struct drm_xocl_mm_stat));
	if (!xocl_mm->bo_usage_stat) {
		err = -ENOMEM;
		goto error;
	}

        for (i = 0; i < MAX_MEM_BANK_COUNT; i++)
		drm_p->mm_usage_stat[i].is_used = false;

	/* Initialize the memory manager */
	xocl_mm->mm = vzalloc(sizeof(struct drm_mm));
	if (!xocl_mm->mm) {
		err = -ENOMEM;
		goto error;
	}

	/* Initialize with max and min possible value */
	drm_mm_init(xocl_mm->mm, 0, U64_MAX);
	xocl_info(drm_p->ddev->dev, "drm_mm_init called for the maximum memory range possible");

	return 0;

error:
	xocl_cleanup_drm_memory_manager(xocl_mm);
	return err;
}

static int xocl_init_drm_memory_manager(struct xocl_drm *drm_p)
{
	struct xocl_mm *xocl_mm = NULL;
        int err = 0;

	mutex_lock(&drm_p->mm_lock);
	xocl_mm = vzalloc(sizeof(struct xocl_mm));
        if (!xocl_mm) {
		mutex_unlock(&drm_p->mm_lock);
                return -ENOMEM;
	}

	err = xocl_init_drm_mm(drm_p, xocl_mm);
	if (err)
		goto error;

	drm_p->xocl_mm = xocl_mm;

	err = xocl_p2p_mem_init(drm_p->xdev);
	if (err && err != -ENODEV)
		xocl_err(drm_p->ddev->dev,
				"init p2p mem failed, err %d", err);

error:
	if (err && err != -ENODEV)
		xocl_cleanup_drm_memory_manager(xocl_mm);

	mutex_unlock(&drm_p->mm_lock);
	return err;
}

static int xocl_cleanup_memory_manager(struct xocl_drm *drm_p)
{
	struct xocl_mm *xocl_mm = NULL;
	int err = 0;

	BUG_ON(!mutex_is_locked(&drm_p->mm_lock));

	xocl_mm = drm_p->xocl_mm;
	if (!xocl_mm)
		return 0;

	err = xocl_check_topology(drm_p);
	if (err)
		return err;

	/* Now cleanup the P2P memory */
        xocl_p2p_mem_cleanup(drm_p->xdev);

        /* cleanup the memory manager */
	xocl_cleanup_drm_memory_manager(xocl_mm);
	drm_p->xocl_mm = NULL;

        return 0;
}

int xocl_init_mem(struct xocl_drm *drm_p, uint32_t slot_id)
{
	size_t ddr_bank_size;
	struct mem_topology *group_topo = NULL;
	struct xocl_mem_stat *mem_stat = NULL;
	struct mem_data *mem_data = NULL;
	uint64_t reserved1 = 0;
	uint64_t reserved2 = 0;
	uint64_t reserved_start;
	uint64_t reserved_end;
	int err = 0;
	int i = -1;

	if (XOCL_DSA_IS_MPSOC(drm_p->xdev)) {
		/* TODO: This is still hardcoding.. */
		reserved1 = 0x80000000;
		reserved2 = 0x1000000;
	}

	mutex_lock(&drm_p->mm_lock);
	drm_p->cma_bank_idx = -1;

	/* Initialize memory stats based on Group topology for this xclbin */
	err = XOCL_GET_GROUP_TOPOLOGY(drm_p->xdev, group_topo, slot_id);
	if (err) {
		mutex_unlock(&drm_p->mm_lock);
		return err;
	}

	for (i = 0; i < group_topo->m_count; i++) {
		mem_data = &group_topo->m_mem_data[i];
		ddr_bank_size = mem_data->m_size * 1024;
		xocl_info(drm_p->ddev->dev, "Memory Bank: %s", mem_data->m_tag);
		xocl_info(drm_p->ddev->dev, "  Base Address:0x%llx",
			mem_data->m_base_address);
		xocl_info(drm_p->ddev->dev, "  Size:0x%lx", ddr_bank_size);
		xocl_info(drm_p->ddev->dev, "  Type:%d", mem_data->m_type);
		xocl_info(drm_p->ddev->dev, "  Used:%d", mem_data->m_used);

		if (XOCL_IS_P2P_MEM(group_topo, i)) {
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

		if (XOCL_IS_STREAM(group_topo, i))
			continue;

		if (XOCL_IS_PS_KERNEL_MEM(group_topo, i))
			continue;

		if (!is_mem_region_valid(drm_p, mem_data))
			continue;

		xocl_info(drm_p->ddev->dev, "   Initializing Memory Bank: %s", mem_data->m_tag);
		xocl_info(drm_p->ddev->dev, "    base_addr:0x%llx, total size:0x%lx",
			mem_data->m_base_address, ddr_bank_size);

		if (convert_mem_tag(mem_data->m_tag) == MEM_TAG_HOST) {
			drm_p->cma_bank_idx = i;
			err = xocl_set_cma_bank(drm_p, mem_data->m_base_address, ddr_bank_size);
			if (err) {
				xocl_err(drm_p->ddev->dev,
					"Run host_mem to setup host memory access, request 0x%lx bytes",
					ddr_bank_size);
				goto done;
			}
		}

		if (XOCL_DSA_IS_MPSOC(drm_p->xdev)) {
			reserved_end = mem_data->m_base_address + ddr_bank_size;
			reserved_start = reserved_end - reserved1 - reserved2;
			xocl_info(drm_p->ddev->dev, "  reserved region:0x%llx - 0x%llx",
				reserved_start, reserved_end - 1);
		}

		mem_stat = vzalloc(sizeof(struct xocl_mem_stat));
		if (!mem_stat) {
			err = -ENOMEM;
			goto done;
		}

		mem_stat->mem_idx = i;
		mem_stat->slot_idx = slot_id;
		list_add_tail(&mem_stat->link, &drm_p->mem_list_head);
		drm_p->mm_usage_stat[i].is_used = true;
	}

done:
	XOCL_PUT_GROUP_TOPOLOGY(drm_p->xdev, slot_id);

	if (err)
		xocl_cleanup_mem_nolock(drm_p, slot_id);

	mutex_unlock(&drm_p->mm_lock);
	xocl_info(drm_p->ddev->dev, "ret %d", err);

	return err;
}
