/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Rajkumar Rampelli <rajkumar@xilinx.com>
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
#include <linux/iommu.h>

#define ICAP_PROGRAMMING_REG		0x0
#define ICAP_PROGRAMMING_WBSTAR_LOC	4
#define ICAP_PROGRAMMING_REG_ENABLE	0x1

#define	READ_REG32(icap_cntrl, off)			\
	(icap_cntrl->base_addr ?		\
	XOCL_READ_REG32(icap_cntrl->base_addr + off) : 0)
#define	WRITE_REG32(icap_cntrl, val, off)		\
	(icap_cntrl->base_addr ?		\
	XOCL_WRITE_REG32(val, icap_cntrl->base_addr + off) : ((void)0))

struct icap_cntrl {
	struct platform_device *pdev;
	void __iomem *base_addr;
	struct xocl_icap_cntrl_privdata *priv_data;
	struct mutex icap_cntrl_lock;
	bool sysfs_created;
	bool support_enabled;
};

static ssize_t load_flash_addr_store(struct device *dev,
                                     struct device_attribute *da,
                                     const char *buf, size_t count)
{
	struct icap_cntrl *ic =	platform_get_drvdata(to_platform_device(dev));
	long addr;

	if (!ic->support_enabled) {
		xocl_dbg(dev, "Icap controller programming is not supported\n");
		return -EINVAL;
	}

	if (kstrtol(buf, 10, &addr)) {
		xocl_err(dev, "invalid input");
		return -EINVAL;
	}

	mutex_lock(&ic->icap_cntrl_lock);
	addr = addr << ICAP_PROGRAMMING_WBSTAR_LOC;
	WRITE_REG32(ic, addr, ICAP_PROGRAMMING_REG);
	mutex_unlock(&ic->icap_cntrl_lock);

	return count;
}

static ssize_t load_flash_addr_show(struct device *dev,
                                    struct device_attribute *da, char *buf)
{
	struct icap_cntrl *ic = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	if (!ic->support_enabled) {
		xocl_dbg(dev, "Icap controller programming is not supported\n");
		return -EINVAL;
	}

	val = READ_REG32(ic, ICAP_PROGRAMMING_REG);
	val = val >> ICAP_PROGRAMMING_WBSTAR_LOC;

	return sprintf(buf, "0x%x\n", val);
}
static DEVICE_ATTR(load_flash_addr, 0644, load_flash_addr_show,
                   load_flash_addr_store);

static ssize_t enable_store(struct device *dev, struct device_attribute *da,
                            const char *buf, size_t count)
{
	struct icap_cntrl *ic = platform_get_drvdata(to_platform_device(dev));
	long enable;
	u32 reg = 0;

	if (!ic->support_enabled) {
		xocl_dbg(dev, "Icap controller programming is not supported\n");
		return -EINVAL;
	}

	if (kstrtol(buf, 10, &enable)) {
		xocl_err(dev, "invalid input");
		return -EINVAL;
	}

	mutex_lock(&ic->icap_cntrl_lock);

	reg = READ_REG32(ic, ICAP_PROGRAMMING_REG);
	if (enable)
		reg |= ICAP_PROGRAMMING_REG_ENABLE;
	else
		reg &= ~ICAP_PROGRAMMING_REG_ENABLE;
	WRITE_REG32(ic, reg, ICAP_PROGRAMMING_REG);

	mutex_unlock(&ic->icap_cntrl_lock);

	return count;
}

static ssize_t enable_show(struct device *dev,
                           struct device_attribute *da, char *buf)
{
	struct icap_cntrl *ic = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	if (!ic->support_enabled) {
		xocl_dbg(dev, "Icap controller programming is not supported\n");
		return -EINVAL;
	}

	val = READ_REG32(ic, ICAP_PROGRAMMING_REG);
	val = val & ICAP_PROGRAMMING_REG_ENABLE;

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR(enable, 0644, enable_show, enable_store);

static struct attribute *icap_cntrl_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_load_flash_addr.attr,
	NULL,
};

static struct attribute_group icap_cntrl_attr_group = {
	.attrs = icap_cntrl_attrs,
};

static void icap_cntrl_sysfs_destroy(struct icap_cntrl *icap_cntrl)
{
	if (!icap_cntrl->sysfs_created)
		return;

	sysfs_remove_group(&icap_cntrl->pdev->dev.kobj, &icap_cntrl_attr_group);
	icap_cntrl->sysfs_created = false;
}

static int icap_cntrl_sysfs_create(struct icap_cntrl *ic)
{
	int ret;

	if (ic->sysfs_created)
		return 0;

	ret = sysfs_create_group(&ic->pdev->dev.kobj, &icap_cntrl_attr_group);
	if (ret) {
		xocl_err(&ic->pdev->dev, "create icap_cntrl attrs failed: 0x%x", ret);
		return ret;
	}

	ic->sysfs_created = true;

	return 0;
}

static int __icap_cntrl_remove(struct platform_device *pdev)
{
	struct icap_cntrl *icap_cntrl;
	void *hdl;

	icap_cntrl = platform_get_drvdata(pdev);
	if (!icap_cntrl) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	xocl_drvinst_release(icap_cntrl, &hdl);

	icap_cntrl_sysfs_destroy(icap_cntrl);

	if (icap_cntrl->base_addr)
		iounmap(icap_cntrl->base_addr);

	mutex_destroy(&icap_cntrl->icap_cntrl_lock);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void icap_cntrl_remove(struct platform_device *pdev)
{
	__icap_cntrl_remove(pdev);
}
#else
#define icap_cntrl_remove __icap_cntrl_remove
#endif

static int icap_cntrl_probe(struct platform_device *pdev)
{
	void *xdev_hdl = xocl_get_xdev(pdev);
	struct icap_cntrl *icap_cntrl;
	struct resource *res;
	int ret = 0;

	icap_cntrl = xocl_drvinst_alloc(&pdev->dev, sizeof(*icap_cntrl));
	if (!icap_cntrl) {
		xocl_err(&pdev->dev, "failed to alloc data");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, icap_cntrl);
	icap_cntrl->pdev = pdev;
	mutex_init(&icap_cntrl->icap_cntrl_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	icap_cntrl->base_addr = ioremap_nocache(res->start,
                                res->end - res->start + 1);
	if (!icap_cntrl->base_addr) {
		ret = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	icap_cntrl->priv_data = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (icap_cntrl->priv_data) {
		if (icap_cntrl->priv_data->flags & XOCL_IC_FLAT_SHELL)
			icap_cntrl->support_enabled = true;
		else
			icap_cntrl->support_enabled = false;
	} else {
		icap_cntrl->support_enabled = xocl_flat_shell_check(xdev_hdl);
	}
	if (icap_cntrl->support_enabled)
		xocl_info(&pdev->dev, "ICAP Controller Programming is Supported");

	ret = icap_cntrl_sysfs_create(icap_cntrl);
	if (ret)
		goto failed;

	return 0;

failed:
	icap_cntrl_remove(pdev);
	return ret;
}

struct platform_device_id icap_cntrl_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ICAP_CNTRL), 0 },
	{ },
};

static struct platform_driver	icap_cntrl_driver = {
	.probe		= icap_cntrl_probe,
	.remove		= icap_cntrl_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ICAP_CNTRL),
	},
	.id_table = icap_cntrl_id_table,
};

int __init xocl_init_icap_controller(void)
{
	return platform_driver_register(&icap_cntrl_driver);
}

void xocl_fini_icap_controller(void)
{
	platform_driver_unregister(&icap_cntrl_driver);
}
