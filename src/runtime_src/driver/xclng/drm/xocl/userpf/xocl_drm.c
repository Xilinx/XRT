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
	struct xocl_dev	*xdev = dev->dev_private;
	int	ret = 0;

	DRM_ENTER("");

	/* We do not allow users to open PRIMARY node, /dev/dri/cardX node.
	 * Users should only open RENDER, /dev/dri/renderX node */
	if (drm_is_primary_client(filp))
		return -EPERM;

	if (get_live_client_size(xdev) > XOCL_MAX_CONCURRENT_CLIENTS)
		return -EBUSY;

	if (MB_SCHEDULER_DEV(xdev))
		ret = xocl_exec_create_client(xdev, &filp->driver_priv);
	return ret;
}

static void xocl_client_release(struct drm_device *dev, struct drm_file *filp)
{
	struct xocl_dev	*xdev = dev->dev_private;
	struct client_ctx *client = filp->driver_priv;
	unsigned bit = xdev->layout ? find_first_bit(client->cu_bitmap, xdev->layout->m_count) : MAX_CUS;

	DRM_ENTER("");

	/* This happens when application exists without formally releasing the contexts on CUs.
	 * Give up our contexts on CUs and our lock on xclbin.
	 * Note, that implicitly CUs (such as CDMA) do not add to ip_reference
	*/
	while (xdev->layout && (bit < xdev->layout->m_count)) {
		if (xdev->ip_reference[bit]) {
			userpf_info(dev->dev_private, "CTX reclaim (%pUb, %d, %u)", &client->xclbin_id, pid_nr(task_tgid(current)),
				    bit);
			xdev->ip_reference[bit]--;
		}
		bit = find_next_bit(client->cu_bitmap, xdev->layout->m_count, bit + 1);
	}
	bitmap_zero(client->cu_bitmap, MAX_CUS);
	if (atomic_read(&client->xclbin_locked))
		(void) xocl_icap_unlock_bitstream(xdev, &client->xclbin_id,pid_nr(task_tgid(current)));

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
};

static void xocl_mailbox_srv(void *arg, void *data, size_t len,
	u64 msgid, int err)
{
	struct xocl_dev	*xdev = (struct xocl_dev *)arg;
	struct mailbox_req *req = (struct mailbox_req *)data;
	int ret = 0;

	if (err != 0)
		return;

	userpf_info(xdev, "received request (%d) from peer\n", req->req);

	switch (req->req) {
	case MAILBOX_REQ_HOT_RESET_BEGIN:
		xocl_reset_notify(xdev->core.pdev, true);
		(void) xocl_peer_response(xdev, msgid, &ret, sizeof (ret));
		break;
	case MAILBOX_REQ_HOT_RESET_END:
		xocl_reset_notify(xdev->core.pdev, false);
		(void) xocl_peer_response(xdev, msgid, &ret, sizeof (ret));
		break;
	case MAILBOX_REQ_RESET_ERT:
		ret = xocl_reset_scheduler(xdev->core.pdev);
		(void) xocl_peer_response(xdev, msgid, &ret, sizeof (ret));
		break;
	default:
		break;
	}
}

