/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include "zocl_drv.h"
#include "zocl_sk.h"
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

void zocl_free_sections(struct drm_zocl_dev *zdev)
{
	if (zdev->ip) {
		vfree(zdev->ip);
		CLEAR(zdev->ip);
	}
	if (zdev->debug_ip) {
		vfree(zdev->debug_ip);
		CLEAR(zdev->debug_ip);
	}
	if (zdev->connectivity) {
		vfree(zdev->connectivity);
		CLEAR(zdev->connectivity);
	}
	if (zdev->topology) {
		vfree(zdev->topology);
		CLEAR(zdev->topology);
	}
}

static irqreturn_t zocl_h2c_isr(int irq, void *arg)
{
	void *mmio_sched = arg;

	DRM_INFO("IRQ number is %d -->\n", irq);
	mmio_sched = mmio_sched + 0x58;
	DRM_INFO("mmio_sched is 0x%x\n", ioread32(mmio_sched));
	mmio_sched = mmio_sched - 0x58;
	DRM_INFO("<-- IRQ handler\n");
	return IRQ_HANDLED;
}

/**
 * find_pdev - Find platform device by name
 *
 * @name: device name
 *
 * Returns a platform device. Returns NULL if not found.
 */
static struct platform_device *find_pdev(char *name)
{
	struct device *dev;
	struct platform_device *pdev;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, name);
	if (!dev)
		return NULL;

	pdev = container_of(dev, struct platform_device, dev);
	return pdev;
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
 * zocl_gem_create_object - Create drm_zocl_bo object instead of DRM CMA object.
 *
 * @dev: DRM device struct
 * @size: This size was not use, just to match function prototype.
 *
 */
struct drm_gem_object *
zocl_gem_create_object(struct drm_device *dev, size_t size)
{
	return kzalloc(sizeof(struct drm_zocl_bo), GFP_KERNEL);
}

void zocl_free_bo(struct drm_gem_object *obj)
{
	struct drm_zocl_bo *zocl_obj;
	struct drm_zocl_dev *zdev;
	int npages;

	if (IS_ERR(obj) || !obj)
		return;

	zocl_obj = to_zocl_bo(obj);
	zdev = obj->dev->dev_private;

	if (!zdev->domain) {
		DRM_INFO("Freeing BO\n");
		zocl_describe(zocl_obj);
		if (zocl_obj->flags & XCL_BO_FLAGS_USERPTR)
			zocl_free_userptr_bo(obj);
		else if (zocl_obj->flags & XCL_BO_FLAGS_HOST_BO)
			zocl_free_host_bo(obj);
		else {
			drm_gem_cma_free_object(obj);

			/* Update memory usage statistics */
			zocl_update_mem_stat(zdev, obj->size, -1);
		}

		return;
	}

	npages = obj->size >> PAGE_SHIFT;
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
			zocl_update_mem_stat(zdev, obj->size, -1);
		}
	}
	if (zocl_obj->sgt)
		sg_free_table(zocl_obj->sgt);
	zocl_obj->sgt = NULL;
	zocl_obj->pages = NULL;
	kfree(zocl_obj);
}

static int
zocl_gem_cma_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_cma_object *cma_obj;
	struct drm_gem_object *gem_obj;
	struct drm_zocl_bo *bo;
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

	gem_obj = vma->vm_private_data;
	cma_obj = to_drm_gem_cma_obj(gem_obj);
	bo = to_zocl_bo(gem_obj);

	/**
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(),
	 * and set the vm_pgoff (used as a fake buffer offset by DRM)
	 * to 0 as we want to map the whole buffer.
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;

	if (bo->flags & XCL_BO_FLAGS_CACHEABLE) {
		/**
		 * Resume the protection field from mmap(). Most likely
		 * it will be cacheable. If there is a case that mmap()
		 * protection field explicitly tells us not to map with
		 * cache enabled, we should comply with it and overwrite
		 * the cacheable BO property.
		 */
		vma->vm_page_prot = prot;
		rc = remap_pfn_range(vma, vma->vm_start,
		    cma_obj->paddr >> PAGE_SHIFT,
		    vma->vm_end - vma->vm_start,
		    prot);

	} else
		rc = dma_mmap_wc(cma_obj->base.dev->dev, vma, cma_obj->vaddr,
		    cma_obj->paddr, vma->vm_end - vma->vm_start);

	if (rc)
		drm_gem_vm_close(vma);

	return rc;
}

