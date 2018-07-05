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
#include "../lib/libxdma_api.h"
#include "common.h"

#define XOCL_FILE_PAGE_OFFSET   0x100000
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

static void xocl_free_object(struct drm_gem_object *obj)
{
	DRM_ENTER("");
	xocl_free_bo(obj);
}

static int xocl_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct mm_struct *mm = current->mm;
	struct xocl_dev	*xdev = dev->dev_private;
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
	if (vsize > xdev->bar_len)
		return -EINVAL;

	DRM_DBG("MAP size %ld", vsize);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;

	res_start = pci_resource_start(xdev->core.pdev, xdev->core.bar_idx);
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
	page_offset = (vmf->address - vma->vm_start) >> PAGE_SHIFT;
#else
	page_offset = ((unsigned long)vmf->virtual_address - vma->vm_start) >>
		PAGE_SHIFT;
#endif

	if (!xobj->pages)
		return VM_FAULT_SIGBUS;

	num_pages = DIV_ROUND_UP(xobj->base.size, PAGE_SIZE);
	if (page_offset > num_pages)
		return VM_FAULT_SIGBUS;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
	ret = vm_insert_page(vma, vmf->address, xobj->pages[page_offset]);
#else
	ret = vm_insert_page(vma, (unsigned long)vmf->virtual_address,
		xobj->pages[page_offset]);
#endif
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
	struct xocl_dev	*xdev = dev->dev_private;
	int	ret = 0;

	DRM_ENTER("");

	if (drm_is_primary_client(filp))
		return -EPERM;

	if (MB_SCHEDULER_DEV(xdev))
		ret = xocl_exec_create_client(xdev, &filp->driver_priv);
	return ret;
}

static void xocl_client_release(struct drm_device *dev, struct drm_file *filp)
{
	struct xocl_dev	*xdev = dev->dev_private;
	struct client_ctx *client = filp->driver_priv;

	DRM_ENTER("");

	if (!uuid_is_null(&xdev->xclbin_id)) {
		(void) xocl_icap_unlock_bitstream(xdev, &client->xclbin_id,
			pid_nr(task_tgid(current)));
	}

	if (MB_SCHEDULER_DEV(xdev))
		xocl_exec_destroy_client(xdev, &filp->driver_priv);
}

static uint xocl_poll(struct file *filp, poll_table *wait)
{
	uint result = 0;
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct xocl_dev	*xdev = dev->dev_private;

	BUG_ON(!priv->driver_priv);

	DRM_ENTER("");
	if (MB_SCHEDULER_DEV(xdev))
		result = xocl_exec_poll_client(xdev, filp, wait,
					       priv->driver_priv);
	return result;
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
};

static const struct file_operations xocl_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= xocl_mmap,
	.poll		= xocl_poll,
	.read		= drm_read,
	.unlocked_ioctl = drm_ioctl,
	.release	= drm_release,
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
	.date				= XOCL_DRIVER_DATE,
	.major				= XOCL_DRIVER_MAJOR,
	.minor				= XOCL_DRIVER_MINOR,
	.patchlevel			= XOCL_DRIVER_PATCHLEVEL,
};

int xocl_drm_init(struct xocl_dev *xdev)
{
	struct drm_device	*ddev = NULL;
	u64			segment = 0;
	int			i, ret = 0;
	u32			ddr_count = 0;
	u64			ddr_size;

	ddev = drm_dev_alloc(&mm_drm_driver, &xdev->core.pdev->dev);
	if (!ddev) {
		userpf_err(xdev, "alloc drm dev failed");
		ret = -ENOMEM;
		goto failed;
	}

	ddev->pdev = xdev->core.pdev;

	ret = drm_dev_register(ddev, 0);
	if (ret) {
		userpf_err(xdev, "register drm dev failed 0x%x", ret);
		goto failed;
	}

	xdev->ddev = ddev;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,4,0)
	ret = drm_dev_set_unique(ddev, dev_name(ddev->dev));
	if (ret) {
		userpf_err(xdev, "set unique name failed 0x%x", ret);
		goto failed;
	}
#endif

	if (!xocl_is_unified(xdev)) {
		userpf_info(xdev, "Non-unified platform");
		ddr_count = xocl_get_ddr_channel_count(xdev);
		ddr_size = xocl_get_ddr_channel_size(xdev);

		xdev->mm = devm_kzalloc(&xdev->core.pdev->dev,
			sizeof(struct drm_mm) * ddr_count, GFP_KERNEL);
		if (!xdev->mm) {
			userpf_err(xdev, "alloc mem failed, ddr %d, sz %lld",
				ddr_count, ddr_size);
			ret = -ENOMEM;
			goto failed;
		}
		xdev->mm_usage_stat = devm_kzalloc(&xdev->core.pdev->dev,
			sizeof(struct drm_xocl_mm_stat) * ddr_count,
			GFP_KERNEL);
		if (!xdev->mm_usage_stat) {
			userpf_err(xdev, "alloc stat failed, ddr %d, sz %lld",
				ddr_count, ddr_size);
			ret = -ENOMEM;
			goto failed;
		}

		for (i = 0; i < ddr_count; i++) {
			xdev->topology.m_data[i].m_used = 1;
			drm_mm_init(&xdev->mm[i], segment, ddr_size);
			segment += ddr_size;
		}
	}

	mutex_init(&xdev->stat_lock);
	mutex_init(&xdev->mm_lock);
	mutex_init(&xdev->ctx_list_lock);
	INIT_LIST_HEAD(&xdev->ctx_list);
	ddev->dev_private = xdev;
	atomic_set(&xdev->needs_reset,0);
	atomic_set(&xdev->outstanding_execs, 0);
	atomic64_set(&xdev->total_execs, 0);
	return 0;

