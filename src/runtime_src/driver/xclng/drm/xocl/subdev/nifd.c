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

#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

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
#include "xclfeatures.h"

#define NIFD_DEV_NAME "nifd" SUBDEV_SUFFIX
#define SUPPORTED_NIFD_IP_VERSION 1
#define SUPPORTED_DRIVER_VERSION 1
#define MINOR_NAME_MASK 0xffffffff

enum NIFD_register_offset
{
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

struct xocl_nifd
{
    void *__iomem nifd_base;
    void *__iomem icap_base;
    unsigned int instance;
    struct cdev sys_cdev;
    struct device *sys_device;
};

static dev_t nifd_dev;

struct xocl_nifd *nifd_global;

static bool nifd_valid;

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

static void write_icap_mux_register(unsigned int value)
{
    iowrite32(value, nifd_global->icap_base);
}

static long start_controlled_clock_free_running(void)
{
    write_nifd_register(0x1, NIFD_STOP_APP);
    return 0;
}

static long stop_controlled_clock(void)
{
    write_nifd_register(0x1, NIFD_STOP_APP);
    return 0;
}

static void start_controlled_clock_stepping(void)
{
    write_nifd_register(0x0, NIFD_START_APP);
}

static void restart_controlled_clock(unsigned int previousMode)
{
    if (previousMode == 0x1)
        start_controlled_clock_free_running();
    else if (previousMode == 0x2)
        start_controlled_clock_stepping();
}

static long start_controlled_clock(void __user *arg)
{
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

static long switch_icap_to_pr(void)
{
    write_icap_mux_register(0x0);
    return 0;
}

static void clear_configuration_memory(unsigned int bank)
{
    switch (bank)
    {
    case 1:
        write_nifd_register(0x1, NIFD_CLEAR_CFG);
        break;
    case 2:
        write_nifd_register(0x1, NIFD_CLEAR_CFG_M2);
        break;
    default:
        // Clear both memories
        write_nifd_register(0x1, NIFD_CLEAR);
        break;
    }
}

static void perform_readback(unsigned int bank)
{
    unsigned int commandWord;
    if (bank == 1)
    {
        commandWord = 0x0;
    }
    else if (bank == 2)
    {
        commandWord = 0x1;
    }
    else
    {
        return;
    }
    write_nifd_register(commandWord, NIFD_START_READBACK);
}

static unsigned int read_nifd_status(void)
{
    return read_nifd_register(NIFD_STATUS);
}

static void add_readback_data(unsigned int frame, unsigned int offset)
{
    frame &= 0x3fffffff;  // Top two bits of frames must be 00
    offset &= 0x3fffffff; // Set the top two bits to 0 first
    offset |= 0x80000000; // Top two bits of offsets must be 10

    //printk("NIFD: Frame: %x Offset: %x\n", frame, offset);

    write_nifd_register(frame, NIFD_CONFIG_DATA_M2);
    write_nifd_register(offset, NIFD_CONFIG_DATA_M2);
}

static long readback_variable_core(unsigned int *arg)
{
    // This function performs the readback operation.  The argument
    //  input data and the result storage is completely located
    //  in kernel space.

    unsigned int clock_status;
    unsigned int num_bits;
    unsigned int i;
    unsigned int frame;
    unsigned int offset;
    unsigned int next_word = 0;
    unsigned int readback_status = 0;
    unsigned int readback_data_word_cnt = 0;

    // Check the current status of the clock and record if it is running
    clock_status = (read_nifd_status() & 0x3);
    // If the clock was running in free running mode, we have to
    //  put it into stepping mode for a little bit in order to get
    //  this to work.  This is a bug in the hardware that needs to be fixed.
    if (clock_status == 1)
    {
        stop_controlled_clock();
        start_controlled_clock_stepping();
    }
    // Stop the clock no matter what
    stop_controlled_clock();
    // Clear Memory-2
    clear_configuration_memory(2);
    // Fill up Memory-2 with all the frames and offsets passed in.
    //  The data is passed in the format of:
    //  [num_bits][frame][offset][frame][offset]...[space for result]
    num_bits = *arg;
    ++arg;
    for (i = 0; i < num_bits; ++i)
    {
        frame = *arg;
        ++arg;
        offset = *arg;
        ++arg;
        add_readback_data(frame, offset);
    }
    perform_readback(2);
    // I should be reading 32-bit words at a time
    readback_status = 0;
    while (readback_status == 0)
    {
        readback_status = (read_nifd_status() & 0x8);
    }

    // The readback is ready, so we need to figure out how many words to read
    readback_data_word_cnt = read_nifd_register(NIFD_READBACK_DATA_WORD_CNT);

    for (i = 0; i < readback_data_word_cnt; ++i)
    {
        next_word = read_nifd_register(NIFD_READBACK_DATA);
        (*arg) = next_word;
        ++arg;
    }
    restart_controlled_clock(clock_status);
    return 0;
}

static long readback_variable(void __user *arg)
{
    // Allocate memory in kernel space, copy over all the information
    //  from user space at once, call the core implemenation,
    //  and finally write back the result.

    // The information will be passed in this format:
    //  [numBits][frame][offset][frame][offset]...[space for result]
    unsigned int num_bits;
    unsigned int num_words;
    unsigned int total_data_size;
    unsigned int *kernel_memory;
    unsigned int core_result;

    if (copy_from_user(&num_bits, arg, sizeof(unsigned int)))
        return -EFAULT;

    num_words = num_bits % 32 ? num_bits / 32 + 1 : num_bits / 32;

    total_data_size = (1 + (num_bits * 2) + num_words) * sizeof(unsigned int);

    //total_data_size = (num_bits * 3 + 1) * sizeof(unsigned int) ;
    kernel_memory = (unsigned int *)(vmalloc(total_data_size));

    if (!kernel_memory)
        return -ENOMEM;

    if (copy_from_user(kernel_memory, arg, total_data_size))
    {
        vfree(kernel_memory);
        return -EFAULT;
    }

    core_result = readback_variable_core(kernel_memory);

    if (core_result)
    {
        vfree(kernel_memory);
        return core_result;
    }

    if (copy_to_user(arg, kernel_memory, total_data_size))
    {
        vfree(kernel_memory);
        return -EFAULT;
    }

    vfree(kernel_memory);
    return 0; // Success
}

static long switch_clock_mode(void __user *arg)
{
    write_nifd_register(0x04, NIFD_CLK_MODES);
    return 0;
}

static void add_breakpoint_data(unsigned int bank, unsigned int frame,
                                unsigned int offset, unsigned int constraint)
{
    enum NIFD_register_offset register_offset;
    switch (bank)
    {
    case 1:
        register_offset = NIFD_CONFIG_DATA;
        break;
    case 2:
        register_offset = NIFD_CONFIG_DATA_M2;
        break;
    default:
        // Do not assign to either bank
        return;
    }

    frame &= 0x3fffffff;      // Top two bits of frames must be 00
    offset &= 0x3fffffff;     // Set the top two bits to 0 first
    constraint &= 0x3fffffff; // Set the top two bits to 0 first
    offset |= 0x80000000;     // Top two bits of offsets must be 10
    constraint |= 0x40000000; // Top two bits of constraints must be 01

    write_nifd_register(frame, register_offset);

    // Switching to match Mahesh's test
    write_nifd_register(constraint, register_offset);
    write_nifd_register(offset, register_offset);
}

static long add_breakpoints_core(unsigned int *arg)
{
    // Format of user data:
    //  [numBreakpoints][frameAddress][frameOffset][constraint]...[condition]

    unsigned int num_breakpoints;
    unsigned int i;
    unsigned int frame_address;
    unsigned int frame_offset;
    unsigned int constraint;
    unsigned int breakpoint_condition;

    // When adding breakpoints, the clock should be stopped
    unsigned int clock_status = (read_nifd_status() & 0x3);
    if (clock_status != 0x3)
        return -EINVAL;

    // All breakpoints need to be set at the same time
    clear_configuration_memory(1);

    num_breakpoints = (*arg);

    ++arg;

    for (i = 0; i < num_breakpoints; ++i)
    {
        frame_address = (*arg);
        ++arg;
        frame_offset = (*arg);
        ++arg;
        constraint = (*arg);
        ++arg;

        add_breakpoint_data(1, frame_address, frame_offset, constraint);
    }

    breakpoint_condition = (*arg);

    write_nifd_register(breakpoint_condition, NIFD_BREAKPOINT_CONDITION);

    return 0; // Success
}

static long add_breakpoints(void __user *arg)
{
    // Format of user data: [numBreakpoints][frameAddress][frameOffset][constraint]...[condition]
    unsigned int num_breakpoints;
    unsigned int total_data_size;
    unsigned int *kernel_memory;
    long result;

    if (copy_from_user(&num_breakpoints, arg, sizeof(unsigned int)))
        return -EFAULT;

    total_data_size = (num_breakpoints * 3 + 1 + 1) * sizeof(unsigned int);
    kernel_memory = (unsigned int *)(vmalloc(total_data_size));
    if (!kernel_memory)
        return -ENOMEM;

    if (copy_from_user(kernel_memory, arg, total_data_size))
    {
        vfree(kernel_memory);
        return -EFAULT;
    }

    result = add_breakpoints_core(kernel_memory);
    vfree(kernel_memory);

    return result;
}

static long remove_breakpoints(void)
{
    unsigned int clock_status = (read_nifd_status() & 0x3);
    stop_controlled_clock();
    clear_configuration_memory(0);
    write_nifd_register(0x1, NIFD_CLEAR);
    restart_controlled_clock(clock_status);

    return 0;
}

static long check_status(void __user *arg)
{
    unsigned int status = read_nifd_status();
    if (copy_to_user(arg, &status, sizeof(unsigned int)))
    {
        return -EFAULT;
    }
    return 0; // Success
}

static long nifd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    // struct xocl_nifd *nifd = filp->private_data;
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
    case NIFD_READBACK_VARIABLE:
        status = readback_variable(data);
        break;
    case NIFD_SWITCH_CLOCK_MODE:
        status = switch_clock_mode(data);
        break;
    case NIFD_ADD_BREAKPOINTS:
        status = add_breakpoints(data);
        break;
    case NIFD_REMOVE_BREAKPOINTS:
        status = remove_breakpoints();
        break;
    case NIFD_CHECK_STATUS:
        status = check_status(data);
        break;
    default:
        status = -ENOIOCTLCMD;
        break;
    }

