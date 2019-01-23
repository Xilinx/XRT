/*
 * A GEM style device manager for PCIe nifd_based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ioctl.h>

#include "../xocl_drv.h"

#define NIFD_DEV_NAME "nifd" SUBDEV_SUFFIX
#define SUPPORTED_NIFD_IP_VERSION 1
#define SUPPORTED_DRIVER_VERSION 1
#define	MINOR_NAME_MASK	0xffffffff

enum NIFD_register_offset {
	NIFD_START_APP = 0x0,
	NIFD_STOP_APP = 0x4,
	NIFD_CLEAR = 0x8,
	NIFD_CLEAR_CFG = 0xc,
	NIFD_CLEAR_BREAKPOINT = 0x10,
	NIFD_CLK_MODES = 0x14,
	NIFD_START_READBACK = 0x18,
	NIFD_CLOCK_COUNT = 0x1c,
	NIFD_CONFIG_DATA = 0x20,
	NIFD_BREAKPOINT_CONDITION = 0x24,
	NIFD_STATUS = 0x28,
	NIFD_CLOCK_CNT = 0x2c,
	NIFD_READBACK_DATA = 0x30,
	NIFD_READBACK_DATA_WORD_CNT = 0x34,
	NIFD_CONFIG_DATA_M2 = 0x38,
	NIFD_CLEAR_CFG_M2 = 0x3c
};

enum NIFD_COMMAND_SEQUENCES
{
    NIFD_ACQUIRE_CU = 0,
    NIFD_RELEASE_CU = 1,
    NIFD_QUERY_CU = 2,
    NIFD_READBACK_VARIABLE = 3,
    NIFD_SWITCH_ICAP_TO_NIFD = 4,
    NIFD_SWITCH_ICAP_TO_PR = 5,
    NIFD_ADD_BREAKPOINTS = 6,
    NIFD_REMOVE_BREAKPOINTS = 7,
    NIFD_CHECK_STATUS = 8,
    NIFD_QUERY_XCLBIN = 9,
    NIFD_STOP_CONTROLLED_CLOCK = 10,
    NIFD_START_CONTROLLED_CLOCK = 11,
    NIFD_SWITCH_CLOCK_MODE = 12
};

struct xocl_nifd {
	void *__iomem nifd_base;
	void *__iomem icap_base;
	unsigned int instance;
	struct cdev sys_cdev;
	struct device *sys_device;
};

static dev_t nifd_dev;

struct xocl_nifd *nifd_global;

/**
 * helper functions
 */
static long write_nifd_register(unsigned int value, enum NIFD_register_offset reg_offset);
static long read_nifd_register(enum NIFD_register_offset reg_offset);
static void write_icap_mux_register(unsigned int value);
static long start_controlled_clock_free_running(void);
static long stop_controlled_clock(void);
static void restart_controlled_clock(unsigned int previousMode);
static void start_controlled_clock_stepping(void);

static long write_nifd_register(unsigned int value, enum NIFD_register_offset reg_offset)
{
	unsigned int offset_value = (unsigned int)(reg_offset);
    unsigned long long int full_addr = (unsigned long long int)(nifd_global->nifd_base) + offset_value;
    void *ptr = (void *)(full_addr);

    iowrite32(value, ptr);
	return 0;
}

static long read_nifd_register(enum NIFD_register_offset reg_offset)
{
	unsigned int offset_value = (unsigned int)(reg_offset);
    unsigned long long int full_addr = (unsigned long long int)(nifd_global->nifd_base) + offset_value;
    void *ptr = (void *)(full_addr);

    return ioread32(ptr);
}

static void write_icap_mux_register(unsigned int value) {
    iowrite32(value, nifd_global->icap_base);
}

static long start_controlled_clock_free_running(void) {
	write_nifd_register(0x1, NIFD_STOP_APP);
	return 0;
}

static long stop_controlled_clock(void) {
    write_nifd_register(0x1, NIFD_STOP_APP);
	return 0;
}

static void start_controlled_clock_stepping(void) {
    write_nifd_register(0x0, NIFD_START_APP);
}

static void restart_controlled_clock(unsigned int previousMode) {
    if (previousMode == 0x1)
        start_controlled_clock_free_running();
    else if (previousMode == 0x2)
        start_controlled_clock_stepping();
}

static long start_controlled_clock(void __user *arg) {
    unsigned int mode = 0;
    if (copy_from_user(&mode, arg, sizeof(unsigned int)))
    {
        return -EFAULT;
    }
    restart_controlled_clock(mode);
    if (mode == 1 || mode == 2)
        return 0;
    return -EINVAL; // Improper input
}

static long switch_icap_to_nifd(void)
{
    write_icap_mux_register(0x1);
    return 0;
}

static long switch_icap_to_nifd(void)
{
    write_icap_mux_register(0x1);
    return 0;
}

