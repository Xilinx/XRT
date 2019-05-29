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

struct xocl_flash {
	struct platform_device	*pdev;

	struct resource *res;
	struct xocl_flash_privdata *priv_data;
};

static ssize_t bar_off_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_flash *flash = dev_get_drvdata(dev);

	return sprintf(buf, "%lld\n", flash->priv_data->bar_off);
}

static DEVICE_ATTR_RO(bar_off);

static ssize_t flash_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_flash *flash = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", (char *)flash->priv_data +
			flash->priv_data->flash_type);
}

static DEVICE_ATTR_RO(flash_type);

static ssize_t properties_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_flash *flash = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", (char *)flash->priv_data +
			flash->priv_data->properties);
}

static DEVICE_ATTR_RO(properties);

static struct attribute *flash_attrs[] = {
	&dev_attr_bar_off.attr,
	&dev_attr_flash_type.attr,
	&dev_attr_properties.attr,
	NULL,
};

static struct attribute_group flash_attr_group = {
	.attrs = flash_attrs,
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

static int flash_probe(struct platform_device *pdev)
{
	struct xocl_flash *flash;
	int ret;

	flash = devm_kzalloc(&pdev->dev, sizeof(*flash), GFP_KERNEL);
	if (!flash)
		return -ENOMEM;

	flash->pdev = pdev;

	flash->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!flash->res) {
		xocl_err(&pdev->dev, "Empty resource");
		return -EINVAL;
	}

	flash->priv_data = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (!flash->priv_data) {
		xocl_err(&pdev->dev, "Empty priv data");
		return -EINVAL;
	}

	xocl_info(&pdev->dev, "Flash IO start: 0x%llx, end: 0x%llx",
		flash->res->start, flash->res->end);

	ret = sysfs_create_flash(flash);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, flash);

	return 0;
}


static int flash_remove(struct platform_device *pdev)
{
	struct xocl_flash	*flash;

	flash = platform_get_drvdata(pdev);
	if (!flash) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	sysfs_destroy_flash(flash);
	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, flash);

	return 0;
}

struct platform_device_id flash_id_table[] = {
	{ XOCL_DEVNAME(XOCL_FLASH), 0 },
	{ },
};

static struct platform_driver	flash_driver = {
	.probe		= flash_probe,
	.remove		= flash_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_FLASH),
	},
	.id_table = flash_id_table,
};

int __init xocl_init_flash(void)
{
	return platform_driver_register(&flash_driver);
}

void xocl_fini_flash(void)
{
	platform_driver_unregister(&flash_driver);
}
