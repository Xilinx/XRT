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

struct xocl_trace_s2mm {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
};

static int trace_s2mm_remove(struct platform_device *pdev)
{
	struct xocl_trace_s2mm *trace_s2mm;

	trace_s2mm = platform_get_drvdata(pdev);
	if (!trace_s2mm) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	if (trace_s2mm->base)
		iounmap(trace_s2mm->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(trace_s2mm);

	return 0;
}

static int trace_s2mm_probe(struct platform_device *pdev)
{
	struct xocl_trace_s2mm *trace_s2mm;
	struct resource *res;
	int err = 0;

	trace_s2mm = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_trace_s2mm));
	if (!trace_s2mm)
		return -ENOMEM;

	trace_s2mm->dev = &pdev->dev;

	platform_set_drvdata(pdev, trace_s2mm);
	mutex_init(&trace_s2mm->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}
		

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	trace_s2mm->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!trace_s2mm->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	trace_s2mm->start_paddr = res->start;
	trace_s2mm->range = res->end - res->start + 1;

done:
	if (err) {
		trace_s2mm_remove(pdev);
		return err;
	}
	return 0;
}

static int trace_s2mm_open(struct inode *inode, struct file *file)
{
	struct xocl_trace_s2mm *trace_s2mm = NULL;

	trace_s2mm = xocl_drvinst_open_single(inode->i_cdev);
	if (!trace_s2mm)
		return -ENXIO;
	file->private_data = trace_s2mm;
	return 0;
}

static int trace_s2mm_close(struct inode *inode, struct file *file)
{
	struct xocl_trace_s2mm *trace_s2mm = file->private_data;

	xocl_drvinst_close(trace_s2mm);
	return 0;
}

long trace_s2mm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_trace_s2mm *trace_s2mm;
	long result = 0;

	trace_s2mm = (struct xocl_trace_s2mm *)filp->private_data;

	mutex_lock(&trace_s2mm->lock);

	switch (cmd) {
	case 1:
		xocl_err(trace_s2mm->dev, "ioctl 1, do nothing");
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&trace_s2mm->lock);

	return result;
}


static int trace_s2mm_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct xocl_trace_s2mm *trace_s2mm = (struct xocl_trace_s2mm *)filp->private_data;
	BUG_ON(!trace_s2mm);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = trace_s2mm->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = trace_s2mm->range - off;


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


static const struct file_operations trace_s2mm_fops = {
	.open = trace_s2mm_open,
	.release = trace_s2mm_close,
	.mmap = trace_s2mm_mmap,
	.unlocked_ioctl = trace_s2mm_ioctl,
};

struct xocl_drv_private trace_s2mm_priv = {
	.fops = &trace_s2mm_fops,
	.dev = -1,
};

struct platform_device_id trace_s2mm_id_table[] = {
	{ XOCL_DEVNAME(XOCL_TRACE_S2MM), (kernel_ulong_t)&trace_s2mm_priv },
	{ },
};

static struct platform_driver	trace_s2mm_driver = {
	.probe		= trace_s2mm_probe,
	.remove		= trace_s2mm_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_TRACE_S2MM),
	},
	.id_table = trace_s2mm_id_table,
};

int __init xocl_init_trace_s2mm(void)
{
	int err = 0;

	err = alloc_chrdev_region(&trace_s2mm_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_TRACE_S2MM);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&trace_s2mm_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(trace_s2mm_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_trace_s2mm(void)
{
	unregister_chrdev_region(trace_s2mm_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&trace_s2mm_driver);
}
