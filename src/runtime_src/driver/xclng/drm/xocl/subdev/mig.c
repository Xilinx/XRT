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


enum ecc_type {
	DRAM_ECC = 0,
	HBM_ECC_PS0,
	HBM_ECC_PS1,
};

#define ECC_STATUS	0x0
#define ECC_ON_OFF	0x8
#define CE_CNT		0xC
#define CE_ADDR_LO	0x1C0
#define CE_ADDR_HI	0x1C4
#define UE_ADDR_LO	0x2C0
#define UE_ADDR_HI	0x2C4
#define INJ_FAULT_REG	0x300

struct hbm_regs {
	u32 cfg_ecc_en;
	u32 scrub_en;
	u32 scrub_init_en;
	u32 cfg_scrub_rmw;
	u8 unuse_pad1[8];
	u32 err_clr;
	u8 unuse_pad2[12];
	u32 cnt_1b_ps0;
	u32 cnt_2b_ps0;
	u32 scrub_done_ps0;
	u32 cnt_1b_ps1;
	u32 cnt_2b_ps1;
	u32 scrub_done_ps1;
	u8 unuse_pad3[12];
	u32 err_gen_1b_ps0;
	u32 err_gen_2b_ps0;
	u32 err_gen_1b_ps1;
	u32 err_gen_2b_ps1;
};

struct ddr_regs {
	u32 ecc_status;
	u8 unuse_pad0[4];
	u32 ecc_on_off;
	u32 ce_cnt;
	u8 unuse_pad1[432];
	u32 ce_addr_lo;
	u32 ce_addr_hi;
	u8 unuse_pad2[248];
	u32 ue_addr_lo;
	u32 ue_addr_hi;
	u8 unuse_pad3[56];
	u32 err_inject;
};


struct xocl_mig {
	void __iomem	*base;
	struct device	*mig_dev;
	enum ecc_type	type;
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
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	ssize_t cnt = 0;

	if (mig->type == HBM_ECC_PS0)
		cnt = sprintf(buf, "%u\n", ioread32(&h_regs->cnt_1b_ps0));
	else if (mig->type == HBM_ECC_PS1)
		cnt = sprintf(buf, "%u\n", ioread32(&h_regs->cnt_1b_ps1));
	else 
		cnt = sprintf(buf, "%u\n", ioread32(MIG_DEV2BASE(dev) + CE_CNT));

	return cnt;
}
static DEVICE_ATTR_RO(ecc_ce_cnt);

static ssize_t ecc_ue_cnt_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t ret = 0;

	if (mig->type == HBM_ECC_PS0)
		ret = ioread32(&h_regs->cnt_2b_ps0);
	else if (mig->type == HBM_ECC_PS1)
		ret = ioread32(&h_regs->cnt_2b_ps1);


	return sprintf(buf, "%u\n", ret);
}
static DEVICE_ATTR_RO(ecc_ue_cnt);

static ssize_t ecc_status_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t err_1b, err_2b, ret;

	if (mig->type == HBM_ECC_PS0) {
		err_1b = ioread32(&h_regs->cnt_1b_ps0);
		err_2b = ioread32(&h_regs->cnt_2b_ps0);
		ret = (err_1b ? 1 : 0) << 1 | (err_2b ? 1 : 0);
	} else if (mig->type == HBM_ECC_PS1) {
		err_1b = ioread32(&h_regs->cnt_1b_ps1);
		err_2b = ioread32(&h_regs->cnt_2b_ps1);
		ret = (err_1b ? 1 : 0) << 1 | (err_2b ? 1 : 0);
	} else {
		ret = ioread32(MIG_DEV2BASE(dev) + ECC_STATUS);
	}

	return sprintf(buf, "%u\n", ret);
}
static DEVICE_ATTR_RO(ecc_status);


static ssize_t ecc_reset_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;

	if(mig->type == DRAM_ECC) {
		iowrite32(0x3, MIG_DEV2BASE(dev) + ECC_STATUS);
		iowrite32(0, MIG_DEV2BASE(dev) + CE_CNT);
	} else {
		iowrite32(0x1, &h_regs->cfg_ecc_en);
		iowrite32(0x1, &h_regs->scrub_en);
		iowrite32(0x1, &h_regs->scrub_init_en);
		iowrite32(0x0, &h_regs->err_clr);
		iowrite32(0x1, &h_regs->err_clr);
		iowrite32(0x0, &h_regs->err_clr);
	}
	return count;
}
static DEVICE_ATTR_WO(ecc_reset);