    return status;
}

static int char_open(struct inode *inode, struct file *file)
{
    struct xocl_nifd *nifd = NULL;
    nifd = xocl_drvinst_open(inode->i_cdev);
    if (!nifd) 
    {
        return -ENXIO;
    }
    if (!nifd_valid)
    {
        return -1;
    }
    return 0;
}

/*
 * Called when the device goes from used to unused.
 */
static int char_close(struct inode *inode, struct file *file)
{
    struct xocl_nifd *nifd = file->private_data;
    xocl_drvinst_close(nifd);
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
    struct FeatureRomHeader rom;
    int err;

    nifd = devm_kzalloc(&pdev->dev, sizeof(*nifd), GFP_KERNEL);
    if (!nifd)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    nifd->nifd_base = ioremap_nocache(res->start, res->end - res->start + 1);
    if (!nifd->nifd_base)
    {
        err = -EIO;
        xocl_err(&pdev->dev, "Map iomem failed");
        goto failed;
    }
    nifd_global = nifd;
    nifd->icap_base = nifd->nifd_base + 0x4000;

    core = xocl_get_xdev(pdev);
    if (!core)
    {
        xocl_err(&pdev->dev, "core is NULL in NIFD probe");
        goto failed;
    }
    xocl_get_raw_header(core, &rom);
    xocl_info(&pdev->dev, "NIFD: looking from NIFD in FeatureBitMap: %lx\n", (long)rom.FeatureBitMap);
    nifd_valid = (long)rom.FeatureBitMap & 0x40000000;
    if (!nifd_valid) {
        return 0;
    }

    cdev_init(&nifd->sys_cdev, &nifd_fops);
    nifd->sys_cdev.owner = THIS_MODULE;
    nifd->instance = XOCL_DEV_ID(core->pdev) | platform_get_device_id(pdev)->driver_data;
    nifd->sys_cdev.dev = MKDEV(MAJOR(nifd_dev), core->dev_minor);
    err = cdev_add(&nifd->sys_cdev, nifd->sys_cdev.dev, 1);
    if (err)
    {
        xocl_err(&pdev->dev, "cdev_add failed, %d", err);
        return err;
    }

    nifd->sys_device = device_create(xrt_class,
                                     &pdev->dev,
                                     nifd->sys_cdev.dev,
                                     NULL,
                                     "%s%d",
                                     platform_get_device_id(pdev)->name,
                                     nifd->instance);
    if (IS_ERR(nifd->sys_device))
    {
        err = PTR_ERR(nifd->sys_device);
        cdev_del(&nifd->sys_cdev);
        goto failed;
    }

    platform_set_drvdata(pdev, nifd);
    xocl_info(&pdev->dev, "NIFD device instance %d initialized\n",
              nifd->instance);
    return 0;

failed:
    xocl_drvinst_free(nifd);
    return err;
}

