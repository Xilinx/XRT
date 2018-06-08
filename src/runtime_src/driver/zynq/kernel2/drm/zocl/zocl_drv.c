/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Min Ma       <min.ma@xilinx.com>
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

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/iommu.h>
#include <linux/pagemap.h>
#include "zocl_drv.h"
#include "sched_exec.h"

#define ZOCL_DRIVER_NAME        "zocl"
#define ZOCL_DRIVER_DESC        "Zynq BO manager"
#define ZOCL_DRIVER_DATE        "20180313"
#define ZOCL_DRIVER_MAJOR       2018
#define ZOCL_DRIVER_MINOR       2
#define ZOCL_DRIVER_PATCHLEVEL  1


#if defined(CONFIG_ARM64)
#define ZOCL_FILE_PAGE_OFFSET   0x00100000
#else
#define ZOCL_FILE_PAGE_OFFSET   0x00010000
#endif

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static const struct vm_operations_struct reg_physical_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys,
#endif
};

void zocl_free_sections(struct drm_zocl_dev *zdev) {
	if (zdev->layout.layout) {
		vfree(zdev->layout.layout);
		CLEAR(zdev->layout.layout);
	}
	if (zdev->debug_layout.layout) {
		vfree(zdev->debug_layout.layout);
		CLEAR(zdev->debug_layout.layout);
	}
	if (zdev->connectivity.connections) {
		vfree(zdev->connectivity.connections);
		CLEAR(zdev->connectivity.connections);
	}
	if (zdev->topology.m_data) {
		vfree(zdev->topology.m_data);
	}
	if (zdev->topology.topology) {
		vfree(zdev->topology.topology);
		CLEAR(zdev->topology.topology);
	}
}

static int zocl_drm_load(struct drm_device *drm, unsigned long flags)
{
	struct platform_device *pdev;
	struct resource *res;
	struct drm_zocl_dev *zdev;
	struct device_node *fnode;
	void __iomem *map;

	DRM_INFO("%s\n", __FUNCTION__);
	pdev = to_platform_device(drm->dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		DRM_ERROR("Failed to map registers: %ld\n", PTR_ERR(map));
		return PTR_ERR(map);
	}

	zdev = devm_kzalloc(drm->dev, sizeof(*zdev), GFP_KERNEL);
	if (!zdev)
		return -ENOMEM;

	zdev->ddev = drm;
	drm->dev_private = zdev;
	zdev->regs = map;
	zdev->res_start = res->start;
	zdev->res_len = resource_size(res);

#if defined(XCLBIN_DOWNLOAD)
	fnode = of_get_child_by_name(of_root, "pcap");
	if (!fnode) {
		DRM_ERROR("FPGA programming device pcap not found\n");
		return -ENODEV;
	}

	zdev->fpga_mgr = of_fpga_mgr_get(fnode);
	if (IS_ERR(zdev->fpga_mgr)) {
		DRM_ERROR("FPGA Manager not found %ld\n", PTR_ERR(zdev->fpga_mgr));
		return PTR_ERR(zdev->fpga_mgr);
	}
#endif

  zocl_init_sysfs(drm->dev);
  
  // Now initial kds
  sched_init_exec(drm);

	/* Initialzie IOMMU */
	if (iommu_present(&platform_bus_type)) {
		struct iommu_domain_geometry *geometry;
		u64 start, end;
		zdev->domain = iommu_domain_alloc(&platform_bus_type);
		if (!zdev->domain)
			return -ENOMEM;

    int ret = 0;
    ret = iommu_attach_device(zdev->domain, &pdev->dev);
    if (ret) {
      DRM_INFO("IOMMU attach device failed. ret(%d)\n", ret);
      iommu_domain_free(zdev->domain);
    }
    // TODO: SMMU can support up to 16 devices. Could we check
    // the how many devices have attached to SMMU? And handle this
    // issue in proper way?

		geometry = &zdev->domain->geometry;
		start = geometry->aperture_start;
		end = geometry->aperture_end;

		DRM_INFO("IOMMU aperture initialized (%#llx-%#llx)\n",
			 start, end);
	}

	platform_set_drvdata(pdev, zdev);
	return 0;
}

struct drm_gem_object *zocl_gem_create_object(struct drm_device *dev, size_t size)
{
	return kzalloc(sizeof(struct drm_zocl_bo), GFP_KERNEL);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,9,0)
