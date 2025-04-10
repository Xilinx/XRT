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

struct iores {
	struct platform_device	*pdev;
	void		__iomem *base_addrs[IORES_MAX];
	resource_size_t		bar_off[IORES_MAX];
	int			bar_idx[IORES_MAX];
};

static struct xocl_iores_map res_map[] = {
	{ RESNAME_GATEPLP, IORES_GATEPLP },
	{ RESNAME_MEMCALIB, IORES_MEMCALIB },
	{ RESNAME_GATEULP, IORES_GATEULP },
	{ RESNAME_GAPPING, IORES_GAPPING},
	{ RESNAME_CLKFREQ_K1_K2, IORES_CLKFREQ_K1_K2},
	{ RESNAME_CLKFREQ_HBM, IORES_CLKFREQ_HBM },
	{ RESNAME_DDR4_RESET_GATE, IORES_DDR4_RESET_GATE},
	{ RESNAME_PCIEMON, IORES_PCIE_MON},
	{ RESNAME_ICAP_RESET, IORES_ICAP_RESET},
};

static int read32(struct platform_device *pdev, u32 id, u32 off, u32 *val)
{
	struct iores *iores = platform_get_drvdata(pdev);

	if (!iores->base_addrs[id])
		return -ENODEV;

	*val = XOCL_READ_REG32(iores->base_addrs[id] + off);

	return 0;
}

static int write32(struct platform_device *pdev, u32 id, u32 off, u32 val)
{
	struct iores *iores = platform_get_drvdata(pdev);

	if (!iores->base_addrs[id])
		return -ENODEV;

	XOCL_WRITE_REG32(val, iores->base_addrs[id] + off);

	return 0;
}

static void __iomem *get_base(struct platform_device *pdev, u32 id)
{
	struct iores *iores = platform_get_drvdata(pdev);

	return iores->base_addrs[id];
}

static uint64_t get_offset(struct platform_device *pdev, u32 id)
{
	struct iores *iores = platform_get_drvdata(pdev);

	return (uint64_t)iores->bar_off[id];
}


static struct xocl_iores_funcs iores_ops = {
	.read32 = read32,
	.write32 = write32,
	.get_base = get_base,
	.get_offset = get_offset,
};

static int __iores_remove(struct platform_device *pdev)
{
	struct iores *iores;
	int i;

	iores = platform_get_drvdata(pdev);
	if (!iores) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	for (i = 0; i < IORES_MAX; i++)
		if (iores->base_addrs[i])
			iounmap(iores->base_addrs[i]);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, iores);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void iores_remove(struct platform_device *pdev)
{
	__iores_remove(pdev);
}
#else
#define iores_remove __iores_remove
#endif

static int iores_probe(struct platform_device *pdev)
{
	struct iores *iores;
	struct resource *res;
	xdev_handle_t xdev;
	int i, id;
	int ret;

	iores = devm_kzalloc(&pdev->dev, sizeof(*iores), GFP_KERNEL);
	if (!iores)
		return -ENOMEM;

	iores->pdev = pdev;

	platform_set_drvdata(pdev, iores);

	for (i = 0, res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		res;
		res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		id = xocl_res_name2id(res_map, ARRAY_SIZE(res_map), res->name);
		if (id > 0) {
			iores->base_addrs[id] = ioremap_nocache(res->start,
					res->end - res->start + 1);
			if (!iores->base_addrs[id]) {
				xocl_err(&pdev->dev, "map basefailed %pR", res);
				iores_remove(pdev);
				return -EINVAL;
			}
			xdev = xocl_get_xdev(pdev);
			ret = xocl_ioaddr_to_baroff(xdev, res->start,
				&iores->bar_idx[id], &iores->bar_off[id]);
			if (ret) {
				xocl_err(&pdev->dev, "get bar off failed %pR", res);
				iores_remove(pdev);
				return -EINVAL;
			}
			xocl_info(&pdev->dev, "Resource %s%pR, id %d, mapped @%lx",
					res->name, res, id,
					(unsigned long)iores->base_addrs[id]);
		}
	}

	return 0;
}

struct xocl_drv_private iores_priv = {
	.ops = &iores_ops,
};

struct platform_device_id iores_id_table[] = {
	{ XOCL_DEVNAME(XOCL_IORES0), (kernel_ulong_t)&iores_priv },
	{ XOCL_DEVNAME(XOCL_IORES1), (kernel_ulong_t)&iores_priv },
	{ XOCL_DEVNAME(XOCL_IORES2), (kernel_ulong_t)&iores_priv },
	{ XOCL_DEVNAME(XOCL_IORES3), (kernel_ulong_t)&iores_priv },
	{ },
};

static struct platform_driver	iores_driver = {
	.probe		= iores_probe,
	.remove		= iores_remove,
	.driver		= {
		.name = XOCL_DEVNAME("iores"),
	},
	.id_table = iores_id_table,
};

int __init xocl_init_iores(void)
{
	return platform_driver_register(&iores_driver);
}

void xocl_fini_iores(void)
{
	platform_driver_unregister(&iores_driver);
}
