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

#define LAPC_OVERALL_STATUS_OFFSET        0x0
#define LAPC_CUMULATIVE_STATUS_0_OFFSET   0x100
#define LAPC_CUMULATIVE_STATUS_1_OFFSET   0x104
#define LAPC_CUMULATIVE_STATUS_2_OFFSET   0x108
#define LAPC_CUMULATIVE_STATUS_3_OFFSET   0x10c

#define LAPC_SNAPSHOT_STATUS_0_OFFSET     0x200
#define LAPC_SNAPSHOT_STATUS_1_OFFSET     0x204
#define LAPC_SNAPSHOT_STATUS_2_OFFSET     0x208
#define LAPC_SNAPSHOT_STATUS_3_OFFSET     0x20c

struct xocl_lapc {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
	struct debug_ip_data	data;
	struct lapc_status status;
};

static void update_status(struct xocl_lapc *lapc)
{
	lapc->status.overall_status = XOCL_READ_REG32(lapc->base + LAPC_OVERALL_STATUS_OFFSET);
	lapc->status.cumulative_status_0 = XOCL_READ_REG32(lapc->base + LAPC_CUMULATIVE_STATUS_0_OFFSET);
	lapc->status.cumulative_status_1 = XOCL_READ_REG32(lapc->base + LAPC_CUMULATIVE_STATUS_1_OFFSET);
	lapc->status.cumulative_status_2 = XOCL_READ_REG32(lapc->base + LAPC_CUMULATIVE_STATUS_2_OFFSET);
	lapc->status.cumulative_status_3 = XOCL_READ_REG32(lapc->base + LAPC_CUMULATIVE_STATUS_3_OFFSET);
	lapc->status.snapshot_status_0 = XOCL_READ_REG32(lapc->base + LAPC_SNAPSHOT_STATUS_0_OFFSET);
	lapc->status.snapshot_status_1 = XOCL_READ_REG32(lapc->base + LAPC_SNAPSHOT_STATUS_1_OFFSET);
	lapc->status.snapshot_status_2 = XOCL_READ_REG32(lapc->base + LAPC_SNAPSHOT_STATUS_2_OFFSET);
	lapc->status.snapshot_status_3 = XOCL_READ_REG32(lapc->base + LAPC_SNAPSHOT_STATUS_3_OFFSET);
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_lapc *lapc = platform_get_drvdata(to_platform_device(dev));
	mutex_lock(&lapc->lock);
	update_status(lapc);
	mutex_unlock(&lapc->lock);
	return sprintf(buf, "%u\n%u\n%u\n%u\n%u\n%u\n%u\n%u\n%u\n",
		lapc->status.overall_status,
		lapc->status.cumulative_status_0,
		lapc->status.cumulative_status_1,
		lapc->status.cumulative_status_2,
		lapc->status.cumulative_status_3,
		lapc->status.snapshot_status_0,
		lapc->status.snapshot_status_1,
		lapc->status.snapshot_status_2,
		lapc->status.snapshot_status_3
		);
}

static DEVICE_ATTR_RO(status);

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_lapc *lapc = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "lapc_%llu\n",lapc->data.m_base_address);
}

static DEVICE_ATTR_RO(name);

static struct attribute *lapc_attrs[] = {
			   &dev_attr_status.attr,
			   &dev_attr_name.attr,
			   NULL,
};

static struct attribute_group lapc_attr_group = {
			   .attrs = lapc_attrs,
};

static int lapc_remove(struct platform_device *pdev)
{
	struct xocl_lapc *lapc;
	void *hdl;

	lapc = platform_get_drvdata(pdev);
	if (!lapc) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &lapc_attr_group);

	xocl_drvinst_release(lapc, &hdl);

	if (lapc->base)
		iounmap(lapc->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int lapc_probe(struct platform_device *pdev)
{
	struct xocl_lapc *lapc;
	struct resource *res;
	void *priv;
	int err = 0;

	lapc = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_lapc));
	if (!lapc)
		return -ENOMEM;

	lapc->dev = &pdev->dev;

	priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (priv)
		memcpy(&lapc->data, priv, sizeof(struct debug_ip_data));

	platform_set_drvdata(pdev, lapc);
	mutex_init(&lapc->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}


	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	lapc->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!lapc->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	lapc->start_paddr = res->start;
	lapc->range = res->end - res->start + 1;

	err = sysfs_create_group(&pdev->dev.kobj, &lapc_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create lapc sysfs attrs failed: %d", err);
	}

done:
	if (err) {
		lapc_remove(pdev);
		return err;
	}
	return 0;
}

static int lapc_open(struct inode *inode, struct file *file)
{
	struct xocl_lapc *lapc = NULL;

	lapc = xocl_drvinst_open_single(inode->i_cdev);
	if (!lapc)
		return -ENXIO;
	file->private_data = lapc;
	return 0;
}

static int lapc_close(struct inode *inode, struct file *file)
{
	struct xocl_lapc *lapc = file->private_data;

	xocl_drvinst_close(lapc);
	return 0;
}

static long lapc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_lapc *lapc;
	long result = 0;

	lapc = (struct xocl_lapc *)filp->private_data;

	mutex_lock(&lapc->lock);

	switch (cmd) {
	case 1:
		xocl_err(lapc->dev, "ioctl 1, do nothing");
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&lapc->lock);

	return result;
}


static int lapc_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct xocl_lapc *lapc = (struct xocl_lapc *)filp->private_data;
	BUG_ON(!lapc);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = lapc->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = lapc->range - off;


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


static const struct file_operations lapc_fops = {
	.open = lapc_open,
	.release = lapc_close,
	.mmap = lapc_mmap,
	.unlocked_ioctl = lapc_ioctl,
};

struct xocl_drv_private lapc_priv = {
	.fops = &lapc_fops,
	.dev = -1,
};

struct platform_device_id lapc_id_table[] = {
	{ XOCL_DEVNAME(XOCL_LAPC), (kernel_ulong_t)&lapc_priv },
	{ },
};

static struct platform_driver	lapc_driver = {
	.probe		= lapc_probe,
	.remove		= lapc_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_LAPC),
	},
	.id_table = lapc_id_table,
};

int __init xocl_init_lapc(void)
{
	int err = 0;

	err = alloc_chrdev_region(&lapc_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_LAPC);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&lapc_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(lapc_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_lapc(void)
{
	unregister_chrdev_region(lapc_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&lapc_driver);
}
