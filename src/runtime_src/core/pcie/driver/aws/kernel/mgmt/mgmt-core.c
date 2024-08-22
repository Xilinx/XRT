/*
 * Simple Driver for Management PF
 *
 * Copyright (C) 2017-2019 Xilinx, Inc.
 *
 * Code borrowed from Xilinx SDAccel XDMA driver
 *
 * Author(s):
 * Sonal Santan <sonal.santan@xilinx.com>
 * Jan Stephan  <j.stephan@hzdr.de>
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/ioctl.h>
#include <linux/types.h>

#include "mgmt-core.h"
#include "mgmt-ioctl.h"

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sonal Santan <sonal.santan@xilinx.com>");

MODULE_VERSION(__stringify(AWSMGMT_DRIVER_MAJOR) "."
	       __stringify(AWSMGMT_DRIVER_MINOR) "."
	       __stringify(AWSMGMT_DRIVER_PATCHLEVEL));

MODULE_DESCRIPTION("AWS Management PF driver");

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(0x1d0f, 0x1040), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

int health_interval = 5;
module_param(health_interval, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(health_interval,
	"Interval (in sec) after which the health thread is run. (1 = Minimum, 5 = default)");

int health_check = 1;
module_param(health_check, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(health_check,
	"Enable health thread that checks the status of AXI Firewall and SYSMON. (0 = disable, 1 = enable)");

static int timeout_config = 1;
module_param(timeout_config, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(timeout_config,
	"Enable AWS timeout configuration (0 = disable, 1 = enable (default))");

static int rate_limiter_config = 0;
module_param(rate_limiter_config, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(rate_limiter_config,
	"Enable AWS rate limiter configuration (0 = disable (default), 1 = enable)");

struct timeout_pair {
	unsigned reg;
	unsigned val;
};

/*
 * Typical values for timeouts
 * 0x07d0 08 us
 * 0x0dac 14 us
 * 0x1388 20 us
 */
static const struct timeout_pair timeout_table[] = {
	{TIMEOUT0, 0x40000000},
	{TIMEOUT1, 0x000007d0},
	{TIMEOUT2, 0x00000271},
	{TIMEOUT3, 0x000007d0},
	{TIMEOUT4, 0x000007d0},
	{TIMEOUT5, 0x000007d0},
	{TIMEOUT6, 0x00000271},
	{TIMEOUT7, 0x000007d0},
	{TIMEOUT8, 0x00000010},
	{TIMEOUT9, 0x0007a120},
};

static const struct timeout_pair timeout_moderation_table[] = {
	{TIMEOUT_MODERATION0, 0x00000400},
	{TIMEOUT_MODERATION1, 0x00400000},
	{TIMEOUT_MODERATION2, 0x000f4240},
	{TIMEOUT_MODERATION3, 0xfff72710},
	{TIMEOUT_MODERATION4, 0x000000a0},
};

static int instance = 0;
static dev_t awsmgmt_devnode;
static struct class *awsmgmt_class;
void __iomem *pf1_addr_reg;

/*
 * Unmap the BAR regions that had been mapped earlier using map_bars()
 */
static void unmap_bars(struct awsmgmt_dev *lro)
{
	int i;
	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	for (i = 0; i < AWSMGMT_MAX_BAR; i++) {
		/* is this BAR mapped? */
		if (lro->bar[i]) {
			/* unmap BAR */
			pci_iounmap(lro->pci_dev, lro->bar[i]);
			/* mark as unmapped */
			lro->bar[i] = NULL;
		}
	}
}

/* map_bars() -- map device regions into kernel virtual address space
 *
 * Map the device memory regions into kernel virtual address space after
 * verifying their sizes respect the minimum sizes needed, given by the
 * bar_map_sizes[] array.
 */