static int xocl_init_non_unified(struct xocl_dev *xdev)
{
	int			i, ret = 0;
	u32			ddr_count = 0;
	u64			ddr_size;
	u64			segment = 0;
	size_t mm_size = 0, mm_stat_size = 0;
	size_t size;
	struct mem_data		*mem_data;

	userpf_info(xdev, "Non-unified platform");
	ddr_count = xocl_get_ddr_channel_count(xdev);
	ddr_size = xocl_get_ddr_channel_size(xdev);

	size = ddr_count * sizeof(void *);
	mm_size = sizeof(struct drm_mm);
	mm_stat_size = sizeof(struct drm_xocl_mm_stat);
	xdev->mm = vzalloc(size);
	if (!xdev->mm) {
		userpf_err(xdev, "alloc mm pointers failed");
		ret = -ENOMEM;
		goto failed;
	}
	xdev->mm_usage_stat = vzalloc(size);
	if (!xdev->mm_usage_stat) {
		userpf_err(xdev, "alloc stat pointers failed");
		ret = -ENOMEM;
		goto failed;
	}

	for (i = 0; i < ddr_count; i++) {
		mem_data = &xdev->topology->m_mem_data[i];
		mem_data->m_used = 1;
		xdev->mm[i] = vzalloc(mm_size);
		if (!xdev->mm[i]) {
			userpf_err(xdev, "alloc mem failed, ddr %d, sz %lld",
					ddr_count, ddr_size);
			ret = -ENOMEM;
			goto failed_at_i;
		}
		xdev->mm_usage_stat[i] = vzalloc(mm_stat_size);
		if (!xdev->mm_usage_stat[i]) {
			userpf_err(xdev, "alloc mem failed, ddr %d, sz %lld",
					ddr_count, ddr_size);
			ret = -ENOMEM;
			goto failed_at_i;
		}
		drm_mm_init(xdev->mm[i], segment, ddr_size);
		segment += ddr_size;
	}

	return 0;

failed_at_i:
	for (; i >= 0; i--) {
		mem_data = &xdev->topology->m_mem_data[i];
		if (xdev->mm[i]) {
			drm_mm_takedown(xdev->mm[i]);
			vfree(xdev->mm[i]);
		}
		if (xdev->mm_usage_stat[i])
			vfree(xdev->mm_usage_stat[i]);
	}

failed:
	if (xdev->mm)
		vfree(xdev->mm);
	if (xdev->mm_usage_stat)
		vfree(xdev->mm_usage_stat);

	return ret;
}

int xocl_drm_init(struct xocl_dev *xdev)
{
	struct drm_device	*ddev = NULL;
	int			ret = 0;

	sscanf(XRT_DRIVER_VERSION, "%d.%d.%d", 
		&mm_drm_driver.major,
		&mm_drm_driver.minor,
		&mm_drm_driver.patchlevel);
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
		ret = xocl_init_non_unified(xdev);
		if (ret) {
			userpf_err(xdev, "Non-unified platform init failed");
			goto failed;
		}
	}

	/* Launch the mailbox server. */
        (void) xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);

	mutex_init(&xdev->stat_lock);
	mutex_init(&xdev->mm_lock);
	mutex_init(&xdev->ctx_list_lock);
	INIT_LIST_HEAD(&xdev->ctx_list);
	ddev->dev_private = xdev;
	atomic_set(&xdev->needs_reset,0);
	atomic_set(&xdev->outstanding_execs, 0);
	atomic64_set(&xdev->total_execs, 0);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	hash_init(xdev->mm_range);
#endif
	return 0;

failed:
	if (!ddev)
		drm_dev_unref(ddev);

	return ret;
}

void xocl_drm_fini(struct xocl_dev *xdev)
{
	xocl_cleanup_mem(xdev);
	xocl_cleanup_connectivity(xdev);
	drm_put_dev(xdev->ddev);
	mutex_destroy(&xdev->ctx_list_lock);
	mutex_destroy(&xdev->stat_lock);
	mutex_destroy(&xdev->mm_lock);
}

void xocl_mm_get_usage_stat(struct xocl_dev *xdev, u32 ddr,
	struct drm_xocl_mm_stat *pstat)
{
	if (xdev->mm_usage_stat[ddr]) {
		pstat->memory_usage = xdev->mm_usage_stat[ddr]->memory_usage;
		pstat->bo_count = xdev->mm_usage_stat[ddr]->bo_count;
	}
}

void xocl_mm_update_usage_stat(struct xocl_dev *xdev, u32 ddr,
	u64 size, int count)
{
	BUG_ON(!xdev->mm_usage_stat[ddr]);

	xdev->mm_usage_stat[ddr]->memory_usage += (count > 0) ? size : -size;
	xdev->mm_usage_stat[ddr]->bo_count += count;
}

int xocl_mm_insert_node(struct xocl_dev *xdev, u32 ddr,
                struct drm_mm_node *node, u64 size)
{
	return drm_mm_insert_node_generic(xdev->mm[ddr], node, size, PAGE_SIZE,
#if defined(XOCL_DRM_FREE_MALLOC)
		0, 0);
#else
		0, 0, 0);
#endif
}