static ssize_t ecc_enabled_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	ssize_t cnt = 0;

	if (mig->type == HBM_ECC_PS0 || mig->type == HBM_ECC_PS1)
		cnt = sprintf(buf, "%u\n", ioread32(&h_regs->cfg_ecc_en));
	else
		cnt = sprintf(buf, "%u\n", ioread32(MIG_DEV2BASE(dev) + ECC_ON_OFF));

	return cnt;
}
static ssize_t ecc_enabled_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;

	if (sscanf(buf, "%d", &val) != 1 || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}
	if (mig->type == HBM_ECC_PS0 || mig->type == HBM_ECC_PS1)
		iowrite32(val, &h_regs->cfg_ecc_en);
	else
		iowrite32(val, MIG_DEV2BASE(dev) + ECC_ON_OFF);

	return count;
}
static DEVICE_ATTR_RW(ecc_enabled);


static ssize_t ecc_clear_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;
	if (sscanf(buf, "%d", &val) != 1 || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	if (mig->type != DRAM_ECC)
		iowrite32(val, &h_regs->err_clr);
	return count;
}
static DEVICE_ATTR_WO(ecc_clear);

#ifdef MIG_DEBUG
static ssize_t ecc_inject_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;
	
	if (sscanf(buf, "%d", &val) != 1 || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	if (mig->type == HBM_ECC_PS0)
		iowrite32(val, &h_regs->err_gen_1b_ps0);
	else if (mig->type == HBM_ECC_PS1)
		iowrite32(val, &h_regs->err_gen_1b_ps1);
	else
		iowrite32(val, MIG_DEV2BASE(dev) + INJ_FAULT_REG);

	return count;
}
static DEVICE_ATTR_WO(ecc_inject);

static ssize_t ecc_inject_2bits_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;
	
	if (sscanf(buf, "%d", &val) != 1 || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}
	if (mig->type == HBM_ECC_PS0)
		iowrite32(val, &h_regs->err_gen_2b_ps0);
	else if (mig->type == HBM_ECC_PS1)
		iowrite32(val, &h_regs->err_gen_2b_ps1);

	return count;
}
static DEVICE_ATTR_WO(ecc_inject_2bits);

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
	&dev_attr_ecc_ue_cnt.attr,
	&dev_attr_ecc_ce_ffa.attr,
	&dev_attr_ecc_ue_ffa.attr,
	&dev_attr_ecc_reset.attr,
	&dev_attr_ecc_clear.attr,
#ifdef MIG_DEBUG
	&dev_attr_ecc_inject.attr,
	&dev_attr_ecc_inject_2bits.attr,
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
	int err, digit_len;
	int idx;
	char *left_parentness = NULL, *right_parentness = NULL, temp[4];

	mig = devm_kzalloc(&pdev->dev, sizeof(*mig), GFP_KERNEL);
	if (!mig)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "resource is NULL");
		return -EINVAL;
	}

	if(!strncasecmp(XOCL_GET_SUBDEV_PRIV(&pdev->dev), "DDR", 3))
		mig->type = DRAM_ECC;
	else if (!strncasecmp(XOCL_GET_SUBDEV_PRIV(&pdev->dev), "HBM", 3)) {
		left_parentness = strstr(XOCL_GET_SUBDEV_PRIV(&pdev->dev), "[");
		right_parentness = strstr(XOCL_GET_SUBDEV_PRIV(&pdev->dev), "]");
		digit_len = right_parentness-(1+left_parentness);
		strncpy(temp, left_parentness+1, digit_len);
		temp[digit_len] = '\0';
		kstrtoint(temp, 10, &idx);

		if(idx % 2)
			mig->type = HBM_ECC_PS1;
		else
			mig->type = HBM_ECC_PS0;
	}

	xocl_info(&pdev->dev, "MIG name: %s, IO start: 0x%llx, end: 0x%llx mig->type %d",
		XOCL_GET_SUBDEV_PRIV(&pdev->dev), res->start, res->end, mig->type);

	if (mig->type == DRAM_ECC)
		mig->base = ioremap_nocache(res->start, res->end - res->start + 1);
	else
		mig->base = ioremap_nocache(res->start, sizeof(struct hbm_regs));

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
