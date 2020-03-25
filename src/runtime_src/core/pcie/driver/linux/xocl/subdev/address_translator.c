/*
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

#include <linux/vmalloc.h>
#include <linux/log2.h>
#include "../xocl_drv.h"
#include "xocl_ioctl.h"


struct trans_addr {
	u32 lo;
	u32 hi;
};

#define ADDR_TRANSLATOR_VER			0x0
#define ADDR_TRANSLATOR_CAP			0x4
#define ADDR_TRANSLATOR_VALID_PAGE			0x8
#define ADDR_TRANSLATOR_ENTRY_NUM			0xC
#define ADDR_TRANSLATOR_MIN_ADDR_LO		0x10
#define ADDR_TRANSLATOR_MIN_ADDR_HI		0x14
#define ADDR_TRANSLATOR_MAX_ADDR_LO		0x18
#define ADDR_TRANSLATOR_MAX_ADDR_HI		0x1C
#define ADDR_TRANSLATOR_ERR_STA			0x20
#define ADDR_TRANSLATOR_FIRST_FAIL_ADDR_LO		0x24
#define ADDR_TRANSLATOR_FIRST_FAIL_ADDR_HI		0x28
#define ADDR_TRANSLATOR_LATEST_FAIL_ADDR_LO	0x2C
#define ADDR_TRANSLATOR_LATEST_FAIL_ADDR_HI	0x30

struct trans_regs {
	u32 ver;
	u32 cap;
	u32 valid_page;
	u32 entries_num;
	struct trans_addr min_addr;
	struct trans_addr max_addr;
	u32 err_status;
	struct trans_addr first_fail_addr;
	struct trans_addr latest_fail_addr;
	u8 padding[204];
	struct trans_addr page_table_phys[256];
};

#define	ADDR_TRANSLATOR_DEV2XDEV(d)	xocl_get_xdev(d)

struct addr_translator {
	void __iomem		*base;
	struct device		*dev;
	struct mutex		lock;
	bool			online;
};

static uint32_t addr_translator_get_entries_num(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);
	uint32_t num = 0;

	mutex_lock(&addr_translator->lock);

	num = (xocl_dr_reg_read32(xdev, &regs->cap)>>6 & 0x1f) << 8;

	mutex_unlock(&addr_translator->lock);
	return num;
}


static int addr_translator_set_page_table(struct platform_device *pdev, uint64_t *phys_addrs, uint64_t base_addr, uint64_t entry_sz, uint32_t num)
{
	int ret = 0, i = 0;
	uint32_t num_max;
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);
	uint64_t min_addr = base_addr, max_addr = base_addr + num * entry_sz;

	mutex_lock(&addr_translator->lock);

	num_max = (xocl_dr_reg_read32(xdev, &regs->cap)>>6 & 0x1f) << 8;

	if (num > num_max) {
		ret = -EINVAL;
		goto done;
	}


	if (!is_power_of_2(num)) {
		ret = -EINVAL;
		goto done;
	}

	for ( ; i < num; ++i) {
		uint64_t addr = phys_addrs[i];

		if (!addr) {
			ret = -EINVAL;
			goto done;
		}

		xocl_dr_reg_write32(xdev, (addr & 0xFFFFFFFF), &regs->page_table_phys[i].lo);
		addr >>= 32;
		xocl_dr_reg_write32(xdev, addr, &regs->page_table_phys[i].hi);

	}

	xocl_dr_reg_write32(xdev, num, &regs->entries_num);


	xocl_dr_reg_write32(xdev, min_addr & 0xFFFFFFFF, &regs->min_addr.lo);
	xocl_dr_reg_write32(xdev, (min_addr>>32) & 0xFFFFFFFF, &regs->min_addr.hi);

	xocl_dr_reg_write32(xdev, max_addr & 0xFFFFFFFF, &regs->max_addr.lo);
	xocl_dr_reg_write32(xdev, (max_addr>>32) & 0xFFFFFFFF, &regs->max_addr.hi);

done:
	mutex_unlock(&addr_translator->lock);
	return num;
}


static struct xocl_addr_translator_funcs addr_translator_ops = {
	.get_entries_num = addr_translator_get_entries_num,
	.set_page_table = addr_translator_set_page_table,
};

static ssize_t num_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u32 num = 0;

	num = addr_translator_get_entries_num(to_platform_device(dev));

	return sprintf(buf, "0x%x\n", num);
}
static DEVICE_ATTR_RO(num);

static struct attribute *addr_translator_attributes[] = {
	&dev_attr_num.attr,
	NULL
};

static const struct attribute_group addr_translator_attrgroup = {
	.attrs = addr_translator_attributes,
};

static int addr_translator_probe(struct platform_device *pdev)
{
	struct addr_translator *addr_translator;
	struct resource *res;
	int err = 0;

	addr_translator = devm_kzalloc(&pdev->dev, sizeof(*addr_translator), GFP_KERNEL);
	if (!addr_translator)
		return -ENOMEM;

	addr_translator->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		goto failed;

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	addr_translator->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!addr_translator->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}
	mutex_init(&addr_translator->lock);
	platform_set_drvdata(pdev, addr_translator);

	err = sysfs_create_group(&pdev->dev.kobj, &addr_translator_attrgroup);
	if (err)
		goto create_addr_translator_failed;

	return 0;

create_addr_translator_failed:
	platform_set_drvdata(pdev, NULL);
failed:
	return err;
}


static int addr_translator_remove(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);

	if (!addr_translator) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &addr_translator_attrgroup);

	if (addr_translator->base)
		iounmap(addr_translator->base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, addr_translator);

	return 0;
}

struct xocl_drv_private addr_translator_priv = {
	.ops = &addr_translator_ops,
};

struct platform_device_id addr_translator_id_table[] = {
	{ XOCL_DEVNAME(XOCL_SRSR), (kernel_ulong_t)&addr_translator_priv },
	{ },
};

static struct platform_driver	addr_translator_driver = {
	.probe		= addr_translator_probe,
	.remove		= addr_translator_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ADDR_TRANSLATOR),
	},
	.id_table = addr_translator_id_table,
};

int __init xocl_init_addr_translator(void)
{
	return platform_driver_register(&addr_translator_driver);
}

void xocl_fini_addr_translator(void)
{
	platform_driver_unregister(&addr_translator_driver);
}
