/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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

#include <linux/pci.h>
#include <linux/platform_device.h>
#include "xclfeatures.h"
#include "../xocl_drv.h"

#define	MAGIC_NUM	0x786e6c78
struct feature_rom {
	void __iomem		*base;
	struct platform_device	*pdev;

	struct FeatureRomHeader	header;
	bool			unified;
	bool			mb_mgmt_enabled;
	bool			mb_sche_enabled;
	bool			are_dev;
	bool			aws_dev;
	bool			runtime_clk_scale_en;
};

/* TODO: replace this with .dtb driven */
static int xocl_init_rom_v5(struct feature_rom *rom5)
{
	struct FeatureRomHeader *header = &rom5->header;

	strcpy(header->VBNVName, "xilinx_u200_dynamic_201910_1");
	header->DDRChannelCount = 4;
	header->DDRChannelSize = 16;
	header->FeatureBitMap = UNIFIED_PLATFORM;

	return 0;
}

static ssize_t VBNV_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%s\n", rom->header.VBNVName);
}
static DEVICE_ATTR_RO(VBNV);

static ssize_t dr_base_addr_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	//TODO: Fix: DRBaseAddress no longer required in feature rom
	if(rom->header.MajorVersion >= 10)
		return sprintf(buf, "%llu\n", rom->header.DRBaseAddress);
	else
		return sprintf(buf, "%u\n", 0);
}
static DEVICE_ATTR_RO(dr_base_addr);

static ssize_t ddr_bank_count_max_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%d\n", rom->header.DDRChannelCount);
}
static DEVICE_ATTR_RO(ddr_bank_count_max);

static ssize_t ddr_bank_size_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%d\n", rom->header.DDRChannelSize);
}
static DEVICE_ATTR_RO(ddr_bank_size);

static ssize_t timestamp_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%llu\n", rom->header.TimeSinceEpoch);
}
static DEVICE_ATTR_RO(timestamp);

static ssize_t FPGA_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%s\n", rom->header.FPGAPartName);
}
static DEVICE_ATTR_RO(FPGA);

static struct attribute *rom_attrs[] = {
	&dev_attr_VBNV.attr,
	&dev_attr_dr_base_addr.attr,
	&dev_attr_ddr_bank_count_max.attr,
	&dev_attr_ddr_bank_size.attr,
	&dev_attr_timestamp.attr,
	&dev_attr_FPGA.attr,
	NULL,
};

static ssize_t raw_show(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	if (off >= sizeof(rom->header))
		return 0;

	if (off + count >= sizeof(rom->header))
		count = sizeof(rom->header) - off;

	memcpy(buf, &rom->header, count);

	return count;
};

static struct bin_attribute raw_attr = {
	.attr = {
		.name = "raw",
		.mode = 0400
	},
	.read = raw_show,
	.write = NULL,
	.size = 0
};

static struct bin_attribute  *rom_bin_attrs[] = {
	&raw_attr,
	NULL,
};

static struct attribute_group rom_attr_group = {
	.attrs = rom_attrs,
	.bin_attrs = rom_bin_attrs,
};

static bool is_unified(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->unified;
}

static bool mb_mgmt_on(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->mb_mgmt_enabled;
}

static bool mb_sched_on(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->mb_sche_enabled && !XOCL_DSA_MB_SCHE_OFF(xocl_get_xdev(pdev));
}

static bool runtime_clk_scale_on(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->runtime_clk_scale_en;
}

static uint32_t* get_cdma_base_addresses(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return (!XOCL_DSA_NO_KDMA(xocl_get_xdev(pdev)) &&
		(rom->header.FeatureBitMap & CDMA)) ?
		rom->header.CDMABaseAddress : 0;
}

static u16 get_ddr_channel_count(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->header.DDRChannelCount;
}

static u64 get_ddr_channel_size(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->header.DDRChannelSize;
}

static u64 get_timestamp(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->header.TimeSinceEpoch;
}

