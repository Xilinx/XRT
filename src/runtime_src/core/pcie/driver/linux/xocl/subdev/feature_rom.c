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
#include "flash_xrt_data.h"
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
	char			uuid[65];
	u32			uuid_len;
	bool			passthrough_virt_en;
};

/* This module param is a workaround for non-VSEC platforms which rely on partition
 * metadata to discover resources. PLatforms using this mechanism should set
 * XOCL_DSAFLAG_CUSTOM_DTB flag. This can support only single device. Getting
 * uuid from VSEC is scalable as it supports multiple devices and EOU.
 *
 * TODO : Remove this after CR-1105444 is fixed
 */
static char *rom_uuid = "firmware_dir";
module_param(rom_uuid, charp, 0644);
MODULE_PARM_DESC(rom_uuid, "uuid value to find firmware directory (max 64 chars)");

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

static ssize_t uuid_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct feature_rom *rom = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%s\n", rom->uuid);
}
static DEVICE_ATTR_RO(uuid);

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
	&dev_attr_uuid.attr,
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

static bool passthrough_virtualization_on(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->passthrough_virt_en;
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

static char *get_uuid(struct platform_device *pdev)
{
	struct feature_rom *rom;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	return rom->uuid;
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

	/* Ignore timestamp matching for AWS platform */
	if (is_aws(pdev))
		return true;

	rom = platform_get_drvdata(pdev);
	BUG_ON(!rom);

	xocl_dbg(&pdev->dev, "Shell timestamp: 0x%llx",
		rom->header.TimeSinceEpoch);
	xocl_dbg(&pdev->dev, "Verify timestamp: 0x%llx", timestamp);

	if (strlen(rom->uuid) > 0) {
		xocl_dbg(&pdev->dev, "2RP platform, skip timestamp check");
		return true;
	}

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

static const char *get_uuid_from_firmware(struct platform_device *pdev,
	const struct axlf *axlf)
{
	int node = -1;
	const void *uuid = NULL;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	const struct axlf_section_header *dtc_header =
		xocl_axlf_section_header(xdev, axlf, PARTITION_METADATA);

	if (dtc_header == NULL)
		return NULL;
	node = xocl_fdt_get_next_prop_by_name(xdev,
		(char *)axlf + dtc_header->m_sectionOffset,
		-1, PROP_LOGIC_UUID, &uuid, NULL);
	if (uuid && node >= 0)
		return uuid;
	return NULL;
}

static inline bool is_multi_rp(struct feature_rom *rom)
{
	return strlen(rom->uuid) > 0;
}

static bool is_valid_firmware(struct platform_device *pdev,
	char *fw_buf, size_t fw_len)
{
	struct axlf *axlf = (struct axlf *)fw_buf;
	struct feature_rom *rom = platform_get_drvdata(pdev);
	size_t axlflen = axlf->m_header.m_length;
	u64 ts = axlf->m_header.m_featureRomTimeStamp;
	u64 rts = rom->header.TimeSinceEpoch;

	if (memcmp(fw_buf, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2)) != 0) {
		xocl_err(&pdev->dev, "unknown fw format");
		return false;
	}

	if (axlflen > fw_len) {
		xocl_err(&pdev->dev, "truncated fw, length: %ld, expect: %ld",
			fw_len, axlflen);
		return false;
	}

	if (xocl_xrt_version_check(xocl_get_xdev(pdev), axlf, true)) {
		xocl_err(&pdev->dev, "fw version is not supported by xrt");
		return false;
	}

	if (is_multi_rp(rom)) {
		const char *uuid = get_uuid_from_firmware(pdev, axlf);
		if (uuid == NULL || strcmp(rom->uuid, uuid) != 0) {
			xocl_err(&pdev->dev, "bad fw UUID: %s, expect: %s",
				uuid ? uuid : "<none>", rom->uuid);
			return false;
		}
	}

	if (ts != rts) {
		xocl_err(&pdev->dev,
			"bad fw timestamp: 0x%llx, exptect: 0x%llx", ts, rts);
		return false;
	}

	return true;
}

/* Return the length of the string or -E2BIG in case of error */
static int get_vendor_firmware_dir(u16 vendor, char *buf, size_t len) {
	switch (vendor) {
		case XOCL_ARISTA_VEN:
			return strscpy(buf, "arista", len);
		default:
		case XOCL_XILINX_VEN:
			return strscpy(buf, "xilinx", len);
	}
}

static int load_firmware_from_flash(struct platform_device *pdev,
	char **fw_buf, size_t *fw_len)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct flash_data_header header = { {{0}} };
	const size_t magiclen = sizeof(header.fdh_id_begin.fdi_magic);
	size_t flash_size = 0;
	int ret = 0;
	char *buf = NULL;
	struct flash_data_ident id = { {0} };

	xocl_dbg(&pdev->dev, "try loading fw from flash");

	ret = xocl_flash_get_size(xdev, &flash_size);
	if (ret == -ENODEV) {
		xocl_dbg(&pdev->dev,
			"no flash subdev");
		return ret;
	} else if (flash_size == 0) {
		xocl_err(&pdev->dev,
			"failed to get flash size");
		return -EINVAL;
	}

	ret = xocl_flash_read(xdev, (char *)&header, sizeof(header),
		flash_size - sizeof(header));
	if (ret) {
		xocl_err(&pdev->dev,
			"failed to read meta data header from flash: %d", ret);
		return ret;
	}
	/* Pick the end ident since header is aligned in the end of flash. */
	id = header.fdh_id_end;

	if (strncmp(id.fdi_magic, XRT_DATA_MAGIC, magiclen)) {
		char tmp[sizeof(id.fdi_magic) + 1] = { 0 };
		memcpy(tmp, id.fdi_magic, magiclen);
		xocl_dbg(&pdev->dev, "ignore meta data, bad magic: %s", tmp);
		return -ENOENT;
	}

	if (id.fdi_version != 0) {
		xocl_dbg(&pdev->dev,
			"flash meta data version is not supported: %d",
			id.fdi_version);
		return -EOPNOTSUPP;
	}

	buf = vmalloc(header.fdh_data_len);
	if (buf == NULL)
		return -ENOMEM;

	ret = xocl_flash_read(xdev, buf, header.fdh_data_len,
		header.fdh_data_offset);
	if (ret) {
		xocl_err(&pdev->dev,
			"failed to read meta data from flash: %d", ret);
	} else if (flash_xrt_data_get_parity32(buf, header.fdh_data_len) ^
		header.fdh_data_parity) {
		xocl_err(&pdev->dev, "meta data is corrupted");
		ret = -EINVAL;
	}

	xocl_dbg(&pdev->dev, "found meta data of %d bytes @0x%x",
		header.fdh_data_len, header.fdh_data_offset);
	*fw_buf = buf;
	*fw_len = header.fdh_data_len;
	return ret;
}

