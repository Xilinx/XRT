/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include "version.h"
#include "../lib/libxdma_api.h"
#include "common.h"
#if RHEL_P2P_SUPPORT
#include <linux/pfn_t.h>
#endif

#if defined(__PPC64__)
#define XOCL_FILE_PAGE_OFFSET   0x10000
#else
#define XOCL_FILE_PAGE_OFFSET   0x100000
#endif

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#ifdef _XOCL_DRM_DEBUG
#define DRM_ENTER(fmt, args...)          \
        printk(KERN_INFO "[DRM] Entering %s:"fmt"\n", __func__, ##args)
#define DRM_DBG(fmt, args...)          \
        printk(KERN_INFO "[DRM] %s:%d:"fmt"\n", __func__,__LINE__, ##args)
#else
#define DRM_ENTER(fmt, args...)
#define DRM_DBG(fmt, args...)
#endif

static char driver_date[9];

static void xocl_free_object(struct drm_gem_object *obj)
{
	DRM_ENTER("");
	xocl_drm_free_bo(obj);
}

static int xocl_open(struct inode *inode, struct file *filp)
{
	struct xocl_drm *drm_p;
	struct drm_file *priv;
	struct drm_device *ddev;
	int ret;

	ret = drm_open(inode, filp);
	if (ret)
		return ret;

	priv = filp->private_data;
	ddev = priv->minor->dev;
	drm_p = xocl_drvinst_open(ddev);
	if (!drm_p)
		return -ENXIO;

	return 0;
}

static int xocl_release(struct inode *inode, struct file *filp)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *ddev = priv->minor->dev;
	struct xocl_drm	*drm_p = ddev->dev_private;
	int ret;

	ret = drm_release(inode, filp);
	xocl_drvinst_close(drm_p);

	return ret;
}

static int xocl_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct mm_struct *mm = current->mm;
	struct xocl_drm *drm_p = dev->dev_private;
	xdev_handle_t xdev = drm_p->xdev;
	unsigned long vsize;
	phys_addr_t res_start;

	DRM_ENTER("vm pgoff %lx", vma->vm_pgoff);

	/*
 	 * If the page offset is > than 4G, then let GEM handle that and do what
 	 * it thinks is best,we will only handle page offsets less than 4G.
 	 */
	if (likely(vma->vm_pgoff >= XOCL_FILE_PAGE_OFFSET)) {
		ret = drm_gem_mmap(filp, vma);
		if (ret)
			return ret;
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

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	vsize = vma->vm_end - vma->vm_start;
	if (vsize > XDEV(xdev)->bar_size)
		return -EINVAL;

	DRM_DBG("MAP size %ld", vsize);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;

	res_start = pci_resource_start(XDEV(xdev)->pdev, XDEV(xdev)->bar_idx);
	ret = io_remap_pfn_range(vma, vma->vm_start,
				 res_start >> PAGE_SHIFT,
				 vsize, vma->vm_page_prot);
	userpf_info(xdev, "io_remap_pfn_range ret code: %d", ret);

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
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
#if RHEL_P2P_SUPPORT
	pfn_t pfn;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
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

	if(xobj->type & XOCL_BO_P2P){
#if RHEL_P2P_SUPPORT
		pfn = phys_to_pfn_t(page_to_phys(xobj->pages[page_offset]), PFN_MAP|PFN_DEV);
		ret = vm_insert_mixed(vma, vmf_address, pfn);
#else
		ret = vm_insert_page(vma, vmf_address, xobj->pages[page_offset]);
#endif
  }
  else{
  	ret = vm_insert_page(vma, vmf_address, xobj->pages[page_offset]);
  }
	switch (ret) {
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
	struct xocl_drm	*drm_p = dev->dev_private;
	int	ret = 0;

	DRM_ENTER("");

	/* We do not allow users to open PRIMARY node, /dev/dri/cardX node.
	 * Users should only open RENDER, /dev/dri/renderX node */
	if (drm_is_primary_client(filp))
		return -EPERM;

	if (get_live_client_size(drm_p->xdev) > XOCL_MAX_CONCURRENT_CLIENTS)
		return -EBUSY;

	ret = xocl_exec_create_client(drm_p->xdev, &filp->driver_priv);
	if (ret)
		goto failed;

	return 0;

failed:
	return ret;
}

static void xocl_client_release(struct drm_device *dev, struct drm_file *filp)
{
	struct xocl_drm	*drm_p = dev->dev_private;

	xocl_exec_destroy_client(drm_p->xdev, &filp->driver_priv);
}

static uint xocl_poll(struct file *filp, poll_table *wait)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct xocl_drm	*drm_p = dev->dev_private;

	BUG_ON(!priv->driver_priv);

	DRM_ENTER("");
	return xocl_exec_poll_client(drm_p->xdev, filp, wait,priv->driver_priv);
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
	DRM_IOCTL_DEF_DRV(XOCL_HOT_RESET, xocl_hot_reset_ioctl,
		  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(XOCL_RECLOCK, xocl_reclock_ioctl,
	  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
};

static long xocl_drm_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long arg)
{
	return drm_ioctl(filp, cmd, arg);
}

static const struct file_operations xocl_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= xocl_open,
	.mmap		= xocl_mmap,
	.poll		= xocl_poll,
	.read		= drm_read,
	.unlocked_ioctl = xocl_drm_ioctl,
	.release	= xocl_release,
};

static const struct vm_operations_struct xocl_vm_ops = {
	.fault = xocl_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static struct drm_driver mm_drm_driver = {
	.driver_features		= DRIVER_GEM | DRIVER_PRIME |
						DRIVER_RENDER,

	.postclose                      = xocl_client_release,
	.open                           = xocl_client_open,

	.gem_free_object		= xocl_free_object,
	.gem_vm_ops			= &xocl_vm_ops,

	.ioctls				= xocl_ioctls,
	.num_ioctls			= ARRAY_SIZE(xocl_ioctls),
	.fops				= &xocl_driver_fops,

	.gem_prime_get_sg_table         = xocl_gem_prime_get_sg_table,
	.gem_prime_import_sg_table      = xocl_gem_prime_import_sg_table,
	.gem_prime_vmap                 = xocl_gem_prime_vmap,
	.gem_prime_vunmap               = xocl_gem_prime_vunmap,
	.gem_prime_mmap			= xocl_gem_prime_mmap,

	.prime_handle_to_fd		= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle		= drm_gem_prime_fd_to_handle,
	.gem_prime_import		= drm_gem_prime_import,
	.gem_prime_export		= drm_gem_prime_export,
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)))
	.set_busid                      = drm_pci_set_busid,
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

	drm_p = xocl_drvinst_alloc(ddev->dev, sizeof(*drm_p));
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

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,4,0)
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
		drm_dev_unref(ddev);
	if (drm_p)
		xocl_drvinst_free(drm_p);

	return NULL;
}

