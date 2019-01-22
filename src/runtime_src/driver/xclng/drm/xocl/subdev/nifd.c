/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
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

/* IOCTL interfaces */
#define XIL_NIFD_MAGIC 0x58564344  // "NIFDD"
#define	MINOR_PUB_HIGH_BIT	0x00000
#define	MINOR_PRI_HIGH_BIT	0x10000
#define MINOR_NAME_MASK		0xffffffff

enum nifd_algo_type {
	NIFD_ALGO_NULL,
	NIFD_ALGO_CFG,
	NIFD_ALGO_BAR
};

struct xil_nifd_ioc {
	unsigned opcode;
	unsigned length;
	unsigned char *tms_buf;
	unsigned char *tdi_buf;
	unsigned char *tdo_buf;
};

struct xil_nifd_properties {
	unsigned int nifd_algo_type;
    unsigned int config_vsec_id;
    unsigned int config_vsec_rev;
    unsigned int bar_index;
    unsigned int bar_offset;
};

#define XDMA_IOCNIFD	     _IOWR(XIL_NIFD_MAGIC, 1, struct xil_nifd_ioc)
#define XDMA_RDNIFD_PROPS _IOR(XIL_NIFD_MAGIC, 2, struct xil_nifd_properties)

#define COMPLETION_LOOP_MAX	100

#define NIFD_BAR_LENGTH_REG	0x0
#define NIFD_BAR_TMS_REG		0x4
#define NIFD_BAR_TDI_REG		0x8
#define NIFD_BAR_TDO_REG		0xC
#define NIFD_BAR_CTRL_REG	0x10

#define NIFD_DEV_NAME "nifd" SUBDEV_SUFFIX

struct xocl_nifd {
	void *__iomem base;
	unsigned int instance;
	struct cdev sys_cdev;
	struct device *sys_device;
};

static dev_t nifd_dev;

static struct xil_nifd_properties nifd_pci_props;

#ifdef __REG_DEBUG__
/* SECTION: Function definitions */
static inline void __write_register(const char *fn, u32 value, void *base,
				unsigned int off)
{
        pr_info("%s: 0x%p, W reg 0x%lx, 0x%x.\n", fn, base, off, value);
        iowrite32(value, base + off);
}

static inline u32 __read_register(const char *fn, void *base, unsigned int off)
{
	u32 v = ioread32(base + off);

        pr_info("%s: 0x%p, R reg 0x%lx, 0x%x.\n", fn, base, off, v);
        return v;
}
#define write_register(v,base,off) __write_register(__func__, v, base, off)
#define read_register(base,off) __read_register(__func__, base, off)

#else
#define write_register(v,base,off)	iowrite32(v, (base) + (off))
#define read_register(base,off)		ioread32((base) + (off))
#endif /* #ifdef __REG_DEBUG__ */


static int nifd_shift_bits(void *base, u32 tms_bits, u32 tdi_bits,
			u32 *tdo_bits)
{
	u32 control;
	u32 write_reg_data;
	int count;

	/* set tms bit */
	write_register(tms_bits, base, NIFD_BAR_TMS_REG);
	/* set tdi bits and shift data out */
	write_register(tdi_bits, base, NIFD_BAR_TDI_REG);

	control = read_register(base, NIFD_BAR_CTRL_REG);
	/* enable shift operation */
	write_reg_data = control | 0x01;
	write_register(write_reg_data, base, NIFD_BAR_CTRL_REG);

	/* poll for completion */
	count = COMPLETION_LOOP_MAX;
	while (count) {
		/* read control reg to check shift operation completion */
		control = read_register(base, NIFD_BAR_CTRL_REG);
		if ((control & 0x01) == 0)
			break;

		count--;
	}

	if (!count)	{
		pr_warn("NIFD bar transaction timed out (0x%0X)\n", control);
		return -ETIMEDOUT;
	}

	/* read tdo bits back out */
	*tdo_bits = read_register(base, NIFD_BAR_TDO_REG);

	return 0;
}

