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

/************************** AXI Stream Monitor (ASM) *********************/

#define XASM_CONTROL_OFFSET           0x0
#define XASM_SAMPLE_OFFSET            0x20
#define XASM_NUM_TRANX_OFFSET         0x80
#define XASM_DATA_BYTES_OFFSET        0x88
#define XASM_BUSY_CYCLES_OFFSET       0x90
#define XASM_STALL_CYCLES_OFFSET      0x98
#define XASM_STARVE_CYCLES_OFFSET     0xA0

/* Control Mask */
#define XASM_COUNTER_RESET_MASK       0x00000001
#define XASM_TRACE_ENABLE_MASK        0x00000002
#define XASM_TRACE_CTRL_MASK          0x2

struct xocl_asm {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
	struct debug_ip_data	data;
	struct asm_counters	counters;
};

/**
 * helper functions
 */
static void update_counters(struct xocl_asm *xocl_asm);
/**
 * ioctl functions
 */
static long reset_counters(struct xocl_asm *xocl_asm);
static long start_counters(struct xocl_asm *xocl_asm);
static long read_counters(struct xocl_asm *xocl_asm, void __user *arg);
static long stop_counters(struct xocl_asm *xocl_asm);
static long start_trace(struct xocl_asm *xocl_asm, void __user *arg);

static long reset_counters(struct xocl_asm *xocl_asm)
{
	uint32_t reg = 0;
	reg = XOCL_READ_REG32(xocl_asm->base + XASM_CONTROL_OFFSET);
	// Start Reset
	reg = reg | XASM_COUNTER_RESET_MASK;
	XOCL_WRITE_REG32(reg, xocl_asm->base + XASM_CONTROL_OFFSET);
	// End Reset
	reg = reg & ~(XASM_COUNTER_RESET_MASK);
	XOCL_WRITE_REG32(reg, xocl_asm->base + XASM_CONTROL_OFFSET);

	return 0;
}

static long start_counters(struct xocl_asm *xocl_asm)
{
	// Read sample register
	// Needs hw implementation
	return 0;
}

static long read_counters(struct xocl_asm *xocl_asm, void __user *arg)
{
	update_counters(xocl_asm);
	if (copy_to_user(arg, &xocl_asm->counters, sizeof(struct asm_counters)))
	{
		return -EFAULT;
	}
	return 0;
}

static long stop_counters(struct xocl_asm *xocl_asm)
{
	// Needs hw implementation
	return 0;
}

static long start_trace(struct xocl_asm *xocl_asm, void __user *arg)
{
	uint32_t options = 0;
	uint32_t reg = 0;
	if (copy_from_user(&options, arg, sizeof(uint32_t)))
	{
		return -EFAULT;
	}
	reg = XOCL_READ_REG32(xocl_asm->base + XASM_CONTROL_OFFSET);
	if (options & XASM_TRACE_CTRL_MASK)
		reg |= XASM_TRACE_ENABLE_MASK;
	else
		reg &= (~XASM_TRACE_ENABLE_MASK);
	XOCL_WRITE_REG32(reg, xocl_asm->base + XASM_CONTROL_OFFSET);
	return 0;
}