failed:
	if (xdev->mm) {
	 	for (i = 0; i < ddr_count; i++) {
			drm_mm_takedown(&xdev->mm[i]);
		}
		devm_kfree(&xdev->core.pdev->dev, xdev->mm);
	}
	if (xdev->mm_usage_stat)
		devm_kfree(&xdev->core.pdev->dev, xdev->mm_usage_stat);

	if (!ddev)
		drm_dev_unref(ddev);

	return ret;
}

void xocl_drm_fini(struct xocl_dev *xdev)
{
	if (xdev->mm)
		devm_kfree(&xdev->core.pdev->dev, xdev->mm);
	if (xdev->mm_usage_stat)
		devm_kfree(&xdev->core.pdev->dev, xdev->mm_usage_stat);

	xocl_cleanup_mem(xdev);

	drm_put_dev(xdev->ddev);

	mutex_destroy(&xdev->ctx_list_lock);
	mutex_destroy(&xdev->stat_lock);
	mutex_destroy(&xdev->mm_lock);
}

void xocl_mm_get_usage_stat(struct xocl_dev *xdev, u32 ddr,
	struct drm_xocl_mm_stat *pstat)
{
	if (xdev->mm_usage_stat) {
		pstat->memory_usage = xdev->mm_usage_stat[ddr].memory_usage;
		pstat->bo_count = xdev->mm_usage_stat[ddr].bo_count;
	}
}

void xocl_mm_update_usage_stat(struct xocl_dev *xdev, u32 ddr,
	u64 size, int count)
{
	BUG_ON(!xdev->mm_usage_stat);

	xdev->mm_usage_stat[ddr].memory_usage += (count > 0) ? size : -size;
	xdev->mm_usage_stat[ddr].bo_count += count;
}

int xocl_mm_insert_node(struct xocl_dev *xdev, u32 ddr,
                struct drm_mm_node *node, u64 size)
{
	return drm_mm_insert_node_generic(&xdev->mm[ddr], node, size, PAGE_SIZE,
#if defined(XOCL_DRM_FREE_MALLOC)
		0, 0);
#else
		0, 0, 0);
#endif
}

int xocl_check_topology(struct xocl_dev *xdev)
{
        struct xocl_mem_topology    *topology;
        u16     i;
        int     err = 0;

        topology = &xdev->topology;

        for (i= 0; i < topology->bank_count; i++) {
                if (topology->m_data[i].m_used) {
                        if (xdev->mm_usage_stat[i].bo_count !=0 ) {
                                err = -EPERM;
                                userpf_err(xdev, "The ddr %d has "
                                        "pre-existing buffer allocations, "
                                        "please exit and re-run.", i);
                        }
                }
        }

        return err;
}

void xocl_cleanup_mem(struct xocl_dev *xdev)
{
        struct xocl_mem_topology *topology;
        u16 i, ddr;

        topology = &xdev->topology;

        ddr = topology->bank_count;
        for (i = 0; i < ddr; i++) {
                if(topology->m_data[i].m_used) {
                        userpf_info(xdev, "Taking down DDR : %d",
                                ddr);
                        drm_mm_takedown(&xdev->mm[i]);
                }
        }

        vfree(topology->m_data);
        vfree(topology->topology);
        memset(topology, 0, sizeof(struct xocl_mem_topology));
        vfree(xdev->connectivity.connections);
        memset(&xdev->connectivity, 0, sizeof(xdev->connectivity));
        vfree(xdev->layout.layout);
        memset(&xdev->layout, 0, sizeof(xdev->layout));
        vfree(xdev->debug_layout.layout);
        memset(&xdev->debug_layout, 0, sizeof(xdev->debug_layout));
}

ssize_t xocl_mm_sysfs_stat(struct xocl_dev *xdev, char *buf, bool raw)
{
	int i;
	ssize_t count = 0;
	ssize_t size = 0;
	const char *txt_fmt = "[%s] %s@0x%012llx (%lluMB): %lluKB %dBOs\n";
	const char *raw_fmt = "%llu %d\n";
	struct mem_topology *topo = xdev->topology.topology;
	struct drm_xocl_mm_stat *stat = xdev->mm_usage_stat;

	mutex_lock(&xdev->ctx_list_lock);
	if (!topo || !stat)
		goto out;

	for (i = 0; i < topo->m_count; i++) {
		if (raw) {
			count = sprintf(buf, raw_fmt,
				stat[i].memory_usage,
				stat[i].bo_count);
		} else {
			count = sprintf(buf, txt_fmt,
				topo->m_mem_data[i].m_used ?
					"IN-USE" : "UNUSED",
				topo->m_mem_data[i].m_tag,
				topo->m_mem_data[i].m_base_address,
				topo->m_mem_data[i].m_size / 1024,
				stat[i].memory_usage / 1024,
				stat[i].bo_count);
		}
		buf += count;
		size += count;
	}
out:
	mutex_unlock(&xdev->ctx_list_lock);
	return size;
}
