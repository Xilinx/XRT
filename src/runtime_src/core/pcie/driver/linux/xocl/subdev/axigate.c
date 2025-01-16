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
	char			ep_name[128];
	bool			sysfs_created;
};

static ssize_t name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct axi_gate *gate = platform_get_drvdata(to_platform_device(dev));

	return snprintf(buf, sizeof(gate->ep_name), "%s\n", gate->ep_name);
}
static DEVICE_ATTR_RO(name);

static ssize_t level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct axi_gate *gate = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%d\n", gate->level);
}
static DEVICE_ATTR_RO(level);

static struct attribute *axigate_attributes[] = {
	&dev_attr_name.attr,
	&dev_attr_level.attr,
	NULL
};

static const struct attribute_group axigate_attrgroup = {
	.attrs = axigate_attributes,
};

#define reg_rd(g, r)					\
	XOCL_READ_REG32(&((struct axi_gate_regs *)g->base)->r)
#define reg_wr(g, v, r)					\
	XOCL_WRITE_REG32(v, &((struct axi_gate_regs *)g->base)->r)

static int axigate_freeze(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct axi_gate *gate = platform_get_drvdata(pdev);
	u32 freeze = 0;

	mutex_lock(&gate->gate_lock);

	freeze = reg_rd(gate, iag_rd);
	if (!freeze)
		goto done; /* Already freeze */

	reg_wr(gate, 0, iag_wr);
	ndelay(500);
	(void) reg_rd(gate, iag_rd);

done:
	mutex_unlock(&gate->gate_lock);

	xocl_xdev_info(xdev, "freeze gate %s level %d",
			gate->ep_name, gate->level);
	return 0;
}

static int axigate_free(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct axi_gate *gate = platform_get_drvdata(pdev);
	u32 freeze;

	mutex_lock(&gate->gate_lock);

	freeze = reg_rd(gate, iag_rd);
	if (freeze)
		goto done; /* Already free */

	reg_wr(gate, 0x2, iag_wr);
	ndelay(500);
	(void) reg_rd(gate, iag_rd);
	reg_wr(gate, 0x3, iag_wr);
	ndelay(500);
	(void) reg_rd(gate, iag_rd);

done:
	mutex_unlock(&gate->gate_lock);
	xocl_xdev_info(xdev, "free gate %s level %d", gate->ep_name,
			gate->level);
	return 0;
}

static int axigate_reset(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct axi_gate *gate = platform_get_drvdata(pdev);

	mutex_lock(&gate->gate_lock);
	reg_wr(gate, 0x0, iag_wr);
	reg_wr(gate, 0x1, iag_wr);
	mutex_unlock(&gate->gate_lock);

	xocl_xdev_info(xdev, "ep_name %s level %d", gate->ep_name, gate->level);
	return 0;
}

static int axigate_status(struct platform_device *pdev, u32 *status)
{
	struct axi_gate *gate = platform_get_drvdata(pdev);

	mutex_lock(&gate->gate_lock);
	*status = reg_rd(gate, iag_rd);
	mutex_unlock(&gate->gate_lock);

	return 0;
}

static struct xocl_axigate_funcs axigate_ops = {
	.freeze = axigate_freeze,
	.free = axigate_free,
	.reset = axigate_reset,
	.get_status = axigate_status,
};

static int __axigate_remove(struct platform_device *pdev)
{
	struct axi_gate *gate;

	gate = platform_get_drvdata(pdev);
	if (!gate) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	if (gate->sysfs_created)
		sysfs_remove_group(&pdev->dev.kobj, &axigate_attrgroup);

	if (gate->base)
		iounmap(gate->base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, gate);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void axigate_remove(struct platform_device *pdev)
{
	__axigate_remove(pdev);
}
#else
#define axigate_remove __axigate_remove
#endif

static int axigate_probe(struct platform_device *pdev)
{
	struct axi_gate *gate;
	struct resource *res;
	xdev_handle_t xdev;
	char *p;
	int ret, len;

	gate = devm_kzalloc(&pdev->dev, sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return -ENOMEM;

	gate->pdev = pdev;

	platform_set_drvdata(pdev, gate);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "Empty resource 0");
		ret = -EINVAL;
		goto failed;
	}

	if (res->name && strlen(res->name)) {
		p = strchr(res->name, ' ');
		len = p ? ((ulong)p - (ulong)res->name) :
			sizeof(gate->ep_name) - 1;

		strncpy(gate->ep_name, res->name, len);
	}

	gate->base = ioremap_nocache(res->start,
		res->end - res->start + 1);
	if (!gate->base) {
		xocl_err(&pdev->dev, "map base iomem failed");
		ret = -EFAULT;
		goto failed;
	}

	gate->level = xocl_subdev_get_level(pdev);
	if (gate->level < 0) {
		xocl_err(&pdev->dev, "did not find level");
		ret = -EINVAL;
		goto failed;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &axigate_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create attr group failed: %d", ret);
		goto failed;
	}
	gate->sysfs_created = true;

	mutex_init(&gate->gate_lock);

	/* force closing gate */
	if (gate->level > XOCL_SUBDEV_LEVEL_BLD) {
		xdev = xocl_get_xdev(pdev);
		xocl_axigate_free(xdev, gate->level - 1);
		if (XDEV(xdev)->fdt_blob)
			xocl_fdt_unblock_ip(xdev, XDEV(xdev)->fdt_blob);
	}

	return 0;

failed:
	axigate_remove(pdev);
	return ret;
}

struct xocl_drv_private axigate_priv = {
	.ops = &axigate_ops,
};

struct platform_device_id axigate_id_table[] = {
	{ XOCL_DEVNAME(XOCL_AXIGATE), (kernel_ulong_t)&axigate_priv },
	{ },
};

static struct platform_driver	axi_gate_driver = {
	.probe		= axigate_probe,
	.remove		= axigate_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_AXIGATE),
	},
	.id_table = axigate_id_table,
};

int __init xocl_init_axigate(void)
{
	return platform_driver_register(&axi_gate_driver);
}

void xocl_fini_axigate(void)
{
	platform_driver_unregister(&axi_gate_driver);
}
