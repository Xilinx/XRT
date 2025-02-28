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

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

/* Registers are defined in pg150-ultrascale-memory-ip.pdf:
 * AXI4-Lite Slave Control/Status Register Map
 */

#define	MIG_PRIVILEGED(mem_hbm)	((mem_hbm)->base != NULL)

#define MIG_DEBUG
#define	MIG_DEV2MIG(dev)	\
	((struct xocl_mem_hbm *)platform_get_drvdata(to_platform_device(dev)))
#define	MIG_DEV2BASE(dev)	(MIG_DEV2MIG(dev)->base)
#define	MIG_ERR(mem_hbm, fmt, arg...)	\
	xocl_err((mem_hbm)->mem_hbm_dev, fmt "\n", ##arg)
#define	MIG_INFO(mem_hbm, fmt, arg...)	\
	xocl_info((mem_hbm)->mem_hbm_dev, fmt "\n", ##arg)


#define MIG_DEFAULT_EXPIRE_SECS 1
#define MIG_MAX_RES		1

#define CALIB_CACHE_SZ		0x4000


enum ecc_type {
	HBM_ECC_PS0 = 0,
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


struct xocl_mem_hbm {
	void __iomem		*base;
	struct device		*mem_hbm_dev;
	enum ecc_type		type;
	struct xcl_mig_ecc	cache;
	struct xocl_mig_label	label;
	u32			ecc_enabled;
	char 			*calib_cache;
};

#define MIG_DEV2XDEV(d)	xocl_get_xdev(to_platform_device(d))

static const struct attribute_group mem_hbm_attrgroup;


static void ecc_reset(struct xocl_mem_hbm *mem_hbm)
{
	struct hbm_regs *h_regs = (struct hbm_regs *)mem_hbm->base;
	xdev_handle_t xdev = MIG_DEV2XDEV(mem_hbm->mem_hbm_dev);

	if (!MIG_PRIVILEGED(mem_hbm)) {
		MIG_INFO(mem_hbm, "Unable to reset from userpf");
		return;
	}

	if (!mem_hbm->ecc_enabled)
		return;

	if (!mem_hbm->base)
		return;

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

static void mem_hbm_ecc_get_prop(struct device *dev, enum ecc_prop kind, void *buf)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mem_hbm *mem_hbm = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mem_hbm->base;
	uint32_t err_1b, err_2b, ret = 0;

	if (MIG_PRIVILEGED(mem_hbm)) {

		if (!h_regs)
			return;

		switch (kind) {
		case MIG_ECC_ENABLE:
			ret = xocl_dr_reg_read32(xdev, &h_regs->cfg_ecc_en);
			*(uint32_t *)buf = ret;
			break;
		case MIG_ECC_STATUS:
			if (mem_hbm->type == HBM_ECC_PS0) {
				err_1b = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps0);
				err_2b = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps0);
				ret = (err_1b ? 1 : 0) << 1 | (err_2b ? 1 : 0);
			} else if (mem_hbm->type == HBM_ECC_PS1) {
				err_1b = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps1);
				err_2b = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps1);
				ret = (err_1b ? 1 : 0) << 1 | (err_2b ? 1 : 0);
			}
			*(uint32_t *)buf = ret;
			break;
		case MIG_ECC_CE_CNT:
			if (mem_hbm->type == HBM_ECC_PS0)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps0);
			else if (mem_hbm->type == HBM_ECC_PS1)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_1b_ps1);
			*(uint32_t *)buf = ret;
			break;
		case MIG_ECC_UE_CNT:
			if (mem_hbm->type == HBM_ECC_PS0)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps0);
			else if (mem_hbm->type == HBM_ECC_PS1)
				ret = xocl_dr_reg_read32(xdev, &h_regs->cnt_2b_ps1);

			*(uint32_t *)buf = ret;
			break;
		default:
			break;
		}
	} else {

		switch (kind) {
		case MIG_ECC_ENABLE:
			*(uint32_t *)buf = mem_hbm->cache.ecc_enabled;
			break;
		case MIG_ECC_STATUS:
			*(uint32_t *)buf = mem_hbm->cache.ecc_status;
			break;
		case MIG_ECC_CE_CNT:
			*(uint32_t *)buf = mem_hbm->cache.ecc_ce_cnt;
			break;
		case MIG_ECC_CE_FFA:
			*(uint64_t *)buf = mem_hbm->cache.ecc_ce_ffa;
			break;
		case MIG_ECC_UE_CNT:
			*(uint32_t *)buf = mem_hbm->cache.ecc_ue_cnt;
			break;
		case MIG_ECC_UE_FFA:
			*(uint64_t *)buf = mem_hbm->cache.ecc_ue_ffa;
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

	mem_hbm_ecc_get_prop(dev, MIG_ECC_UE_FFA, &val);
	return sprintf(buf, "0x%llx\n", val);
}
static DEVICE_ATTR_RO(ecc_ue_ffa);

static ssize_t ecc_ce_ffa_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint64_t addr;

	mem_hbm_ecc_get_prop(dev, MIG_ECC_CE_FFA, &addr);
	return sprintf(buf, "0x%llx\n", addr);
}
static DEVICE_ATTR_RO(ecc_ce_ffa);

