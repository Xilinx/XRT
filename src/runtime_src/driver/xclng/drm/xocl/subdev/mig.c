/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Chien-Wei Lan 
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

#define ECC_STATUS    0x0
#define ECC_EN_IRQ    0x4
#define ECC_ON_OFF    0x8
#define CE_CNT        0xC

#define FAULT_REG     0x300

#define	MIG_MAX_NUM		4

#define MIG_DEBUG      1

struct xocl_mig {
	void __iomem		**base;
	struct device		*mig_dev;
};


static int mig_get_prop(struct platform_device *pdev, struct xocl_mig	*mig, 
	uint32_t bank, uint32_t *val)
{

	if(!mig){
		xocl_err(&pdev->dev, "found no mig %d", bank);
		return -EINVAL;
	}

	if(!mig->base[bank]){
		xocl_err(&pdev->dev, "invalid bank %d", bank);
		return -EINVAL;
	}

	*val = ioread32(mig->base[bank]+CE_CNT);

	return 0;
}

static ssize_t ecc_cnt0_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_mig	*mig = platform_get_drvdata(pdev);
	uint32_t val, bank;

	if(sscanf(da->attr.name, "ecc_cnt%d", &bank) !=1){
		return -EINVAL;
	}

	if(mig_get_prop(pdev, mig, bank, &val))
		val = 0xffffdead;

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(ecc_cnt0);

static ssize_t ecc_cnt1_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_mig	*mig = platform_get_drvdata(pdev);
	uint32_t val, bank;

	if(sscanf(da->attr.name, "ecc_cnt%d", &bank) !=1){
		return -EINVAL;
	}

	if(mig_get_prop(pdev, mig, bank, &val))
		val = 0xffffdead;

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(ecc_cnt1);

static ssize_t ecc_cnt2_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_mig	*mig = platform_get_drvdata(pdev);
	uint32_t val, bank;

	if(sscanf(da->attr.name, "ecc_cnt%d", &bank) !=1){
		return -EINVAL;
	}

	if(mig_get_prop(pdev, mig, bank, &val))
		val = 0xffffdead;

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(ecc_cnt2);

static ssize_t ecc_cnt3_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_mig	*mig = platform_get_drvdata(pdev);
	uint32_t val, bank;

	if(sscanf(da->attr.name, "ecc_cnt%d", &bank) !=1){
		return -EINVAL;
	}

	if(mig_get_prop(pdev, mig, bank, &val))
		val = 0xffffdead;

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(ecc_cnt3);

static ssize_t cnt_reset_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_mig	*mig = platform_get_drvdata(pdev);
	uint32_t bank;

	if (sscanf(buf, "%d", &bank) != 1 || (bank >= MIG_MAX_NUM)) {
		xocl_err(&pdev->dev, "input should be: echo bank > cnt_reset");
		return -EINVAL;
	}

	if(!mig->base[bank]){
		xocl_err(&pdev->dev, "invalid bank %d", bank);
		return -EINVAL;
	}

	iowrite32(0, mig->base[bank]+CE_CNT);

	return count;
}
static DEVICE_ATTR_WO(cnt_reset);

#ifdef MIG_DEBUG
static ssize_t ecc_inject_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_mig	*mig = platform_get_drvdata(pdev);
	uint32_t bank;

	if (sscanf(buf, "%d", &bank) != 1 || (bank >= MIG_MAX_NUM)) {
		xocl_err(&pdev->dev, "input should be: echo bank > ecc_inject");
		return -EINVAL;
	}

	if(!mig->base[bank]){
		xocl_err(&pdev->dev, "invalid bank %d", bank);
		return -EINVAL;
	}

	iowrite32(1, mig->base[bank]+FAULT_REG);

	return count;
}
static DEVICE_ATTR_WO(ecc_inject);
#endif 

static struct attribute *mig_attributes[] = {
	&dev_attr_ecc_cnt0.attr,
	&dev_attr_ecc_cnt1.attr,
	&dev_attr_ecc_cnt2.attr,
	&dev_attr_ecc_cnt3.attr,
	&dev_attr_cnt_reset.attr,
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
	struct xocl_dev_core *core;
	int err;

	mig = platform_get_drvdata(pdev);
	core = XDEV(xocl_get_xdev(pdev));

	err = sysfs_create_group(&pdev->dev.kobj, &mig_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create pw group failed: 0x%x", err);
		goto create_grp_failed;
	}

	return 0;

create_grp_failed:
	return err;
}

static int mig_probe(struct platform_device *pdev)
{
	struct xocl_mig *mig;
	struct resource *res;
	int err, i;

	mig = devm_kzalloc(&pdev->dev, sizeof(*mig), GFP_KERNEL);
	if (!mig)
		return -ENOMEM;
	
	mig->base = devm_kzalloc(&pdev->dev, MIG_MAX_NUM*sizeof(void __iomem *), GFP_KERNEL);
	if (!mig->base)
		return -ENOMEM;

	for(i =0; i < MIG_MAX_NUM ;++i){
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			xocl_err(&pdev->dev, "resource %d is NULL", i);
			return 0;
		}
		xocl_info(&pdev->dev, "MIG IO start: 0x%llx, end: 0x%llx",
			res->start, res->end);

		mig->base[i] = ioremap_nocache(res->start, res->end - res->start + 1);
		if (!mig->base[i]) {
			err = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}
	}

	platform_set_drvdata(pdev, mig);

	err = mgmt_sysfs_create_mig(pdev);
	if (err) {
		goto create_mig_failed;
	}

	return 0;

create_mig_failed:
	platform_set_drvdata(pdev, NULL);
failed:
	return err;
}


static int mig_remove(struct platform_device *pdev)
{
	struct xocl_mig	*mig;
	int i;

	mig = platform_get_drvdata(pdev);
	if (!mig) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	mgmt_sysfs_destroy_mig(pdev);

	if (mig->base){
		for(i =0; i < MIG_MAX_NUM ;++i){
			if(mig->base[i])
				iounmap(mig->base[i]);
		}
	}

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, mig->base);
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