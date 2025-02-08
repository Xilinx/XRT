/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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

#define	GPIO_CFG_CTRL_CHANNEL	0x0
#define	GPIO_CFG_STA_CHANNEL	0x8

#define	SWITCH_TO_CU_INTR	0x1
#define	SWITCH_TO_ERT_INTR	~SWITCH_TO_CU_INTR

#define	WAKE_MB_UP		0x2
#define	CLEAR_MB_WAKEUP		~WAKE_MB_UP

#ifdef SCHED_VERBOSE
#define	CFGGPIO_ERR(cfg_gpio, fmt, arg...)	\
	xocl_err(cfg_gpio->dev, fmt "", ##arg)
#define	CFGGPIO_WARN(cfg_gpio, fmt, arg...)	\
	xocl_warn(cfg_gpio->dev, fmt "", ##arg)
#define	CFGGPIO_INFO(cfg_gpio, fmt, arg...)	\
	xocl_info(cfg_gpio->dev, fmt "", ##arg)
#define	CFGGPIO_DBG(cfg_gpio, fmt, arg...)	\
	xocl_info(cfg_gpio->dev, fmt "", ##arg)

#else
#define	CFGGPIO_ERR(cfg_gpio, fmt, arg...)	\
	xocl_err(cfg_gpio->dev, fmt "", ##arg)
#define	CFGGPIO_WARN(cfg_gpio, fmt, arg...)	\
	xocl_warn(cfg_gpio->dev, fmt "", ##arg)
#define	CFGGPIO_INFO(cfg_gpio, fmt, arg...)	\
	xocl_info(cfg_gpio->dev, fmt "", ##arg)
#define	CFGGPIO_DBG(cfg_gpio, fmt, arg...)
#endif

struct config_gpio {
	struct device		*dev;
	struct platform_device	*pdev;
	void __iomem		*cfg_gpio;
	struct mutex		lock;
};


static int32_t gpio_cfg(struct platform_device *pdev, enum ert_gpio_cfg type)
{
	struct config_gpio *cfg_gpio = platform_get_drvdata(pdev);
	int32_t ret = 0, val = 0;

	if (!cfg_gpio->cfg_gpio) {
		CFGGPIO_WARN(cfg_gpio, "%s ERT config gpio not found\n", __func__);
		return -ENODEV;
	}
	mutex_lock(&cfg_gpio->lock);
	val = ioread32(cfg_gpio->cfg_gpio);

	switch (type) {
	case INTR_TO_ERT:
		val &= SWITCH_TO_ERT_INTR;
		iowrite32(val, cfg_gpio->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		ret = xocl_intc_set_mode(xocl_get_xdev(pdev), ERT_INTR);
		break;
	case INTR_TO_CU:
		val |= SWITCH_TO_CU_INTR;
		iowrite32(val, cfg_gpio->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		ret = xocl_intc_set_mode(xocl_get_xdev(pdev), CU_INTR);
		break;
	case MB_WAKEUP:
		val |= WAKE_MB_UP;
		iowrite32(val, cfg_gpio->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case MB_SLEEP:
		val &= CLEAR_MB_WAKEUP;
		iowrite32(val, cfg_gpio->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);

		ret = ioread32(cfg_gpio->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		while (!ret)
			ret = ioread32(cfg_gpio->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		break;
	case MB_WAKEUP_CLR:
		val &= CLEAR_MB_WAKEUP;
		iowrite32(val, cfg_gpio->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case MB_STATUS:
		ret = ioread32(cfg_gpio->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		break;
	default:
		break;
	}
	mutex_unlock(&cfg_gpio->lock);
	return ret;
}
static int __config_gpio_remove(struct platform_device *pdev)
{
	struct config_gpio *config_gpio;
	void *hdl;

	config_gpio = platform_get_drvdata(pdev);
	if (!config_gpio) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_drvinst_release(config_gpio, &hdl);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void config_gpio_remove(struct platform_device *pdev)
{
	__config_gpio_remove(pdev);
}
#else
#define config_gpio_remove __config_gpio_remove
#endif

static int config_gpio_probe(struct platform_device *pdev)
{
	struct config_gpio *config_gpio;
	struct resource *res;
	int err = 0;

	config_gpio = xocl_drvinst_alloc(&pdev->dev, sizeof(struct config_gpio));
	if (!config_gpio)
		return -ENOMEM;

	config_gpio->dev = &pdev->dev;
	config_gpio->pdev = pdev;

	platform_set_drvdata(pdev, config_gpio);
	mutex_init(&config_gpio->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		xocl_info(&pdev->dev, "CFG GPIO start: 0x%llx, end: 0x%llx",
			res->start, res->end);

		config_gpio->cfg_gpio = ioremap_nocache(res->start, res->end - res->start + 1);
		if (!config_gpio->cfg_gpio) {
			err = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto done;
		}
	}

done:
	if (err) {
		config_gpio_remove(pdev);
		return err;
	}
	return 0;
}

static struct xocl_config_gpio_funcs config_gpio_ops = {
	.gpio_cfg = gpio_cfg,
};

struct xocl_drv_private config_gpio_priv = {
	.ops = &config_gpio_ops,
	.dev = -1,
};

struct platform_device_id config_gpio_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CFG_GPIO), (kernel_ulong_t)&config_gpio_priv },
	{ },
};

static struct platform_driver	config_gpio_driver = {
	.probe		= config_gpio_probe,
	.remove		= config_gpio_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CFG_GPIO),
	},
	.id_table = config_gpio_id_table,
};

int __init xocl_init_config_gpio(void)
{
	return platform_driver_register(&config_gpio_driver);
}

void xocl_fini_config_gpio(void)
{
	platform_driver_unregister(&config_gpio_driver);
}