static int map_bars(struct awsmgmt_dev *lro)
{
	int rc;
	int i;

	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	/* iterate through all the BARs */
	for (i = 0; i < AWSMGMT_MAX_BAR; i++) {
		resource_size_t bar_length = pci_resource_len(lro->pci_dev, i);
		resource_size_t map_length = bar_length;
		lro->bar[i] = NULL;
		printk(KERN_INFO "%s: %s Idx: %d, bar len: %d \n", DRV_NAME, __FUNCTION__, i, (int)bar_length);

		/* skip non-present BAR2 and higher */
		if (!bar_length) continue;

		lro->bar[i] = pci_iomap(lro->pci_dev, i, map_length);
		if (!lro->bar[i]) {
			printk(KERN_DEBUG "Could not map BAR #%d. See bar_map_size option to reduce the map size.\n", i);
			rc = -EIO;
			goto fail;
		}

		lro->bar_map_size[i] = bar_length;
	}
	/* succesfully mapped all required BAR regions */
	rc = 0;
	goto success;
fail:
	/* unwind; unmap any BARs that we did map */
	unmap_bars(lro);
success:
	return rc;
}

static int pcie_link_info(const struct awsmgmt_dev *lro, struct xclmgmt_ioc_info *obj)
{
	u16 stat;
	long result;

	obj->pcie_link_width = 0;
	obj->pcie_link_speed = 0;
	result = pcie_capability_read_word(lro->pci_dev, PCI_EXP_LNKSTA, &stat);
	if (result)
		return result;
	obj->pcie_link_width = (stat & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
	obj->pcie_link_speed = stat & PCI_EXP_LNKSTA_CLS;
	return 0;
}

#if 0
static bool are_migs_calibrated(const struct awsmgmt_dev *lro )
{
	u32 val;
	if(lro->is1DDR) {
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_STATUS_OFFSET);
		printk(KERN_INFO "Card %d MIG C status = %d\n", lro->instance, val);
		if(val != 1) return false;
		return true;
	}
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_STATUS_OFFSET);
	printk(KERN_INFO "Card %d MIG A status = %d\n", lro->instance, val);
	if(val != 1) return false;
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_STATUS_OFFSET);
	printk(KERN_INFO "Card %d MIG B status = %d\n", lro->instance, val);
	if(val != 1) return false;
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_STATUS_OFFSET);
	printk(KERN_INFO "Card %d MIG C status = %d\n", lro->instance, val);
	if(val != 1) return false;
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_STATUS_OFFSET);
	printk(KERN_INFO "Card %d MIG D status = %d\n", lro->instance, val);
	if(val != 1) return false;
	return true;
}
#endif

void device_info(struct awsmgmt_dev *lro, struct xclmgmt_ioc_info *obj)
{
	u32 val;
	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	memset(obj, 0, sizeof(struct xclmgmt_ioc_info));
	obj->vendor = lro->pci_dev->vendor;
	obj->device = lro->pci_dev->device;
	obj->subsystem_vendor = lro->pci_dev->subsystem_vendor;
	obj->subsystem_device = lro->pci_dev->subsystem_device;
	obj->driver_version = AWSMGMT_DRIVER_MAJOR * 1000 + AWSMGMT_DRIVER_MINOR * 100 + AWSMGMT_DRIVER_PATCHLEVEL;
	obj->device_version = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + VERSION_BASE);

	/* MIG calibration */
	if(lro->is1DDR) {
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_STATUS_OFFSET);
		printk("Card %d MIG C status = %d\n", lro->instance, val);
		obj->mig_calibration[0] = (val == 1);
	} else {
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_STATUS_OFFSET);
		printk("Card %d MIG A status = %d\n", lro->instance, val);
		obj->mig_calibration[0] = (val == 1);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_STATUS_OFFSET);
		printk("Card %d MIG B status = %d\n", lro->instance, val);
		obj->mig_calibration[1] = (val == 1);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_STATUS_OFFSET);
		printk("Card %d MIG C status = %d\n", lro->instance, val);
		obj->mig_calibration[2] = (val == 1);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_STATUS_OFFSET);
		printk("Card %d MIG D status = %d\n", lro->instance, val);
		obj->mig_calibration[3] = (val == 1);
	}

	fill_frequency_info(lro, obj);
	pcie_link_info(lro, obj);
}

static int version_ioctl(struct awsmgmt_dev *lro, void __user *arg)
{
	struct xclmgmt_ioc_info obj;
	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	device_info(lro, &obj);
	if (copy_to_user(arg, &obj, sizeof(struct xclmgmt_ioc_info)))
		return -EFAULT;
	return 0;
}

