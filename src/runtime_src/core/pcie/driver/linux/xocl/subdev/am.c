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

struct xocl_am {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
};

static int am_remove(struct platform_device *pdev)
{
	struct xocl_am *am;

	am = platform_get_drvdata(pdev);
	if (!am) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	if (am->base)
		iounmap(am->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(am);

	return 0;
}

static int am_probe(struct platform_device *pdev)
{
	struct xocl_am *am;
	struct resource *res;
	int err = 0;

	am = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_am));
	if (!am)
		return -ENOMEM;

	am->dev = &pdev->dev;

	platform_set_drvdata(pdev, am);
	mutex_init(&am->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}
		

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	am->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!am->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	am->start_paddr = res->start;
	am->range = res->end - res->start + 1;

done:
	if (err) {
		am_remove(pdev);
		return err;
	}
	return 0;
}

static int am_open(struct inode *inode, struct file *file)
{
	struct xocl_am *am = NULL;

	am = xocl_drvinst_open_single(inode->i_cdev);
	if (!am)
		return -ENXIO;
	file->private_data = am;
	return 0;
}

static int am_close(struct inode *inode, struct file *file)
{
	struct xocl_am *am = file->private_data;

	xocl_drvinst_close(am);
	return 0;
}

long am_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_am *am;
	long result = 0;

	am = (struct xocl_am *)filp->private_data;

	mutex_lock(&am->lock);

	switch (cmd) {
	case 1:
		xocl_err(am->dev, "ioctl 1, do nothing");
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&am->lock);

	return result;
}


static int am_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct xocl_am *am = (struct xocl_am *)filp->private_data;
	BUG_ON(!am);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = am->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = am->range - off;


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
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
#else
	vma->vm_flags |= VM_IO | VM_RESERVED;
#endif

	/* make MMIO accessible to user space */
	rc = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
				vsize, vma->vm_page_prot);
	if (rc)
		return -EAGAIN;
	return rc;
}


static const struct file_operations am_fops = {
	.open = am_open,
	.release = am_close,
	.mmap = am_mmap,
	.unlocked_ioctl = am_ioctl,
};

struct xocl_drv_private am_priv = {
	.fops = &am_fops,
	.dev = -1,
};

struct platform_device_id am_id_table[] = {
	{ XOCL_DEVNAME(XOCL_AM), (kernel_ulong_t)&am_priv },
	{ },
};

static struct platform_driver	am_driver = {
	.probe		= am_probe,
	.remove		= am_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_AM),
	},
	.id_table = am_id_table,
};

int __init xocl_init_am(void)
{
	int err = 0;

	err = alloc_chrdev_region(&am_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_AM);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&am_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(am_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_am(void)
{
	unregister_chrdev_region(am_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&am_driver);
}
