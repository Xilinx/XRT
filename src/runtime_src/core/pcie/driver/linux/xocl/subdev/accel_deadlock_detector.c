/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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

struct xocl_accel_deadlock_detector {
    void __iomem         *base;
    struct device        *dev;
    uint64_t             start_paddr;
    uint64_t             range;
    struct mutex         lock;
    struct debug_ip_data data;
};

static ssize_t name_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = platform_get_drvdata(to_platform_device(dev));

    return sprintf(buf, "accel_deadlock_%llu\n",accel_deadlock_detector->data.m_base_address);
}

static DEVICE_ATTR_RO(name);

static ssize_t status_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    uint64_t result = 0;
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = platform_get_drvdata(to_platform_device(dev));

    mutex_lock(&accel_deadlock_detector->lock);
    result = XOCL_READ_REG32(accel_deadlock_detector->base + 0x0);
    mutex_unlock(&accel_deadlock_detector->lock);
    return sprintf(buf, "%llu\n",result);
}

static DEVICE_ATTR_RO(status);

static struct attribute *accel_deadlock_detector_attrs[] = {
    &dev_attr_name.attr,
    &dev_attr_status.attr,
    NULL,
};

static struct attribute_group accel_deadlock_detector_attr_group = {
    .attrs = accel_deadlock_detector_attrs,
};

static int __accel_deadlock_detector_remove(struct platform_device *pdev)
{
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = NULL;
    void *hdl = NULL;

    accel_deadlock_detector = platform_get_drvdata(pdev);
    if (!accel_deadlock_detector) {
        xocl_err(&pdev->dev, "driver data is NULL");
        return -EINVAL;
    }

    sysfs_remove_group(&pdev->dev.kobj, &accel_deadlock_detector_attr_group);

    xocl_drvinst_release(accel_deadlock_detector, &hdl);

    if (accel_deadlock_detector->base) {
        iounmap(accel_deadlock_detector->base);
    }

    platform_set_drvdata(pdev, NULL);

    xocl_drvinst_free(hdl);

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void accel_deadlock_detector_remove(struct platform_device *pdev)
{
    __accel_deadlock_detector_remove(pdev);
}
#else
#define accel_deadlock_detector_remove __accel_deadlock_detector_remove
#endif

static int accel_deadlock_detector_probe(struct platform_device *pdev)
{
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = NULL;
    struct resource *res = NULL;
    void *priv = NULL;
    int err = 0;

    accel_deadlock_detector = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_accel_deadlock_detector));
    if (!accel_deadlock_detector) {
        err = -ENOMEM;
        xocl_err(&pdev->dev, "xocl_drvinst_alloc failed for accel_deadlock_detector_probe , err (-ENOMEM): %d", err);
        return -ENOMEM;
    }

    accel_deadlock_detector->dev = &pdev->dev;

    priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
    if (priv) {
        memcpy(&accel_deadlock_detector->data, priv, sizeof(struct debug_ip_data));
    }

    platform_set_drvdata(pdev, accel_deadlock_detector);
    mutex_init(&accel_deadlock_detector->lock);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        err = -ENOMEM;
        xocl_err(&pdev->dev, "platform_get_resource failed for accel_deadlock_detector_probe , err (-ENOMEM): %d", err);
        goto done;
    }


    xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
                res->start, res->end);

    accel_deadlock_detector->base = ioremap_nocache(res->start, res->end - res->start + 1);
    if (!accel_deadlock_detector->base) {
        err = -EIO;
        xocl_err(&pdev->dev, "Map iomem failed");
        goto done;
    }

    accel_deadlock_detector->start_paddr = res->start;
    accel_deadlock_detector->range = res->end - res->start + 1;

    err = sysfs_create_group(&pdev->dev.kobj, &accel_deadlock_detector_attr_group);
    if (err) {
        xocl_err(&pdev->dev, "create accel_deadlock_detector sysfs attrs failed: %d", err);
    }

done:
    if (err) {
        accel_deadlock_detector_remove(pdev);
        return err;
    }
    return 0;
}

static int accel_deadlock_detector_open(struct inode *inode, struct file *file)
{
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = NULL;

    accel_deadlock_detector = xocl_drvinst_open_single(inode->i_cdev);
    if (!accel_deadlock_detector) {
        return -ENXIO;
    }
    file->private_data = accel_deadlock_detector;
    return 0;
}

