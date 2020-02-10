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

#define	MIG_PRIVILEGED(mig)	((mig)->base != NULL)

#define MIG_DEBUG
#define	MIG_DEV2MIG(dev)	\
	((struct xocl_mig *)platform_get_drvdata(to_platform_device(dev)))
#define	MIG_DEV2BASE(dev)	(MIG_DEV2MIG(dev)->base)
#define	MIG_ERR(mig, fmt, arg...)	\
	xocl_err((mig)->mig_dev, fmt "\n", ##arg)
#define	MIG_INFO(mig, fmt, arg...)	\
	xocl_info((mig)->mig_dev, fmt "\n", ##arg)


#define MIG_DEFAULT_EXPIRE_SECS 1

#define MIG_MAX_RES		1

enum ecc_type {
	DRAM_ECC = 0,
	HBM_ECC_PS0,
	HBM_ECC_PS1,
};

enum ecc_prop {
	MIG_ECC_ENABLE = 0,
	MIG_ECC_STATUS,
	MIG_ECC_CE_CNT,
	MIG_ECC_CE_FFA,
	MIG_ECC_UE_CNT,
	MIG_ECC_UE_FFA,
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
	u8 unuse_pad0[72];
	u32 cfg_mask;
	u8 unuse_pad1[100];
	u32 cfg_hbm_cb_en;
	u8 unuse_pad2[5964];
	u32 cfg_dm_en;
	u32 cfg_rmw_en;
	u8 unuse_pad3[1016];
	u32 cfg_ecc_en;
	u32 scrub_en;
	u32 scrub_init_en;
	u32 cfg_scrub_rmw;
	u8 unuse_pad4[8];
	u32 err_clr;
	u8 unuse_pad5[12];
	u32 cnt_1b_ps0;
	u32 cnt_2b_ps0;
	u32 scrub_done_ps0;
	u32 cnt_1b_ps1;
	u32 cnt_2b_ps1;
	u32 scrub_done_ps1;
	u8 unuse_pad6[12];
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
	void __iomem		*base;
	struct device		*mig_dev;
	enum ecc_type		type;
	struct xcl_mig_ecc	cache;
	struct xocl_mig_label	mig_label;
	u32			ecc_enabled;
};

#define MIG_DEV2XDEV(d)	xocl_get_xdev(to_platform_device(d))

static void ecc_reset(struct xocl_mig *mig)
{
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	xdev_handle_t xdev = MIG_DEV2XDEV(mig->mig_dev);

	if (!MIG_PRIVILEGED(mig)) {
		MIG_INFO(mig, "Unable to reset from userpf");
		return;
	}

	if (!mig->ecc_enabled)
		return;

	if (mig->type == DRAM_ECC) {
		xocl_dr_reg_write32(xdev, 0x3, mig->base + ECC_STATUS);
		xocl_dr_reg_write32(xdev, 0, mig->base + CE_CNT);
	} else {
		xocl_dr_reg_write32(xdev, 0x1, &h_regs->cfg_ecc_en);
		/*                    cfg_mask  cfg_hbm_cb_en  cfg_dm_en  cfg_rmw_en
		 *  HBM enable            0            1           0          1
		 *  HBM disable           1            0           1          0
		 */
		xocl_dr_reg_write32(xdev, 0x0, &h_regs->cfg_mask);
		xocl_dr_reg_write32(xdev, 0x1, &h_regs->cfg_hbm_cb_en);
		xocl_dr_reg_write32(xdev, 0x0, &h_regs->cfg_dm_en);
		xocl_dr_reg_write32(xdev, 0x1, &h_regs->cfg_rmw_en);
		xocl_dr_reg_write32(xdev, 0x1, &h_regs->scrub_en);
		xocl_dr_reg_write32(xdev, 0x1, &h_regs->scrub_init_en);
		xocl_dr_reg_write32(xdev, 0x0, &h_regs->err_clr);
		xocl_dr_reg_write32(xdev, 0x1, &h_regs->err_clr);
		xocl_dr_reg_write32(xdev, 0x0, &h_regs->err_clr);
	}
}

static void mig_ecc_get_prop(struct device *dev, enum ecc_prop kind, void *buf)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t err_1b, err_2b, ret = 0;
	uint64_t addr = 0;

	if (MIG_PRIVILEGED(mig)) {

		switch (kind) {
		case MIG_ECC_ENABLE:
			if (mig->type == HBM_ECC_PS0 || mig->type == HBM_ECC_PS1)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cfg_ecc_en);
			else
				ret = xocl_dr_reg_read32(xdev, MIG_DEV2BASE(dev) + ECC_ON_OFF);
			*(uint32_t *)buf = ret;
			break;
		case MIG_ECC_STATUS:
			if (mig->type == HBM_ECC_PS0) {
				err_1b = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps0);
				err_2b = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps0);
				ret = (err_1b ? 1 : 0) << 1 | (err_2b ? 1 : 0);
			} else if (mig->type == HBM_ECC_PS1) {
				err_1b = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps1);
				err_2b = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps1);
				ret = (err_1b ? 1 : 0) << 1 | (err_2b ? 1 : 0);
			} else {
				ret = xocl_dr_reg_read32(xdev, MIG_DEV2BASE(dev) + ECC_STATUS);
			}
			*(uint32_t *)buf = ret;
			break;
		case MIG_ECC_CE_CNT:
			if (mig->type == HBM_ECC_PS0)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps0);
			else if (mig->type == HBM_ECC_PS1)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps1);
			else
				ret = xocl_dr_reg_read32(xdev, MIG_DEV2BASE(dev) + CE_CNT);

			*(uint32_t *)buf = ret;
			break;
		case MIG_ECC_CE_FFA:
			addr = xocl_dr_reg_read32(xdev, MIG_DEV2BASE(dev) + CE_ADDR_HI);
			addr <<= 32;
			addr |= xocl_dr_reg_read32(xdev, MIG_DEV2BASE(dev) + CE_ADDR_LO);
			*(uint64_t *)buf = addr;
			break;
		case MIG_ECC_UE_CNT:
			if (mig->type == HBM_ECC_PS0)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps0);
			else if (mig->type == HBM_ECC_PS1)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps1);

			*(uint32_t *)buf = ret;
			break;
		case MIG_ECC_UE_FFA:
			addr = xocl_dr_reg_read32(xdev, MIG_DEV2BASE(dev) + UE_ADDR_HI);
			addr <<= 32;
			addr |= xocl_dr_reg_read32(xdev, MIG_DEV2BASE(dev) + UE_ADDR_LO);
			*(uint64_t *)buf = addr;
			break;
		default:
			break;
		}
	} else {

		switch (kind) {
		case MIG_ECC_ENABLE:
			*(uint32_t *)buf = mig->cache.ecc_enabled;
			break;
		case MIG_ECC_STATUS:
			*(uint32_t *)buf = mig->cache.ecc_status;
			break;
		case MIG_ECC_CE_CNT:
			*(uint32_t *)buf = mig->cache.ecc_ce_cnt;
			break;
		case MIG_ECC_CE_FFA:
			*(uint64_t *)buf = mig->cache.ecc_ce_ffa;
			break;
		case MIG_ECC_UE_CNT:
			*(uint32_t *)buf = mig->cache.ecc_ue_cnt;
			break;
		case MIG_ECC_UE_FFA:
			*(uint64_t *)buf = mig->cache.ecc_ue_ffa;
			break;
		default:
			break;
		}
	}
}

