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

#define AXI_FIFO_RLR                    0x24
#define AXI_FIFO_RESET_VALUE            0xA5
#define AXI_FIFO_SRR                    0x28
#define AXI_FIFO_RDFR                   0x18

struct trace_fifo_lite {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
};

/**
 * ioctl functions
 */
static long reset_fifo(struct trace_fifo_lite *fifo);
static long get_numbytes(struct trace_fifo_lite *fifo, void __user *arg);

static long reset_fifo(struct trace_fifo_lite *fifo)
{
	uint32_t regValue = AXI_FIFO_RESET_VALUE;
	XOCL_WRITE_REG32(regValue, fifo->base + AXI_FIFO_SRR );
	XOCL_WRITE_REG32(regValue, fifo->base + AXI_FIFO_RDFR);
	return 0;
}

static long get_numbytes(struct trace_fifo_lite *fifo, void __user *arg)
{
	uint32_t fifoCount = 0;
	uint32_t numBytes = 0;

	fifoCount = XOCL_READ_REG32(fifo->base + AXI_FIFO_RLR);
	// Read bits 22:0 per AXI-Stream FIFO product guide (PG080, 10/1/14)
	numBytes = fifoCount & 0x7FFFFF;

	if (copy_to_user(arg, &numBytes, sizeof(uint32_t)))
	{
		return -EFAULT;
	}
	return 0;
}

static int trace_fifo_lite_remove(struct platform_device *pdev)
{
	struct trace_fifo_lite *trace_fifo_lite;
	void *hdl;

	trace_fifo_lite = platform_get_drvdata(pdev);
	if (!trace_fifo_lite) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_drvinst_release(trace_fifo_lite, &hdl);

	if (trace_fifo_lite->base)
		iounmap(trace_fifo_lite->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int trace_fifo_lite_probe(struct platform_device *pdev)
{
	struct trace_fifo_lite *trace_fifo_lite;
	struct resource *res;
	int err = 0;

	trace_fifo_lite = xocl_drvinst_alloc(&pdev->dev, sizeof(struct trace_fifo_lite));
	if (!trace_fifo_lite)
		return -ENOMEM;

	trace_fifo_lite->dev = &pdev->dev;

	platform_set_drvdata(pdev, trace_fifo_lite);
	mutex_init(&trace_fifo_lite->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}


	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	trace_fifo_lite->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!trace_fifo_lite->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	trace_fifo_lite->start_paddr = res->start;
	trace_fifo_lite->range = res->end - res->start + 1;

done:
	if (err) {
		trace_fifo_lite_remove(pdev);
		return err;
	}
	return 0;
}

static int trace_fifo_lite_open(struct inode *inode, struct file *file)
{
	struct trace_fifo_lite *trace_fifo_lite = NULL;

	trace_fifo_lite = xocl_drvinst_open_single(inode->i_cdev);
	if (!trace_fifo_lite)
		return -ENXIO;
	file->private_data = trace_fifo_lite;
	return 0;
}

static int trace_fifo_lite_close(struct inode *inode, struct file *file)
{
	struct trace_fifo_lite *trace_fifo_lite = file->private_data;

	xocl_drvinst_close(trace_fifo_lite);
	return 0;
}

static long trace_fifo_lite_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct trace_fifo_lite *trace_fifo_lite;
	void __user *data;
	long result = 0;

	trace_fifo_lite = (struct trace_fifo_lite *)filp->private_data;
	data = (void __user *)(arg);

	mutex_lock(&trace_fifo_lite->lock);

	switch (cmd) {
	case TR_FIFO_IOC_RESET:
		result = reset_fifo(trace_fifo_lite);
		break;
	case TR_FIFO_IOC_GET_NUMBYTES:
		result = get_numbytes(trace_fifo_lite, data);
		break;
	default:
		result = -ENOTTY;
	}
	mutex_unlock(&trace_fifo_lite->lock);

	return result;
}


static int trace_fifo_lite_mmap(struct file *filp, struct vm_area_struct *vma)
{

	int rc;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	struct trace_fifo_lite *trace_fifo_lite = (struct trace_fifo_lite *)filp->private_data;
	BUG_ON(!trace_fifo_lite);

        off = vma->vm_pgoff << PAGE_SHIFT;
        if (off >= trace_fifo_lite->range) {
            return -EINVAL;
        }

	/* BAR physical address */
	phys = trace_fifo_lite->start_paddr + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = trace_fifo_lite->range - off;


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


static const struct file_operations trace_fifo_lite_fops = {
	.open = trace_fifo_lite_open,
	.release = trace_fifo_lite_close,
	.mmap = trace_fifo_lite_mmap,
	.unlocked_ioctl = trace_fifo_lite_ioctl,
};

struct xocl_drv_private trace_fifo_lite_priv = {
	.fops = &trace_fifo_lite_fops,
	.dev = -1,
};

struct platform_device_id trace_fifo_lite_id_table[] = {
	{ XOCL_DEVNAME(XOCL_TRACE_FIFO_LITE), (kernel_ulong_t)&trace_fifo_lite_priv },
	{ },
};

static struct platform_driver	trace_fifo_lite_driver = {
	.probe		= trace_fifo_lite_probe,
	.remove		= trace_fifo_lite_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_TRACE_FIFO_LITE),
	},
	.id_table = trace_fifo_lite_id_table,
};

int __init xocl_init_trace_fifo_lite(void)
{
	int err = 0;

	err = alloc_chrdev_region(&trace_fifo_lite_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_TRACE_FIFO_LITE);
	if (err < 0)
		goto err_chrdev_reg;

	err = platform_driver_register(&trace_fifo_lite_driver);
	if (err < 0)
		goto err_driver_reg;

	return 0;
err_driver_reg:
	unregister_chrdev_region(trace_fifo_lite_priv.dev, 1);
err_chrdev_reg:
	return err;
}

void xocl_fini_trace_fifo_lite(void)
{
	unregister_chrdev_region(trace_fifo_lite_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&trace_fifo_lite_driver);
}