static int accel_deadlock_detector_close(struct inode *inode, struct file *file)
{
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = file->private_data;

    xocl_drvinst_close(accel_deadlock_detector);
    return 0;
}

static long accel_deadlock_detector_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = NULL;
    void __user *data = NULL;
    long result = 0;
    uint32_t reg = 0;

    accel_deadlock_detector = (struct xocl_accel_deadlock_detector *)filp->private_data;
    data = (void __user *)(arg);

    mutex_lock(&accel_deadlock_detector->lock);
    switch (cmd) {
        case ACCEL_DEADLOCK_DETECTOR_IOC_RESET:
            break;
        case ACCEL_DEADLOCK_DETECTOR_IOC_GET_STATUS:
            reg = XOCL_READ_REG32(accel_deadlock_detector->base + 0x0);
            if (copy_to_user(data, &reg, sizeof(uint32_t))) {
                result = -EFAULT;
            }
            break;
        default:
            result = -ENOTTY;
    }
    mutex_unlock(&accel_deadlock_detector->lock);

    return result;
}


static int accel_deadlock_detector_mmap(struct file *filp, struct vm_area_struct *vma)
{

    int rc = 0;
    unsigned long off   = 0;
    unsigned long phys  = 0;
    unsigned long vsize = 0;
    unsigned long psize = 0;
    struct xocl_accel_deadlock_detector *accel_deadlock_detector = (struct xocl_accel_deadlock_detector *)filp->private_data;
    BUG_ON(!accel_deadlock_detector);

    off = vma->vm_pgoff << PAGE_SHIFT;
    if (off >= accel_deadlock_detector->range) {
        return -EINVAL;
    }

    // BAR physical address
    phys = accel_deadlock_detector->start_paddr + off;
    vsize = vma->vm_end - vma->vm_start;
    // complete resource
    psize = accel_deadlock_detector->range - off;


    if (vsize > psize) {
        return -EINVAL;
    }

    // pages must not be cached as this would result in cache line sized accesses to the end point
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    // prevent touching the pages (byte access) for swap-in, and prevent the pages from being swapped out
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

    // make MMIO accessible to user space
    rc = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
                            vsize, vma->vm_page_prot);
    if (rc) {
        return -EAGAIN;
    }
    return rc;
}


static const struct file_operations accel_deadlock_detector_fops = {
    .open = accel_deadlock_detector_open,
    .release = accel_deadlock_detector_close,
    .mmap = accel_deadlock_detector_mmap,
    .unlocked_ioctl = accel_deadlock_detector_ioctl,
};

struct xocl_drv_private accel_deadlock_detector_priv = {
    .fops = &accel_deadlock_detector_fops,
    .dev = -1,
};

struct platform_device_id accel_deadlock_detector_id_table[] = {
    { XOCL_DEVNAME(XOCL_ACCEL_DEADLOCK_DETECTOR), (kernel_ulong_t)&accel_deadlock_detector_priv },
    { },
};

static struct platform_driver  accel_deadlock_detector_driver = {
    .probe    = accel_deadlock_detector_probe,
    .remove    = accel_deadlock_detector_remove,
    .driver    = {
            .name = XOCL_DEVNAME(XOCL_ACCEL_DEADLOCK_DETECTOR),
        },
    .id_table = accel_deadlock_detector_id_table,
};

int __init xocl_init_accel_deadlock_detector(void)
{
    int err = 0;

    err = alloc_chrdev_region(&accel_deadlock_detector_priv.dev, 0, XOCL_MAX_DEVICES,
                                XOCL_ACCEL_DEADLOCK_DETECTOR);
    if (err < 0) {
        goto err_chrdev_reg;
    }

    err = platform_driver_register(&accel_deadlock_detector_driver);
    if (err < 0) {
        goto err_driver_reg;
    }

    return 0;

err_driver_reg:
    unregister_chrdev_region(accel_deadlock_detector_priv.dev, 1);
err_chrdev_reg:
    return err;
}

void xocl_fini_accel_deadlock_detector(void)
{
    unregister_chrdev_region(accel_deadlock_detector_priv.dev, XOCL_MAX_DEVICES);
    platform_driver_unregister(&accel_deadlock_detector_driver);
}
