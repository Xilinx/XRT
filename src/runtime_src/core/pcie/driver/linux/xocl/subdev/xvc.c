/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

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
#define XIL_XVC_MAGIC 0x58564344  // "XVCD"
#define MINOR_NAME_MASK		0xffffffff

enum xvc_algo_type {
	XVC_ALGO_NULL,
	XVC_ALGO_CFG,
	XVC_ALGO_BAR
};

struct xil_xvc_ioc {
	unsigned int opcode;
	unsigned int length;
	unsigned char *tms_buf;
	unsigned char *tdi_buf;
	unsigned char *tdo_buf;
};

struct xil_xvc_properties {
	unsigned int xvc_algo_type;
	unsigned int config_vsec_id;
	unsigned int config_vsec_rev;
	unsigned int bar_index;
	unsigned int bar_offset;
};

#define XDMA_IOCXVC	     _IOWR(XIL_XVC_MAGIC, 1, struct xil_xvc_ioc)
#define XDMA_RDXVC_PROPS _IOR(XIL_XVC_MAGIC, 2, struct xil_xvc_properties)

#define COMPLETION_LOOP_MAX	100

#define XVC_BAR_LENGTH_REG	0x0
#define XVC_BAR_TMS_REG		0x4
#define XVC_BAR_TDI_REG		0x8
#define XVC_BAR_TDO_REG		0xC
#define XVC_BAR_CTRL_REG	0x10

#define XVC_DEV_NAME "xvc" SUBDEV_SUFFIX

struct xocl_xvc {
	void *__iomem base;
};

static struct xil_xvc_properties xvc_pci_props;

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
#define write_register(v, base, off) __write_register(__func__, v, base, off)
#define read_register(base, off) __read_register(__func__, base, off)

#else
#define write_register(v, base, off) iowrite32(v, (base) + (off))
#define read_register(base, off) ioread32((base) + (off))
#endif /* #ifdef __REG_DEBUG__ */


static int xvc_shift_bits(void *base, u32 tms_bits, u32 tdi_bits,
			  u32 *tdo_bits)
{
	u32 control;
	u32 write_reg_data;
	int count;

	/* set tms bit */
	write_register(tms_bits, base, XVC_BAR_TMS_REG);
	/* set tdi bits and shift data out */
	write_register(tdi_bits, base, XVC_BAR_TDI_REG);

	control = read_register(base, XVC_BAR_CTRL_REG);
	/* enable shift operation */
	write_reg_data = control | 0x01;
	write_register(write_reg_data, base, XVC_BAR_CTRL_REG);

	/* poll for completion */
	count = COMPLETION_LOOP_MAX;
	while (count) {
		/* read control reg to check shift operation completion */
		control = read_register(base, XVC_BAR_CTRL_REG);
		if ((control & 0x01) == 0)
			break;

		count--;
	}

	if (!count)	{
		pr_warn("XVC bar transaction timed out (0x%0X)\n", control);
		return -ETIMEDOUT;
	}

	/* read tdo bits back out */
	*tdo_bits = read_register(base, XVC_BAR_TDO_REG);

	return 0;
}

