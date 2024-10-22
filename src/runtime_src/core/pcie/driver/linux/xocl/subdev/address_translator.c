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

#define	ADDR_TRANSLATOR_DEV2XDEV(d)	xocl_get_xdev(d)

struct trans_addr {
	u32 lo;
	u32 hi;
};


/*	ver: 0x0 RO 		Bit 5-0: Revision
 *				Bit 9-6: Minor version
 *				Bit 13-10: Major version
 *				Bit 31-14: all-zeroes (Reserved)
 *	cap: 0x4 RO 		Bit 7-0: MAX_APERTURE_SIZE (power of 2)
 *				Bit 15-8: APERTURE_SIZ (power of 2)
 *				Bit 24-16: MAX_NUM_APERTURES (1~256)
 *				Bit 31-25: all-zeroes (Reserved)
 *	entry_num: 0x8 RW	Bit 8-0: NUM_APERTURES
 *				Bit 31-9: all-zeroes (Reserved)
 *	base_addr: 0x10	RW	Bit 31-0: low 32 bit address
 *				Bit 63-32: high 32 bit address
 *	addr_range: 0x18 RW	Bit 7-0: SI_ADDR_RANGE (power of 2)
 *				Bit 31-8: all-zeroes (Reserved)
 *	page_table_phys: 0x800	Bit 31-0: low 32 bit address
 *			~0xFFC	Bit 63-32: high 32 bit address
 */
struct trans_regs {
	u32 ver;
	u32 cap;
	u32 entry_num;
	u32 unused;
	struct trans_addr base_addr;
	u32 addr_range;
	u8 padding[2020];
	struct trans_addr page_table_phys[256];
};

struct addr_translator {
	void __iomem		*base;
	struct device		*dev;
	struct mutex		lock;
	uint32_t		range;
	uint32_t		slot_num;
	uint64_t		slot_sz;
	uint32_t		num_max;
	uint64_t		phys_addrs[1];
};

static const struct attribute_group addr_translator_attrgroup;

static uint32_t addr_translator_get_entries_num(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	uint32_t num = 0;

	mutex_lock(&addr_translator->lock);
	num = addr_translator->num_max;
	mutex_unlock(&addr_translator->lock);
	return num;
}

static uint64_t addr_translator_get_host_mem_size(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	u64 num = 0, entry_sz = 0;

	mutex_lock(&addr_translator->lock);
	num = addr_translator->slot_num;
	entry_sz = addr_translator->slot_sz;
	mutex_unlock(&addr_translator->lock);
	return num*entry_sz;
}

static uint64_t addr_translator_get_range(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);	
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);
	u64 range = 0, num = 0, log = 0;

	mutex_lock(&addr_translator->lock);
	num = addr_translator->slot_num;
	if (num) {
		log = xocl_dr_reg_read32(xdev, &regs->addr_range);
		range = (1ULL)<<log;
	}
	mutex_unlock(&addr_translator->lock);
	return range;
}

static uint64_t addr_translator_get_base_addr(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);
	u64 hi = 0, lo = 0;

	mutex_lock(&addr_translator->lock);
	lo = xocl_dr_reg_read32(xdev, &regs->base_addr.lo);
	hi = xocl_dr_reg_read32(xdev, &regs->base_addr.hi);
	mutex_unlock(&addr_translator->lock);
	return (hi<<32)+lo;
}

static int addr_translator_set_page_table(struct platform_device *pdev, uint64_t *phys_addrs, uint64_t entry_sz, uint32_t num)
{
	int ret = 0, i = 0;
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);

	mutex_lock(&addr_translator->lock);

	if (num > addr_translator->num_max) {
		xocl_warn(&pdev->dev, "try to set page table with entry %d, cap %d ", num, addr_translator->num_max);
		num = addr_translator->num_max;
	}

	if (!is_power_of_2(num)) {
		ret = -EINVAL;
		goto done;
	}

	/* disable remapper first */
	xocl_dr_reg_write32(xdev, 0, &regs->entry_num);

	for ( ; i < num; ++i) {
		uint64_t addr = phys_addrs[i];
		if (!addr) {
			ret = -EINVAL;
			goto done;
		}
	}
	/* Save the reserve number for enable_remap*/
	addr_translator->slot_num = num;
	addr_translator->slot_sz = entry_sz;
	memcpy(addr_translator->phys_addrs, phys_addrs, num*sizeof(uint64_t));
done:
	mutex_unlock(&addr_translator->lock);
	return ret;
}
static int addr_translator_set_address(struct platform_device *pdev, uint64_t base_addr, uint64_t range)
{
	int ret = 0, i = 0;
	uint32_t num, range_in_log;
	uint64_t entry_sz, host_mem_size;
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;

	/* First, set regs->entry_num as 0 to initiate anddress translator 
	 * Program the base address or 0 to regs->base_addr
	 * Write the num back to enable new remap setting
	 */
	num = addr_translator->slot_num;
	entry_sz = addr_translator->slot_sz;
	host_mem_size = num*entry_sz;
	if (!host_mem_size)
		return ret;

	range = min(host_mem_size, range);
	range_in_log = ilog2(range);

	/* Calculate how many entries we have to program
	 * For example: host_mem_size 16G, slot_sz 1G, range 4G
	 * We only need 4 slots to cover 4G
	 */
	num = range/entry_sz;
	if (!is_power_of_2(num))
		return -EINVAL;

	xocl_dr_reg_write32(xdev, 0, &regs->entry_num);

	for (i = 0; i < num; ++i) {
		uint64_t addr = addr_translator->phys_addrs[i];
		if (!addr)
			return -EINVAL;

		xocl_dr_reg_write32(xdev, (addr & 0xFFFFFFFF), &regs->page_table_phys[i].lo);
		addr >>= 32;
		xocl_dr_reg_write32(xdev, addr, &regs->page_table_phys[i].hi);
	}

	/* disable remapper first */
	xocl_dr_reg_write32(xdev, range_in_log, &regs->addr_range);
	xocl_dr_reg_write32(xdev, base_addr & 0xFFFFFFFF, &regs->base_addr.lo);
	xocl_dr_reg_write32(xdev, (base_addr>>32) & 0xFFFFFFFF, &regs->base_addr.hi);
	/* reinitiate remapper */
	xocl_dr_reg_write32(xdev, num, &regs->entry_num);

	return ret;
}

