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

#include <linux/vmalloc.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"


#define	REG_STATUS_OFFSET		0x00000000
#define	REG_CTRL_OFFSET			0x00000004
#define	REG_CALIB_OFFSET		0x00000008
#define	REG_XSDB_RAM_BASE		0x00004000

#define	FULL_CALIB_TIMEOUT		100
#define	FAST_CALIB_TIMEOUT		15

#define	CTRL_BIT_SYS_RST		0x00000001
#define	CTRL_BIT_XSDB_SELECT		0x00000010
#define	CTRL_BIT_MEM_INIT_SKIP		0x00000020
#define	CTRL_BIT_RESTORE_EN		0x00000040
#define	CTRL_BIT_RESTORE_COMPLETE	0x00000080
#define	CTRL_BIT_SREF_REQ		0x00000100

#define	STATUS_BIT_CALIB_COMPLETE	0x00000001
#define	STATUS_BIT_SREF_ACK		0x00000100

#define	SRSR_DEV2XDEV(d)	xocl_get_xdev(to_platform_device(d))

struct xocl_ddr_srsr {
	void __iomem		*base;
	struct device		*dev;
	struct mutex		lock;
	uint32_t		*calib_cache;
	uint32_t		cache_size;
	bool			restored;
};

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u32 status = 1;

	return sprintf(buf, "0x%x\n", status);
}
static DEVICE_ATTR_RO(status);

static struct attribute *xocl_ddr_srsr_attributes[] = {
	&dev_attr_status.attr,
	NULL
};

static const struct attribute_group xocl_ddr_srsr_attrgroup = {
	.attrs = xocl_ddr_srsr_attributes,
};

static int srsr_full_calibration(struct platform_device *pdev)
{
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);
	xdev_handle_t xdev = SRSR_DEV2XDEV(xocl_ddr_srsr->dev);
	int i = 0, err = -ETIMEDOUT;
	u32 val;

	xocl_dr_reg_write32(xdev, CTRL_BIT_SYS_RST, xocl_ddr_srsr->base+REG_CTRL_OFFSET);
	xocl_dr_reg_write32(xdev, 0x0, xocl_ddr_srsr->base+REG_CTRL_OFFSET);


	/* Safe to say, full calibration should finish in 2000ms*/
	for (; i < FULL_CALIB_TIMEOUT; ++i) {
		val = xocl_dr_reg_read32(xdev, xocl_ddr_srsr->base+REG_STATUS_OFFSET);
		if (val & STATUS_BIT_CALIB_COMPLETE) {
			err = 0;
			break;
		}
		msleep(20);
	}
	return err;
}

static int srsr_save_calib(struct platform_device *pdev)
{
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);
	xdev_handle_t xdev = SRSR_DEV2XDEV(xocl_ddr_srsr->dev);
	int i = 0, err = -ETIMEDOUT;
	u32 val = 0;
	u32 cache_size_in_words = xocl_ddr_srsr->cache_size/sizeof(uint32_t);

	if(!xocl_ddr_srsr->calib_cache)
		return err;

	mutex_lock(&xocl_ddr_srsr->lock);
	xocl_dr_reg_write32(xdev, CTRL_BIT_SREF_REQ, xocl_ddr_srsr->base+REG_CTRL_OFFSET);
	for ( ; i < 20; ++i) {
		val = xocl_dr_reg_read32(xdev, xocl_ddr_srsr->base+REG_STATUS_OFFSET);
		if (val == (STATUS_BIT_SREF_ACK|STATUS_BIT_CALIB_COMPLETE)) {
			err = 0;
			break;
		}
		msleep(20);
	}

	xocl_dr_reg_write32(xdev, CTRL_BIT_SREF_REQ | CTRL_BIT_XSDB_SELECT, xocl_ddr_srsr->base+REG_CTRL_OFFSET);

	for (i = 0; i < cache_size_in_words; ++i) {
		val = xocl_dr_reg_read32(xdev, xocl_ddr_srsr->base+REG_XSDB_RAM_BASE+i*4);
		*(xocl_ddr_srsr->calib_cache+i) = val;
	}

	mutex_unlock(&xocl_ddr_srsr->lock);
	return err;
}