static int nifd_remove(struct platform_device *pdev)
{
    struct xocl_nifd *nifd;
    struct xocl_dev_core *core;

    core = xocl_get_xdev(pdev);
    if (!core)
    {
        xocl_err(&pdev->dev, "core is NULL in NIFD remove");
    }

    nifd = platform_get_drvdata(pdev);
    if (!nifd)
    {
        xocl_err(&pdev->dev, "driver data is NULL");
        return -EINVAL;
    }
    device_destroy(xrt_class, nifd->sys_cdev.dev);
    cdev_del(&nifd->sys_cdev);
    if (nifd->nifd_base)
        iounmap(nifd->nifd_base);

    platform_set_drvdata(pdev, NULL);
    // devm_kfree(&pdev->dev, nifd);
    xocl_drvinst_free(nifd);

    return 0;
}

struct platform_device_id nifd_id_table[] = {
    {XOCL_NIFD_PRI, 0},
    {},
};

static struct platform_driver nifd_driver = {
    .probe = nifd_probe,
    .remove = nifd_remove,
    .driver = {
        .name = NIFD_DEV_NAME,
    },
    .id_table = nifd_id_table,
};

int __init xocl_init_nifd(void)
{
    int err = 0;
    err = alloc_chrdev_region(&nifd_dev, 0, XOCL_MAX_DEVICES, NIFD_DEV_NAME);
    if (err < 0)
        goto err_register_chrdev;

    err = platform_driver_register(&nifd_driver);
    if (err)
    {
        goto err_driver_reg;
    }
    return 0;

err_driver_reg:
    unregister_chrdev_region(nifd_dev, XOCL_MAX_DEVICES);
err_register_chrdev:
    return err;
}

void xocl_fini_nifd(void)
{
    unregister_chrdev_region(nifd_dev, XOCL_MAX_DEVICES);
    platform_driver_unregister(&nifd_driver);
}