static bool is_are(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->are_dev;
}

static bool is_aws(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->aws_dev;
}

static bool verify_timestamp(struct platform_device *pdev, u64 timestamp)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	xocl_info(&pdev->dev, "Shell timestamp: 0x%llx",
		rom->header.TimeSinceEpoch);
	xocl_info(&pdev->dev, "Verify timestamp: 0x%llx", timestamp);
	return (rom->header.TimeSinceEpoch == timestamp);
}

static int get_raw_header(struct platform_device *pdev, void *header)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	memcpy(header, &rom->header, sizeof (rom->header));

	return 0;
}

static struct xocl_rom_funcs rom_ops = {
	.is_unified = is_unified,
	.mb_mgmt_on = mb_mgmt_on,
	.mb_sched_on = mb_sched_on,
	.cdma_addr = get_cdma_base_addresses,
	.get_ddr_channel_count = get_ddr_channel_count,
	.get_ddr_channel_size = get_ddr_channel_size,
	.is_are = is_are,
	.is_aws = is_aws,
	.verify_timestamp = verify_timestamp,
	.get_timestamp = get_timestamp,
	.get_raw_header = get_raw_header,
	.runtime_clk_scale_on = runtime_clk_scale_on,
};

static int get_header_from_peer(struct feature_rom *rom)
{
	struct FeatureRomHeader *header;
	
	header = XOCL_GET_SUBDEV_PRIV(&rom->pdev->dev);
	if (!header)
		return -ENODEV;

	memcpy(&rom->header, header, sizeof(*header));

	return 0;
}

static int get_header_from_iomem(struct feature_rom *rom)
{
	struct platform_device *pdev = rom->pdev;
	u32	val;
	u16	vendor, did;
	int	ret = 0;

	val = ioread32(rom->base);
	if (val != MAGIC_NUM) {
		vendor = XOCL_PL_TO_PCI_DEV(pdev)->vendor;
		did = XOCL_PL_TO_PCI_DEV(pdev)->device;
		if (vendor == 0x1d0f && (did == 0x1042 || did == 0xf010)) { // MAGIC, we should define elsewhere
			xocl_info(&pdev->dev,
				"Found AWS VU9P Device without featureROM");
			/*
 			 * This is AWS device. Fill the FeatureROM struct.
 			 * Right now it doesn't have FeatureROM
 			 */
			memset(rom->header.EntryPointString, 0,
				sizeof(rom->header.EntryPointString));
			strncpy(rom->header.EntryPointString, "xlnx", 4);
			memset(rom->header.FPGAPartName, 0,
				sizeof(rom->header.FPGAPartName));
			strncpy(rom->header.FPGAPartName, "AWS VU9P", 8);
			memset(rom->header.VBNVName, 0,
				sizeof(rom->header.VBNVName));
			strncpy(rom->header.VBNVName,
				"xilinx_aws-vu9p-f1_dynamic_5_0", 35);
			rom->header.MajorVersion = 4;
			rom->header.MinorVersion = 0;
			rom->header.VivadoBuildID = 0xabcd;
			rom->header.IPBuildID = 0xabcd;
			rom->header.TimeSinceEpoch = 0xabcd;
			rom->header.DDRChannelCount = 4;
			rom->header.DDRChannelSize = 16;
			rom->header.FeatureBitMap = 0x0;
			rom->header.FeatureBitMap = UNIFIED_PLATFORM;
			rom->unified = true;
			rom->aws_dev = true;

			xocl_info(&pdev->dev, "Enabling AWS dynamic 5.0 Shell");
		} else {
			xocl_err(&pdev->dev, "Magic number does not match, "
			"actual 0x%x, expected 0x%x", val, MAGIC_NUM);
			ret = -ENODEV;
			goto failed;
		}
	}

	xocl_memcpy_fromio(&rom->header, rom->base, sizeof(rom->header));

	if (rom->header.MajorVersion == 5)
		xocl_init_rom_v5(rom);

failed:
	return ret;
}