void xocl_drm_fini(struct xocl_drm *drm_p)
{
	xocl_cleanup_mem(drm_p);
	drm_put_dev(drm_p->ddev);
	mutex_destroy(&drm_p->mm_lock);

	xocl_drvinst_free(drm_p);
}

void xocl_mm_get_usage_stat(struct xocl_drm *drm_p, u32 ddr,
	struct drm_xocl_mm_stat *pstat)
{
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

int xocl_mm_insert_node(struct xocl_drm *drm_p, u32 ddr,
                struct drm_mm_node *node, u64 size)
{
	return drm_mm_insert_node_generic(drm_p->mm[ddr], node, size, PAGE_SIZE,
#if defined(XOCL_DRM_FREE_MALLOC)
		0, 0);
#else
		0, 0, 0);
#endif
}

int xocl_check_topology(struct xocl_drm *drm_p)
{
	struct mem_topology    *topology;
	u16     i;
	int     err = 0;

	topology = XOCL_MEM_TOPOLOGY(drm_p->xdev);
	if (topology == NULL)
		return 0;

	for (i = 0; i < topology->m_count; i++) {
		if (!topology->m_mem_data[i].m_used)
			continue;

		if (topology->m_mem_data[i].m_type == MEM_STREAMING)
			continue;

		if (drm_p->mm_usage_stat[i]->bo_count != 0) {
			err = -EPERM;
			xocl_err(drm_p->ddev->dev,
				       	"The ddr %d has pre-existing buffer "
					"allocations, please exit and re-run.",
					i);
		}
	}

	return err;
}

