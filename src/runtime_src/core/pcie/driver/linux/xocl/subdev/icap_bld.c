/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi Hou <lizhih@xilinx.com>
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
#include "mgmt-ioctl.h"

struct icap_bld {
	struct platform_device	*pdev;
	void		__iomem *base;
	void		__iomem *bldgate;
};

#if 0
static ssize_t properties_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR_RO(properties);

static struct attribute *icap_bld_attrs[] = {
	&dev_attr_properties.attr,
	NULL,
};

static struct attribute_group icap_bld_attr_group = {
	.attrs = icap_bld_attrs,
};

static int sysfs_create_flash(struct xocl_flash *flash)
{
	int ret;

	ret  = sysfs_create_group(&flash->pdev->dev.kobj, 
			&flash_attr_group);
	if (ret)
		xocl_err(&flash->pdev->dev, "create sysfs failed %d", ret);


	return ret;
}

static void sysfs_destroy_flash(struct xocl_flash *flash)
{
	sysfs_remove_group(&flash->pdev->dev.kobj,
			&flash_attr_group);
}
#endif

static int icap_bld_remove(struct platform_device *pdev)
{
	struct icap_bld	*icap;

	icap = platform_get_drvdata(pdev);
	if (!icap) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	if (icap->base)
		iounmap(icap->base);

	if (icap->bldgate)
		iounmap(icap->bldgate);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, icap);

	return 0;
}

static int icap_bld_probe(struct platform_device *pdev)
{
	struct icap_bld *icap;
	struct resource *res;
	int ret;

	icap = devm_kzalloc(&pdev->dev, sizeof(*icap), GFP_KERNEL);
	if (!icap)
		return -ENOMEM;

	icap->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "Empty resource 0");
		ret = -EINVAL;
		goto failed;
	}

	icap->base = ioremap_nocache(res->start,
			res->end - res->start + 1);
	if (!icap->base) {
		xocl_err(&pdev->dev, "map base iomem failed");
		ret = -EFAULT;
		goto failed;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		xocl_err(&pdev->dev, "Empty resource 1");
		ret = -EINVAL;
		goto failed;
	}

	icap->bldgate = ioremap_nocache(res->start,
			res->end - res->start + 1);
	if (!icap->bldgate) {
		xocl_err(&pdev->dev, "map bldgate failed");
		ret = -EFAULT;
		goto failed;
	}

	xocl_info(&pdev->dev, "icap probe success");

	platform_set_drvdata(pdev, icap);

	return 0;

failed:
	icap_bld_remove(pdev);
	return ret;
}

struct platform_device_id icap_bld_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ICAP_BLD), 0 },
	{ },
};

static struct platform_driver	icap_bld_driver = {
	.probe		= icap_bld_probe,
	.remove		= icap_bld_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ICAP_BLD),
	},
	.id_table = icap_bld_id_table,
};

int __init xocl_init_icap_bld(void)
{
	return platform_driver_register(&icap_bld_driver);
}

void xocl_fini_icap_bld(void)
{
	platform_driver_unregister(&icap_bld_driver);
}