static int load_firmware_from_disk(struct platform_device *pdev, char **fw_buf,
	size_t *fw_len, char *suffix)
{
	struct feature_rom *rom = platform_get_drvdata(pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(pdev);
	struct pci_dev *pcidev_user = NULL;
	u16 vendor = le16_to_cpu(pcidev->vendor);
	u16 subdevice = le16_to_cpu(pcidev->subsystem_device);
	u16 deviceid = le16_to_cpu(pcidev->device);
	int funcid = PCI_FUNC(pcidev->devfn);
	int slotid = PCI_SLOT(pcidev->devfn);
	u64 timestamp = rom->header.TimeSinceEpoch;
	int err = 0;
	char fw_name[256];
	char vendor_fw_dir[16];

	if (funcid != 0) {
		pcidev_user = pci_get_slot(pcidev->bus,
			PCI_DEVFN(slotid, funcid - 1));
		if (!pcidev_user) {
			pcidev_user = pci_get_device(pcidev->vendor,
				pcidev->device + 1, NULL);
		}
		if (pcidev_user)
			deviceid = le16_to_cpu(pcidev_user->device);
	}

	err = get_vendor_firmware_dir(vendor, vendor_fw_dir, sizeof(vendor_fw_dir));
	// Failure returns -E2BIG
	if (err < 0)
		return err;

	/* For 2RP, only uuid is provided */
	if (is_multi_rp(rom)) {
		snprintf(fw_name, sizeof(fw_name),
			"%s/%s/partition.%s", vendor_fw_dir, rom->uuid, suffix);
	} else {
		snprintf(fw_name, sizeof(fw_name),
			"%s/%04x-%04x-%04x-%016llx.%s",
			vendor_fw_dir, vendor, deviceid, subdevice, timestamp, suffix);
	}

	xocl_dbg(&pdev->dev, "try loading fw: %s", fw_name);
	err = xocl_request_firmware(&pcidev->dev, fw_name, fw_buf, fw_len);
	if (err && !is_multi_rp(rom)) {
		snprintf(fw_name, sizeof(fw_name),
			"%s/%04x-%04x-%04x-%016llx.%s",
			vendor_fw_dir, vendor, (deviceid + 1), subdevice, timestamp, suffix);
		xocl_dbg(&pdev->dev, "try loading fw: %s", fw_name);
		err = xocl_request_firmware(&pcidev->dev, fw_name, fw_buf, fw_len);
	}

	return err;
}

static int load_firmware_from_vmr(struct platform_device *pdev,
	char **fw_buf, size_t *fw_len)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	return xocl_vmr_load_firmware(xdev, fw_buf, fw_len);
}