static ssize_t ecc_ce_cnt_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t ret;

	mem_hbm_ecc_get_prop(dev, MIG_ECC_CE_CNT, &ret);
	return sprintf(buf, "%u\n", ret);
}
static DEVICE_ATTR_RO(ecc_ce_cnt);

static ssize_t ecc_ue_cnt_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t ret;

	mem_hbm_ecc_get_prop(dev, MIG_ECC_UE_CNT, &ret);
	return sprintf(buf, "%u\n", ret);
}
static DEVICE_ATTR_RO(ecc_ue_cnt);

static ssize_t ecc_status_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t status;

	mem_hbm_ecc_get_prop(dev, MIG_ECC_STATUS, &status);
	return sprintf(buf, "%u\n", status);
}
static DEVICE_ATTR_RO(ecc_status);


static ssize_t ecc_reset_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct xocl_mem_hbm *mem_hbm = MIG_DEV2MIG(dev);

	ecc_reset(mem_hbm);
	return count;
}
static DEVICE_ATTR_WO(ecc_reset);

static ssize_t ecc_enabled_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	uint32_t enable;

	mem_hbm_ecc_get_prop(dev, MIG_ECC_ENABLE, &enable);
	return sprintf(buf, "%u\n", enable);
}
static ssize_t ecc_enabled_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mem_hbm *mem_hbm = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mem_hbm->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mem_hbm))
		return count;

	if (!mem_hbm->ecc_enabled)
		return count;

	if (!mem_hbm->base)
		return -ENODEV;

	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

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

	return count;
}
static DEVICE_ATTR_RW(ecc_enabled);


static ssize_t ecc_clear_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mem_hbm *mem_hbm = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mem_hbm->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mem_hbm))
		return count;

	if (!mem_hbm->ecc_enabled)
		return count;
	if (!mem_hbm->base)
		return -ENODEV;

	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	xocl_dr_reg_write32(xdev, val, &h_regs->err_clr);
	return count;
}
static DEVICE_ATTR_WO(ecc_clear);

#ifdef MIG_DEBUG
static ssize_t ecc_inject_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mem_hbm *mem_hbm = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mem_hbm->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mem_hbm))
		return count;

	if (!mem_hbm->ecc_enabled)
		return count;

	if (!mem_hbm->base)
		return -ENODEV;


	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}

	if (mem_hbm->type == HBM_ECC_PS0)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_1b_ps0);
	else if (mem_hbm->type == HBM_ECC_PS1)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_1b_ps1);

	return count;
}
static DEVICE_ATTR_WO(ecc_inject);

