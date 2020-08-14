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

#include "../xocl_drv.h"

/* ERT gpio config has two channels 
 * CHANNEL 0 is control channel :
 * BIT 0: 0x0 Selects interrupts from embedded scheduler HW block
 * 	  0x1 Selects interrupts from the CU INTCs
 * BIT 2-1: TBD
 *
 * CHANNEL 1 is status channel :
 * BIT 0: check microblazer status
 */

#define GPIO_CFG_CTRL_CHANNEL	0x0
#define GPIO_CFG_STA_CHANNEL	0x8

#define SWITCH_TO_CU_INTR	0x1
#define SWITCH_TO_ERT_INTR	~SWITCH_TO_CU_INTR

#define FORCE_MB_SLEEP		0x2
#define WAKE_MB_UP		~FORCE_MB_SLEEP


#ifdef SCHED_VERBOSE
#define	ERTUSER_ERR(ert_30, fmt, arg...)	\
	xocl_err(ert_30->dev, fmt "", ##arg)
#define	ERTUSER_INFO(ert_30, fmt, arg...)	\
	xocl_info(ert_30->dev, fmt "", ##arg)	
#define	ERTUSER_DBG(ert_30, fmt, arg...)	\
	xocl_info(ert_30->dev, fmt "", ##arg)
#else
#define	ERTUSER_ERR(ert_30, fmt, arg...)	\
	xocl_err(ert_30->dev, fmt "", ##arg)
#define	ERTUSER_INFO(ert_30, fmt, arg...)	\
	xocl_info(ert_30->dev, fmt "", ##arg)	
#define	ERTUSER_DBG(ert_30, fmt, arg...)
#endif


#define sched_debug_packet(packet, size)				\
({									\
	int i;								\
	u32 *data = (u32 *)packet;					\
	for (i = 0; i < size; ++i)					    \
		DRM_INFO("packet(0x%p) execbuf[%d] = 0x%x\n", data, i, data[i]); \
})

struct xocl_ert_30 {
	struct device		*dev;
	struct platform_device	*pdev;
	void __iomem		*cfg_gpio;
	struct mutex 		lock;
	struct xocl_ert_sched_privdata ert_cfg_priv;
};

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	//struct xocl_ert_30 *ert_30 = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "ert_30");
}

static DEVICE_ATTR_RO(name);

static struct attribute *ert_30_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};

static struct attribute_group ert_30_attr_group = {
	.attrs = ert_30_attrs,
};

static uint32_t ert_30_gpio_cfg(struct platform_device *pdev, enum ert_gpio_cfg type)
{
	struct xocl_ert_30 *ert_30 = platform_get_drvdata(pdev);
	uint32_t ret = 0, val = 0;

	val = ioread32(ert_30->cfg_gpio);

	switch (type) {
	case INTR_TO_ERT:
		val &= SWITCH_TO_ERT_INTR;
		iowrite32(val, ert_30->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case INTR_TO_CU:
		val |= SWITCH_TO_CU_INTR;
		iowrite32(val, ert_30->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case MB_WAKEUP:
		val &= WAKE_MB_UP;
		iowrite32(val, ert_30->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case MB_SLEEP:
		val |= FORCE_MB_SLEEP;
		iowrite32(val, ert_30->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case MB_STATUS:
		ret = ioread32(ert_30->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		break;
	default:
		break;
	}

	return ret;
}

static struct xocl_ert_30_funcs ert_30_ops = {
	.gpio_cfg = ert_30_gpio_cfg,
};

static int ert_30_remove(struct platform_device *pdev)
{
	struct xocl_ert_30 *ert_30;
	void *hdl;

	ert_30 = platform_get_drvdata(pdev);
	if (!ert_30) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &ert_30_attr_group);

	xocl_drvinst_release(ert_30, &hdl);

	if (ert_30->cfg_gpio)
		iounmap(ert_30->cfg_gpio);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int ert_30_probe(struct platform_device *pdev)
{
	struct xocl_ert_30 *ert_30;
	struct resource *res;
	int err = 0;
	struct xocl_ert_sched_privdata *priv = NULL;

	ert_30 = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_ert_30));
	if (!ert_30)
		return -ENOMEM;

	ert_30->dev = &pdev->dev;
	ert_30->pdev = pdev;

	platform_set_drvdata(pdev, ert_30);
	mutex_init(&ert_30->lock);

	if (XOCL_GET_SUBDEV_PRIV(&pdev->dev)) {
		priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
		memcpy(&ert_30->ert_cfg_priv, priv, sizeof(*priv));
	} else {
		xocl_err(&pdev->dev, "did not get private data");
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "did not get memory");
		err = -ENOMEM;
		goto done;
	}

	xocl_info(&pdev->dev, "CFG GPIO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	ert_30->cfg_gpio = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!ert_30->cfg_gpio) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &ert_30_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create ert_30 sysfs attrs failed: %d", err);
	}

done:
	if (err) {
		ert_30_remove(pdev);
		return err;
	}
	return 0;
}

struct xocl_drv_private ert_30_priv = {
	.ops = &ert_30_ops,
	.dev = -1,
};

struct platform_device_id ert_30_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ERT_30), (kernel_ulong_t)&ert_30_priv },
	{ },
};

static struct platform_driver	ert_30_driver = {
	.probe		= ert_30_probe,
	.remove		= ert_30_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ERT_30),
	},
	.id_table = ert_30_id_table,
};

int __init xocl_init_ert_30(void)
{
	return platform_driver_register(&ert_30_driver);
}

void xocl_fini_ert_30(void)
{
	platform_driver_unregister(&ert_30_driver);
}