static void update_counters(struct xocl_asm *xocl_asm)
{
	uint64_t low = 0, high = 0, sample_interval = 0;
	// This latches the sampled metric counters
	sample_interval = XOCL_READ_REG32(xocl_asm->base + XASM_SAMPLE_OFFSET);
	// Read the sampled metric counters
	low = XOCL_READ_REG32(xocl_asm->base + XASM_NUM_TRANX_OFFSET);
	high = XOCL_READ_REG32(xocl_asm->base + XASM_NUM_TRANX_OFFSET + 0x4);
	xocl_asm->counters.num_tranx =  (high << 32) | low;
	low = XOCL_READ_REG32(xocl_asm->base + XASM_DATA_BYTES_OFFSET);
	high = XOCL_READ_REG32(xocl_asm->base + XASM_DATA_BYTES_OFFSET + 0x4);
	xocl_asm->counters.data_bytes =  (high << 32) | low;
	low = XOCL_READ_REG32(xocl_asm->base + XASM_BUSY_CYCLES_OFFSET);
	high = XOCL_READ_REG32(xocl_asm->base + XASM_BUSY_CYCLES_OFFSET + 0x4);
	xocl_asm->counters.busy_cycles =  (high << 32) | low;
	low = XOCL_READ_REG32(xocl_asm->base + XASM_STALL_CYCLES_OFFSET);
	high = XOCL_READ_REG32(xocl_asm->base + XASM_STALL_CYCLES_OFFSET + 0x4);
	xocl_asm->counters.stall_cycles =  (high << 32) | low;
	low = XOCL_READ_REG32(xocl_asm->base + XASM_STARVE_CYCLES_OFFSET);
	high = XOCL_READ_REG32(xocl_asm->base + XASM_STARVE_CYCLES_OFFSET + 0x4);
	xocl_asm->counters.starve_cycles =  (high << 32) | low;
}

static ssize_t counters_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_asm *xocl_asm = platform_get_drvdata(to_platform_device(dev));
	mutex_lock(&xocl_asm->lock);
	update_counters(xocl_asm);
	mutex_unlock(&xocl_asm->lock);
	return sprintf(buf, "%llu\n%llu\n%llu\n%llu\n%llu\n",
		xocl_asm->counters.num_tranx,
		xocl_asm->counters.data_bytes,
		xocl_asm->counters.busy_cycles,
		xocl_asm->counters.stall_cycles,
		xocl_asm->counters.starve_cycles
		);
}

static DEVICE_ATTR_RO(counters);

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_asm *xocl_asm = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "axistream_mon_%llu\n",xocl_asm->data.m_base_address);
}

static DEVICE_ATTR_RO(name);

static struct attribute *asm_attrs[] = {
			   &dev_attr_counters.attr,
			   &dev_attr_name.attr,
			   NULL,
};

static struct attribute_group asm_attr_group = {
			   .attrs = asm_attrs,
};

static int __asm_remove(struct platform_device *pdev)
{
	struct xocl_asm *xocl_asm;
	void *hdl;

	xocl_asm = platform_get_drvdata(pdev);
	if (!xocl_asm) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &asm_attr_group);

	xocl_drvinst_release(xocl_asm, &hdl);

	if (xocl_asm->base)
		iounmap(xocl_asm->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void asm_remove(struct platform_device *pdev)
{
	__asm_remove(pdev);
}
#else
#define asm_remove __asm_remove
#endif

static int asm_probe(struct platform_device *pdev)
{
	struct xocl_asm *xocl_asm;
	struct resource *res;
	int err = 0;
	void *priv;

	xocl_asm = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_asm));
	if (!xocl_asm)
		return -ENOMEM;

	xocl_asm->dev = &pdev->dev;

	priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (priv)
		memcpy(&xocl_asm->data, priv, sizeof(struct debug_ip_data));

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

	err = sysfs_create_group(&pdev->dev.kobj, &asm_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create asm sysfs attrs failed: %d", err);
	}

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

static long asm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_asm *xocl_asm;
	void __user *data;
	long result = 0;

	xocl_asm = (struct xocl_asm *)filp->private_data;
	data = (void __user *)(arg);

	mutex_lock(&xocl_asm->lock);

	switch (cmd) {
	case ASM_IOC_RESET:
		result = reset_counters(xocl_asm);
		break;
	case ASM_IOC_STARTCNT:
		result = start_counters(xocl_asm);
		break;
	case ASM_IOC_READCNT:
		result = read_counters(xocl_asm, data);
		break;
	case ASM_IOC_STOPCNT:
		result = stop_counters(xocl_asm);
		break;
	case ASM_IOC_STARTTRACE:
		result = start_trace(xocl_asm, data);
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
        if (off >= xocl_asm->range) {
            return -EINVAL;
        }

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