static int zocl_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct drm_zocl_bo *bo = NULL;
	unsigned long vsize;
	int rc;

	/* If the page offset is > 4G (64 bit) or 2G (32 bit),
	 * then we are trying to map GEM BO
	 */
	if (likely(vma->vm_pgoff >= ZOCL_FILE_PAGE_OFFSET)) {
		if (!zdev->domain)
			return zocl_gem_cma_mmap(filp, vma);

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
	struct sched_client_ctx *fpriv = filp->driver_priv;

	if (!fpriv)
		return;

	zocl_untrack_ctx(dev, fpriv);
	kfree(fpriv);

	DRM_INFO("Pid %d closed device\n", pid_nr(task_tgid(current)));
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
	if (counter > 0) {
		atomic_dec(&fpriv->trigger);
		ret = POLLIN;
	}
	mutex_unlock(&fpriv->lock);

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
	DRM_IOCTL_DEF_DRV(ZOCL_READ_AXLF, zocl_read_axlf_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SK_GETCMD, zocl_sk_getcmd_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SK_CREATE, zocl_sk_create_ioctl,
			DRM_AUTH|DRM_UNLOCKED|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(ZOCL_SK_REPORT, zocl_sk_report_ioctl,
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
	.gem_free_object           = zocl_free_bo,
	.gem_vm_ops                = &zocl_bo_vm_ops,
	.gem_create_object         = zocl_gem_create_object,
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
	{ .compatible = "xlnx,zocl-ert", },
	{ /* end of table */ },
};
MODULE_DEVICE_TABLE(of, zocl_drm_of_match);

/* init xilinx opencl drm platform */
static int zocl_drm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct drm_device *drm;
	struct drm_zocl_dev	*zdev;
	struct platform_device *subdev;
	struct resource *res;
	struct resource res_mem;
	void __iomem *map;
	int index;
	int irq;
	int ret;

	id = of_match_node(zocl_drm_of_match, pdev->dev.of_node);
	DRM_INFO("Probing for %s\n", id->compatible);

	/* Get resource and ioremap */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		DRM_ERROR("Failed to map registers: %ld\n", PTR_ERR(map));
		return PTR_ERR(map);
	}

	/* Create zocl device and initial */
	zdev = devm_kzalloc(&pdev->dev, sizeof(*zdev), GFP_KERNEL);
	if (!zdev)
		return -ENOMEM;

	zdev->regs       = map;
	zdev->res_start  = res->start;
	zdev->res_len    = resource_size(res);

	/* Record and get IRQ number */
	for (index = 0; index < MAX_CU_NUM; index++) {
		irq = platform_get_irq(pdev, index);
		if (irq < 0)
			break;
		DRM_DEBUG("CU(%d) IRQ %d\n", index, irq);
		zdev->irq[index] = irq;
	}
	zdev->cu_num = index;

	zdev->host_mem = 0xFFFFFFFFFFFFFFFF;
	zdev->host_mem_len = 0;
	/* If reserved memory region are not found, just keep going */
	ret = get_reserved_mem_region(&pdev->dev, &res_mem);
	if (!ret) {
		DRM_INFO("Reserved memory for host at 0x%llx, size 0x%llx\n",
			 res_mem.start, resource_size(&res_mem));
		zdev->host_mem = res_mem.start;
		zdev->host_mem_len = resource_size(&res_mem);
	}

	subdev = find_pdev("80180000.ert_hw");
	if (subdev) {
		DRM_INFO("ert_hw found -> %p\n", subdev);
		zdev->ert = (struct zocl_ert_dev *)platform_get_drvdata(subdev);
	}

#if defined(XCLBIN_DOWNLOAD)
	fnode = of_get_child_by_name(of_root, "pcap");
	if (!fnode) {
		DRM_ERROR("FPGA programming device pcap not found\n");
		return -ENODEV;
	}

	zdev->fpga_mgr = of_fpga_mgr_get(fnode);
	if (IS_ERR(zdev->fpga_mgr)) {
		DRM_ERROR("FPGA Manager not found %ld\n",
				PTR_ERR(zdev->fpga_mgr));
		return PTR_ERR(zdev->fpga_mgr);
	}
#endif

	/* Initialzie IOMMU */
	if (iommu_present(&platform_bus_type)) {
		struct iommu_domain_geometry *geometry;
		u64 start, end;
		int ret = 0;

		zdev->domain = iommu_domain_alloc(&platform_bus_type);
		if (!zdev->domain)
			return -ENOMEM;

		ret = iommu_attach_device(zdev->domain, &pdev->dev);
		if (ret) {
			DRM_INFO("IOMMU attach device failed. ret(%d)\n", ret);
			iommu_domain_free(zdev->domain);
		}

		geometry = &zdev->domain->geometry;
		start = geometry->aperture_start;
		end = geometry->aperture_end;

		DRM_INFO("IOMMU aperture initialized (%#llx-%#llx)\n",
				start, end);
	}

	platform_set_drvdata(pdev, zdev);

	/* Create and register DRM device */
	drm = drm_dev_alloc(&zocl_driver, &pdev->dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err0;

	drm->dev_private = zdev;
	zdev->ddev       = drm;

	/* Initial sysfs */
	rwlock_init(&zdev->attr_rwlock);
	ret = zocl_init_sysfs(drm->dev);
	if (ret)
		goto err0;

	/* Now initial kds */
	ret = sched_init_exec(drm);
	if (ret)
		goto err1;

	return 0;
err1:
	zocl_fini_sysfs(drm->dev);
err0:
	drm_dev_unref(drm);
	return ret;
}

/* exit xilinx opencl drm platform */
static int zocl_drm_platform_remove(struct platform_device *pdev)
{
	struct drm_zocl_dev *zdev = platform_get_drvdata(pdev);
	struct drm_device *drm = zdev->ddev;

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

	if (drm) {
		drm_dev_unregister(drm);
		drm_dev_unref(drm);
	}

	return 0;
}

static struct platform_driver zocl_drm_private_driver = {
	.probe			= zocl_drm_platform_probe,
	.remove			= zocl_drm_platform_remove,
	.driver			= {
		.name		        = "zocl-drm",
		.of_match_table	= zocl_drm_of_match,
	},
};

static struct platform_driver *const drivers[] = {
	&zocl_ert_driver,
};

static int __init zocl_init(void)
{
	int ret;

	/* Make sure register sub device in the first place. */
	ret = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	if (ret < 0)
		return ret;

	ret = platform_driver_register(&zocl_drm_private_driver);
	if (ret < 0)
		goto err;

	return 0;
err:
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
	return ret;
}
module_init(zocl_init);

static void __exit zocl_exit(void)
{
	/* Remove zocl driver first, as it is using other driver */
	platform_driver_unregister(&zocl_drm_private_driver);
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
}
module_exit(zocl_exit);

MODULE_VERSION(__stringify(ZOCL_DRIVER_MAJOR) "."
		__stringify(ZOCL_DRIVER_MINOR) "."
		__stringify(ZOCL_DRIVER_PATCHLEVEL));

MODULE_DESCRIPTION(ZOCL_DRIVER_DESC);
MODULE_AUTHOR("Sonal Santan <sonal.santan@xilinx.com>");
MODULE_LICENSE("GPL");