static int load_firmware(struct platform_device *pdev, char **fw, size_t *len)
{
	char *buf = NULL;
	size_t size = 0;
	int ret;

	/*
	 * If this is a vmr devices, load firmware from device first.
	 * If this is not a vmr device, we continue to try next possible
	 * location.
	 */
	ret = load_firmware_from_vmr(pdev, &buf, &size);
	if (ret)
		ret = load_firmware_from_disk(pdev, &buf, &size, "xsabin");
	if (ret)
		ret = load_firmware_from_disk(pdev, &buf, &size, "dsabin");
	if (ret)
		ret = load_firmware_from_flash(pdev, &buf, &size);
	if (ret) {
		xocl_err(&pdev->dev, "can't load firmware, ret:%d, give up", ret);
		return ret;
	}

	if (!is_valid_firmware(pdev, buf, size)) {
		vfree(buf);
		return -EINVAL;
	}

	*fw = buf;
	*len = size;
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
	.load_firmware = load_firmware,
	.passthrough_virtualization_on = passthrough_virtualization_on,
	.get_uuid = get_uuid,
};

static int get_header_from_peer(struct feature_rom *rom)
{
	struct FeatureRomHeader *header;
	struct resource res;
	xdev_handle_t xdev = xocl_get_xdev(rom->pdev);
	int offset = 0;
	const u64 *io_off = NULL;

	header = XOCL_GET_SUBDEV_PRIV(&rom->pdev->dev);
	if (!header)
		return -ENODEV;

	memcpy(&rom->header, header, sizeof(*header));

	xocl_xdev_dbg(xdev, "Searching CDMA in dtb.");
	offset = xocl_fdt_path_offset(xdev, XDEV(xdev)->fdt_blob,
				      "/" NODE_ENDPOINTS "/" RESNAME_KDMA);
	if (offset < 0)
		return 0;

	io_off = xocl_fdt_getprop(xdev, XDEV(xdev)->fdt_blob, offset,
				  PROP_IO_OFFSET, NULL);
	if (!io_off) {
		xocl_xdev_err(xdev, "dtb maybe corrupted\n");
		return -EINVAL;
	}
	res.start = be64_to_cpu(io_off[0]);
	rom->header.FeatureBitMap |= CDMA;
	memset(rom->header.CDMABaseAddress, 0,
	       sizeof(rom->header.CDMABaseAddress));
	rom->header.CDMABaseAddress[0] = (uint32_t)res.start;
	xocl_xdev_dbg(xdev, "CDMA is on, CU offset: 0x%x",
		       rom->header.CDMABaseAddress[0]);

	return 0;
}