static long char_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct awsmgmt_char *lro_char;
	struct awsmgmt_dev *lro;
	int result = 0;

	printk(KERN_INFO "mgmgt ioctl called. \n");
	/* fetch device specific data stored earlier during open */
	lro_char = (struct awsmgmt_char *)file->private_data;
	BUG_ON(!lro_char);
	lro = lro_char->lro;

	if (_IOC_TYPE(cmd) != XCLMGMT_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		result = !AWSMGMT_ACCESS_OK(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		result = !AWSMGMT_ACCESS_OK(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (result)
		return -EFAULT;

	switch (cmd) {
	case XCLMGMT_IOCINFO:
		printk(KERN_INFO "mgmgt INFO ioctl called. \n");
		result = version_ioctl(lro, (void __user *)arg);
		break;
	case XCLMGMT_IOCICAPDOWNLOAD:
		printk(KERN_INFO "mgmgt ICAP ioctl called. \n");
		result = bitstream_ioctl(lro, (void __user *)arg);
		break;
	case XCLMGMT_IOCICAPDOWNLOAD_AXLF:
		printk(KERN_INFO "mgmgt axlf ioctl called. \n");
		result = bitstream_ioctl_axlf(lro, (void __user *)arg);
		break;
	case XCLMGMT_IOCFREQSCALE:
		result = ocl_freqscaling_ioctl(lro, (void __user *)arg);
		break;
	case XCLMGMT_IOCREBOOT:
		result = -EINVAL;
		break;
	case XCLMGMT_IOCERRINFO:
		result = -EINVAL;
		break;
	default:
		result = -ENOTTY;
	}
	return result;
}


/*
 * Called when the device goes from unused to used.
 */
static int char_open(struct inode *inode, struct file *file)
{
	struct awsmgmt_char *lro_char;
	/* pointer to containing data structure of the character device inode */
	lro_char = container_of(inode->i_cdev, struct awsmgmt_char, cdev);

	/* create a reference to our char device in the opened file */
	file->private_data = lro_char;
	printk(KERN_INFO "/dev/awsmgmt0 %s opened\n", DRV_NAME);
	return 0;
}

/*
 * Called when the device goes from used to unused.
 */
static int char_close(struct inode *inode, struct file *file)
{
	struct awsmgmt_dev *lro;
	struct awsmgmt_char *lro_char = (struct awsmgmt_char *)file->private_data;
	BUG_ON(!lro_char);

	/* fetch device specific data stored earlier during open */
	printk(KERN_INFO "Closing node %s (0x%p, 0x%p)\n", DRV_NAME, inode, file);
	lro = lro_char->lro;
	BUG_ON(!lro);

	return 0;
}



/* maps the PCIe BAR into user space for memory-like access using mmap() */
static int bridge_mmap(struct file *file, struct vm_area_struct *vma)
{
	int rc;
	struct awsmgmt_dev *lro;
	struct awsmgmt_char *lro_char = (struct awsmgmt_char *)file->private_data;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;
	BUG_ON(!lro_char);
	lro = lro_char->lro;
	BUG_ON(!lro);

	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = pci_resource_start(lro->pci_dev, lro_char->bar) + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = pci_resource_end(lro->pci_dev, lro_char->bar) - pci_resource_start(lro->pci_dev, lro_char->bar) + 1 - off;

	printk(KERN_INFO "mmap(): lro_char = 0x%08lx\n", (unsigned long)lro_char);
	printk(KERN_INFO "mmap(): lro_char->bar = %d\n", lro_char->bar);
	printk(KERN_INFO "mmap(): lro = 0x%p\n", lro);
	printk(KERN_INFO "mmap(): pci_dev = 0x%08lx\n", (unsigned long)lro->pci_dev);

	printk(KERN_INFO "off = 0x%lx\n", off);
	printk(KERN_INFO "start = 0x%llx\n", (unsigned long long)pci_resource_start(lro->pci_dev, lro_char->bar));
	printk(KERN_INFO "phys = 0x%lx\n", phys);

	if (vsize > psize)
		return -EINVAL;
	/* pages must not be cached as this would result in cache line sized
	   accesses to the end point */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	/* prevent touching the pages (byte access) for swap-in,
	   and prevent the pages from being swapped out */
#ifndef VM_RESERVED
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
#else
	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#endif
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
	vma->vm_flags |= VM_IO | VM_RESERVED;
#else
	vm_flags_set(vma, VM_IO | VM_RESERVED);
#endif
#endif
	/* make MMIO accessible to user space */
	rc = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
				vsize, vma->vm_page_prot);
	printk(KERN_INFO "io_remap_pfn_range(vma=0x%p, vma->vm_start=0x%lx, phys=0x%lx, size=%lu) = %d\n",
	       vma, vma->vm_start, phys >> PAGE_SHIFT, vsize, rc);
	if (rc)
		return -EAGAIN;
	//vma->vm_ops = &vm_ops;
	return 0;
}



/*
 * character device file operations for control bus (through control bridge)
 */
static struct file_operations ctrl_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.mmap = bridge_mmap,
	.unlocked_ioctl = char_ioctl,
};