static long nifd_ioctl_helper(struct xocl_nifd *nifd, const void __user *arg)
{
	struct xil_nifd_ioc nifd_obj;
	unsigned int opcode;
	unsigned int total_bits;
	unsigned int total_bytes;
	unsigned int bits, bits_left;
	unsigned char *buffer = NULL;
	unsigned char *tms_buf = NULL;
	unsigned char *tdi_buf = NULL;
	unsigned char *tdo_buf = NULL;
	void __iomem *iobase = nifd->base;
	u32 control_reg_data;
	u32 write_reg_data;
	int rv;

	rv = copy_from_user((void *)&nifd_obj, arg,
				sizeof(struct xil_nifd_ioc));
	/* anything not copied ? */
	if (rv) {
		pr_info("copy_from_user nifd_obj failed: %d.\n", rv);
		goto cleanup;
	}

	opcode = nifd_obj.opcode;

	/* Invalid operation type, no operation performed */
	if (opcode != 0x01 && opcode != 0x02) {
		pr_info("UNKNOWN opcode 0x%x.\n", opcode);
		return -EINVAL;
	}

	total_bits = nifd_obj.length;
	total_bytes = (total_bits + 7) >> 3;

	buffer = (char *)kmalloc(total_bytes * 3, GFP_KERNEL);
	if (!buffer) {
		pr_info("OOM %u, op 0x%x, len %u bits, %u bytes.\n",
			3 * total_bytes, opcode, total_bits, total_bytes);
		rv = -ENOMEM;
		goto cleanup;
	}
	tms_buf = buffer;
	tdi_buf = tms_buf + total_bytes;
	tdo_buf = tdi_buf + total_bytes;

	rv = copy_from_user((void *)tms_buf, nifd_obj.tms_buf, total_bytes);
	if (rv) {
		pr_info("copy tmfs_buf failed: %d/%u.\n", rv, total_bytes);
		goto cleanup;
	}
	rv = copy_from_user((void *)tdi_buf, nifd_obj.tdi_buf, total_bytes);
	if (rv) {
		pr_info("copy tdi_buf failed: %d/%u.\n", rv, total_bytes);
		goto cleanup;
	}

	// If performing loopback test, set loopback bit (0x02) in control reg
	if (opcode == 0x02)
	{
		control_reg_data = read_register(iobase, NIFD_BAR_CTRL_REG);
		write_reg_data = control_reg_data | 0x02;
		write_register(write_reg_data, iobase, NIFD_BAR_CTRL_REG);
	}

	/* set length register to 32 initially if more than one
 	 * word-transaction is to be done */
	if (total_bits >= 32)
		write_register(0x20, iobase, NIFD_BAR_LENGTH_REG);

	for (bits = 0, bits_left = total_bits; bits < total_bits; bits += 32,
		bits_left -= 32) {
		unsigned int bytes = bits >> 3;
		unsigned int shift_bytes = 4;
		u32 tms_store = 0;
		u32 tdi_store = 0;
		u32 tdo_store = 0;

		if (bits_left < 32) {
			/* set number of bits to shift out */
			write_register(bits_left, iobase, NIFD_BAR_LENGTH_REG);
			shift_bytes = (bits_left + 7) >> 3;
		}

		memcpy(&tms_store, tms_buf + bytes, shift_bytes);
		memcpy(&tdi_store, tdi_buf + bytes, shift_bytes);

		/* Shift data out and copy to output buffer */
		rv = nifd_shift_bits(iobase, tms_store, tdi_store, &tdo_store);
		if (rv < 0)
			goto cleanup;

		memcpy(tdo_buf + bytes, &tdo_store, shift_bytes);
	}

	// If performing loopback test, reset loopback bit in control reg
	if (opcode == 0x02)
	{
		control_reg_data = read_register(iobase, NIFD_BAR_CTRL_REG);
		write_reg_data = control_reg_data & ~(0x02);
		write_register(write_reg_data, iobase, NIFD_BAR_CTRL_REG);
	}

	rv = copy_to_user((void *)nifd_obj.tdo_buf, tdo_buf, total_bytes);
	if (rv) {
		pr_info("copy back tdo_buf failed: %d/%u.\n", rv, total_bytes);
		rv = -EFAULT;
		goto cleanup;
	}

cleanup:
	if (buffer)
		kfree(buffer);

	mmiowb();

	return rv;
}

static long nifd_read_properties(struct xocl_nifd *nifd, const void __user *arg)
{
	int status = 0;
	struct xil_nifd_properties nifd_props_obj;

	nifd_props_obj.nifd_algo_type   = (unsigned int) nifd_pci_props.nifd_algo_type;
	nifd_props_obj.config_vsec_id  = nifd_pci_props.config_vsec_id;
	nifd_props_obj.config_vsec_rev = nifd_pci_props.config_vsec_rev;
	nifd_props_obj.bar_index 	  = nifd_pci_props.bar_index;
	nifd_props_obj.bar_offset	  = nifd_pci_props.bar_offset;

	if (copy_to_user((void *)arg, &nifd_props_obj, sizeof(nifd_props_obj))) {
		status = -ENOMEM;
	}

	mmiowb();
	return status;
}

static long nifd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_nifd *nifd = filp->private_data;
	long status = 0;

	switch (cmd)
	{
		case XDMA_IOCNIFD:
			status = nifd_ioctl_helper(nifd, (void __user *)arg);
			break;
		case XDMA_RDNIFD_PROPS:
			status = nifd_read_properties(nifd, (void __user *)arg);
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

	/* pointer to containing structure of the character device inode */
	nifd = container_of(inode->i_cdev, struct xocl_nifd, sys_cdev);
	/* create a reference to our char device in the opened file */
	file->private_data = nifd;
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
	nifd->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!nifd->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

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

	// Update PCIe BAR properties in a global structure
	nifd_pci_props.nifd_algo_type   = NIFD_ALGO_BAR;
	nifd_pci_props.config_vsec_id  = 0;
	nifd_pci_props.config_vsec_rev = 0;
	nifd_pci_props.bar_index       = core->bar_idx;
	nifd_pci_props.bar_offset      = (unsigned int) res->start - (unsigned int) 
									pci_resource_start(core->pdev, core->bar_idx);

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
	if (nifd->base)
		iounmap(nifd->base);

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
