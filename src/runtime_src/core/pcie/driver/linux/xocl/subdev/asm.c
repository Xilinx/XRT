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

struct xocl_asm {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
};

static int asm_remove(struct platform_device *pdev)
{
	struct xocl_asm *xocl_asm;
	void *hdl;

	xocl_asm = platform_get_drvdata(pdev);
	if (!xocl_asm) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_drvinst_release(xocl_asm, &hdl);

	if (xocl_asm->base)
		iounmap(xocl_asm->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int asm_probe(struct platform_device *pdev)
{
	struct xocl_asm *xocl_asm;
	struct resource *res;
	int err = 0;

	xocl_asm = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_asm));
	if (!xocl_asm)
		return -ENOMEM;

	xocl_asm->dev = &pdev->dev;

	platform_set_drvdata(pdev, xocl_asm);
	mutex_init(&xocl_asm->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}
		

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	xocl_asm->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!xocl_asm->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	xocl_asm->start_paddr = res->start;
	xocl_asm->range = res->end - res->start + 1;

done:
	if (err) {
		asm_remove(pdev);
		return err;
	}
	return 0;
}

static int asm_open(struct inode *inode, struct file *file)
{
	struct xocl_asm *xocl_asm = NULL;

	xocl_asm = xocl_drvinst_open_single(inode->i_cdev);
	if (!xocl_asm)
		return -ENXIO;
	file->private_data = xocl_asm;
	return 0;
}

static int asm_close(struct inode *inode, struct file *file)
{
	struct xocl_asm *xocl_asm = file->private_data;

	xocl_drvinst_close(xocl_asm);
	return 0;
}

long asm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_asm *xocl_asm;
	long result = 0;

	xocl_asm = (struct xocl_asm *)filp->private_data;

	mutex_lock(&xocl_asm->lock);

	switch (cmd) {
	case 1:
		xocl_err(xocl_asm->dev, "ioctl 1, do nothing");
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&xocl_asm->lock);

	return result;
}


static int asm_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct xocl_asm *xocl_asm = (struct xocl_asm *)filp->private_data;
	BUG_ON(!xocl_asm);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = xocl_asm->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = xocl_asm->range - off;


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


static const struct file_operations asm_fops = {
	.open = asm_open,
	.release = asm_close,
	.mmap = asm_mmap,
	.unlocked_ioctl = asm_ioctl,
};

struct xocl_drv_private asm_priv = {
	.fops = &asm_fops,
	.dev = -1,
};

struct platform_device_id asm_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ASM), (kernel_ulong_t)&asm_priv },
	{ },
};

static struct platform_driver	asm_driver = {
	.probe		= asm_probe,
	.remove		= asm_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ASM),
	},
	.id_table = asm_id_table,
};

int __init xocl_init_asm(void)
{
	int err = 0;

	err = alloc_chrdev_region(&asm_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_ASM);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&asm_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(asm_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_asm(void)
{
	unregister_chrdev_region(asm_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&asm_driver);
}