static int zocl_drm_unload(struct drm_device *drm)
#else
static void zocl_drm_unload(struct drm_device *drm)
#endif
{
	struct drm_zocl_dev *zdev = drm->dev_private;
	if (zdev->domain) {
		iommu_detach_device(zdev->domain, drm->dev);
		iommu_domain_free(zdev->domain);
	}
#if defined(XCLBIN_DOWNLOAD)
	fpga_mgr_put(zdev->fpga_mgr);
#endif
  sched_fini_exec(drm);
	zocl_free_sections(zdev);
  zocl_fini_sysfs(drm->dev);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,9,0)
  return 0;
#endif
}

void zocl_free_bo(struct drm_gem_object *obj)
{
	struct drm_zocl_bo *zocl_obj;
	struct drm_zocl_dev *zdev;
	int npages;

	if (IS_ERR(obj) || !obj )
		return;

	zocl_obj = to_zocl_bo(obj);
	zdev = obj->dev->dev_private;

	if (!zdev->domain) {
		DRM_INFO("Freeing BO\n");
		zocl_describe(zocl_obj);
		if (zocl_obj->flags == DRM_ZOCL_BO_FLAGS_USERPTR)
			zocl_free_userptr_bo(obj);
		else
			drm_gem_cma_free_object(obj);
	} else {
		npages = obj->size >> PAGE_SHIFT;
		drm_gem_object_release(obj);

		if (zocl_obj->vmapping)
			vunmap(zocl_obj->vmapping);
		zocl_obj->vmapping = NULL;

		zocl_iommu_unmap_bo(obj->dev, zocl_obj);
		if (zocl_obj->pages) {
			if (zocl_bo_userptr(zocl_obj)) {
				release_pages(zocl_obj->pages, npages, 0);
				drm_free_large(zocl_obj->pages);
			}
			else
				drm_gem_put_pages(obj, zocl_obj->pages, false, false);
		}
		if (zocl_obj->sgt)
			sg_free_table(zocl_obj->sgt);
		zocl_obj->sgt = NULL;
		zocl_obj->pages = NULL;
		kfree(zocl_obj);
	}
}

static int zocl_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct drm_zocl_bo *bo = NULL;
	unsigned long vsize;
	int rc;

	/* If the page offset is > 4G (64 bit) or 2G (32 bit), then we are trying
	 * to map GEM BO
	 */
	if (likely(vma->vm_pgoff >= ZOCL_FILE_PAGE_OFFSET)) {
		if (!zdev->domain) {
			return drm_gem_cma_mmap(filp, vma);
		} else {
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
			vma->vm_flags &= ~VM_PFNMAP;
			vma->vm_flags |= VM_MIXEDMAP;
			/* Reset the fake offset used to identify the BO */
			vma->vm_pgoff = 0;
			return 0;
		}
	}

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	vsize = vma->vm_end - vma->vm_start;
	if (vsize > zdev->res_len)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_RESERVED;

	vma->vm_ops = &reg_physical_vm_ops;
	rc = io_remap_pfn_range(vma, vma->vm_start,
				zdev->res_start >> PAGE_SHIFT,
				vsize, vma->vm_page_prot);

	return rc;
}

static int zocl_bo_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_zocl_bo *bo = to_zocl_bo(obj);
	struct drm_zocl_dev *zdev = obj->dev->dev_private;
	struct page *page;
	pgoff_t offset;
	int err;

	if (!zdev->domain) {
		return 0;
	}

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

static int zocl_client_open(struct drm_device *dev, struct drm_file *filp)
{
  struct sched_client_ctx *fpriv = kzalloc(sizeof(*fpriv), GFP_KERNEL);
  if (!fpriv)
    return -ENOMEM;

  filp->driver_priv = fpriv;
  mutex_init(&fpriv->lock);
  atomic_set(&fpriv->trigger, 0);
  zocl_track_ctx(dev, fpriv);
  DRM_INFO("Pid %d opened device\n", pid_nr(task_tgid(current)));
  return 0;
}

static void zocl_client_release(struct drm_device *dev, struct drm_file *filp)
{
  struct drm_zocl_dev *zdev = dev->dev_private;
  struct sched_client_ctx *fpriv = filp->driver_priv;

  if (!fpriv)
    return;

  zocl_untrack_ctx(dev, fpriv);

  DRM_INFO("Pid %d closed device\n", pid_nr(task_tgid(current)));
  return;
}

static unsigned int zocl_poll(struct file *filp, poll_table *wait)
{
  int counter;
  struct drm_file *priv = filp->private_data;
  struct drm_device *dev = priv->minor->dev;
  struct drm_zocl_dev *zdev = dev->dev_private;
  struct sched_client_ctx *fpriv = priv->driver_priv;
  int ret = 0;

  BUG_ON(!fpriv);

  poll_wait(filp, &zdev->exec->poll_wait_queue, wait);

  mutex_lock(&fpriv->lock);
  counter = atomic_read(&fpriv->trigger);
  if (counter >0) {
    atomic_dec(&fpriv->trigger);
    ret = POLLIN;
  }
  mutex_unlock(&fpriv->lock);

  DRM_INFO("Pid %d poll device\n", pid_nr(task_tgid(current)));
  return ret;
}