static ssize_t ecc_ue_ffa_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint64_t val;

	mig_ecc_get_prop(dev, MIG_ECC_UE_FFA, &val);
	return sprintf(buf, "0x%llx\n", val);
}
static DEVICE_ATTR_RO(ecc_ue_ffa);

static ssize_t ecc_ce_ffa_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint64_t addr;

	mig_ecc_get_prop(dev, MIG_ECC_CE_FFA, &addr);
	return sprintf(buf, "0x%llx\n", addr);
}
static DEVICE_ATTR_RO(ecc_ce_ffa);

static ssize_t ecc_ce_cnt_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t ret;

	mig_ecc_get_prop(dev, MIG_ECC_CE_CNT, &ret);
	return sprintf(buf, "%u\n", ret);
}
static DEVICE_ATTR_RO(ecc_ce_cnt);

static ssize_t ecc_ue_cnt_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t ret;

	mig_ecc_get_prop(dev, MIG_ECC_UE_CNT, &ret);
	return sprintf(buf, "%u\n", ret);
}
static DEVICE_ATTR_RO(ecc_ue_cnt);

static ssize_t ecc_status_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t status;

	mig_ecc_get_prop(dev, MIG_ECC_STATUS, &status);
	return sprintf(buf, "%u\n", status);
}
static DEVICE_ATTR_RO(ecc_status);


