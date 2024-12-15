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

/************************ Accelerator Monitor (AM, earlier SAM) ************************/

#define XAM_CONTROL_OFFSET                          0x08
#define XAM_TRACE_CTRL_OFFSET                       0x10
#define XAM_SAMPLE_OFFSET                           0x20
#define XAM_ACCEL_EXECUTION_COUNT_OFFSET            0x80
#define XAM_ACCEL_EXECUTION_CYCLES_OFFSET           0x84
#define XAM_ACCEL_STALL_INT_OFFSET                  0x88
#define XAM_ACCEL_STALL_STR_OFFSET                  0x8c
#define XAM_ACCEL_STALL_EXT_OFFSET                  0x90
#define XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET       0x94
#define XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET       0x98
#define XAM_ACCEL_TOTAL_CU_START_OFFSET             0x9c
#define XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET      0xA0
#define XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET     0xA4
#define XAM_ACCEL_STALL_INT_UPPER_OFFSET            0xA8
#define XAM_ACCEL_STALL_STR_UPPER_OFFSET            0xAc
#define XAM_ACCEL_STALL_EXT_UPPER_OFFSET            0xB0
#define XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET 0xB4
#define XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET 0xB8
#define XAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET       0xbc
#define XAM_BUSY_CYCLES_OFFSET                      0xC0
#define XAM_BUSY_CYCLES_UPPER_OFFSET                0xC4
#define XAM_MAX_PARALLEL_ITER_OFFSET                0xC8
#define XAM_MAX_PARALLEL_ITER_UPPER_OFFSET          0xCC

/* SAM Trace Control Masks */
#define XAM_TRACE_STALL_SELECT_MASK    0x0000001c
#define XAM_COUNTER_RESET_MASK         0x00000002
#define XAM_DATAFLOW_EN_MASK           0x00000008

struct xocl_am {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
	struct debug_ip_data	data;
	struct am_counters counters;
};

/**
 * helper functions
 */
static void update_counters(struct xocl_am *am);
/**
 * ioctl functions
 */
static long reset_counters(struct xocl_am *am);
static long start_counters(struct xocl_am *am);
static long read_counters(struct xocl_am *am, void __user *arg);
static long stop_counters(struct xocl_am *am);
static long start_trace(struct xocl_am *am, void __user *arg);
static long stop_trace(struct xocl_am *am);
static long config_dataflow(struct xocl_am *am, void __user *arg);

static long reset_counters(struct xocl_am *am)
{
	uint32_t reg = 0;
	reg = XOCL_READ_REG32(am->base + XAM_CONTROL_OFFSET);
	// Start Reset
	reg = reg | XAM_COUNTER_RESET_MASK;
	XOCL_WRITE_REG32(reg, am->base + XAM_CONTROL_OFFSET);
	// End Reset
	reg = reg & ~(XAM_COUNTER_RESET_MASK);
	XOCL_WRITE_REG32(reg, am->base + XAM_CONTROL_OFFSET);
	return 0;
}

static long start_counters(struct xocl_am *am)
{
	// Needs hw implementation
	return 0;
}

static long read_counters(struct xocl_am *am, void __user *arg)
{
	update_counters(am);
	if (copy_to_user(arg, &am->counters, sizeof(struct am_counters)))
	{
		return -EFAULT;
	}
	return 0;
}

static long stop_counters(struct xocl_am *am)
{
	// Needs hw implementation
	return 0;
}

static long start_trace(struct xocl_am *am, void __user *arg)
{
	uint32_t options = 0;
	uint32_t reg = 0;
	if (copy_from_user(&options, arg, sizeof(uint32_t)))
	{
		return -EFAULT;
	}
	// Set Stall trace control register bits
	// Bit 1 : CU (Always ON)  Bit 2 : INT  Bit 3 : STR  Bit 4 : Ext
	reg = ((options & XAM_TRACE_STALL_SELECT_MASK) >> 1) | 0x1;
	XOCL_WRITE_REG32(reg, am->base + XAM_TRACE_CTRL_OFFSET);
	return 0;
}

static long stop_trace(struct xocl_am *am)
{
	uint32_t reg = 0;
	XOCL_WRITE_REG32(reg, am->base + XAM_TRACE_CTRL_OFFSET);
	return 0;
}

