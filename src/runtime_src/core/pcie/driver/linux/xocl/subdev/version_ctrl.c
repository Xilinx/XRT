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

//Version control registers
#define VERSION_CTRL_REG                       0x0
#define VERSION_CTRL_REG_FLAT_SHELL_MASK       0x80000000

#define VERSION_CTRL_MISC_REG                  0xC
#define VERSION_CTRL_MISC_REG_CMC_IN_BITFILE   0x2

#define	READ_REG32(vc, off)		\
	(vc->base ?		\
	XOCL_READ_REG32(vc->base + off) : 0)

struct version_ctrl {
	struct platform_device *pdev;
	void __iomem *base;
	struct xocl_version_ctrl_privdata *priv_data;
	bool sysfs_created;
	bool			flat_shell;
	bool			cmc_in_bitfile;
};

static bool flat_shell_check(struct platform_device *pdev)
{
	struct version_ctrl *vc;

	vc = platform_get_drvdata(pdev);
	if (!vc)
		return false;

	return vc->flat_shell;
}

static bool cmc_in_bitfile(struct platform_device *pdev)
{
	struct version_ctrl *vc;

	vc = platform_get_drvdata(pdev);
	if (!vc)
		return false;

	return vc->cmc_in_bitfile;
}

static ssize_t version_show(struct device *dev,
                            struct device_attribute *da, char *buf)
{
	struct version_ctrl *vc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	val = READ_REG32(vc, VERSION_CTRL_REG);

	return sprintf(buf, "0x%x\n", val);
}
static DEVICE_ATTR_RO(version);

static ssize_t cmc_in_bitfile_show(struct device *dev,
                           struct device_attribute *da, char *buf)
{
	struct version_ctrl *vc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	val = READ_REG32(vc, VERSION_CTRL_MISC_REG);

	return sprintf(buf, "%d\n", (val & VERSION_CTRL_MISC_REG_CMC_IN_BITFILE));
}
static DEVICE_ATTR_RO(cmc_in_bitfile);

static struct attribute *version_ctrl_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_cmc_in_bitfile.attr,
	NULL,
};

static struct attribute_group version_ctrl_attr_group = {
	.attrs = version_ctrl_attrs,
};

static void version_ctrl_sysfs_destroy(struct version_ctrl *version_ctrl)
{
	if (!version_ctrl->sysfs_created)
		return;

	sysfs_remove_group(&version_ctrl->pdev->dev.kobj, &version_ctrl_attr_group);
	version_ctrl->sysfs_created = false;
}

static int version_ctrl_sysfs_create(struct version_ctrl *vc)
{
	int ret;

	if (vc->sysfs_created)
		return 0;

	ret = sysfs_create_group(&vc->pdev->dev.kobj, &version_ctrl_attr_group);
	if (ret) {
		xocl_err(&vc->pdev->dev, "create version_ctrl attrs failed: 0x%x", ret);
		return ret;
	}

	vc->sysfs_created = true;

	return 0;
}

static int __version_ctrl_remove(struct platform_device *pdev)
{
	struct version_ctrl *version_ctrl;
	void *hdl;

	version_ctrl = platform_get_drvdata(pdev);
	if (!version_ctrl) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	xocl_drvinst_release(version_ctrl, &hdl);

	version_ctrl_sysfs_destroy(version_ctrl);

	if (version_ctrl->base)
		iounmap(version_ctrl->base);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void version_ctrl_remove(struct platform_device *pdev)
{
	__version_ctrl_remove(pdev);
}
#else
#define version_ctrl_remove __version_ctrl_remove
#endif

static struct xocl_version_ctrl_funcs vc_ops = {
	.flat_shell_check = flat_shell_check,
	.cmc_in_bitfile = cmc_in_bitfile,
};

static int version_ctrl_probe(struct platform_device *pdev)
{
	struct version_ctrl *version_ctrl;
	struct resource *res;
	u32 reg;
	int ret = 0;

	version_ctrl = xocl_drvinst_alloc(&pdev->dev, sizeof(*version_ctrl));
	if (!version_ctrl) {
		xocl_err(&pdev->dev, "failed to alloc data");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, version_ctrl);
	version_ctrl->pdev = pdev;

	version_ctrl->priv_data = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	version_ctrl->base = ioremap_nocache(res->start,
                                res->end - res->start + 1);
	if (!version_ctrl->base) {
		ret = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	reg = READ_REG32(version_ctrl, VERSION_CTRL_REG);
	if (reg & VERSION_CTRL_REG_FLAT_SHELL_MASK)
		version_ctrl->flat_shell = true;

	reg = READ_REG32(version_ctrl, VERSION_CTRL_MISC_REG);
	if (reg & VERSION_CTRL_MISC_REG_CMC_IN_BITFILE)
		version_ctrl->cmc_in_bitfile = true;

	ret = version_ctrl_sysfs_create(version_ctrl);
	if (ret)
		goto failed;

	return 0;

failed:
	version_ctrl_remove(pdev);
	return ret;
}

struct xocl_drv_private version_ctrl_priv = {
	.ops = &vc_ops,
};

struct platform_device_id version_ctrl_id_table[] = {
	{ XOCL_DEVNAME(XOCL_VERSION_CTRL), (kernel_ulong_t)&version_ctrl_priv },
	{ },
};

static struct platform_driver	version_ctrl_driver = {
	.probe		= version_ctrl_probe,
	.remove		= version_ctrl_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_VERSION_CTRL),
	},
	.id_table = version_ctrl_id_table,
};

int __init xocl_init_version_control(void)
{
	return platform_driver_register(&version_ctrl_driver);
}

void xocl_fini_version_control(void)
{
	platform_driver_unregister(&version_ctrl_driver);
}
