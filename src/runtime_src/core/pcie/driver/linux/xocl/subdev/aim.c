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

/************************ AXI Interface Monitor (AIM, earlier SPM) ***********************/

/* Address offsets in core */
#define XAIM_CONTROL_OFFSET                          0x08
#define XAIM_TRACE_CTRL_OFFSET                       0x10
#define XAIM_SAMPLE_OFFSET                           0x20
#define XAIM_SAMPLE_WRITE_BYTES_OFFSET               0x80
#define XAIM_SAMPLE_WRITE_TRANX_OFFSET               0x84
#define XAIM_SAMPLE_WRITE_LATENCY_OFFSET             0x88
#define XAIM_SAMPLE_READ_BYTES_OFFSET                0x8C
#define XAIM_SAMPLE_READ_TRANX_OFFSET                0x90
#define XAIM_SAMPLE_READ_LATENCY_OFFSET              0x94
// The following two registers are still in the hardware,
//  but are unused
//#define XAIM_SAMPLE_MIN_MAX_WRITE_LATENCY_OFFSET   0x98
//#define XAIM_SAMPLE_MIN_MAX_READ_LATENCY_OFFSET    0x9C
#define XAIM_SAMPLE_OUTSTANDING_COUNTS_OFFSET        0xA0
#define XAIM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET        0xA4
#define XAIM_SAMPLE_LAST_WRITE_DATA_OFFSET           0xA8
#define XAIM_SAMPLE_LAST_READ_ADDRESS_OFFSET         0xAC
#define XAIM_SAMPLE_LAST_READ_DATA_OFFSET            0xB0
#define XAIM_SAMPLE_READ_BUSY_CYCLES_OFFSET          0xB4
#define XAIM_SAMPLE_WRITE_BUSY_CYCLES_OFFSET         0xB8
#define XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET         0xC0
#define XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET         0xC4
#define XAIM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET       0xC8
#define XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET          0xCC
#define XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET          0xD0
#define XAIM_SAMPLE_READ_LATENCY_UPPER_OFFSET        0xD4
// Reserved for high 32-bits of MIN_MAX_WRITE_LATENCY - 0xD8
// Reserved for high 32-bits of MIN_MAX_READ_LATENCY  - 0xDC
#define XAIM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET  0xE0
#define XAIM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET  0xE4
#define XAIM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET     0xE8
#define XAIM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET   0xEC
#define XAIM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET      0xF0
#define XAIM_SAMPLE_READ_BUSY_CYCLES_UPPER_OFFSET    0xF4
#define XAIM_SAMPLE_WRITE_BUSY_CYCLES_UPPER_OFFSET   0xF8

/* SPM Control Register masks */
#define XAIM_CR_COUNTER_RESET_MASK               0x00000002
#define XAIM_CR_COUNTER_ENABLE_MASK              0x00000001
#define XAIM_TRACE_CTRL_MASK                     0x00000003

/* Debug IP layout properties mask bits */
#define XAIM_HOST_PROPERTY_MASK                  0x4
#define XAIM_64BIT_PROPERTY_MASK                 0x8

struct xocl_aim {
	void __iomem		*base;
	struct device		*dev;
	uint64_t		start_paddr;
	uint64_t		range;
	struct mutex 		lock;
	struct debug_ip_data	data;
	struct aim_counters counters;
};

/**
 * helper functions
 */
static void update_counters(struct xocl_aim *aim);
/**
 * ioctl functions
 */
static long reset_counters(struct xocl_aim *aim);
static long start_counters(struct xocl_aim *aim);
static long read_counters(struct xocl_aim *aim, void __user *arg);
static long stop_counters(struct xocl_aim *aim);
static long start_trace(struct xocl_aim *aim, void __user *arg);

static long reset_counters(struct xocl_aim *aim)
{
	uint32_t reg = 0;
	// Original Value
	reg = XOCL_READ_REG32(aim->base + XAIM_CONTROL_OFFSET);
	// Start Reset
	reg = reg | XAIM_CR_COUNTER_RESET_MASK;
	XOCL_WRITE_REG32(reg, aim->base + XAIM_CONTROL_OFFSET);
	// End Reset
	reg = reg & ~(XAIM_CR_COUNTER_RESET_MASK);
	XOCL_WRITE_REG32(reg, aim->base + XAIM_CONTROL_OFFSET);
	return 0;
}

static long start_counters(struct xocl_aim *aim)
{
	uint32_t reg = 0;
	// Original Value
	reg = XOCL_READ_REG32(aim->base + XAIM_CONTROL_OFFSET);
	// Start AXI-MM monitor metric counters
	reg = reg | XAIM_CR_COUNTER_ENABLE_MASK;
	XOCL_WRITE_REG32(reg, aim->base + XAIM_CONTROL_OFFSET);
	// Read from sample register to ensure total time is read again at end
	XOCL_READ_REG32(aim->base + XAIM_SAMPLE_OFFSET);
	return 0;
}

static long read_counters(struct xocl_aim *aim, void __user *arg)
{
	update_counters(aim);
	if (copy_to_user(arg, &aim->counters, sizeof(struct aim_counters)))
	{
		return -EFAULT;
	}
	return 0;
}

