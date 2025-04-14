/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@xilinx.com
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

/*
 * the PCIe Firewall IP to protect against host access to BARs which are not
 * available i.e. when the PLP is in reset, not yet configured or not
 * implemented.
 *
 * Details of the IP are available at:
 *     https://confluence.xilinx.com/display/XIP/PCIe+Firewall+IP+Development
 *
 * Following server warm/cold boot or hot reset, the PCIe Firewall will
 * automatically respond to accesses to BARs implemented in the PLP for compute
 * platforms i.e.
 *    PF0, BAR2
 *    PF1, BAR2
 *    PF1, BAR4
 * Once the PLP has been programmed and ep_pr_isolate_plp_00 has been released
 * from reset, then XRT should program the PCIe Firewall IP to clear the
 * appropriate bits in the Enable Response Register (0x8) to allow transactions
 * to propagate to the PLP.
 */

#include "../xocl_drv.h"

struct firewall_regs {
	u32		fwr_ip_ver;
	u32		fwr_cap;
	u32		fwr_en_resp;
	u32		fwr_pf0_resp_addr;
	u32		fwr_pf1_resp_addr;
	u32		fwr_pf2_resp_addr;
	u32		fwr_pf3_resp_addr;
} __attribute__((packed));

struct firewall {
	struct platform_device	*pdev;
	void		__iomem	*base;
	struct mutex		fw_lock;
};

#define reg_rd(g, r)					\
	XOCL_READ_REG32(&((struct firewall_regs *)g->base)->r)
#define reg_wr(g, v, r)					\
	XOCL_WRITE_REG32(v, &((struct firewall_regs *)g->base)->r)

#define UNBLOCK_BIT(pf, bar)					\
	(1U << ((pf) * 6 + (bar)))

static int firewall_unblock(struct platform_device *pdev, int pf, int bar)
{
	struct firewall *firewall = platform_get_drvdata(pdev);
	u32 val;

	mutex_lock(&firewall->fw_lock);
	val = reg_rd(firewall, fwr_en_resp);
	if (val & UNBLOCK_BIT(pf, bar)) {
		xocl_info(&pdev->dev, "unblock pf%d, bar%d", pf, bar);
		reg_wr(firewall, (~UNBLOCK_BIT(pf, bar)) & val, fwr_en_resp);
	}
	mutex_unlock(&firewall->fw_lock);

	return 0;
}

static struct xocl_pcie_firewall_funcs firewall_ops = {
	.unblock = firewall_unblock,
};

static int __firewall_remove(struct platform_device *pdev)
{
	struct firewall *firewall;
	void *hdl;

	firewall = platform_get_drvdata(pdev);
	if (!firewall) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	xocl_drvinst_release(firewall, &hdl);

	if (firewall->base)
		iounmap(firewall->base);

	mutex_destroy(&firewall->fw_lock);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void firewall_remove(struct platform_device *pdev)
{
	__firewall_remove(pdev);
}
#else
#define firewall_remove __firewall_remove
#endif

static int firewall_probe(struct platform_device *pdev)
{
	struct firewall *firewall;
	struct resource *res;
	int ret = 0;

	firewall = xocl_drvinst_alloc(&pdev->dev, sizeof(*firewall));
	if (!firewall) {
		xocl_err(&pdev->dev, "failed to alloc data");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, firewall);
	firewall->pdev = pdev;
	mutex_init(&firewall->fw_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		xocl_err(&pdev->dev, "failed to get resource");
		goto failed;
	}

	firewall->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!firewall->base) {
		ret = -EFAULT;
		xocl_err(&pdev->dev, "failed to map resource");
		goto failed;
	}

	return 0;
failed:
	firewall_remove(pdev);
	return ret;
}

static struct xocl_drv_private firewall_priv = {
	.ops = &firewall_ops,
};

static struct platform_device_id firewall_id_table[] = {
	{ XOCL_DEVNAME(XOCL_PCIE_FIREWALL), (kernel_ulong_t)&firewall_priv },
	{ },
};

static struct platform_driver   firewall_driver = {
	.probe		= firewall_probe,
	.remove		= firewall_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_PCIE_FIREWALL),
	},
	.id_table = firewall_id_table,
};

int __init xocl_init_pcie_firewall(void)
{
	return platform_driver_register(&firewall_driver);
}

void xocl_fini_pcie_firewall(void)
{
	platform_driver_unregister(&firewall_driver);
}