static long xvc_ioctl_helper(struct xocl_xvc *xvc, const void __user *arg)
{
	struct xil_xvc_ioc xvc_obj;
	unsigned int opcode;
	unsigned int total_bits;
	unsigned int total_bytes;
	unsigned int bits, bits_left;
	unsigned char *buffer = NULL;
	unsigned char *tms_buf = NULL;
	unsigned char *tdi_buf = NULL;
	unsigned char *tdo_buf = NULL;
	void __iomem *iobase = xvc->base;
	u32 control_reg_data;
	u32 write_reg_data;
	int rv;

	rv = copy_from_user((void *)&xvc_obj, arg, sizeof(struct xil_xvc_ioc));
	/* anything not copied ? */
	if (rv) {
		pr_info("copy_from_user xvc_obj failed: %d.\n", rv);
		goto cleanup;
	}

	opcode = xvc_obj.opcode;

	/* Invalid operation type, no operation performed */
	if (opcode != 0x01 && opcode != 0x02) {
		pr_info("UNKNOWN opcode 0x%x.\n", opcode);
		return -EINVAL;
	}

	total_bits = xvc_obj.length;
	if (total_bits == 0) {
		pr_err("%s: received invalid obj len %u bits for op 0x%x.\n",
		       __func__, total_bits, opcode);
		return -EINVAL;
	}

	/* Fixing integer overflow scenario */
	if (total_bits >= UINT_MAX - 7)
		total_bits = UINT_MAX - 7;

	total_bytes = (total_bits + 7) >> 3;

	buffer = kmalloc(total_bytes * 3, GFP_KERNEL);
	if (!buffer) {
		pr_info("OOM %u, op 0x%x, len %u bits, %u bytes.\n",
			3 * total_bytes, opcode, total_bits, total_bytes);
		rv = -ENOMEM;
		goto cleanup;
	}
	tms_buf = buffer;
	tdi_buf = tms_buf + total_bytes;
	tdo_buf = tdi_buf + total_bytes;

	rv = copy_from_user((void *)tms_buf, xvc_obj.tms_buf, total_bytes);
	if (rv) {
		pr_info("copy tmfs_buf failed: %d/%u.\n", rv, total_bytes);
		goto cleanup;
	}
	rv = copy_from_user((void *)tdi_buf, xvc_obj.tdi_buf, total_bytes);
	if (rv) {
		pr_info("copy tdi_buf failed: %d/%u.\n", rv, total_bytes);
		goto cleanup;
	}

	// If performing loopback test, set loopback bit (0x02) in control reg
	if (opcode == 0x02) {
		control_reg_data = read_register(iobase, XVC_BAR_CTRL_REG);
		write_reg_data = control_reg_data | 0x02;
		write_register(write_reg_data, iobase, XVC_BAR_CTRL_REG);
	}

	/* set length register to 32 initially if more than one
	 * word-transaction is to be done
	 */
	if (total_bits >= 32)
		write_register(0x20, iobase, XVC_BAR_LENGTH_REG);

	for (bits = 0, bits_left = total_bits; bits < total_bits; bits += 32,
		bits_left -= 32) {
		unsigned int bytes = bits >> 3;
		unsigned int shift_bytes = 4;
		u32 tms_store = 0;
		u32 tdi_store = 0;
		u32 tdo_store = 0;

		if (bits_left < 32) {
			/* set number of bits to shift out */
			write_register(bits_left, iobase, XVC_BAR_LENGTH_REG);
			shift_bytes = (bits_left + 7) >> 3;
		}

		memcpy(&tms_store, tms_buf + bytes, shift_bytes);
		memcpy(&tdi_store, tdi_buf + bytes, shift_bytes);

		/* Shift data out and copy to output buffer */
		rv = xvc_shift_bits(iobase, tms_store, tdi_store, &tdo_store);
		if (rv < 0)
			goto cleanup;

		memcpy(tdo_buf + bytes, &tdo_store, shift_bytes);
	}

	// If performing loopback test, reset loopback bit in control reg
	if (opcode == 0x02) {
		control_reg_data = read_register(iobase, XVC_BAR_CTRL_REG);
		write_reg_data = control_reg_data & ~(0x02);
		write_register(write_reg_data, iobase, XVC_BAR_CTRL_REG);
	}

	rv = copy_to_user((void *)xvc_obj.tdo_buf, tdo_buf, total_bytes);
	if (rv) {
		pr_info("copy back tdo_buf failed: %d/%u.\n", rv, total_bytes);
		rv = -EFAULT;
		goto cleanup;
	}

cleanup:
	kfree(buffer);

	mmiowb();

	return rv;
}

static long xvc_read_properties(struct xocl_xvc *xvc, const void __user *arg)
{
	int status = 0;
	struct xil_xvc_properties xvc_props_obj;

	xvc_props_obj.xvc_algo_type   = (unsigned int) xvc_pci_props.xvc_algo_type;
	xvc_props_obj.config_vsec_id  = xvc_pci_props.config_vsec_id;
	xvc_props_obj.config_vsec_rev = xvc_pci_props.config_vsec_rev;
	xvc_props_obj.bar_index		  = xvc_pci_props.bar_index;
	xvc_props_obj.bar_offset	  = xvc_pci_props.bar_offset;

	if (copy_to_user((void *)arg, &xvc_props_obj, sizeof(xvc_props_obj)))
		status = -ENOMEM;

	mmiowb();
	return status;
}