static ssize_t ecc_inject_2bits_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	xdev_handle_t xdev = MIG_DEV2XDEV(dev);
	struct xocl_mem_hbm *mem_hbm = MIG_DEV2MIG(dev);
	struct hbm_regs *h_regs = (struct hbm_regs *)mem_hbm->base;
	uint32_t val;

	if (!MIG_PRIVILEGED(mem_hbm))
		return count;


	if (!mem_hbm->ecc_enabled)
		return count;

	if (!mem_hbm->base)
		return -ENODEV;

	if (kstrtoint(buf, 10, &val) || val > 1) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0|1] > ecc_enabled");
		return -EINVAL;
	}
	if (mem_hbm->type == HBM_ECC_PS0)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_2b_ps0);
	else if (mem_hbm->type == HBM_ECC_PS1)
		xocl_dr_reg_write32(xdev, val, &h_regs->err_gen_2b_ps1);

	return count;
}
static DEVICE_ATTR_WO(ecc_inject_2bits);

#endif


/* Standard sysfs entry for all dynamic subdevices. */
static ssize_t name_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_mem_hbm *mem_hbm = MIG_DEV2MIG(dev);

	return sprintf(buf, "%s\n", (char *)mem_hbm->label.tag);
}
static DEVICE_ATTR_RO(name);


static struct attribute *mem_hbm_attributes[] = {
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

static void mem_hbm_get_data(struct platform_device *pdev, void *buf, size_t entry_sz)
{
	struct xocl_mem_hbm *mem_hbm = platform_get_drvdata(pdev);
	struct xcl_mig_ecc mem_hbm_ecc = {0};

	if (!MIG_PRIVILEGED(mem_hbm))
		return;

	mem_hbm_ecc_get_prop(&pdev->dev, MIG_ECC_STATUS, &mem_hbm_ecc.ecc_status);
	mem_hbm_ecc_get_prop(&pdev->dev, MIG_ECC_ENABLE, &mem_hbm_ecc.ecc_enabled);
	mem_hbm_ecc_get_prop(&pdev->dev, MIG_ECC_CE_CNT, &mem_hbm_ecc.ecc_ce_cnt);
	mem_hbm_ecc_get_prop(&pdev->dev, MIG_ECC_UE_CNT, &mem_hbm_ecc.ecc_ue_cnt);
	mem_hbm_ecc_get_prop(&pdev->dev, MIG_ECC_CE_FFA, &mem_hbm_ecc.ecc_ce_ffa);
	mem_hbm_ecc_get_prop(&pdev->dev, MIG_ECC_UE_FFA, &mem_hbm_ecc.ecc_ue_ffa);
	mem_hbm_ecc.mem_type = mem_hbm->label.mem_type;
	mem_hbm_ecc.mem_idx = mem_hbm->label.mem_idx;

	memcpy(buf, &mem_hbm_ecc, entry_sz);
}

static void mem_hbm_set_data(struct platform_device *pdev, void *buf)
{
	struct xocl_mem_hbm *mem_hbm = platform_get_drvdata(pdev);

	if (!buf)
		return;

	if (MIG_PRIVILEGED(mem_hbm))
		return;

	memcpy(&mem_hbm->cache, buf, sizeof(struct xcl_mig_ecc));

}

static uint32_t mem_hbm_get_id(struct platform_device *pdev)
{
	struct xocl_mem_hbm *mem_hbm = platform_get_drvdata(pdev);
	uint32_t id = (mem_hbm->label.mem_type << 16) + mem_hbm->label.mem_idx;

	return id;
}

static struct xocl_mig_funcs mem_hbm_ops = {
	.get_data	= mem_hbm_get_data,
	.set_data	= mem_hbm_set_data,
	.get_id		= mem_hbm_get_id,
};

static const struct attribute_group mem_hbm_attrgroup = {
	.attrs = mem_hbm_attributes,
};

static void sysfs_destroy_mem_hbm(struct platform_device *pdev)
{
	struct xocl_mem_hbm *mem_hbm;

	mem_hbm = platform_get_drvdata(pdev);
	sysfs_remove_group(&pdev->dev.kobj, &mem_hbm_attrgroup);
}

static int sysfs_create_mem_hbm(struct platform_device *pdev)
{
	struct xocl_mem_hbm *mem_hbm;
	int err;

	mem_hbm = platform_get_drvdata(pdev);
	err = sysfs_create_group(&pdev->dev.kobj, &mem_hbm_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create pw group failed: 0x%x", err);
		return err;
	}

	return 0;
}

static int mem_hbm_probe(struct platform_device *pdev)
{
	struct xocl_mem_hbm *mem_hbm;
	struct resource *res;
	void *priv;
	int err, digit_len, idx, i;
	char *left_parentness = NULL, *right_parentness = NULL, temp[4];

	mem_hbm = devm_kzalloc(&pdev->dev, sizeof(*mem_hbm), GFP_KERNEL);
	if (!mem_hbm)
		return -ENOMEM;

	mem_hbm->mem_hbm_dev = &pdev->dev;

	priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (priv)
		memcpy(&mem_hbm->label, priv, sizeof(struct xocl_mig_label));

	if (!strncasecmp(mem_hbm->label.tag, "HBM", 3)) {
		left_parentness = strstr(mem_hbm->label.tag, "[");
		right_parentness = strstr(mem_hbm->label.tag, "]");
		digit_len = right_parentness-(1+left_parentness);
		strncpy(temp, left_parentness+1, digit_len);
		temp[digit_len] = '\0';

		if (kstrtoint(temp, 10, &idx) != 0)
			return -EINVAL;

		mem_hbm->label.mem_type = MEM_HBM;
		if (idx % 2)
			mem_hbm->type = HBM_ECC_PS1;
		else
			mem_hbm->type = HBM_ECC_PS0;
	}

	for (i = 0; i < MIG_MAX_RES; ++i) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			break;

		xocl_info(&pdev->dev, "MIG name: %s, IO start: 0x%llx, end: 0x%llx mig->type %d",
			(char *)mem_hbm->label.tag, res->start, res->end, mem_hbm->type);

		mem_hbm->base = ioremap_nocache(res->start, sizeof(struct hbm_regs));
		if (!mem_hbm->base) {
			xocl_err(&pdev->dev, "Map iomem failed");
			return -EIO;
		}
	}
	platform_set_drvdata(pdev, mem_hbm);