static int srsr_fast_calib(struct platform_device *pdev, bool retention)
{
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);
	xdev_handle_t xdev = SRSR_DEV2XDEV(xocl_ddr_srsr->dev);
	int i = 0, err = -ETIMEDOUT;
	u32 val, write_val = CTRL_BIT_RESTORE_EN | CTRL_BIT_XSDB_SELECT;
	u32 cache_size_in_words = xocl_ddr_srsr->cache_size/sizeof(uint32_t);

	if(!xocl_ddr_srsr->calib_cache)
		return err;

	if (retention)
		write_val |= CTRL_BIT_MEM_INIT_SKIP;

	xocl_dr_reg_write32(xdev, write_val, xocl_ddr_srsr->base+REG_CTRL_OFFSET);

	msleep(20);
	for (i = 0; i < cache_size_in_words; ++i) {
		val = *(xocl_ddr_srsr->calib_cache+i);
		xocl_dr_reg_write32(xdev, val, xocl_ddr_srsr->base+REG_XSDB_RAM_BASE+i*4);
	}

	write_val = CTRL_BIT_RESTORE_EN | CTRL_BIT_RESTORE_COMPLETE;
	if (retention)
		write_val |= CTRL_BIT_MEM_INIT_SKIP;

	xocl_dr_reg_write32(xdev, write_val, xocl_ddr_srsr->base+REG_CTRL_OFFSET);

	/* Safe to say, fast calibration should finish in 300ms*/
	for ( i = 0; i < FAST_CALIB_TIMEOUT; ++i) {
		val = xocl_dr_reg_read32(xdev, xocl_ddr_srsr->base+REG_STATUS_OFFSET);
		if (val & STATUS_BIT_CALIB_COMPLETE) {
			err = 0;
			break;
		}
		msleep(20);
	}

	xocl_dr_reg_write32(xdev, CTRL_BIT_RESTORE_COMPLETE, xocl_ddr_srsr->base+REG_CTRL_OFFSET);
	val = xocl_dr_reg_read32(xdev, xocl_ddr_srsr->base+REG_CTRL_OFFSET);
	return err;
}

static int srsr_calib(struct platform_device *pdev, bool retention)
{
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);
	xdev_handle_t xdev = SRSR_DEV2XDEV(xocl_ddr_srsr->dev);
	int err = -1;
	uint32_t addr0, addr1;

	mutex_lock(&xocl_ddr_srsr->lock);

	if (xocl_ddr_srsr->restored)
		err = srsr_fast_calib(pdev, retention);

	/* Fast calibration fails then fall back to full calibration
	 * Wipe out calibration cache before full calibration
	 */
	if (err) {
		err = srsr_full_calibration(pdev);
		if (err)
			goto done;

		/* END_ADDR0/1 provides the end address for a given memory configuration
		 * END_ADDR 0 is lower 9 bits, the other one is higher 9 bits
		 * E.g. addr0 = 0x155,     0'b 1 0101 0101
		 *      addr1 = 0x5    0'b 0101
		 *                     0'b 01011 0101 0101
		 *                   =  0xB55
		 * and the total size is 0xB55+1
		 * Check the value, it should not excess predefined XSDB range
		 */
		addr0 = xocl_dr_reg_read32(xdev, xocl_ddr_srsr->base+REG_XSDB_RAM_BASE+4);
		addr1 = xocl_dr_reg_read32(xdev, xocl_ddr_srsr->base+REG_XSDB_RAM_BASE+8);

		xocl_ddr_srsr->cache_size = (((addr1 << 9) | addr0)+1)*sizeof(uint32_t);
		if (xocl_ddr_srsr->cache_size >= 0x4000) {
			err = -ENOMEM;
			goto done;
		}
		vfree(xocl_ddr_srsr->calib_cache);
		xocl_ddr_srsr->calib_cache = vzalloc(xocl_ddr_srsr->cache_size);
		if (!xocl_ddr_srsr->calib_cache) {
			err = -ENOMEM;
			goto done;
		}
	}
done:
	mutex_unlock(&xocl_ddr_srsr->lock);
	return err;
}

