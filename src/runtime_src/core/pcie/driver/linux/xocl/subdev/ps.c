/*
 * Processor System manager for Alveo board.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors: Min.Ma@xilinx.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

#define MAX_RETRY       50
#define RETRY_INTERVAL  100       //ms

#define MAX_WAIT        12
#define WAIT_INTERVAL   5000	//ms

#define RESET_REG	0xC

#define ERT_READY_MASK  0x8
#define RES_DONE_MASK   0x4
#define RES_TYPE_MASK   0x3

#define SK_RESET	0x1
#define ERT_RESET	0x2
#define SYS_RESET	0x3

#define READ_REG32(ps, off)	\
	XOCL_READ_REG32(ps->base_addr + off)
#define WRITE_REG32(ps, val, off)       \
	XOCL_WRITE_REG32(val, ps->base_addr + off)

struct xocl_ps {
	struct platform_device	*pdev;
	void __iomem		*base_addr;
	struct mutex		 ps_lock;
};

/*
 * Processor system reset supports 3 type of reset
 * Host set reset type in the scratchpad register then wait PS
 * set reset done bit.
 */
static void ps_reset(struct platform_device *pdev, int type)
{
	struct xocl_ps *ps;
	u32 reg;
	int retry = 0;

	xocl_info(&pdev->dev, "Reset Processor System...");
	ps = platform_get_drvdata(pdev);
	if (!ps)
		return;

	mutex_lock(&ps->ps_lock);
	reg = READ_REG32(ps, RESET_REG);
	/* Set reset type in scratchpad register */
	switch(type) {
	case 1:
		xocl_info(&pdev->dev, "Soft Kernel reset...");
		reg = (reg & ~RES_TYPE_MASK) | SK_RESET;
		break;
	case 2:
		xocl_info(&pdev->dev, "ERT reset...");
		reg = (reg & ~RES_TYPE_MASK) | ERT_RESET;
		break;
	case 3:
		xocl_info(&pdev->dev, "SYS reset...");
		reg = (reg & ~RES_TYPE_MASK) | SYS_RESET;
		break;
	default:
		xocl_info(&pdev->dev, "Unknown reset type");
	}
	WRITE_REG32(ps, reg, RESET_REG);

	do {
		reg = READ_REG32(ps, RESET_REG);
		msleep(RETRY_INTERVAL);
	} while (retry++ < MAX_RETRY && !(reg & RES_DONE_MASK));

	if (retry >= MAX_RETRY) {
		xocl_err(&pdev->dev, "Reset time out");
		mutex_unlock(&ps->ps_lock);
		return;
	}

	/* Clear reset done bit */
	reg &= ~RES_DONE_MASK;
	WRITE_REG32(ps, reg, RESET_REG);
	mutex_unlock(&ps->ps_lock);
}

/* Wait Processor system goes into ready status */
static void ps_wait(struct platform_device *pdev)
{
	struct xocl_ps *ps;
	u32 reg;
	int retry = 0;

	xocl_info(&pdev->dev, "Wait Processor System ready...");
	ps = platform_get_drvdata(pdev);
	if (!ps)
		return;

	mutex_lock(&ps->ps_lock);
	do {
		reg = READ_REG32(ps, RESET_REG);
		msleep(WAIT_INTERVAL);
	} while(retry++ < MAX_WAIT && !(reg & ERT_READY_MASK));

	if (retry >= MAX_WAIT)
		xocl_err(&pdev->dev, "PS wait time out");

	mutex_unlock(&ps->ps_lock);
}

static struct xocl_ps_funcs ps_ops = {
	.reset		= ps_reset,
	.wait		= ps_wait,
};

static int ps_remove(struct platform_device *pdev)
{
	struct xocl_ps *ps;

	ps = platform_get_drvdata(pdev);
	if (!ps)
		return 0;

	if (ps->base_addr)
		iounmap(ps->base_addr);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, ps);

	mutex_destroy(&ps->ps_lock);

	return 0;
};

struct xocl_drv_private ps_priv = {
	.ops = &ps_ops,
};

static int ps_probe(struct platform_device *pdev)
{
	struct xocl_ps *ps;
	struct resource *res;
	int err;

	ps = devm_kzalloc(&pdev->dev, sizeof(*ps), GFP_KERNEL);
	if (!ps) {
		xocl_err(&pdev->dev, "out of memory");
		return -ENOMEM;
	}

	ps->pdev = pdev;
	platform_set_drvdata(pdev, ps);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		  res->start, res->end);
	ps->base_addr = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!ps->base_addr) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	mutex_init(&ps->ps_lock);

	return 0;
failed:
	ps_remove(pdev);
	return err;
};

struct platform_device_id ps_id_table[] = {
	{ XOCL_DEVNAME(XOCL_PS), (kernel_ulong_t)&ps_priv },
	{ },
};

static struct platform_driver ps_driver = {
	.probe		= ps_probe,
	.remove		= ps_remove,
	.driver		= {
		.name = "xocl_ps",
	},
	.id_table = ps_id_table,
};

int __init xocl_init_ps(void)
{
	return platform_driver_register(&ps_driver);
}

void xocl_fini_ps(void)
{
	platform_driver_unregister(&ps_driver);
}
