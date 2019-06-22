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

struct axi_gate_regs {
	u32		iag_wr;
	u32		iag_rvsd;
	u32		iag_rd;
} __attribute__((packed));

struct axi_gate {
	struct platform_device	*pdev;
	struct mutex		gate_lock;
	void		__iomem *base;
	int			level;
	bool			freeze;
};

#define reg_rd(g, r)					\
	XOCL_READ_REG32(&((struct axi_gate_regs *)g->base)->r)
#define reg_wr(g, v, r)					\
	XOCL_WRITE_REG32(v, &((struct axi_gate_regs *)g->base)->r)

static int axi_gate_freeze(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct axi_gate *gate = platform_get_drvdata(pdev);
	int i, ret = 0;

	mutex_lock(&gate->gate_lock);
	if (gate->freeze)
		goto failed;

	for (i = XOCL_SUBDEV_LEVEL_MAX- 1; i > gate->level; i--) {
		ret = xocl_subdev_offline_by_level(xdev, i);
		if (ret) {
			xocl_xdev_err(xdev, "failed offline level %d devs, %d",
				i, ret);
			goto failed;
		}
	}
	(void) reg_rd(gate, iag_rd);
	reg_wr(gate, 0, iag_wr);
	(void) reg_rd(gate, iag_rd);
	gate->freeze = true;

failed:
	mutex_unlock(&gate->gate_lock);
	return ret;
}

static int axi_gate_free(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct axi_gate *gate = platform_get_drvdata(pdev);
	int i, ret = 0;

	mutex_lock(&gate->gate_lock);
	if (!gate->freeze)
		goto failed;

	(void) reg_rd(gate, iag_rd);
	reg_wr(gate, 0x2, iag_wr);
	ndelay(500);
	(void) reg_rd(gate, iag_rd);
	reg_wr(gate, 0x3, iag_wr);
	ndelay(500);
	(void) reg_rd(gate, iag_rd);
	gate->freeze = false;

	for (i = gate->level + 1; i < XOCL_SUBDEV_LEVEL_MAX; i++) {
		ret = xocl_subdev_online_by_level(xdev, i);
		if (ret) {
			xocl_xdev_err(xdev, "failed online level %d devs, %d",
				i, ret);
			goto failed;
		}
	}

failed:
	mutex_unlock(&gate->gate_lock);
	return ret;
}

static struct xocl_axigate_funcs axigate_ops = {
	.freeze = axi_gate_freeze,
	.free = axi_gate_free,
};

struct xocl_drv_private axigate_priv = {
	.ops = &axigate_ops,
};

struct platform_device_id iores_id_table[] = {
	{ XOCL_DEVNAME(XOCL_AXIGATE), (kernel_ulong_t)&axigate_priv },
	{ },
};

static struct platform_driver	axi_gate_driver = {
	.probe		= iores_probe,
	.remove		= iores_remove,
	.driver		= {
		.name = XOCL_DEVNAME("iores"),
	},
	.id_table = iores_id_table,
};

int __init xocl_init_axigate(void)
{
	return platform_driver_register(&axi_gate_driver);
}

void xocl_fini_axigate(void)
{
	platform_driver_unregister(&axi_gate_driver);
}