static ssize_t ecc_reset_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);

	ecc_reset(mig);
	return count;
}
static DEVICE_ATTR_WO(ecc_reset);

static ssize_t ecc_enabled_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t enable;

	mig_ecc_get_prop(dev, MIG_ECC_ENABLE, &enable);
	return sprintf(buf, "%u\n", enable);
}
static ssize_t ecc_enabled_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mig))
		return count;

	if (!mig->ecc_enabled)
		return count;

	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	if (mig->type == HBM_ECC_PS0 || mig->type == HBM_ECC_PS1) {

		val &= 0x1;
		xocl_dr_reg_write32(xdev, val, &h_regs->cfg_ecc_en);
		/*                    cfg_mask  cfg_hbm_cb_en  cfg_dm_en  cfg_rmw_en
		 *  HBM enable            0            1           0          1
		 *  HBM disable           1            0           1          0
		 */
		xocl_dr_reg_write32(xdev, val^1, &h_regs->cfg_mask);
		xocl_dr_reg_write32(xdev, val, &h_regs->cfg_hbm_cb_en);
		xocl_dr_reg_write32(xdev, val^1, &h_regs->cfg_dm_en);
		xocl_dr_reg_write32(xdev, val, &h_regs->cfg_rmw_en);
	} else {
		xocl_dr_reg_write32(xdev, val, MIG_DEV2BASE(dev) + ECC_ON_OFF);
	}

	return count;
}
static DEVICE_ATTR_RW(ecc_enabled);


static ssize_t ecc_clear_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mig))
		return count;

	if (!mig->ecc_enabled)
		return count;

	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	if (mig->type != DRAM_ECC)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_clr);
	return count;
}
static DEVICE_ATTR_WO(ecc_clear);

#ifdef MIG_DEBUG
static ssize_t ecc_inject_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mig))
		return count;

	if (!mig->ecc_enabled)
		return count;

	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	if (mig->type == HBM_ECC_PS0)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_1b_ps0);
	else if (mig->type == HBM_ECC_PS1)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_1b_ps1);
	else
		xocl_dr_reg_write32(xdev, val, MIG_DEV2BASE(dev) + INJ_FAULT_REG);

	return count;
}
static DEVICE_ATTR_WO(ecc_inject);

static ssize_t ecc_inject_2bits_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mig *mig = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mig->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mig))
		return count;

	if (!mig->ecc_enabled)
		return count;

	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}
	if (mig->type == HBM_ECC_PS0)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_2b_ps0);
	else if (mig->type == HBM_ECC_PS1)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_2b_ps1);

	return count;
}
static DEVICE_ATTR_WO(ecc_inject_2bits);

#endif


/* Standard sysfs entry for all dynamic subdevices. */
static ssize_t name_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_mig *mig = MIG_DEV2MIG(dev);

	return sprintf(buf, "%s\n", (char *)mig->mig_label.tag);
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



static void mig_get_data(struct platform_device *pdev, void *buf, size_t entry_sz)
{
	struct xocl_mig *mig = platform_get_drvdata(pdev);
	struct xcl_mig_ecc mig_ecc = {0};

	if (!MIG_PRIVILEGED(mig))
		return;

	mig_ecc_get_prop(&pdev->dev, MIG_ECC_STATUS, &mig_ecc.ecc_status);
	mig_ecc_get_prop(&pdev->dev, MIG_ECC_ENABLE, &mig_ecc.ecc_enabled);
	mig_ecc_get_prop(&pdev->dev, MIG_ECC_CE_CNT, &mig_ecc.ecc_ce_cnt);
	mig_ecc_get_prop(&pdev->dev, MIG_ECC_UE_CNT, &mig_ecc.ecc_ue_cnt);
	mig_ecc_get_prop(&pdev->dev, MIG_ECC_CE_FFA, &mig_ecc.ecc_ce_ffa);
	mig_ecc_get_prop(&pdev->dev, MIG_ECC_UE_FFA, &mig_ecc.ecc_ue_ffa);
	mig_ecc.mem_type = mig->mig_label.mem_type;
	mig_ecc.mem_idx = mig->mig_label.mem_idx;

	memcpy(buf, &mig_ecc, entry_sz);
}