/* create_char() -- create a character device interface to data or control bus
 *
 * If at least one SG DMA engine is specified, the character device interface
 * is coupled to the SG DMA file operations which operate on the data bus. If
 * no engines are specified, the interface is coupled with the control bus.
 */
static struct awsmgmt_char *create_char(struct awsmgmt_dev *lro, int bar)
{
	struct awsmgmt_char *lro_char;
	int rc;
	unsigned major;

	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	/* allocate book keeping data structure */
	lro_char = kzalloc(sizeof(struct awsmgmt_char), GFP_KERNEL);
	if (!lro_char)
		return NULL;

	/* dynamically pick a number into cdevno */
	lro_char->lro = lro;
	lro_char->bar = bar;
	/* couple the control device file operations to the character device */
	cdev_init(&lro_char->cdev, &ctrl_fops);
	lro_char->cdev.owner = THIS_MODULE;
	major = MAJOR(awsmgmt_devnode);
	printk(KERN_INFO "%s: %s major no: %u, instance: %d\n", DRV_NAME, __FUNCTION__, major, lro->instance);
	lro_char->cdev.dev = MKDEV(major, lro->instance);
	rc = cdev_add(&lro_char->cdev, lro_char->cdev.dev, 1);
	if (rc < 0) {
		printk(KERN_INFO "cdev_add() = %d\n", rc);
		goto fail_add;
	}
	printk(KERN_INFO "%s: %s cdev_add done\n", DRV_NAME, __FUNCTION__);

	lro_char->sys_device = device_create(awsmgmt_class, &lro->pci_dev->dev, lro_char->cdev.dev, NULL,
					     DRV_NAME "%d", lro->instance);

	printk(KERN_INFO "%s: %s device_create done\n", DRV_NAME, __FUNCTION__);
	if (IS_ERR(lro_char->sys_device)) {
		printk(KERN_INFO "%s: %s device_create failed\n", DRV_NAME, __FUNCTION__);
		rc = PTR_ERR(lro_char->sys_device);
		goto fail_device;
	}
	else
		goto success;
fail_device:
	cdev_del(&lro_char->cdev);
fail_add:
	printk(KERN_INFO "%s: %s something failed\n", DRV_NAME, __FUNCTION__);
	kfree(lro_char);
	lro_char = NULL;
success:
	return lro_char;
}

static int destroy_sg_char(struct awsmgmt_char *lro_char)
{
	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	BUG_ON(!lro_char);
	BUG_ON(!lro_char->lro);
	BUG_ON(!awsmgmt_class);
	BUG_ON(!lro_char->sys_device);
	if (lro_char->sys_device)
		device_destroy(awsmgmt_class, lro_char->cdev.dev);
	cdev_del(&lro_char->cdev);
	kfree(lro_char);
	return 0;
}

static u64 find_feature_id(const struct awsmgmt_dev *lro)
{
	u64 low = 0;
	u64 high = 0;
	//TODO: Umang :Find out the feature register offset
//	low = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + FEATURE_ID);
//	high = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + FEATURE_ID + 8);
	return low | (high << 32);
}

