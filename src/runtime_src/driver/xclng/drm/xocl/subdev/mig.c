/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
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

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

/* Registers are defined in pg150-ultrascale-memory-ip.pdf:
 * AXI4-Lite Slave Control/Status Register Map
 */

#define MIG_DEBUG
#define	MIG_DEV2MIG(dev)	\
	((struct xocl_mig *)platform_get_drvdata(to_platform_device(dev)))
#define	MIG_DEV2BASE(dev)	(MIG_DEV2MIG(dev)->base)

#define ECC_STATUS	0x0
#define ECC_ON_OFF	0x8
#define CE_CNT		0xC
#define CE_ADDR_LO	0x1C0
#define CE_ADDR_HI	0x1C4
#define UE_ADDR_LO	0x2C0
#define UE_ADDR_HI	0x2C4
#define INJ_FAULT_REG	0x300

struct xocl_mig {
	void __iomem	*base;
	struct device	*mig_dev;
};

static ssize_t ecc_ue_ffa_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint64_t val = ioread32(MIG_DEV2BASE(dev) + UE_ADDR_HI);
	val <<= 32;
	val |= ioread32(MIG_DEV2BASE(dev) + UE_ADDR_LO);
	return sprintf(buf, "0x%llx\n", val);
}
static DEVICE_ATTR_RO(ecc_ue_ffa);


static ssize_t ecc_ce_ffa_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint64_t val = ioread32(MIG_DEV2BASE(dev) + CE_ADDR_HI);
	val <<= 32;
	val |= ioread32(MIG_DEV2BASE(dev) + CE_ADDR_LO);
	return sprintf(buf, "0x%llx\n", val);
}
static DEVICE_ATTR_RO(ecc_ce_ffa);


static ssize_t ecc_ce_cnt_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	return sprintf(buf, "%u\n", ioread32(MIG_DEV2BASE(dev) + CE_CNT));
}
static DEVICE_ATTR_RO(ecc_ce_cnt);


static ssize_t ecc_status_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	return sprintf(buf, "%u\n", ioread32(MIG_DEV2BASE(dev) + ECC_STATUS));
}
static DEVICE_ATTR_RO(ecc_status);


static ssize_t ecc_reset_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	iowrite32(0x3, MIG_DEV2BASE(dev) + ECC_STATUS);
	iowrite32(0, MIG_DEV2BASE(dev) + CE_CNT);
	return count;
}
static DEVICE_ATTR_WO(ecc_reset);


static ssize_t ecc_enabled_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	return sprintf(buf, "%u\n", ioread32(MIG_DEV2BASE(dev) + ECC_ON_OFF));
}
static ssize_t ecc_enabled_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	uint32_t val;

	if (sscanf(buf, "%d", &val) != 1 || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	iowrite32(val, MIG_DEV2BASE(dev) + ECC_ON_OFF);
	return count;
}
static DEVICE_ATTR_RW(ecc_enabled);


#ifdef MIG_DEBUG
static ssize_t ecc_inject_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	iowrite32(1, MIG_DEV2BASE(dev) + INJ_FAULT_REG);
	return count;
}
static DEVICE_ATTR_WO(ecc_inject);
#endif


/* Standard sysfs entry for all dynamic subdevices. */
static ssize_t name_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	return sprintf(buf, "%s\n", XOCL_GET_SUBDEV_PRIV(dev));
}
static DEVICE_ATTR_RO(name);


static struct attribute *mig_attributes[] = {
	&dev_attr_name.attr,
	&dev_attr_ecc_enabled.attr,
	&dev_attr_ecc_status.attr,
	&dev_attr_ecc_ce_cnt.attr,
	&dev_attr_ecc_ce_ffa.attr,
	&dev_attr_ecc_ue_ffa.attr,
	&dev_attr_ecc_reset.attr,
#ifdef MIG_DEBUG
	&dev_attr_ecc_inject.attr,
#endif
	NULL
};

static const struct attribute_group mig_attrgroup = {
	.attrs = mig_attributes,
};

static void mgmt_sysfs_destroy_mig(struct platform_device *pdev)
{
	struct xocl_mig *mig;

	mig = platform_get_drvdata(pdev);
	sysfs_remove_group(&pdev->dev.kobj, &mig_attrgroup);
}

static int mgmt_sysfs_create_mig(struct platform_device *pdev)
{
	struct xocl_mig *mig;
	int err;

	mig = platform_get_drvdata(pdev);
	err = sysfs_create_group(&pdev->dev.kobj, &mig_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create pw group failed: 0x%x", err);
		return err;
	}

	return 0;
}

static int mig_probe(struct platform_device *pdev)
{
	struct xocl_mig *mig;
	struct resource *res;
	int err;

	mig = devm_kzalloc(&pdev->dev, sizeof(*mig), GFP_KERNEL);
	if (!mig)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "resource is NULL");
		return -EINVAL;
	}

	xocl_info(&pdev->dev, "MIG name: %s, IO start: 0x%llx, end: 0x%llx",
		XOCL_GET_SUBDEV_PRIV(&pdev->dev), res->start, res->end);

	mig->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!mig->base) {
		xocl_err(&pdev->dev, "Map iomem failed");
		return -EIO;
	}

	platform_set_drvdata(pdev, mig);

	err = mgmt_sysfs_create_mig(pdev);
	if (err) {
		platform_set_drvdata(pdev, NULL);
		iounmap(mig->base);
		return err;
	}

	return 0;
}


static int mig_remove(struct platform_device *pdev)
{
	struct xocl_mig	*mig;

	mig = platform_get_drvdata(pdev);
	if (!mig) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_info(&pdev->dev, "MIG name: %s", XOCL_GET_SUBDEV_PRIV(&pdev->dev));

	mgmt_sysfs_destroy_mig(pdev);

	if (mig->base)
		iounmap(mig->base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, mig);

	return 0;
}

struct platform_device_id mig_id_table[] = {
	{ XOCL_MIG, 0 },
	{ },
};

static struct platform_driver	mig_driver = {
	.probe		= mig_probe,
	.remove		= mig_remove,
	.driver		= {
		.name = "xocl_mig",
	},
	.id_table = mig_id_table,
};

int __init xocl_init_mig(void)
{
	return platform_driver_register(&mig_driver);
}

void xocl_fini_mig(void)
{
	platform_driver_unregister(&mig_driver);
}
