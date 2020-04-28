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

struct xocl_aim {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
};

static int aim_remove(struct platform_device *pdev)
{
	struct xocl_aim *aim;
	void *hdl;

	aim = platform_get_drvdata(pdev);
	if (!aim) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_drvinst_release(aim, &hdl);

	if (aim->base)
		iounmap(aim->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int aim_probe(struct platform_device *pdev)
{
	struct xocl_aim *aim;
	struct resource *res;
	int err = 0;

	aim = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_aim));
	if (!aim)
		return -ENOMEM;

	aim->dev = &pdev->dev;

	platform_set_drvdata(pdev, aim);
	mutex_init(&aim->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}
		

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	aim->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!aim->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	aim->start_paddr = res->start;
	aim->range = res->end - res->start + 1;

done:
	if (err) {
		aim_remove(pdev);
		return err;
	}
	return 0;
}

static int aim_open(struct inode *inode, struct file *file)
{
	struct xocl_aim *aim = NULL;

	aim = xocl_drvinst_open_single(inode->i_cdev);
	if (!aim)
		return -ENXIO;
	file->private_data = aim;
	return 0;
}

static int aim_close(struct inode *inode, struct file *file)
{
	struct xocl_aim *aim = file->private_data;

	xocl_drvinst_close(aim);
	return 0;
}

long aim_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_aim *aim;
	long result = 0;

	aim = (struct xocl_aim *)filp->private_data;

	mutex_lock(&aim->lock);

	switch (cmd) {
	case 1:
		xocl_err(aim->dev, "ioctl 1, do nothing");
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&aim->lock);

	return result;
}


static int aim_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct xocl_aim *aim = (struct xocl_aim *)filp->private_data;
	BUG_ON(!aim);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = aim->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = aim->range - off;


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


static const struct file_operations aim_fops = {
	.open = aim_open,
	.release = aim_close,
	.mmap = aim_mmap,
	.unlocked_ioctl = aim_ioctl,
};

struct xocl_drv_private aim_priv = {
	.fops = &aim_fops,
	.dev = -1,
};

struct platform_device_id aim_id_table[] = {
	{ XOCL_DEVNAME(XOCL_AIM), (kernel_ulong_t)&aim_priv },
	{ },
};

static struct platform_driver	aim_driver = {
	.probe		= aim_probe,
	.remove		= aim_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_AIM),
	},
	.id_table = aim_id_table,
};

int __init xocl_init_aim(void)
{
	int err = 0;

	err = alloc_chrdev_region(&aim_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_AIM);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&aim_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(aim_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_aim(void)
{
	unregister_chrdev_region(aim_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&aim_driver);
}
