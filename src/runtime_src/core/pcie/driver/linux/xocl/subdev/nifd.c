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
};

/**
 * helper functions
 */
static long write_nifd_register(struct xocl_nifd* nifd, 
                                unsigned int value, 
                                enum NIFD_register_offset reg_offset);
static long read_nifd_register(struct xocl_nifd* nifd, 
                                enum NIFD_register_offset reg_offset);
static void write_icap_mux_register(struct xocl_nifd* nifd,
                                unsigned int value);
static long start_controlled_clock_free_running(struct xocl_nifd* nifd);
static long stop_controlled_clock(struct xocl_nifd* nifd);
static void restart_controlled_clock(struct xocl_nifd* nifd, 
                                    unsigned int previousMode);
static void start_controlled_clock_stepping(struct xocl_nifd* nifd);

static long write_nifd_register(struct xocl_nifd* nifd, 
                                unsigned int value, 
                                enum NIFD_register_offset reg_offset)
{
    unsigned int offset_value = (unsigned int)(reg_offset);
    unsigned long long int full_addr = 
    (unsigned long long int)(nifd->nifd_base) + offset_value;
    void *ptr = (void *)(full_addr);

    iowrite32(value, ptr);
    return 0;
}

static long read_nifd_register(struct xocl_nifd* nifd, 
                            enum NIFD_register_offset reg_offset)
{
    unsigned int offset_value = (unsigned int)(reg_offset);
    unsigned long long int full_addr = 
                        (unsigned long long int)(nifd->nifd_base) 
                        + offset_value;
    void *ptr = (void *)(full_addr);

    return ioread32(ptr);
}

static void write_icap_mux_register(struct xocl_nifd* nifd, 
                                    unsigned int value)
{
    iowrite32(value, nifd->icap_base);
}

static long start_controlled_clock_free_running(struct xocl_nifd* nifd)
{
    write_nifd_register(nifd, 0x3, NIFD_START_APP) ;
    return 0;
}

static long stop_controlled_clock(struct xocl_nifd* nifd)
{
    write_nifd_register(nifd, 0x1, NIFD_STOP_APP);
    return 0;
}

static void start_controlled_clock_stepping(struct xocl_nifd* nifd)
{
    write_nifd_register(nifd, 0x0, NIFD_START_APP);
}

static void restart_controlled_clock(struct xocl_nifd* nifd,
                                    unsigned int previousMode)
{
    if (previousMode == 0x1)
        start_controlled_clock_free_running(nifd);
    else if (previousMode == 0x2)
        start_controlled_clock_stepping(nifd);
}

static long start_controlled_clock(struct xocl_nifd* nifd, void __user *arg)
{
    unsigned int mode = 0;
    if (copy_from_user(&mode, arg, sizeof(unsigned int)))
    {
        return -EFAULT;
    }
    restart_controlled_clock(nifd, mode);
    if (mode == 1 || mode == 2)
        return 0;
    return -EINVAL; // Improper input
}

static long switch_icap_to_nifd(struct xocl_nifd* nifd)
{
    write_icap_mux_register(nifd, 0x1);
    return 0;
}

static long switch_icap_to_pr(struct xocl_nifd* nifd)
{
    write_icap_mux_register(nifd, 0x0);
    return 0;
}

static void clear_configuration_memory(struct xocl_nifd* nifd, unsigned int bank)
{
    switch (bank)
    {
    case 1:
        write_nifd_register(nifd, 0x1, NIFD_CLEAR_CFG);
        break;
    case 2:
        write_nifd_register(nifd, 0x1, NIFD_CLEAR_CFG_M2);
        break;
    default:
        // Clear both memories
        write_nifd_register(nifd, 0x1, NIFD_CLEAR);
        break;
    }
}

static void perform_readback(struct xocl_nifd* nifd, unsigned int bank)
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
    write_nifd_register(nifd, commandWord, NIFD_START_READBACK);
}

static unsigned int read_nifd_status(struct xocl_nifd* nifd)
{
    return read_nifd_register(nifd, NIFD_STATUS);
}

static void add_readback_data(struct xocl_nifd* nifd, unsigned int frame, unsigned int offset)
{
    frame &= 0x3fffffff;  // Top two bits of frames must be 00
    offset &= 0x3fffffff; // Set the top two bits to 0 first
    offset |= 0x80000000; // Top two bits of offsets must be 10

    //printk("NIFD: Frame: %x Offset: %x\n", frame, offset);

    write_nifd_register(nifd, frame, NIFD_CONFIG_DATA_M2);
    write_nifd_register(nifd, offset, NIFD_CONFIG_DATA_M2);
}