static int init_rom_by_dtb(struct feature_rom *rom)
{
	xdev_handle_t xdev = xocl_get_xdev(rom->pdev);
	struct FeatureRomHeader *header = &rom->header;
	struct resource res;
	const char *vbnv;
	int i, ret;

	header->FeatureBitMap = UNIFIED_PLATFORM;
	*(u32 *)header->EntryPointString = MAGIC_NUM;
	if (XDEV(xdev)->priv.vbnv)
		strncpy(header->VBNVName, XDEV(xdev)->priv.vbnv,
				sizeof (header->VBNVName) - 1);

	/* overwrite if vbnv property exists */
	if (XDEV(xdev)->fdt_blob) {
		vbnv = fdt_getprop(XDEV(xdev)->fdt_blob, 0, "vbnv", NULL);
		if (vbnv) {
			xocl_xdev_dbg(xdev, "found vbnv prop, %s", vbnv);
			strncpy(header->VBNVName, vbnv,
				sizeof(header->VBNVName) - 1);
			for (i = 0; i < strlen(header->VBNVName); i++)
				if (header->VBNVName[i] == ':' ||
					header->VBNVName[i] == '.')
					header->VBNVName[i] = '_';
		}
	}

	xocl_xdev_dbg(xdev, "Searching ERT and CMC in dtb.");
	ret = xocl_subdev_get_resource(xdev, NODE_CMC_FW_MEM,
			IORESOURCE_MEM, &res);
	if (!ret) {
		xocl_xdev_dbg(xdev, "CMC is on");
		header->FeatureBitMap |= BOARD_MGMT_ENBLD;
	}

	ret = xocl_subdev_get_resource(xdev, NODE_ERT_FW_MEM,
			IORESOURCE_MEM, &res);
	if (!ret) {
		xocl_xdev_dbg(xdev, "ERT is on");
		header->FeatureBitMap |= MB_SCHEDULER;
	}

	return 0;
}

static int get_header_from_dtb(struct feature_rom *rom)
{
	int i, j = 0;

	for (i = rom->uuid_len / 2 - 4;
	    i >= 0 && j < rom->uuid_len;
	    i -= 4, j += 8) {
		sprintf(&rom->uuid[j], "%08x", ioread32(rom->base + i));
	}
	xocl_dbg(&rom->pdev->dev, "UUID %s", rom->uuid);

	return init_rom_by_dtb(rom);
}