static void mig_set_data(struct platform_device *pdev, void *buf)
{
	struct xocl_mig *mig = platform_get_drvdata(pdev);

	if (!buf)
		return;

	if (MIG_PRIVILEGED(mig))
		return;

	memcpy(&mig->cache, buf, sizeof(struct xcl_mig_ecc));

}

static uint32_t mig_get_id(struct platform_device *pdev)
{
	struct xocl_mig *mig = platform_get_drvdata(pdev);
	uint32_t id = (mig->mig_label.mem_type << 16) + mig->mig_label.mem_idx;

	return id;
}


static struct xocl_mig_funcs mig_ops = {
	.get_data	= mig_get_data,
	.set_data	= mig_set_data,
	.get_id		= mig_get_id,
};

static const struct attribute_group mig_attrgroup = {
	.attrs = mig_attributes,
};

static void sysfs_destroy_mig(struct platform_device *pdev)
{
	struct xocl_mig *mig;

	mig = platform_get_drvdata(pdev);
	sysfs_remove_group(&pdev->dev.kobj, &mig_attrgroup);
}

static int sysfs_create_mig(struct platform_device *pdev)
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
	int idx, i;
	char *left_parentness = NULL, *right_parentness = NULL, temp[4];

	mig = devm_kzalloc(&pdev->dev, sizeof(*mig), GFP_KERNEL);
	if (!mig)
		return -ENOMEM;

	mig->mig_dev = &pdev->dev;

	memcpy(&mig->mig_label, XOCL_GET_SUBDEV_PRIV(&pdev->dev), sizeof(struct xocl_mig_label));


	if (!strncasecmp(mig->mig_label.tag, "DDR", 3)) {
		mig->type = DRAM_ECC;
		mig->mig_label.mem_type = MEM_DRAM;
	} else if (!strncasecmp(mig->mig_label.tag, "bank", 4)) {
		mig->type = DRAM_ECC;
		mig->mig_label.mem_type = MEM_DRAM;
	} else if (!strncasecmp(mig->mig_label.tag, "HBM", 3)) {
		left_parentness = strstr(mig->mig_label.tag, "[");
		right_parentness = strstr(mig->mig_label.tag, "]");
		digit_len = right_parentness-(1+left_parentness);
		strncpy(temp, left_parentness+1, digit_len);
		temp[digit_len] = '\0';

		if (kstrtoint(temp, 10, &idx) != 0)
			return -EINVAL;

		mig->mig_label.mem_type = MEM_HBM;
		if (idx % 2)
			mig->type = HBM_ECC_PS1;
		else
			mig->type = HBM_ECC_PS0;
	}

	for (i = 0; i < MIG_MAX_RES; ++i) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			break;

		xocl_info(&pdev->dev, "MIG name: %s, IO start: 0x%llx, end: 0x%llx mig->type %d",
			(char *)mig->mig_label.tag, res->start, res->end, mig->type);

		if (mig->type == DRAM_ECC)
			mig->base = ioremap_nocache(res->start, res->end - res->start + 1);
		else
			mig->base = ioremap_nocache(res->start, sizeof(struct hbm_regs));

		if (!mig->base) {
			xocl_err(&pdev->dev, "Map iomem failed");
			return -EIO;
		}
	}
	platform_set_drvdata(pdev, mig);

	err = sysfs_create_mig(pdev);
	if (err) {
		platform_set_drvdata(pdev, NULL);
		iounmap(mig->base);
		return err;
	}
	/* check MIG_ECC_ENABLE before reset*/
	mig_ecc_get_prop(&pdev->dev, MIG_ECC_ENABLE, &mig->ecc_enabled);

	ecc_reset(mig);

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
	sysfs_destroy_mig(pdev);
	if (mig->base)
		iounmap(mig->base);
	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, mig);

	return 0;
}

struct xocl_drv_private mig_priv = {
	.ops = &mig_ops,
};

struct platform_device_id mig_id_table[] = {
	{ XOCL_DEVNAME(XOCL_MIG), (kernel_ulong_t)&mig_priv },
	{ },
};

static struct platform_driver	mig_driver = {
	.probe		= mig_probe,
	.remove		= mig_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_MIG),
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