static long readback_variable_core(struct xocl_nifd* nifd, unsigned int num_bits, unsigned int *arg)
{
    // This function performs the readback operation.  The argument
    //  input data and the result storage is completely located
    //  in kernel space.

    unsigned int clock_status;
    unsigned int i;
    unsigned int frame;
    unsigned int offset;
    unsigned int next_word = 0;
    unsigned int readback_status = 0;
    unsigned int readback_data_word_cnt = 0;
    unsigned int timeout_limit = 100;
    unsigned int timeout_counter = 0;

    // Check the current status of the clock and record if it is running
    clock_status = (read_nifd_status(nifd) & 0x3);
    // If the clock was running in free running mode, we have to
    // put it into stepping mode for a little bit in order to get
    // this to work.  This is a bug in the hardware that needs to 
    // be fixed.
    if (clock_status == 1) {
        stop_controlled_clock(nifd);
        start_controlled_clock_stepping(nifd);
    }
    // Stop the clock no matter what
    stop_controlled_clock(nifd);
    // Clear Memory-2
    clear_configuration_memory(nifd, 2);
    // Fill up Memory-2 with all the frames and offsets passed in.
    //  The data is passed in the format of:
    //  [frame][offset][frame][offset]...[space for result]

    for (i = 0; i < num_bits; ++i) {
        frame = *arg;
        ++arg;
        offset = *arg;
        ++arg;
        add_readback_data(nifd, frame, offset);
    }
    perform_readback(nifd, 2);
    // I should be reading 32-bit words at a time
    readback_status = 0;
    timeout_limit = timeout_limit * num_bits;
    while (readback_status == 0 && timeout_counter < timeout_limit) {
        msleep(100);
        readback_status = (read_nifd_status(nifd) & 0x8);
        ++timeout_counter;
    }

    if (timeout_counter == timeout_limit)
        return -1;

    // The readback is ready, so we need to figure out how many 
    // words to read
    readback_data_word_cnt =
      read_nifd_register(nifd, NIFD_READBACK_DATA_WORD_CNT);

    for (i = 0; i < readback_data_word_cnt; ++i) {
        next_word = read_nifd_register(nifd, NIFD_READBACK_DATA);
        (*arg) = next_word;
        ++arg;
    }
    restart_controlled_clock(nifd, clock_status);
    return 0;
}

static long readback_variable(struct xocl_nifd* nifd, void __user *arg)
{
    // Allocate memory in kernel space, copy over all the information
    // from user space at once, call the core implementaion,
    // and finally write back the result.

    // The information will be passed in this format:
    //  [numBits][frame][offset][frame][offset]...[space for result]
    unsigned int num_bits;
    void __user *data_payload;
    unsigned int result_space_size;
    unsigned int total_data_payload_size;
    unsigned int *kernel_memory;
    unsigned int core_result;

    // Copy the first unsigned int, which will determine the size of
    // the rest of the data to copy.
    if (copy_from_user(&num_bits, arg, sizeof(unsigned int)))
        return -EFAULT;

    // Validate num_bits to prevent integer overflow
    if (num_bits > UINT_MAX / 2 - 1)
	return -EINVAL;

    // We pack the results into the space for the result.  Each
    // frame + offset pair will read a single bit that gets packed.
    result_space_size = num_bits % 32 ? num_bits / 32 + 1 : num_bits / 32;

    // The total amount of the payload buffer that should have been passed
    // in by the user will have two unsigned ints per bit and the space
    // to store the result
    total_data_payload_size =
      ((num_bits * 2) + result_space_size) * sizeof(unsigned int);

    kernel_memory = (unsigned int *)(vmalloc(total_data_payload_size));

    if (!kernel_memory)
        return -ENOMEM;

    // We've already seen the num_bits at the beginning of the user data,
    // and used it to determine the amount of kernel memory to allocate,
    // so don't read it again.  Instead, only read the data payload portion
    data_payload = (void*)((unsigned int*)(arg) + 1);

    if (copy_from_user(kernel_memory, data_payload, total_data_payload_size)) {
        vfree(kernel_memory);
        return -EFAULT;
    }

    core_result = readback_variable_core(nifd, num_bits, kernel_memory);

    if (core_result) {
        vfree(kernel_memory);
        return core_result;
    }

    // We don't copy back the num_bits, only the payload portion which
    // contains the read data
    if (copy_to_user(data_payload, kernel_memory, total_data_payload_size)) {
        vfree(kernel_memory);
        return -EFAULT;
    }

    vfree(kernel_memory);
    return 0; // Success
}