static int addr_translator_enable_remap(struct platform_device *pdev, uint64_t base_addr, uint64_t range)
{
	int ret = 0;
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);

	mutex_lock(&addr_translator->lock);
	ret = addr_translator_set_address(pdev, base_addr, range);
	mutex_unlock(&addr_translator->lock);
	return ret;
}

static int addr_translator_disable_remap(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;

	mutex_lock(&addr_translator->lock);
	xocl_dr_reg_write32(xdev, 0, &regs->addr_range);
	xocl_dr_reg_write32(xdev, 0, &regs->base_addr.lo);
	xocl_dr_reg_write32(xdev, 0, &regs->base_addr.hi);
	xocl_dr_reg_write32(xdev, 0, &regs->entry_num);
	mutex_unlock(&addr_translator->lock);
	return 0;
}

static int addr_translator_clean(struct platform_device *pdev)
{
	struct addr_translator *addr_translator = platform_get_drvdata(pdev);
	xdev_handle_t xdev = ADDR_TRANSLATOR_DEV2XDEV(pdev);
	struct trans_regs *regs = (struct trans_regs *)addr_translator->base;

	mutex_lock(&addr_translator->lock);
	xocl_dr_reg_write32(xdev, 0, &regs->addr_range);
	xocl_dr_reg_write32(xdev, 0, &regs->base_addr.lo);
	xocl_dr_reg_write32(xdev, 0, &regs->base_addr.hi);
	xocl_dr_reg_write32(xdev, 0, &regs->entry_num);
	addr_translator->slot_num = 0;
	addr_translator->slot_sz = 0;
	memset(addr_translator->phys_addrs, 0, sizeof(uint64_t)*addr_translator->num_max);
	mutex_unlock(&addr_translator->lock);
	return 0;
}

static int addr_translator_offline(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &addr_translator_attrgroup);
	addr_translator_disable_remap(pdev);

	return 0;
}

static int addr_translator_online(struct platform_device *pdev)
{
	int ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &addr_translator_attrgroup);
	if (ret)
		xocl_err(&pdev->dev, "create addr_translator failed: %d", ret);

	return ret;
}

static struct xocl_addr_translator_funcs addr_translator_ops = {
	.online_cb = addr_translator_online,
	.offline_cb = addr_translator_offline,
	.get_entries_num = addr_translator_get_entries_num,
	.set_page_table = addr_translator_set_page_table,
	.get_range = addr_translator_get_range,
	.get_host_mem_size = addr_translator_get_host_mem_size,
	.enable_remap = addr_translator_enable_remap,
	.disable_remap = addr_translator_disable_remap,
	.clean = addr_translator_clean,
	.get_base_addr = addr_translator_get_base_addr,
};

static ssize_t num_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u32 num = 0;

	num = addr_translator_get_entries_num(to_platform_device(dev));

	return sprintf(buf, "%d\n", num);
}
static DEVICE_ATTR_RO(num);

static ssize_t addr_range_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u64 range = addr_translator_get_range(to_platform_device(dev));

	return sprintf(buf, "%lld\n", range);
}
static DEVICE_ATTR_RO(addr_range);

static ssize_t host_mem_size_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u64 range = addr_translator_get_host_mem_size(to_platform_device(dev));

	return sprintf(buf, "%lld\n", range);
}
static DEVICE_ATTR_RO(host_mem_size);

static ssize_t base_address_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	u64 addr;

	addr = addr_translator_get_base_addr(pdev);
	return sprintf(buf, "0x%llx\n", addr);
}
static DEVICE_ATTR_RO(base_address);

static struct attribute *addr_translator_attributes[] = {
	&dev_attr_num.attr,
	&dev_attr_base_address.attr,
	&dev_attr_addr_range.attr,
	&dev_attr_host_mem_size.attr,
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
	void __iomem	*base;
	uint32_t num = 0;
	struct trans_regs *regs;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		goto failed;

	base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}
	regs = (struct trans_regs *)base;
	num = (ioread32(&regs->cap)>>16 & 0x1ff);

	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx, max_slot_nums %d",
		res->start, res->end, num);

	addr_translator = devm_kzalloc(&pdev->dev, sizeof(*addr_translator)+sizeof(uint64_t)*num, GFP_KERNEL);
	if (!addr_translator)
		return -ENOMEM;

	addr_translator->dev = &pdev->dev;

	addr_translator->range = res->end - res->start + 1;

	addr_translator->base = base;

	addr_translator->num_max = num;

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


static int __addr_translator_remove(struct platform_device *pdev)
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void addr_translator_remove(struct platform_device *pdev)
{
	__addr_translator_remove(pdev);
}
#else
#define addr_translator_remove __addr_translator_remove
#endif

struct xocl_drv_private addr_translator_priv = {
	.ops = &addr_translator_ops,
};

struct platform_device_id addr_translator_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ADDR_TRANSLATOR), (kernel_ulong_t)&addr_translator_priv },
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