const struct vm_operations_struct zocl_bo_vm_ops = {
	.fault = zocl_bo_fault,
	.open  = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct drm_ioctl_desc zocl_ioctls[] = {
	DRM_IOCTL_DEF_DRV(ZOCL_CREATE_BO, zocl_create_bo_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_USERPTR_BO, zocl_userptr_bo_ioctl,
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
	DRM_IOCTL_DEF_DRV(ZOCL_READ_AXLF, zocl_read_axlf_ioctl,
				DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
#if defined(XCLBIN_DOWNLOAD)
	DRM_IOCTL_DEF_DRV(ZOCL_PCAP_DOWNLOAD, zocl_pcap_download_ioctl,
			  DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW)
#endif
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
  .driver_features           = DRIVER_GEM | DRIVER_PRIME | DRIVER_RENDER,
  .open                      = zocl_client_open,
  .postclose                 = zocl_client_release,
  .load                      = zocl_drm_load,
  .unload                    = zocl_drm_unload,
  .gem_free_object           = zocl_free_bo,
  .gem_vm_ops                = &zocl_bo_vm_ops,
	.gem_create_object				 = zocl_gem_create_object,
  .prime_handle_to_fd        = drm_gem_prime_handle_to_fd,
  .prime_fd_to_handle        = drm_gem_prime_fd_to_handle,
  .gem_prime_import          = drm_gem_prime_import,
  .gem_prime_export          = drm_gem_prime_export,
  .gem_prime_get_sg_table    = drm_gem_cma_prime_get_sg_table,
  .gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
  .gem_prime_vmap            = drm_gem_cma_prime_vmap,
  .gem_prime_vunmap          = drm_gem_cma_prime_vunmap,
  .gem_prime_mmap            = drm_gem_cma_prime_mmap,
  .ioctls                    = zocl_ioctls,
  .num_ioctls                = ARRAY_SIZE(zocl_ioctls),
  .fops                      = &zocl_driver_fops,
  .name                      = ZOCL_DRIVER_NAME,
  .desc                      = ZOCL_DRIVER_DESC,
  .date                      = ZOCL_DRIVER_DATE,
  .major                     = ZOCL_DRIVER_MAJOR,
  .minor                     = ZOCL_DRIVER_MINOR,
  .patchlevel                = ZOCL_DRIVER_PATCHLEVEL,
};

static const struct of_device_id zocl_drm_of_match[] = {
	{ .compatible = "xlnx,zocl", },
	{ .compatible = "xlnx,zoclsvm", },
	{ /* end of table */ },
};

/* init xilinx opencl drm platform */
static int zocl_drm_platform_probe(struct platform_device *pdev)
{
  struct drm_device *dev;
  int ret;
	DRM_INFO("Probing for %s\n", zocl_drm_of_match[0].compatible);
  
  dev = drm_dev_alloc(&zocl_driver, &pdev->dev);
  if (IS_ERR(dev))
    return PTR_ERR(dev);

  ret = drm_dev_register(dev, 0);
  if (ret)
    goto err;

  return 0;

err:
  drm_dev_unref(dev);
  return ret;
}

/* exit xilinx opencl drm platform */
static int zocl_drm_platform_remove(struct platform_device *pdev)
{
	struct drm_zocl_dev *zdev = platform_get_drvdata(pdev);

	if (zdev->ddev) {
		drm_dev_unregister(zdev->ddev);
		drm_dev_unref(zdev->ddev);
	}

	return 0;
}

MODULE_DEVICE_TABLE(of, zocl_drm_of_match);

static struct platform_driver zocl_drm_private_driver = {
	.probe			= zocl_drm_platform_probe,
	.remove			= zocl_drm_platform_remove,
	.driver			= {
		.name		        = "zocl-drm",
		.of_match_table	= zocl_drm_of_match,
	},
};

module_platform_driver(zocl_drm_private_driver);

MODULE_VERSION(__stringify(ZOCL_DRIVER_MAJOR) "."
		__stringify(ZOCL_DRIVER_MINOR) "."
		__stringify(ZOCL_DRIVER_PATCHLEVEL));

MODULE_DESCRIPTION(ZOCL_DRIVER_DESC);
MODULE_AUTHOR("Sonal Santan <sonal.santan@xilinx.com>");
MODULE_LICENSE("GPL");
