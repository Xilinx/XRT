/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
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

#define	PMC_ERR(pmc, fmt, arg...)	\
	xocl_err(&(pmc)->pmc_pdev->dev, fmt "\n", ##arg)
#define	PMC_WARN(pmc, fmt, arg...)	\
	xocl_warn(&(pmc)->pmc_pdev->dev, fmt "\n", ##arg)
#define	PMC_INFO(pmc, fmt, arg...)	\
	xocl_info(&(pmc)->pmc_pdev->dev, fmt "\n", ##arg)
#define	PMC_DBG(pmc, fmt, arg...)	\
	xocl_dbg(&(pmc)->pmc_pdev->dev, fmt "\n", ##arg)

#define	PMC_ERR1_STATUS_MASK	(1 << 24)
#define	PMC_ERR_OUT1_EN_MASK	(1 << 24)
#define	PMC_POR1_EN_MASK	(1 << 24)
#define	PMC_REG_ERR_OUT1_MASK	0x20
#define	PMC_REG_ERR_OUT1_EN	0x24
#define	PMC_REG_POR1_MASK	0x40
#define	PMC_REG_POR1_EN		0x44

#define	PL_TO_PMC_ERROR_SIGNAL_PATH_MASK	(1 << 0)

enum {
	PMC_IORES_INTR = 0,
	PMC_IORES_MUX,
	PMC_IORES_MAX,
};

struct xocl_iores_map pmc_res_map[] = {
	{ RESNAME_PMC_INTR, PMC_IORES_INTR },
	{ RESNAME_PMC_MUX, PMC_IORES_MUX },
};

struct pmc {
	struct platform_device  *pmc_pdev;
	void __iomem 		*pmc_base_address[PMC_IORES_MAX]; 
	struct mutex 		pmc_lock;
};

static int pmc_enable_reset(struct platform_device *pdev)
{
	struct pmc *pmc = platform_get_drvdata(pdev);
	void __iomem *pmc_intr, *pmc_mux;
	u32 val;
	int rc = 0;

	mutex_lock(&pmc->pmc_lock);
	/*
	 * The pmc_intr register is a temporary workaround in driver, it will
	 * be handled in CIPs and then removed from metadata.
	 */
	pmc_intr = pmc->pmc_base_address[PMC_IORES_INTR];
	if (pmc_intr != NULL) {
		val = XOCL_READ_REG32(pmc_intr);
		if (val & PMC_ERR1_STATUS_MASK) {
			val &= ~PMC_ERR1_STATUS_MASK;
			XOCL_WRITE_REG32(val, pmc_intr); 
		}

		XOCL_WRITE_REG32(PMC_ERR_OUT1_EN_MASK, pmc_intr + PMC_REG_ERR_OUT1_EN);
		val = XOCL_READ_REG32(pmc_intr + PMC_REG_ERR_OUT1_MASK);
		if (val & PMC_ERR_OUT1_EN_MASK) {
			PMC_ERR(pmc, "mask 0x%x for PMC_REG_ERR_OUT1_MASK 0x%x "
			    "should be 0.\n", PMC_ERR_OUT1_EN_MASK, val);
			rc = -EIO;
			goto done;
		}

		XOCL_WRITE_REG32(PMC_POR1_EN_MASK, pmc_intr + PMC_REG_POR1_EN);
		val = XOCL_READ_REG32(pmc_intr + PMC_REG_POR1_MASK);
		if (val & PMC_POR1_EN_MASK) {
			PMC_ERR(pmc, "mask 0x%x for PMC_REG_POR1_MASK 0x%x "
			    "should be 0.\n", PMC_POR1_EN_MASK, val);
			rc = -EIO;
			goto done;
		}
	}

	pmc_mux = pmc->pmc_base_address[PMC_IORES_MUX];
	if (pmc_mux == NULL) {
		PMC_ERR(pmc, "%s failed, %s is missing in metadata.\n",
		    __func__, RESNAME_PMC_MUX);
		rc = -EINVAL;
		goto done;
	}

	val = XOCL_READ_REG32(pmc_mux);
	val |= PL_TO_PMC_ERROR_SIGNAL_PATH_MASK;
	XOCL_WRITE_REG32(val, pmc_mux); 

	PMC_INFO(pmc, "mux control is 0x%x.\n", XOCL_READ_REG32(pmc_mux));
done:
	mutex_unlock(&pmc->pmc_lock);
	return rc;
}

static ssize_t mux_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct pmc *pmc = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&pmc->pmc_lock);
	if (pmc->pmc_base_address[PMC_IORES_MUX]) {
		cnt += sprintf(buf + cnt, "0x%x\n",
		    XOCL_READ_REG32(pmc->pmc_base_address[PMC_IORES_MUX]));
	}

	mutex_unlock(&pmc->pmc_lock);
	return cnt;
}
static DEVICE_ATTR_RO(mux_control);

static struct attribute *pmc_attrs[] = {
	&dev_attr_mux_control.attr,
	NULL,
};

static struct attribute_group pmc_attr_group = {
	.attrs = pmc_attrs,
};

static struct xocl_pmc_funcs pmc_ops = {
	.enable_reset = pmc_enable_reset,
};

static int __pmc_remove(struct platform_device *pdev)
{
	struct pmc *pmc;

	pmc = platform_get_drvdata(pdev);
	if (!pmc) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &pmc_attr_group);
	mutex_destroy(&pmc->pmc_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, pmc);

	PMC_INFO(pmc, "successfully removed pmc subdev");
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void pmc_remove(struct platform_device *pdev)
{
	__pmc_remove(pdev);
}
#else
#define pmc_remove __pmc_remove
#endif

static int pmc_probe(struct platform_device *pdev)
{
	struct pmc *pmc = NULL;
	struct resource *res;
	int ret, i, id;

	pmc = devm_kzalloc(&pdev->dev, sizeof(*pmc), GFP_KERNEL);
	if (!pmc)
		return -ENOMEM;

	platform_set_drvdata(pdev, pmc);
	pmc->pmc_pdev = pdev;
	mutex_init(&pmc->pmc_lock);

	for (i = 0, res = platform_get_resource(pdev, IORESOURCE_MEM, 0); res;
		res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {

		id = xocl_res_name2id(pmc_res_map, ARRAY_SIZE(pmc_res_map),
			res->name);

		if (id >= 0) {
			pmc->pmc_base_address[id] =
				ioremap_nocache(res->start,
				res->end - res->start + 1);
			if (!pmc->pmc_base_address[id]) {
				PMC_ERR(pmc, "map base %pR failed", res);
				ret = -EINVAL;
				goto failed;
			} else {
				PMC_INFO(pmc, "res[%d] %s mapped @ %lx",
				    i, res->name,
				    (unsigned long)pmc->pmc_base_address[id]);
			}
		}
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &pmc_attr_group);
	if (ret) {
		PMC_ERR(pmc, "create pmc attrs failed: %d", ret);
		goto failed;
	}

	PMC_INFO(pmc, "successfully initialized pmc subdev");
	return 0;

failed:
	(void) pmc_remove(pdev);
	return ret;
}

struct xocl_drv_private pmc_priv = {
	.ops = &pmc_ops,
};

struct platform_device_id pmc_id_table[] = {
	{ XOCL_DEVNAME(XOCL_PMC), (kernel_ulong_t)&pmc_priv },
	{ },
};

static struct platform_driver pmc_driver = {
	.probe		= pmc_probe,
	.remove		= pmc_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_PMC),
	},
	.id_table = pmc_id_table,
};

int __init xocl_init_pmc(void)
{
	return platform_driver_register(&pmc_driver);
}

void xocl_fini_pmc(void)
{
	platform_driver_unregister(&pmc_driver);
}