static long xvc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xocl_xvc *xvc = filp->private_data;
	long status = 0;

	switch (cmd) {
	case XDMA_IOCXVC:
		status = xvc_ioctl_helper(xvc, (void __user *)arg);
		break;
	case XDMA_RDXVC_PROPS:
		status = xvc_read_properties(xvc, (void __user *)arg);
		break;
	default:
		status = -ENOIOCTLCMD;
		break;
	}

	return status;
}

static int char_open(struct inode *inode, struct file *file)
{
	struct xocl_xvc *xvc = NULL;

	xvc = xocl_drvinst_open(inode->i_cdev);
	if (!xvc)
		return -ENXIO;

	/* create a reference to our char device in the opened file */
	file->private_data = xvc;
	return 0;
}

/*
 * Called when the device goes from used to unused.
 */
static int char_close(struct inode *inode, struct file *file)
{
	struct xocl_xvc *xvc = file->private_data;

	xocl_drvinst_close(xvc);
	return 0;
}


/*
 * character device file operations for the XVC
 */
static const struct file_operations xvc_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.unlocked_ioctl = xvc_ioctl,
};

static int xvc_probe(struct platform_device *pdev)
{
	struct xocl_xvc *xvc;
	struct resource *res;
	struct xocl_dev_core *core;
	int bar_idx;
	resource_size_t bar_off;
	int err;

	xvc = xocl_drvinst_alloc(&pdev->dev, sizeof(*xvc));
	if (!xvc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	xvc->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!xvc->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	core = xocl_get_xdev(pdev);

	err = xocl_ioaddr_to_baroff(core, res->start, &bar_idx, &bar_off);
	if (err) {
		xocl_err(&pdev->dev, "failed to get bar info %d", err);
		goto failed;
	}
	// Update PCIe BAR properties in a global structure
	xvc_pci_props.xvc_algo_type   = XVC_ALGO_BAR;
	xvc_pci_props.config_vsec_id  = 0;
	xvc_pci_props.config_vsec_rev = 0;
	xvc_pci_props.bar_index	      = bar_idx;
	xvc_pci_props.bar_offset      = bar_off;

	platform_set_drvdata(pdev, xvc);
	xocl_info(&pdev->dev, "XVC device instance initialized\n");

	return 0;
failed:
	if (xvc->base)
		iounmap(xvc->base);
	xocl_drvinst_release(xvc, NULL);

	return err;
}


static int __xvc_remove(struct platform_device *pdev)
{
	struct xocl_xvc	*xvc;
	void *hdl;

	xvc = platform_get_drvdata(pdev);
	if (!xvc) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	xocl_drvinst_release(xvc, &hdl);
	if (xvc->base)
		iounmap(xvc->base);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void xvc_remove(struct platform_device *pdev)
{
	__xvc_remove(pdev);
}
#else
#define xvc_remove __xvc_remove
#endif

struct xocl_drv_private xvc_pub = {
	.ops = NULL,
	.fops = &xvc_fops,
	.dev = -1,
};

struct xocl_drv_private xvc_pri = {
	.ops = NULL,
	.fops = &xvc_fops,
	.dev = -1,
};

struct platform_device_id xvc_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XVC_PUB), (kernel_ulong_t)&xvc_pub },
	{ XOCL_DEVNAME(XOCL_XVC_PRI), (kernel_ulong_t)&xvc_pri },
	{ },
};

static struct platform_driver	xvc_driver = {
	.probe		= xvc_probe,
	.remove		= xvc_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XVC_DEV_NAME),
	},
	.id_table = xvc_id_table,
};

int __init xocl_init_xvc(void)
{
	int err = 0;

	err = alloc_chrdev_region(&xvc_pub.dev, 0, XOCL_MAX_DEVICES,
			XVC_DEV_NAME);
	if (err < 0)
		goto err_register_chrdev;
	xvc_pri.dev = xvc_pub.dev;

	err = platform_driver_register(&xvc_driver);
	if (err)
		goto err_driver_reg;
	return 0;

err_driver_reg:
	unregister_chrdev_region(xvc_pub.dev, XOCL_MAX_DEVICES);
err_register_chrdev:
	return err;
}

void xocl_fini_xvc(void)
{
	unregister_chrdev_region(xvc_pub.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&xvc_driver);
}
