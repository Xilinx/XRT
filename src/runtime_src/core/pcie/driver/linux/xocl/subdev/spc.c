/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Chien-Wei Lan <chienwei@xilinx.com>
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

#include "../xocl_drv.h"
#include "profile_ioctl.h"

#define XSPC_PC_ASSERTED_OFFSET 0x0
#define XSPC_CURRENT_PC_OFFSET  0x100
#define XSPC_SNAPSHOT_PC_OFFSET 0x200

struct xocl_spc {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
	struct debug_ip_data	data;
	struct spc_status	status;
};

static void update_status(struct xocl_spc *spc)
{
	spc->status.pc_asserted = XOCL_READ_REG32(spc->base + XSPC_PC_ASSERTED_OFFSET);
	spc->status.current_pc = XOCL_READ_REG32(spc->base + XSPC_CURRENT_PC_OFFSET);
	spc->status.snapshot_pc = XOCL_READ_REG32(spc->base + XSPC_SNAPSHOT_PC_OFFSET);
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_spc *spc = platform_get_drvdata(to_platform_device(dev));
	mutex_lock(&spc->lock);
	update_status(spc);
	mutex_unlock(&spc->lock);
	return sprintf(buf, "%u\n%u\n%u\n",
		spc->status.pc_asserted,
		spc->status.current_pc,
		spc->status.snapshot_pc
		);
}

static DEVICE_ATTR_RO(status);

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_spc *spc = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "spc_%llu\n",spc->data.m_base_address);
}

static DEVICE_ATTR_RO(name);

static struct attribute *spc_attrs[] = {
			   &dev_attr_status.attr,
			   &dev_attr_name.attr,
			   NULL,
};

static struct attribute_group spc_attr_group = {
			   .attrs = spc_attrs,
};

static int __spc_remove(struct platform_device *pdev)
{
	struct xocl_spc *spc;
	void *hdl;

	spc = platform_get_drvdata(pdev);
	if (!spc) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &spc_attr_group);

	xocl_drvinst_release(spc, &hdl);

	if (spc->base)
		iounmap(spc->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void spc_remove(struct platform_device *pdev)
{
	__spc_remove(pdev);
}
#else
#define spc_remove __spc_remove
#endif

static int spc_probe(struct platform_device *pdev)
{
	struct xocl_spc *spc;
	struct resource *res;
	void *priv;
	int err = 0;

	spc = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_spc));
	if (!spc)
		return -ENOMEM;

	spc->dev = &pdev->dev;

	priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (priv)
		memcpy(&spc->data, priv, sizeof(struct debug_ip_data));

	platform_set_drvdata(pdev, spc);
	mutex_init(&spc->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}


	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	spc->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!spc->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	spc->start_paddr = res->start;
	spc->range = res->end - res->start + 1;

	err = sysfs_create_group(&pdev->dev.kobj, &spc_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create spc sysfs attrs failed: %d", err);
	}

done:
	if (err) {
		spc_remove(pdev);
		return err;
	}
	return 0;
}

static int spc_open(struct inode *inode, struct file *file)
{
	struct xocl_spc *spc = NULL;

	spc = xocl_drvinst_open_single(inode->i_cdev);
	if (!spc)
		return -ENXIO;
	file->private_data = spc;
	return 0;
}

static int spc_close(struct inode *inode, struct file *file)
{
	struct xocl_spc *spc = file->private_data;

	xocl_drvinst_close(spc);
	return 0;
}

static long spc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_spc *spc;
	long result = 0;

	spc = (struct xocl_spc *)filp->private_data;

	mutex_lock(&spc->lock);

	switch (cmd) {
	case 1:
		xocl_err(spc->dev, "ioctl 1, do nothing");
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&spc->lock);

	return result;
}


static int spc_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct xocl_spc *spc = (struct xocl_spc *)filp->private_data;
	BUG_ON(!spc);

	off = vma->vm_pgoff << PAGE_SHIFT;

	if (off > spc->range) {
		xocl_err(spc->dev, "invalid mmap offset: 0x%lx", off);
		return -EINVAL;
	}

	/* BAR physical address */
	phys = spc->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = spc->range - off;


	if (vsize > psize)
		return -EINVAL;

	/*
	 * pages must not be cached as this would result in cache line sized
	 * accesses to the end point
	 */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	/*
	 * prevent touching the pages (byte access) for swap-in,
	 * and prevent the pages from being swapped out
	 */
#ifndef VM_RESERVED
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0) && !defined(RHEL_9_5_GE)
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
#else
	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#endif
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0) && !defined(RHEL_9_5_GE)
	vma->vm_flags |= VM_IO | VM_RESERVED;
#else
	vm_flags_set(vma, VM_IO | VM_RESERVED);
#endif
#endif

	/* make MMIO accessible to user space */
	rc = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
				vsize, vma->vm_page_prot);
	if (rc)
		return -EAGAIN;
	return rc;
}


static const struct file_operations spc_fops = {
	.open = spc_open,
	.release = spc_close,
	.mmap = spc_mmap,
	.unlocked_ioctl = spc_ioctl,
};

struct xocl_drv_private spc_priv = {
	.fops = &spc_fops,
	.dev = -1,
};

struct platform_device_id spc_id_table[] = {
	{ XOCL_DEVNAME(XOCL_SPC), (kernel_ulong_t)&spc_priv },
	{ },
};

static struct platform_driver	spc_driver = {
	.probe		= spc_probe,
	.remove		= spc_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_SPC),
	},
	.id_table = spc_id_table,
};

int __init xocl_init_spc(void)
{
	int err = 0;

	err = alloc_chrdev_region(&spc_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_SPC);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&spc_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(spc_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_spc(void)
{
	unregister_chrdev_region(spc_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&spc_driver);
}