static int srsr_read_calib(struct platform_device *pdev, void *calib_cache, uint32_t size)
{
	int ret = 0;
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);

	BUG_ON(!xocl_ddr_srsr->calib_cache);
	BUG_ON(!calib_cache);
	BUG_ON(size != xocl_ddr_srsr->cache_size);

	mutex_lock(&xocl_ddr_srsr->lock);
	memcpy(calib_cache, xocl_ddr_srsr->calib_cache, size);
	mutex_unlock(&xocl_ddr_srsr->lock);
	return ret;
}

static int srsr_write_calib(struct platform_device *pdev, const void *calib_cache, uint32_t size)
{
	int ret = 0, err = 0;
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);

	BUG_ON(!calib_cache);

	mutex_lock(&xocl_ddr_srsr->lock);
	xocl_ddr_srsr->cache_size = size;

	vfree(xocl_ddr_srsr->calib_cache);
	xocl_ddr_srsr->calib_cache = vzalloc(xocl_ddr_srsr->cache_size);
	if (!xocl_ddr_srsr->calib_cache) {
		err = -ENOMEM;
		goto done;
	}

	memcpy(xocl_ddr_srsr->calib_cache, calib_cache, size);

	xocl_ddr_srsr->restored = true;
done:
	mutex_unlock(&xocl_ddr_srsr->lock);
	return ret;
}

static uint32_t srsr_cache_size(struct platform_device *pdev)
{
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);

	return xocl_ddr_srsr->cache_size;
}


static struct xocl_srsr_funcs srsr_ops = {
	.save_calib = srsr_save_calib,
	.calib = srsr_calib,
	.read_calib = srsr_read_calib,
	.write_calib = srsr_write_calib,
	.cache_size = srsr_cache_size,
};

static int xocl_ddr_srsr_probe(struct platform_device *pdev)
{
	struct xocl_ddr_srsr *xocl_ddr_srsr;
	struct resource *res;
	int err = 0;

	xocl_ddr_srsr = devm_kzalloc(&pdev->dev, sizeof(*xocl_ddr_srsr), GFP_KERNEL);
	if (!xocl_ddr_srsr)
		return -ENOMEM;

	xocl_ddr_srsr->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		goto failed;

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	xocl_ddr_srsr->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!xocl_ddr_srsr->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}
	mutex_init(&xocl_ddr_srsr->lock);
	platform_set_drvdata(pdev, xocl_ddr_srsr);

	err = sysfs_create_group(&pdev->dev.kobj, &xocl_ddr_srsr_attrgroup);
	if (err)
		goto create_xocl_ddr_srsr_failed;

	return 0;

create_xocl_ddr_srsr_failed:
	platform_set_drvdata(pdev, NULL);
failed:
	return err;
}


static int __xocl_ddr_srsr_remove(struct platform_device *pdev)
{
	struct xocl_ddr_srsr *xocl_ddr_srsr = platform_get_drvdata(pdev);

	if (!xocl_ddr_srsr) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &xocl_ddr_srsr_attrgroup);

	if (xocl_ddr_srsr->base)
		iounmap(xocl_ddr_srsr->base);

	vfree(xocl_ddr_srsr->calib_cache);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, xocl_ddr_srsr);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void xocl_ddr_srsr_remove(struct platform_device *pdev)
{
	__xocl_ddr_srsr_remove(pdev);
}
#else
#define xocl_ddr_srsr_remove __xocl_ddr_srsr_remove
#endif

struct xocl_drv_private srsr_priv = {
	.ops = &srsr_ops,
};

struct platform_device_id xocl_ddr_srsr_id_table[] = {
	{ XOCL_DEVNAME(XOCL_SRSR), (kernel_ulong_t)&srsr_priv },
	{ },
};

static struct platform_driver	xocl_ddr_srsr_driver = {
	.probe		= xocl_ddr_srsr_probe,
	.remove		= xocl_ddr_srsr_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_SRSR),
	},
	.id_table = xocl_ddr_srsr_id_table,
};

int __init xocl_init_srsr(void)
{
	return platform_driver_register(&xocl_ddr_srsr_driver);
}

void xocl_fini_srsr(void)
{
	platform_driver_unregister(&xocl_ddr_srsr_driver);
}