int enable_ddrs(const struct awsmgmt_dev *lro)
{
	int val,i;
	printk(KERN_INFO "%s: %s \n", DRV_NAME, __FUNCTION__);
	printk("Card %d Resetting MIGs\n", lro->instance);

	if(lro->is1DDR)
	{
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);

		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);

		msleep(50);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);

		iowrite32(0x1, lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);

		msleep(50);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);

		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);

		msleep(50);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);

	} else {
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG A config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG B config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG D config status %d\n", lro->instance, val);

		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_CONFIG_OFFSET);

		msleep(50);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG A config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG B config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG D config status %d\n", lro->instance, val);

		iowrite32(0x1, lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x1, lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x1, lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x1, lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_CONFIG_OFFSET);

		msleep(50);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG A config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG B config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG D config status %d\n", lro->instance, val);

		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		iowrite32(0x0, lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_CONFIG_OFFSET);

		msleep(50);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRA_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG A config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRB_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG B config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRC_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG C config status %d\n", lro->instance, val);
		val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDRD_CALIBRATION_BASE + DDR_CONFIG_OFFSET);
		printk("Card %d MIG D config status %d\n", lro->instance, val);
	}

	for(i = 0; i < 10; ++i) {
		msleep(500);
//		if(are_migs_calibrated(lro))
//			goto end;
	}

//	printk(KERN_ERR "Enable DDR: MIGS are not calibrated. \n");
//	return -ETIMEDOUT;

	if(lro->is1DDR)
		printk(KERN_INFO "Initialized 1 Memory Controller\n");
	else
		printk(KERN_INFO "Initialized 4 Memory Controllers\n");
	return 0;
}


static int init_user_logic(const struct awsmgmt_dev *lro)
{
	u32 value;
	u32 mask = BIT(5);
	u32 isolation_mask = BIT(31);

	value = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + PROTECTION_LOGIC_CONFIG);
	if (!timeout_config) {
		mask = ~mask;
		value &= mask;
	}
	else {
		value |= mask;
	}

	iowrite32(value, lro->bar[AWSMGMT_MAIN_BAR] + PROTECTION_LOGIC_CONFIG);

	value = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + PRISOLATION_BASE);
	isolation_mask = ~isolation_mask;
	value &= isolation_mask;
	iowrite32(value, lro->bar[AWSMGMT_MAIN_BAR] + PRISOLATION_BASE);

	/* Mimic F1 which returns 0xFFFFFFFF in case of timeout */
	iowrite32(TIMEOUT_RESPONSE_DATA, lro->bar[AWSMGMT_MAIN_BAR] + TIMEOUT_RESPONSE_CONFIG);

	/**
	 * Ignore the errors from enable ddr
	 * Scenario: Board has been programmed with a corrupt xclbin which is not
	 * allowing the migs to calibrate. In this situation if we try to insmod
	 * the driver, enable ddr would error out and we wont be able to do anything.
	 * So, we ignore the error here allowing the useer to download a valid
	 * xclbin file later.
	 */
	enable_ddrs(lro);
	return 0;
}

static int init_timeout_configuration(const struct awsmgmt_dev *lro)
{
	int i;
	u32 value0, value1;
	printk(KERN_INFO "Card %d Programming device timeout configuration registers...\n", lro->instance);
	for (i = 0; i < 10; i++) {
		value0 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + timeout_table[i].reg);
		iowrite32(timeout_table[i].val, lro->bar[AWSMGMT_MAIN_BAR] + timeout_table[i].reg);
		value1 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + timeout_table[i].reg);
		printk(KERN_INFO "[%d]\t0x%x:\told[0x%x]\tnew[0x%x]\n", i, timeout_table[i].reg, value0, value1);
	}

	printk(KERN_INFO "Card %d Programming device timeout moderation registers...\n", lro->instance);
	for (i = 0; i < 5; i++) {
		value0 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + timeout_moderation_table[i].reg);
		iowrite32(timeout_moderation_table[i].val, lro->bar[AWSMGMT_MAIN_BAR] + timeout_moderation_table[i].reg);
		value1 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + timeout_moderation_table[i].reg);
		printk(KERN_INFO "[%d]\t0x%x:\told[0x%x]\tnew[0x%x]\n", i, timeout_moderation_table[i].reg, value0, value1);
	}
	return 0;
}