static int get_header_from_vsec(struct feature_rom *rom)
{
	xdev_handle_t xdev = xocl_get_xdev(rom->pdev);
	int bar;
	u64 offset;
	int ret;

	ret = xocl_subdev_vsec(xdev, XOCL_VSEC_UUID_ROM, &bar, &offset, NULL);
	if (ret) {
		/* XOCL_DSAFLAG_CUSTOM_DTB is used for non-VSEC platforms which
		 * still wanted to use partition metadata to discover resources
		 */
		if (XDEV(xdev)->priv.flags & XOCL_DSAFLAG_CUSTOM_DTB) {
			rom->uuid_len = strlen(rom_uuid);
			if (rom->uuid_len == 0 || rom->uuid_len > 64) {
				xocl_xdev_info(xdev, "Invalid ROM UUID");
				return -EINVAL;
			}
			strcpy(rom->uuid,rom_uuid);
			xocl_xdev_info(xdev, "rom UUID is: %s",rom->uuid);
			return init_rom_by_dtb(rom);
		}
		xocl_xdev_info(xdev, "Does not get UUID ROM");
		return -ENODEV;
	}

	offset += pci_resource_start(XDEV(xdev)->pdev, bar);
	xocl_xdev_dbg(xdev, "Mapping uuid at offset 0x%llx", offset);
	rom->base = ioremap_nocache(offset, PAGE_SIZE);
	rom->uuid_len = 32;

	return get_header_from_dtb(rom);
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
		if (vendor == 0x1d0f && (did == 0x1042 || did == 0xf010 || did == 0xf011 || did == 0x9048 || did == 0x9248)) {
			xocl_dbg(&pdev->dev,
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
			if (did == 0xf010)
				strncpy(rom->header.VBNVName,
					AWS_F1_XDMA_SHELL_NAME, strlen(AWS_F1_XDMA_SHELL_NAME));
			else if (did == 0xf011)
				strncpy(rom->header.VBNVName,
					AWS_F1_NODMA_SHELL_NAME, strlen(AWS_F1_NODMA_SHELL_NAME));
			else if (did == 0x9048 || did == 0x9248)
				strncpy(rom->header.VBNVName,
					AWS_F2_XDMA_SHELL_NAME, strlen(AWS_F2_XDMA_SHELL_NAME));
			else
				strncpy(rom->header.VBNVName,
					AWS_F1_DYNAMIC_SHELL_NAME, strlen(AWS_F1_DYNAMIC_SHELL_NAME));
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
	} else
		xocl_memcpy_fromio(&rom->header, rom->base,
				sizeof(rom->header));

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
	platform_set_drvdata(pdev, rom);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		xocl_dbg(&pdev->dev, "Get header from VSEC");
		ret = get_header_from_vsec(rom);
		if (ret)
			(void)get_header_from_peer(rom);
	} else {
		rom->base = ioremap_nocache(res->start, res->end - res->start + 1);
		if (!rom->base) {
			ret = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}

		if (!strcmp(res->name, "uuid")) {
			rom->uuid_len = 64;
			(void)get_header_from_dtb(rom);
		} else
			(void)get_header_from_iomem(rom);
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

	if(rom->header.FeatureBitMap & PASSTHROUGH_VIRTUALIZATION)
		rom->passthrough_virt_en = true;

	ret = sysfs_create_group(&pdev->dev.kobj, &rom_attr_group);
	if (ret) {
		xocl_err(&pdev->dev, "create sysfs failed");
		goto failed;
	}

	tmp = rom->header.EntryPointString;
	xocl_dbg(&pdev->dev, "ROM magic : %c%c%c%c",
		tmp[0], tmp[1], tmp[2], tmp[3]);
	xocl_dbg(&pdev->dev, "VBNV: %s", rom->header.VBNVName);
	xocl_dbg(&pdev->dev, "DDR channel count : %d",
		rom->header.DDRChannelCount);
	xocl_dbg(&pdev->dev, "DDR channel size: %d GB",
		rom->header.DDRChannelSize);
	xocl_dbg(&pdev->dev, "Major Version: %d", rom->header.MajorVersion);
	xocl_dbg(&pdev->dev, "Minor Version: %d", rom->header.MinorVersion);
	xocl_dbg(&pdev->dev, "IPBuildID: %u", rom->header.IPBuildID);
	xocl_dbg(&pdev->dev, "TimeSinceEpoch: %llx",
		rom->header.TimeSinceEpoch);
	xocl_dbg(&pdev->dev, "FeatureBitMap: %llx", rom->header.FeatureBitMap);

	return 0;

failed:
	if (rom->base)
		iounmap(rom->base);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, rom);
	return ret;
}

static int feature_rom_remove(struct platform_device *pdev)
{
	struct feature_rom *rom;

	xocl_dbg(&pdev->dev, "Remove feature rom");
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