static long switch_clock_mode(struct xocl_nifd* nifd)
{
    write_nifd_register(nifd, 0x04, NIFD_CLK_MODES);
    return 0;
}

static void add_breakpoint_data(struct xocl_nifd* nifd, 
                                unsigned int bank, 
                                unsigned int frame,
                                unsigned int offset, 
                                unsigned int constraint)
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

    write_nifd_register(nifd, frame, register_offset);

    // Switching to match Mahesh's test
    write_nifd_register(nifd, constraint, register_offset);
    write_nifd_register(nifd, offset, register_offset);
}

static long add_breakpoints_core(struct xocl_nifd* nifd, unsigned int num_breakpoints, unsigned int *arg)
{
    // Format of user data:
    // [frameAddress]
    // [frameOffset]
    // [constraint]...[condition]

    unsigned int i;
    unsigned int frame_address;
    unsigned int frame_offset;
    unsigned int constraint;
    unsigned int breakpoint_condition;

    // When adding breakpoints, the clock should be stopped
    unsigned int clock_status = (read_nifd_status(nifd) & 0x3);
    if (clock_status != 0x3)
        return -EINVAL;

    // All breakpoints need to be set at the same time
    clear_configuration_memory(nifd, 1);

    for (i = 0; i < num_breakpoints; ++i) {
        frame_address = (*arg);
        ++arg;
        frame_offset = (*arg);
        ++arg;
        constraint = (*arg);
        ++arg;

        add_breakpoint_data(nifd, 1, frame_address, frame_offset, constraint);
    }

    breakpoint_condition = (*arg);

    write_nifd_register(nifd, breakpoint_condition, NIFD_BREAKPOINT_CONDITION);

    return 0; // Success
}

static long add_breakpoints(struct xocl_nifd* nifd, void __user *arg)
{
    // Format of user data: 
    // [numBreakpoints][frameAddress]
    // [frameOffset][constraint]...[condition]
    unsigned int num_breakpoints;
    void __user *data_payload;
    unsigned int total_data_payload_size;
    //    unsigned int total_data_size;
    unsigned int *kernel_memory;
    long result;

    // First, copy the number of breakpoints from the user.  This will
    // determine the amount of kernel memory we allocate.
    if (copy_from_user(&num_breakpoints, arg, sizeof(unsigned int)))
        return -EFAULT;

    // Every breakpoint will have an address, offset, and constraint.
    // So the total size of the payload will be 3 times the number
    // of breakpoints plus an additional unsigned int to store the overall
    // condition

    // Validate num_breakpoints to prevent integer overflow
    if (num_breakpoints > UINT_MAX / (3 * sizeof(unsigned int)) - 1)
        return -EINVAL;

    total_data_payload_size = ((num_breakpoints*3) + 1) * sizeof(unsigned int);

    kernel_memory = (unsigned int *)(vmalloc(total_data_payload_size));
    if (!kernel_memory)
        return -ENOMEM;

    // We've already copied the num_breakpoints and allocated memory based
    // on that number, so don't reread it.  Instead only copy over the
    // payload.
    data_payload = (void*)((unsigned int*)(arg) + 1);

    if (copy_from_user(kernel_memory, data_payload, total_data_payload_size)) {
        vfree(kernel_memory);
        return -EFAULT;
    }

    result = add_breakpoints_core(nifd, num_breakpoints, kernel_memory);
    vfree(kernel_memory);

    return result;
}

static long remove_breakpoints(struct xocl_nifd* nifd)
{
    unsigned int clock_status = (read_nifd_status(nifd) & 0x3);
    stop_controlled_clock(nifd);
    clear_configuration_memory(nifd, 0);
    write_nifd_register(nifd, 0x1, NIFD_CLEAR);
    restart_controlled_clock(nifd, clock_status);

    return 0;
}

static long check_status(struct xocl_nifd* nifd, void __user *arg)
{
    unsigned int status = read_nifd_status(nifd);
    if (copy_to_user(arg, &status, sizeof(unsigned int)))
    {
        return -EFAULT;
    }
    return 0; // Success
}