static long nifd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_nifd *nifd = filp->private_data;
	long status = 0;

	void __user *data = (void __user *)(arg);

	switch (cmd)
	{
		case NIFD_STOP_CONTROLLED_CLOCK:
			status = stop_controlled_clock();
			break;
		case NIFD_START_CONTROLLED_CLOCK:
			status = start_controlled_clock(data);
			break;
		case NIFD_SWITCH_ICAP_TO_NIFD:
			status = switch_icap_to_nifd();
			break;
		case NIFD_SWITCH_ICAP_TO_PR:
			status = switch_icap_to_pr();
			break;
		default:
			status = -ENOIOCTLCMD;
			break;
	}

	return status;
}

static int char_open(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * Called when the device goes from used to unused.
 */
static int char_close(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * character device file operations for the NIFD
 */
static const struct file_operations nifd_fops = {
        .owner = THIS_MODULE,
        .open = char_open,
        .release = char_close,
        .unlocked_ioctl = nifd_ioctl,
};

static int nifd_probe(struct platform_device *pdev)
{
	struct xocl_nifd *nifd;
	struct resource *res;
	struct xocl_dev_core *core;
	int err;

	printk("NIFD: probe => start");

	nifd = devm_kzalloc(&pdev->dev, sizeof(*nifd), GFP_KERNEL);
	if (!nifd)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	nifd->nifd_base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!nifd->nifd_base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}
	nifd_global = nifd;
	nifd->icap_base = nifd->nifd_base + 0x4000;

	core = xocl_get_xdev(pdev);
	if (!core) {
		printk("NIFD: probe => core is null");
	} else {
		printk("NIFD: probe => core is NOT null");
	}

	cdev_init(&nifd->sys_cdev, &nifd_fops);
	nifd->sys_cdev.owner = THIS_MODULE;
	nifd->instance = XOCL_DEV_ID(core->pdev) | platform_get_device_id(pdev)->driver_data;
	nifd->sys_cdev.dev = MKDEV(MAJOR(nifd_dev), core->dev_minor);
	err = cdev_add(&nifd->sys_cdev, nifd->sys_cdev.dev, 1);
	if (err) {
		xocl_err(&pdev->dev, "cdev_add failed, %d",err);
		return err;
	}

	nifd->sys_device = device_create(xrt_class, 
								&pdev->dev,
								nifd->sys_cdev.dev,
								NULL, 
								"%s%d",
								platform_get_device_id(pdev)->name,
								nifd->instance);
	if (IS_ERR(nifd->sys_device)) {
		err = PTR_ERR(nifd->sys_device);
		cdev_del(&nifd->sys_cdev);
		goto failed;
	}

	platform_set_drvdata(pdev, nifd);
	xocl_info(&pdev->dev, "NIFD device instance %d initialized\n",
		nifd->instance);

	printk("NIFD: probe => done");

failed:
	return err;
}


static int nifd_remove(struct platform_device *pdev)
{
	struct xocl_nifd	*nifd;
	struct xocl_dev_core *core;

	core = xocl_get_xdev(pdev);
	if (!core) {
		printk("NIFD: remove => core is null");
	} else {
		printk("NIFD: remove => core is NOT null");
	}

	nifd = platform_get_drvdata(pdev);
	if (!nifd) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	device_destroy(xrt_class, nifd->sys_cdev.dev);
	cdev_del(&nifd->sys_cdev);
	if (nifd->nifd_base)
		iounmap(nifd->nifd_base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, nifd);

	return 0;
}

struct platform_device_id nifd_id_table[] = {
	{ XOCL_NIFD_PRI, 0 },
	{ },
};

static struct platform_driver	nifd_driver = {
	.probe		= nifd_probe,
	.remove		= nifd_remove,
	.driver		= {
		.name = NIFD_DEV_NAME,
	},
	.id_table = nifd_id_table,
};

int __init xocl_init_nifd(void)
{
	int err = 0;
	printk("NIFD: init => start");
	err = alloc_chrdev_region(&nifd_dev, 0, XOCL_MAX_DEVICES, NIFD_DEV_NAME);
	if (err < 0)
		goto err_register_chrdev;

	printk("NIFD: init => platform_driver_register start");
	err = platform_driver_register(&nifd_driver);
	printk("NIFD: init => platform_driver_register return");
	if (err) {
		printk("NIFD: init => platform_driver_register err");
		goto err_driver_reg;
	}
	printk("NIFD: init => done");
	return 0;

err_driver_reg:
	unregister_chrdev_region(nifd_dev, XOCL_MAX_DEVICES);
err_register_chrdev:
	printk("NIFD: init => err");
	return err;
}

void xocl_fini_nifd(void)
{
	unregister_chrdev_region(nifd_dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&nifd_driver);
}