uint32_t xocl_get_shared_ddr(struct xocl_drm *drm_p, struct mem_data *m_data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	struct xocl_mm_wrapper *wrapper;
	uint64_t start_addr = m_data->m_base_address;
	uint64_t sz = m_data->m_size*1024;

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

void xocl_cleanup_mem(struct xocl_drm *drm_p)
{
	struct mem_topology *topology;
	u16 i, ddr;
	uint64_t addr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	struct xocl_mm_wrapper *wrapper;
	struct hlist_node *tmp;
#endif

	topology = XOCL_MEM_TOPOLOGY(drm_p->xdev);
	if (topology) {
		ddr = topology->m_count;
		for (i = 0; i < ddr; i++) {
			if (!topology->m_mem_data[i].m_used)
				continue;

			if (topology->m_mem_data[i].m_type == MEM_STREAMING)
				continue;

			xocl_info(drm_p->ddev->dev, "Taking down DDR : %d", i);
			addr = topology->m_mem_data[i].m_base_address;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
			hash_for_each_possible_safe(drm_p->mm_range, wrapper,
					tmp, node, addr) {
				if (wrapper->ddr == i) {
					hash_del(&wrapper->node);
					vfree(wrapper);
					drm_mm_takedown(drm_p->mm[i]);
					vfree(drm_p->mm[i]);
					vfree(drm_p->mm_usage_stat[i]);
				}
			}
#endif
			drm_p->mm[i] = NULL;
			drm_p->mm_usage_stat[i] = NULL;
		}
	}
	vfree(drm_p->mm);
	drm_p->mm = NULL;
	vfree(drm_p->mm_usage_stat);
	drm_p->mm_usage_stat = NULL;
	vfree(drm_p->mm_p2p_off);
	drm_p->mm_p2p_off = NULL;
}

int xocl_init_mem(struct xocl_drm *drm_p)
{
	size_t length = 0;
	size_t mm_size = 0, mm_stat_size = 0;
	size_t size = 0, wrapper_size = 0;
	size_t ddr_bank_size;
	struct mem_topology *topo;
	struct mem_data *mem_data;
	uint32_t shared;
	struct xocl_mm_wrapper *wrapper = NULL;
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

	topo = XOCL_MEM_TOPOLOGY(drm_p->xdev);
	if (topo == NULL)
		return 0;

	length = topo->m_count * sizeof(struct mem_data);
	size = topo->m_count * sizeof(void *);
	wrapper_size = sizeof(struct xocl_mm_wrapper);
	mm_size = sizeof(struct drm_mm);
	mm_stat_size = sizeof(struct drm_xocl_mm_stat);

	xocl_info(drm_p->ddev->dev, "Topology count = %d, data_length = %ld",
		topo->m_count, length);

	drm_p->mm = vzalloc(size);
	drm_p->mm_usage_stat = vzalloc(size);
	drm_p->mm_p2p_off = vzalloc(topo->m_count * sizeof(u64));
	if (!drm_p->mm || !drm_p->mm_usage_stat || !drm_p->mm_p2p_off) {
		err = -ENOMEM;
		goto failed;
	}

	for (i = 0; i < topo->m_count; i++) {
		mem_data = &topo->m_mem_data[i];
		ddr_bank_size = mem_data->m_size * 1024;

		xocl_info(drm_p->ddev->dev, "  Mem Index %d", i);
		xocl_info(drm_p->ddev->dev, "  Base Address:0x%llx",
			mem_data->m_base_address);
		xocl_info(drm_p->ddev->dev, "  Size:0x%lx", ddr_bank_size);
		xocl_info(drm_p->ddev->dev, "  Type:%d", mem_data->m_type);
		xocl_info(drm_p->ddev->dev, "  Used:%d", mem_data->m_used);
	}

	/* Initialize the used banks and their sizes */
	/* Currently only fixed sizes are supported */
	for (i = 0; i < topo->m_count; i++) {
		mem_data = &topo->m_mem_data[i];
		if (!mem_data->m_used)
			continue;

		if (mem_data->m_type == MEM_STREAMING ||
			mem_data->m_type == MEM_STREAMING_CONNECTION)
			continue;

		ddr_bank_size = mem_data->m_size * 1024;
		xocl_info(drm_p->ddev->dev, "Allocating DDR bank%d", i);
		xocl_info(drm_p->ddev->dev, "  base_addr:0x%llx, total size:0x%lx",
			mem_data->m_base_address, ddr_bank_size);

		if (XOCL_DSA_IS_MPSOC(drm_p->xdev)) {
			reserved_end = mem_data->m_base_address + ddr_bank_size;
			reserved_start = reserved_end - reserved1 - reserved2;
			xocl_info(drm_p->ddev->dev, "  reserved region:0x%llx - 0x%llx",
				reserved_start, reserved_end - 1);
		}

		shared = xocl_get_shared_ddr(drm_p, mem_data);
		if (shared != 0xffffffff) {
			xocl_info(drm_p->ddev->dev, "Found duplicated memory region!");
			drm_p->mm[i] = drm_p->mm[shared];
			drm_p->mm_usage_stat[i] = drm_p->mm_usage_stat[shared];
			continue;
		}

		xocl_info(drm_p->ddev->dev, "Found a new memory region");
		wrapper = vzalloc(wrapper_size);
		drm_p->mm[i] = vzalloc(mm_size);
		drm_p->mm_usage_stat[i] = vzalloc(mm_stat_size);

		if (!drm_p->mm[i] || !drm_p->mm_usage_stat[i] || !wrapper) {
			err = -ENOMEM;
			goto failed;
		}

		wrapper->start_addr = mem_data->m_base_address;
		wrapper->size = mem_data->m_size*1024;
		wrapper->mm = drm_p->mm[i];
		wrapper->mm_usage_stat = drm_p->mm_usage_stat[i];
		wrapper->ddr = i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
		hash_add(drm_p->mm_range, &wrapper->node, wrapper->start_addr);
#endif

		drm_mm_init(drm_p->mm[i], mem_data->m_base_address,
				ddr_bank_size - reserved1 - reserved2);
		drm_p->mm_p2p_off[i] = ddr_bank_size * i;

		xocl_info(drm_p->ddev->dev, "drm_mm_init called");
	}

	return 0;

failed:
	vfree(wrapper);
	if (drm_p->mm) {
		for (; i >= 0; i--) {
			drm_mm_takedown(drm_p->mm[i]);
			vfree(drm_p->mm[i]);
			vfree(drm_p->mm_usage_stat[i]);
		}
		vfree(drm_p->mm);
		drm_p->mm = NULL;
	}
	vfree(drm_p->mm_usage_stat);
	drm_p->mm_usage_stat = NULL;
	vfree(drm_p->mm_p2p_off);
	drm_p->mm_p2p_off = NULL;

	return err;
}