static long config_dataflow(struct xocl_am *am, void __user *arg)
{
	uint32_t options = 0;
	uint32_t reg = 0;
	if (copy_from_user(&options, arg, sizeof(uint32_t)))
	{
		return -EFAULT;
	}
	if (options == 0)
	{
		return 0;
	}
	reg = XOCL_READ_REG32(am->base + XAM_CONTROL_OFFSET);
	reg = reg | XAM_DATAFLOW_EN_MASK;
	XOCL_WRITE_REG32(reg, am->base + XAM_CONTROL_OFFSET);
	return 0;
}

static void update_counters(struct xocl_am *am)
{
	uint64_t low = 0, high = 0, sample_interval = 0;

	// This latches the sampled metric counters
	sample_interval = XOCL_READ_REG32(am->base + XAM_SAMPLE_OFFSET);
	// Read the sampled metric counters
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_EXECUTION_COUNT_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET);
	am->counters.end_count =  (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_TOTAL_CU_START_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET);
	am->counters.start_count = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_EXECUTION_CYCLES_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET);
	am->counters.exec_cycles = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_STALL_INT_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_STALL_INT_UPPER_OFFSET);
	am->counters.stall_int_cycles = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_STALL_STR_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_STALL_STR_UPPER_OFFSET);
	am->counters.stall_str_cycles = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_STALL_EXT_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_STALL_EXT_UPPER_OFFSET);
	am->counters.stall_ext_cycles = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_BUSY_CYCLES_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_BUSY_CYCLES_UPPER_OFFSET);
	am->counters.busy_cycles = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_MAX_PARALLEL_ITER_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET);
	am->counters.max_parallel_iterations = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET);
	am->counters.max_exec_cycles = (high << 32) | low;
	low = XOCL_READ_REG32(am->base + XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET);
	high = XOCL_READ_REG32(am->base + XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET);
	am->counters.min_exec_cycles = (high << 32) | low;
}

static ssize_t counters_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_am *am = platform_get_drvdata(to_platform_device(dev));
	mutex_lock(&am->lock);
	update_counters(am);
	mutex_unlock(&am->lock);
	return sprintf(buf, "%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n",
		am->counters.end_count,
		am->counters.start_count,
		am->counters.exec_cycles,
		am->counters.stall_int_cycles,
		am->counters.stall_str_cycles,
		am->counters.stall_ext_cycles,
		am->counters.busy_cycles,
		am->counters.max_parallel_iterations,
		am->counters.max_exec_cycles,
		am->counters.min_exec_cycles
		);
}

static DEVICE_ATTR_RO(counters);

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_am *am = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "accel_mon_%llu\n",am->data.m_base_address);
}

static DEVICE_ATTR_RO(name);

static struct attribute *am_attrs[] = {
			   &dev_attr_counters.attr,
			   &dev_attr_name.attr,
			   NULL,
};

static struct attribute_group am_attr_group = {
			   .attrs = am_attrs,
};

static int am_remove(struct platform_device *pdev)
{
	struct xocl_am *am;
	void *hdl;

	am = platform_get_drvdata(pdev);
	if (!am) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &am_attr_group);

	xocl_drvinst_release(am, &hdl);

	if (am->base)
		iounmap(am->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int am_probe(struct platform_device *pdev)
{
	struct xocl_am *am;
	struct resource *res;
	int err = 0;
	void *priv;

	am = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_am));
	if (!am)
		return -ENOMEM;

	am->dev = &pdev->dev;

	priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (priv)
		memcpy(&am->data, priv, sizeof(struct debug_ip_data));

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

	err = sysfs_create_group(&pdev->dev.kobj, &am_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create am sysfs attrs failed: %d", err);
	}

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

static long am_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_am *am;
	void __user *data;
	long result = 0;

	am = (struct xocl_am *)filp->private_data;
	data = (void __user *)(arg);

	mutex_lock(&am->lock);

	switch (cmd) {
	case AM_IOC_RESET:
		result = reset_counters(am);
		break;
	case AM_IOC_STARTCNT:
		result = start_counters(am);
		break;
	case AM_IOC_READCNT:
		result = read_counters(am, data);
		break;
	case AM_IOC_STOPCNT:
		result = stop_counters(am);
		break;
	case AM_IOC_STARTTRACE:
		result = start_trace(am, data);
		break;
	case AM_IOC_STOPTRACE:
		result = stop_trace(am);
		break;
	case AM_IOC_CONFIGDFLOW:
		result = config_dataflow(am, data);
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
        if (off >= am->range) {
            return -EINVAL;
        }

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
