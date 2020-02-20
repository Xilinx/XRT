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

struct trace_funnel {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
};

static int trace_funnel_remove(struct platform_device *pdev)
{
	struct trace_funnel *trace_funnel;

	trace_funnel = platform_get_drvdata(pdev);
	if (!trace_funnel) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	if (trace_funnel->base)
		iounmap(trace_funnel->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(trace_funnel);

	return 0;
}

static int trace_funnel_probe(struct platform_device *pdev)
{
	struct trace_funnel *trace_funnel;
	struct resource *res;
	int err = 0;

	trace_funnel = xocl_drvinst_alloc(&pdev->dev, sizeof(struct trace_funnel));
	if (!trace_funnel)
		return -ENOMEM;

	trace_funnel->dev = &pdev->dev;

	platform_set_drvdata(pdev, trace_funnel);
	mutex_init(&trace_funnel->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}
		

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	trace_funnel->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!trace_funnel->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	trace_funnel->start_paddr = res->start;
	trace_funnel->range = res->end - res->start + 1;

done:
	if (err) {
		trace_funnel_remove(pdev);
		return err;
	}
	return 0;
}

static int trace_funnel_open(struct inode *inode, struct file *file)
{
	struct trace_funnel *trace_funnel = NULL;

	trace_funnel = xocl_drvinst_open_single(inode->i_cdev);
	if (!trace_funnel)
		return -ENXIO;
	file->private_data = trace_funnel;
	return 0;
}

static int trace_funnel_close(struct inode *inode, struct file *file)
{
	struct trace_funnel *trace_funnel = file->private_data;

	xocl_drvinst_close(trace_funnel);
	return 0;
}

long trace_funnel_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct trace_funnel *trace_funnel;
	long result = 0;

	trace_funnel = (struct trace_funnel *)filp->private_data;

	mutex_lock(&trace_funnel->lock);

	switch (cmd) {
	case 1:
		xocl_err(trace_funnel->dev, "ioctl 1, do nothing");
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&trace_funnel->lock);

	return result;
}


static int trace_funnel_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct trace_funnel *trace_funnel = (struct trace_funnel *)filp->private_data;
	BUG_ON(!trace_funnel);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = trace_funnel->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = trace_funnel->range - off;


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


static const struct file_operations trace_funnel_fops = {
	.open = trace_funnel_open,
	.release = trace_funnel_close,
	.mmap = trace_funnel_mmap,
	.unlocked_ioctl = trace_funnel_ioctl,
};

struct xocl_drv_private trace_funnel_priv = {
	.fops = &trace_funnel_fops,
	.dev = -1,
};

struct platform_device_id trace_funnel_id_table[] = {
	{ XOCL_DEVNAME(XOCL_TRACE_FUNNEL), (kernel_ulong_t)&trace_funnel_priv },
	{ },
};

static struct platform_driver	trace_funnel_driver = {
	.probe		= trace_funnel_probe,
	.remove		= trace_funnel_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_TRACE_FUNNEL),
	},
	.id_table = trace_funnel_id_table,
};

int __init xocl_init_trace_funnel(void)
{
	int err = 0;

	err = alloc_chrdev_region(&trace_funnel_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_TRACE_FUNNEL);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&trace_funnel_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(trace_funnel_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_trace_funnel(void)
{
	unregister_chrdev_region(trace_funnel_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&trace_funnel_driver);
}