static int feature_rom_probe(struct platform_device *pdev)
{
	struct feature_rom *rom;
	struct resource *res;
	char	*tmp;
	int	ret;

	rom = devm_kzalloc(&pdev->dev, sizeof(*rom), GFP_KERNEL);
	if (!rom)
		return -ENOMEM;

	rom->pdev =  pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		ret = get_header_from_peer(rom);
	else {
		rom->base = ioremap_nocache(res->start, res->end - res->start + 1);
		if (!rom->base) {
			ret = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}

		ret = get_header_from_iomem(rom);
	}

	if (strstr(rom->header.VBNVName, "-xare")) {
		/*
		 * ARE device, ARE is mapped like another DDR inside FPGA;
		 * map_connects as M04_AXI
		 */
		rom->header.DDRChannelCount = rom->header.DDRChannelCount - 1;
		rom->are_dev = true;
	}

	if(rom->header.FeatureBitMap & UNIFIED_PLATFORM)
		rom->unified = true;

	if(rom->header.FeatureBitMap & BOARD_MGMT_ENBLD)
		rom->mb_mgmt_enabled = true;

	if(rom->header.FeatureBitMap & MB_SCHEDULER)
		rom->mb_sche_enabled = true;

	if(rom->header.FeatureBitMap & RUNTIME_CLK_SCALE)
		rom->runtime_clk_scale_en = true;

	ret = sysfs_create_group(&pdev->dev.kobj, &rom_attr_group);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs failed");
		goto failed;
	}

	tmp = rom->header.EntryPointString;
	xocl_info(&pdev->dev, "ROM magic : %c%c%c%c",
		tmp[0], tmp[1], tmp[2], tmp[3]);
	xocl_info(&pdev->dev, "VBNV: %s", rom->header.VBNVName);
	xocl_info(&pdev->dev, "DDR channel count : %d",
		rom->header.DDRChannelCount);
	xocl_info(&pdev->dev, "DDR channel size: %d GB",
		rom->header.DDRChannelSize);
	xocl_info(&pdev->dev, "Major Version: %d", rom->header.MajorVersion);
	xocl_info(&pdev->dev, "Minor Version: %d", rom->header.MinorVersion);
	xocl_info(&pdev->dev, "IPBuildID: %u", rom->header.IPBuildID);
	xocl_info(&pdev->dev, "TimeSinceEpoch: %llx",
		rom->header.TimeSinceEpoch);
	xocl_info(&pdev->dev, "FeatureBitMap: %llx", rom->header.FeatureBitMap);

	platform_set_drvdata(pdev, rom);

	return 0;

failed:
	if (rom->base)
		iounmap(rom->base);
	devm_kfree(&pdev->dev, rom);
	return ret;
}

static int feature_rom_remove(struct platform_device *pdev)
{
	struct feature_rom *rom;

	xocl_info(&pdev->dev, "Remove feature rom");
	rom = platform_get_drvdata(pdev);
	if (!rom) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	if (rom->base)
		iounmap(rom->base);

	sysfs_remove_group(&pdev->dev.kobj, &rom_attr_group);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, rom);
	return 0;
}

struct xocl_drv_private rom_priv = {
	.ops = &rom_ops,
};

struct platform_device_id rom_id_table[] =  {
	{ XOCL_DEVNAME(XOCL_FEATURE_ROM), (kernel_ulong_t)&rom_priv },
	{ },
};

static struct platform_driver	feature_rom_driver = {
	.probe		= feature_rom_probe,
	.remove		= feature_rom_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_FEATURE_ROM),
	},
	.id_table = rom_id_table,
};

int __init xocl_init_feature_rom(void) {
	return platform_driver_register(&feature_rom_driver);
}

void xocl_fini_feature_rom(void)
{
	return platform_driver_unregister(&feature_rom_driver);
}