/*
 * 0x444 -- Rate Limiter Enable
 * Bit 0 -- Enable limiting on the XDMA interface
 * Bit 1 -- Enable limiting on the PCIS interface (MMIO)
 *
 * 0x448 – Rate limiter values
 * Bits 07:00 -- XDMA AW (writes)
 * Bits 15:08 -- XDMA AR (reads)
 * Bits 23:16 -- PCIS AW
 * Bits 31:24 -- PCIS AR
 */
static int init_rate_limiter(const struct awsmgmt_dev *lro)
{
	u32 value0;
	u32 value1 = 0x1600 | 0x16;
	printk(KERN_INFO "Card %d Programming device rate limiter registers...\n", lro->instance);

	value0 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_CONFIG);
	iowrite32(value1, lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_CONFIG);
	value1 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_CONFIG);
	printk(KERN_INFO "Config:\told[0x%x]\tnew[0x%x]\n", value0, value1);

	value1 = 0x1;
	value0 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);
	iowrite32(value1, lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);
	value1 = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);
	printk(KERN_INFO "Status:\told[0x%x]\tnew[0x%x]\n", value0, value1);

	return 0;
}


struct pci_dev *find_user_node(const struct pci_dev *pdev)
{
	unsigned int slot = PCI_SLOT(pdev->devfn);
	unsigned int func = PCI_FUNC(pdev->devfn);
	if (func != 2)
		return NULL;
	return pci_get_slot(pdev->bus, PCI_DEVFN(slot, func-2));
}

static int awsmgmt_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	u32 value;
	struct awsmgmt_dev *lro = NULL;

	printk(KERN_INFO "probe(pdev = 0x%p, pci_id = 0x%p)\n", pdev, id);


	rc = pci_enable_device(pdev);
	if (rc) {
		printk(KERN_DEBUG "pci_enable_device() failed, rc = %d.\n", rc);
		return rc;
	}

	/* allocate zeroed device book keeping structure */
	lro = kzalloc(sizeof(struct awsmgmt_dev), GFP_KERNEL);
	if (!lro) {
		printk(KERN_DEBUG "Could not kzalloc(awsmgmt_dev).\n");
		goto err_alloc;
	}
	/* create a device to driver reference */
	dev_set_drvdata(&pdev->dev, lro);
	/* create a driver to device reference */
	lro->pci_dev = pdev;
	printk(KERN_DEBUG "probe() lro = 0x%p\n", lro);
	value = lro->pci_dev->subsystem_device;
	printk(KERN_DEBUG "pci_indevice()\n");

	printk(KERN_DEBUG "pci_request_regions()\n");
	rc = pci_request_regions(pdev, DRV_NAME);
	/* could not request all regions? */
	if (rc) {
		printk(KERN_DEBUG "pci_request_regions() = %d, device in use?\n", rc);
		goto err_regions;
	}

	printk(KERN_DEBUG "map_bars()\n");
	/* map BARs */
	rc = map_bars(lro);
	if (rc)
		goto err_map;

	pf1_addr_reg = lro->bar[AWSMGMT_MAIN_BAR] + PF1_TUNNEL_BASE;

	lro->instance = instance++;
	lro->user_char_dev = create_char(lro, 0);
	if (!lro->user_char_dev) {
		printk(KERN_DEBUG "create_char(user_char_dev) failed\n");
		goto err_cdev;
	}

	value = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + VERSION_BASE);
	printk(KERN_INFO "Initialized AWS Management Function Version %x\n", value);

	lro->stash.magic = 0xBBBBBBBBUL;
	lro->feature_id = find_feature_id(lro);
	lro->user_pci_dev = find_user_node(pdev);
	if (!lro->user_pci_dev) {
		printk(KERN_ERR "Could not find user physical function for instance %d\n", lro->instance);
		goto err_user;
	}