int xocl_check_topology(struct xocl_dev *xdev)
{
	struct mem_topology    *topology;
	u16     i;
	int     err = 0;

	topology = xdev->topology;
	if (topology == NULL)
		return 0;

	for (i = 0; i < topology->m_count; i++) {
		if (!topology->m_mem_data[i].m_used)
			continue;

		if (topology->m_mem_data[i].m_type == MEM_STREAMING)
			continue;

		if (xdev->mm_usage_stat[i]->bo_count != 0) {
			err = -EPERM;
			userpf_err(xdev, "The ddr %d has pre-existing buffer "
					"allocations, please exit and re-run.",
					i);
		}
	}

	return err;
}

uint32_t xocl_get_shared_ddr(struct xocl_dev *xdev, struct mem_data *m_data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	struct xocl_mm_wrapper *wrapper;
	uint64_t start_addr = m_data->m_base_address;
	uint64_t sz = m_data->m_size*1024;

	hash_for_each_possible(xdev->mm_range, wrapper, node, start_addr) {
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

void xocl_cleanup_mem(struct xocl_dev *xdev)
{
	struct mem_topology *topology;
	u16 i, ddr;
	uint64_t addr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
	struct xocl_mm_wrapper *wrapper;
#endif

	topology = xdev->topology;
	if (topology == NULL)
		return;

	ddr = topology->m_count;
	for (i = 0; i < ddr; i++) {
		if (!topology->m_mem_data[i].m_used)
			continue;

		if (topology->m_mem_data[i].m_type == MEM_STREAMING)
			continue;

		userpf_info(xdev, "Taking down DDR : %d", i);
		addr = topology->m_mem_data[i].m_base_address;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
		hash_for_each_possible(xdev->mm_range, wrapper, node, addr) {
			if (wrapper->ddr == i) {
				hash_del(&wrapper->node);
				vfree(wrapper);
				drm_mm_takedown(xdev->mm[i]);
				vfree(xdev->mm[i]);
				vfree(xdev->mm_usage_stat[i]);
			}
		}
#endif
		xdev->mm[i] = NULL;
		xdev->mm_usage_stat[i] = NULL;
	}

	vfree(xdev->mm);
	xdev->mm = NULL;
	vfree(xdev->mm_usage_stat);
	xdev->mm_usage_stat = NULL;
	vfree(xdev->topology);
	xdev->topology = NULL;
}

void xocl_cleanup_connectivity(struct xocl_dev *xdev)
{
	vfree(xdev->layout);
	xdev->layout = NULL;
	vfree(xdev->debug_layout);
	xdev->debug_layout = NULL;
	vfree(xdev->connectivity);
	xdev->connectivity = NULL;
}

ssize_t xocl_mm_sysfs_stat(struct xocl_dev *xdev, char *buf, bool raw)
{
	int i;
	ssize_t count = 0;
	ssize_t size = 0;
	size_t memory_usage = 0;
	unsigned bo_count = 0;
	const char *txt_fmt = "[%s] %s@0x%012llx (%lluMB): %lluKB %dBOs\n";
	const char *raw_fmt = "%llu %d\n";
	struct mem_topology *topo = xdev->topology;
	struct drm_xocl_mm_stat **stat = xdev->mm_usage_stat;

	mutex_lock(&xdev->ctx_list_lock);
	if (!topo || !stat)
		goto out;

	for (i = 0; i < topo->m_count; i++) {
		if (topo->m_mem_data[i].m_type == MEM_STREAMING)
			continue;

		if (raw) {
			memory_usage = 0;
			bo_count = 0;
			if (stat[i]) {
				memory_usage = stat[i]->memory_usage;
				bo_count = stat[i]->bo_count;
			}

			count = sprintf(buf, raw_fmt,
				memory_usage,
				bo_count);
		} else {
			count = sprintf(buf, txt_fmt,
				topo->m_mem_data[i].m_used ?
					"IN-USE" : "UNUSED",
				topo->m_mem_data[i].m_tag,
				topo->m_mem_data[i].m_base_address,
				topo->m_mem_data[i].m_size / 1024,
				stat[i]->memory_usage / 1024,
				stat[i]->bo_count);
		}
		buf += count;
		size += count;
	}
out:
	mutex_unlock(&xdev->ctx_list_lock);
	return size;
}