static long nifd_ioctl(struct file *filp, unsigned int cmd, 
                        unsigned long arg)
{
    struct xocl_nifd *nifd = filp->private_data;
    long status = 0;

    void __user *data = (void __user *)(arg);

    switch (cmd)
    {
    case NIFD_STOP_CONTROLLED_CLOCK:
        status = stop_controlled_clock(nifd);
        break;
    case NIFD_START_CONTROLLED_CLOCK:
        status = start_controlled_clock(nifd, data);
        break;
    case NIFD_SWITCH_ICAP_TO_NIFD:
        status = switch_icap_to_nifd(nifd);
        break;
    case NIFD_SWITCH_ICAP_TO_PR:
        status = switch_icap_to_pr(nifd);
        break;
    case NIFD_READBACK_VARIABLE:
        status = readback_variable(nifd, data);
        break;
    case NIFD_SWITCH_CLOCK_MODE:
        status = switch_clock_mode(nifd);
        break;
    case NIFD_ADD_BREAKPOINTS:
        status = add_breakpoints(nifd, data);
        break;
    case NIFD_REMOVE_BREAKPOINTS:
        status = remove_breakpoints(nifd);
        break;
    case NIFD_CHECK_STATUS:
        status = check_status(nifd, data);
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
    file->private_data = nifd;
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
    struct FeatureRomHeader rom = { {0} };
    bool nifd_valid = false;
    int err = 0;

    nifd = xocl_drvinst_alloc(&pdev->dev, sizeof(*nifd));
    if (!nifd)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    nifd->nifd_base = ioremap_nocache(res->start, 
                        res->end - res->start + 1);
    if (!nifd->nifd_base)
    {
        err = -EIO;
        xocl_err(&pdev->dev, "Map iomem failed");
        goto failed;
    }
    nifd->icap_base = nifd->nifd_base + 0x4000;

    core = xocl_get_xdev(pdev);
    if (!core)
    {
        xocl_err(&pdev->dev, "core is NULL in NIFD probe");
        goto failed;
    }
    xocl_get_raw_header(core, &rom);
    xocl_info(&pdev->dev, 
            "NIFD: looking from NIFD in FeatureBitMap: %lx\n", 
            (long)rom.FeatureBitMap);
    nifd_valid = (long)rom.FeatureBitMap & 0x40000000;
    if (!nifd_valid) {
        err = -EINVAL;
        goto failed;
    }

    platform_set_drvdata(pdev, nifd);
    xocl_info(&pdev->dev, "NIFD device instance %d initialized\n",
              nifd->instance);
    return 0;

failed:
    xocl_drvinst_release(nifd, NULL);
    return err;
}

static int __nifd_remove(struct platform_device *pdev)
{
    struct xocl_nifd *nifd;
    struct xocl_dev_core *core;
    void *hdl;

    core = xocl_get_xdev(pdev);
    if (!core)
        xocl_info(&pdev->dev, "core is NULL in NIFD remove");

    nifd = platform_get_drvdata(pdev);
    if (!nifd) {
        xocl_err(&pdev->dev, "driver data is NULL");
        return -EINVAL;
    }
    xocl_drvinst_release(nifd, &hdl);

    if (nifd->nifd_base)
        iounmap(nifd->nifd_base);
    platform_set_drvdata(pdev, NULL);
    xocl_drvinst_free(hdl);

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void nifd_remove(struct platform_device *pdev)
{
    __nifd_remove(pdev);
}
#else
#define nifd_remove __nifd_remove
#endif

struct xocl_drv_private nifd_priv = {
	.ops = NULL,
	.fops = &nifd_fops,
	.dev = -1,
};

struct platform_device_id nifd_id_table[] = {
    {XOCL_DEVNAME(XOCL_NIFD_PRI), (kernel_ulong_t)&nifd_priv},
    {},
};

static struct platform_driver nifd_driver = {
    .probe = nifd_probe,
    .remove = nifd_remove,
    .driver = {
        .name = XOCL_DEVNAME(NIFD_DEV_NAME),
    },
    .id_table = nifd_id_table,
};

int __init xocl_init_nifd(void)
{
    int err = 0;
    err = alloc_chrdev_region(&nifd_priv.dev, 
                            0, 
                            XOCL_MAX_DEVICES, 
                            NIFD_DEV_NAME);
    if (err < 0)
        goto err_register_chrdev;

    err = platform_driver_register(&nifd_driver);
    if (err)
    {
        goto err_driver_reg;
    }
    return 0;

err_driver_reg:
    unregister_chrdev_region(nifd_priv.dev, XOCL_MAX_DEVICES);
err_register_chrdev:
    return err;
}

void xocl_fini_nifd(void)
{
    unregister_chrdev_region(nifd_priv.dev, XOCL_MAX_DEVICES);
    platform_driver_unregister(&nifd_driver);
}