static long stop_counters(struct xocl_aim *aim)
{
	uint32_t reg = 0;
	// Original Value
	reg = XOCL_READ_REG32(aim->base + XAIM_CONTROL_OFFSET);
	// Start AXI-MM monitor metric counters
	reg = reg | ~(XAIM_CR_COUNTER_ENABLE_MASK);
	XOCL_WRITE_REG32(reg, aim->base + XAIM_CONTROL_OFFSET);
	return 0;
}

static long start_trace(struct xocl_aim *aim, void __user *arg)
{
	uint32_t options = 0;
	uint32_t reg = 0;
	if (copy_from_user(&options, arg, sizeof(uint32_t)))
	{
		return -EFAULT;
	}
	reg = options & XAIM_TRACE_CTRL_MASK;
	XOCL_WRITE_REG32(reg, aim->base + XAIM_TRACE_CTRL_OFFSET);
	return 0;
}

static void update_counters(struct xocl_aim *aim)
{
	uint64_t low = 0, high = 0, sample_interval = 0;
	// This latches the sampled metric counters
	sample_interval = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_OFFSET);
	// Read the sampled metric counters
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_BYTES_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET);
	aim->counters.wr_bytes =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_TRANX_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET);
	aim->counters.wr_tranx =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_LATENCY_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET);
	aim->counters.wr_latency =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_BUSY_CYCLES_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_WRITE_BUSY_CYCLES_UPPER_OFFSET);
	aim->counters.wr_busy_cycles =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_BYTES_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET);
	aim->counters.rd_bytes =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_TRANX_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET);
	aim->counters.rd_tranx =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_LATENCY_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_LATENCY_UPPER_OFFSET);
	aim->counters.rd_latency =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_BUSY_CYCLES_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_READ_BUSY_CYCLES_UPPER_OFFSET);
	aim->counters.rd_busy_cycles =  (high << 32) | low;

	// Debug Registers
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_OUTSTANDING_COUNTS_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET);
	aim->counters.outstanding_cnt =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET);
	aim->counters.wr_last_address =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_WRITE_DATA_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET);
	aim->counters.wr_last_data =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_READ_ADDRESS_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET);
	aim->counters.rd_last_address =  (high << 32) | low;
	low = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_READ_DATA_OFFSET);
	high = XOCL_READ_REG32(aim->base + XAIM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET);
	aim->counters.rd_last_data =  (high << 32) | low;
}

static ssize_t counters_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_aim *aim = platform_get_drvdata(to_platform_device(dev));
	mutex_lock(&aim->lock);
	update_counters(aim);
	mutex_unlock(&aim->lock);
	return sprintf(buf, "%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n%llu\n",
		aim->counters.wr_bytes,
		aim->counters.wr_tranx,
		aim->counters.wr_latency,
		aim->counters.wr_busy_cycles,
		aim->counters.rd_bytes,
		aim->counters.rd_tranx,
		aim->counters.rd_latency,
		aim->counters.rd_busy_cycles,
		aim->counters.outstanding_cnt,
		aim->counters.wr_last_address,
		aim->counters.wr_last_data,
		aim->counters.rd_last_address,
		aim->counters.rd_last_data
		);
}

static DEVICE_ATTR_RO(counters);

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_aim *aim = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "aximm_mon_%llu\n",aim->data.m_base_address);
}

static DEVICE_ATTR_RO(name);

static struct attribute *aim_attrs[] = {
			   &dev_attr_counters.attr,
			   &dev_attr_name.attr,
			   NULL,
};

static struct attribute_group aim_attr_group = {
			   .attrs = aim_attrs,
};

static int __aim_remove(struct platform_device *pdev)
{
	struct xocl_aim *aim;
	void *hdl;

	aim = platform_get_drvdata(pdev);
	if (!aim) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &aim_attr_group);

	xocl_drvinst_release(aim, &hdl);

	if (aim->base)
		iounmap(aim->base);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void aim_remove(struct platform_device *pdev)
{
	__aim_remove(pdev);
}
#else
#define aim_remove __aim_remove
#endif

static int aim_probe(struct platform_device *pdev)
{
	struct xocl_aim *aim;
	struct resource *res;
	void *priv;
	int err = 0;

	aim = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_aim));
	if (!aim)
		return -ENOMEM;

	aim->dev = &pdev->dev;

	priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (priv)
		memcpy(&aim->data, priv, sizeof(struct debug_ip_data));

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

	err = sysfs_create_group(&pdev->dev.kobj, &aim_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create aim sysfs attrs failed: %d", err);
	}

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

static long aim_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_aim *aim;
	void __user *data;
	long result = 0;

	aim = (struct xocl_aim *)filp->private_data;
	data = (void __user *)(arg);

	mutex_lock(&aim->lock);

	switch (cmd) {
	case AIM_IOC_RESET:
		result = reset_counters(aim);
		break;
	case AIM_IOC_STARTCNT:
		result = start_counters(aim);
		break;
	case AIM_IOC_READCNT:
		result = read_counters(aim, data);
		break;
	case AIM_IOC_STOPCNT:
		result = stop_counters(aim);
		break;
	case AIM_IOC_STARTTRACE:
		result = start_trace(aim, data);
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
        if (off >= aim->range) {
            return -EINVAL;
        }

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