	err = sysfs_create_mem_hbm(pdev);
	if (err) {
		platform_set_drvdata(pdev, NULL);
		return err;
	}

	/* check MIG_ECC_ENABLE before reset*/
	mem_hbm_ecc_get_prop(&pdev->dev, MIG_ECC_ENABLE, &mem_hbm->ecc_enabled);


	ecc_reset(mem_hbm);

	return 0;
}

static int __mem_hbm_remove(struct platform_device *pdev)
{
	struct xocl_mem_hbm	*mem_hbm;

	mem_hbm = platform_get_drvdata(pdev);
	if (!mem_hbm) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	vfree(mem_hbm->calib_cache);
	mem_hbm->calib_cache = NULL;

	sysfs_destroy_mem_hbm(pdev);
	platform_set_drvdata(pdev, NULL);

	devm_kfree(&pdev->dev, mem_hbm);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void mem_hbm_remove(struct platform_device *pdev)
{
	__mem_hbm_remove(pdev);
}
#else
#define mem_hbm_remove __mem_hbm_remove
#endif

struct xocl_drv_private mem_hbm_priv = {
	.ops = &mem_hbm_ops,
};

struct platform_device_id mem_hbm_id_table[] = {
	{ XOCL_DEVNAME(XOCL_MIG_HBM), (kernel_ulong_t)&mem_hbm_priv },
	{ },
};

static struct platform_driver	mem_hbm_driver = {
	.probe		= mem_hbm_probe,
	.remove		= mem_hbm_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_MIG_HBM),
	},
	.id_table = mem_hbm_id_table,
};

int __init xocl_init_mem_hbm(void)
{
	return platform_driver_register(&mem_hbm_driver);
}

void xocl_fini_mem_hbm(void)
{
	platform_driver_unregister(&mem_hbm_driver);
}