#if 0
	rc = load_boot_firmware(lro);
	if (rc)
		goto err_user;
#endif

	rc = init_user_logic(lro);
	if (rc)
		goto err_user;

	if (timeout_config) {
		rc = init_timeout_configuration(lro);
		if (rc)
			goto err_user;
	}

	if (rate_limiter_config) {
		rc = init_rate_limiter(lro);
		if (rc)
			goto err_user;
	}
	mgmt_init_sysfs(&pdev->dev);

	init_firewall(lro);
	init_health_thread(lro);
	goto end;

err_user:
	destroy_sg_char(lro->user_char_dev);
err_cdev:
	unmap_bars(lro);
err_map:
	pci_release_regions(pdev);
err_regions:
	kfree(lro);
	dev_set_drvdata(&pdev->dev, NULL);
err_alloc:
	pci_disable_device(pdev);
end:
	return rc;
}

static void awsmgmt_remove(struct pci_dev *pdev)
{
	struct awsmgmt_dev *lro;
	printk(KERN_DEBUG "remove(0x%p)\n", pdev);
	if ((pdev == 0) || (dev_get_drvdata(&pdev->dev) == 0)) {
		printk(KERN_DEBUG
		       "remove(dev = 0x%p) pdev->dev.driver_data = 0x%p\n",
		       pdev, dev_get_drvdata(&pdev->dev));
		return;
	}
	lro = (struct awsmgmt_dev *)dev_get_drvdata(&pdev->dev);
	printk(KERN_DEBUG
	       "remove(dev = 0x%p) where pdev->dev.driver_data = 0x%p\n",
	       pdev, lro);
	if (lro->pci_dev != pdev) {
		printk(KERN_DEBUG
		       "pdev->dev.driver_data->pci_dev (0x%08lx) != pdev (0x%08lx)\n",
		       (unsigned long)lro->pci_dev, (unsigned long)pdev);
	}

	fini_health_thread(lro);

	/* remove user character device */
	if (lro->user_char_dev) {
		destroy_sg_char(lro->user_char_dev);
		lro->user_char_dev = 0;
	}

	/* unmap the BARs */
	unmap_bars(lro);
	printk(KERN_DEBUG "Unmapping BARs.\n");
	pci_disable_device(pdev);
	pci_release_regions(pdev);

	kfree(lro);
	mgmt_fini_sysfs(&pdev->dev);
	dev_set_drvdata(&pdev->dev, NULL);
}


static struct pci_driver awsmgmt_driver = {
	.name = DRV_NAME,
	.id_table = pci_ids,
	.probe = awsmgmt_probe,
	.remove = awsmgmt_remove,
	/* resume, suspend are optional */
};

static int __init awsmgmt_init(void)
{
	int res;

	printk(KERN_INFO DRV_NAME " init()\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
	xrt_class = class_create(THIS_MODULE, DRV_NAME);
#else
	awsmgmt_class = class_create(DRV_NAME);
#endif	
	if (IS_ERR(awsmgmt_class))
		return PTR_ERR(awsmgmt_class);
	res = alloc_chrdev_region(&awsmgmt_devnode, AWSMGMT_MINOR_BASE,
				  AWSMGMT_MINOR_COUNT, DRV_NAME);
	if (res)
		goto alloc_err;

	res = pci_register_driver(&awsmgmt_driver);
	if (!res)
		return 0;

	unregister_chrdev_region(awsmgmt_devnode, AWSMGMT_MINOR_COUNT);
alloc_err:
	printk(KERN_INFO DRV_NAME" alloc error\n");
	class_destroy(awsmgmt_class);
	return res;
}

static void awsmgmt_exit(void)
{
	printk(KERN_INFO DRV_NAME" exit()\n");
	/* unregister this driver from the PCI bus driver */
	pci_unregister_driver(&awsmgmt_driver);
	unregister_chrdev_region(awsmgmt_devnode, AWSMGMT_MINOR_COUNT);
	class_destroy(awsmgmt_class);
}

module_init(awsmgmt_init);
module_exit(awsmgmt_exit);
