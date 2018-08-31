/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
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

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/vmalloc.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

/* Registers are defined in pg150-ultrascale-memory-ip.pdf: 
 * AXI4-Lite Slave Control/Status Register Map 
 */
#define XLNX_DNA_MEMORY_MAP_MAGIC_IS_DEFINED                        (0x3E4D7732)
#define XLNX_DNA_MAJOR_MINOR_VERSION_REGISTER_OFFSET                0x00          //  RO
#define XLNX_DNA_REVISION_REGISTER_OFFSET                           0x04          //  RO
#define XLNX_DNA_CAPABILITY_REGISTER_OFFSET                         0x08          //  RO 
//#define XLNX_DNA_SCRATCHPAD_REGISTER_OFFSET                         (0x0C)          //  RO (31-1) + RW (0)
#define XLNX_DNA_STATUS_REGISTER_OFFSET                             0x10            //  RO
#define XLNX_DNA_FSM_DNA_WORD_WRITE_COUNT_REGISTER_OFFSET           (0x14)          //  RO
#define XLNX_DNA_FSM_CERTIFICATE_WORD_WRITE_COUNT_REGISTER_OFFSET   (0x18)          //  RO
#define XLNX_DNA_MESSAGE_START_AXI_ONLY_REGISTER_OFFSET             (0x20)          //  RO (31-1) + RW (0)
#define XLNX_DNA_READBACK_REGISTER_2_OFFSET                         0x40            //  RO XLNX_DNA_BOARD_DNA_95_64 
#define XLNX_DNA_READBACK_REGISTER_1_OFFSET                         0x44            //  RO XLNX_DNA_BOARD_DNA_63_32 
#define XLNX_DNA_READBACK_REGISTER_0_OFFSET                         0x48            //  RO XLNX_DNA_BOARD_DNA_31_0  
#define XLNX_DNA_DATA_AXI_ONLY_REGISTER_OFFSET                      (0x80)          //  WO
#define XLNX_DNA_CERTIFICATE_DATA_AXI_ONLY_REGISTER_OFFSET          (0xC0)          //  WO - 512 bit aligned.
#define XLNX_DNA_MAX_ADDRESS_WORDS                                  (0xC4)

struct xocl_xlnx_dna {
	void __iomem		*base;
	struct device		*xlnx_dna_dev;
	struct mutex		xlnx_dna_lock;
};

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xlnx_dna *xlnx_dna = dev_get_drvdata(dev);
	u32 status;

	status = ioread32(xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);

	return sprintf(buf, "0x%x\n", status);
}
static DEVICE_ATTR_RO(status);

static ssize_t dna_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xlnx_dna *xlnx_dna = dev_get_drvdata(dev);
	uint32_t dna96_64, dna63_32, dna31_0;

	dna96_64 = ioread32(xlnx_dna->base+XLNX_DNA_READBACK_REGISTER_2_OFFSET);
	dna63_32 = ioread32(xlnx_dna->base+XLNX_DNA_READBACK_REGISTER_1_OFFSET);
	dna31_0  = ioread32(xlnx_dna->base+XLNX_DNA_READBACK_REGISTER_0_OFFSET);

	return sprintf(buf, "%08x%08x%08x\n", dna96_64, dna63_32, dna31_0);
}
static DEVICE_ATTR_RO(dna);

static ssize_t capability_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xlnx_dna *xlnx_dna = dev_get_drvdata(dev);
	u32 capability;

	capability = ioread32(xlnx_dna->base+XLNX_DNA_CAPABILITY_REGISTER_OFFSET);

	return sprintf(buf, "0x%x\n", capability);
}
static DEVICE_ATTR_RO(capability);


static ssize_t dna_version_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xlnx_dna *xlnx_dna = dev_get_drvdata(dev);
	u32 version;

	version = ioread32(xlnx_dna->base+XLNX_DNA_MAJOR_MINOR_VERSION_REGISTER_OFFSET);

	return sprintf(buf, "%d.%d\n", version>>16,version&0xffff);
}
static DEVICE_ATTR_RO(dna_version);

static ssize_t revision_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xlnx_dna *xlnx_dna = dev_get_drvdata(dev);
	u32 revision;

	revision = ioread32(xlnx_dna->base+XLNX_DNA_REVISION_REGISTER_OFFSET);

	return sprintf(buf, "%d\n", revision);
}
static DEVICE_ATTR_RO(revision);

static struct attribute *xlnx_dna_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_dna.attr,
	&dev_attr_capability.attr,
	&dev_attr_dna_version.attr,
	&dev_attr_revision.attr,
	NULL
};

static const struct attribute_group xlnx_dna_attrgroup = {
	.attrs = xlnx_dna_attributes,
};

static uint32_t dna_status(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	uint32_t status = 0;
	uint8_t retries = 5;
	bool rsa4096done = false;
	if (!xlnx_dna)
		return status;

	while(!rsa4096done && retries){
		status = ioread32(xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);
		if(status>>8 & 0x1){
			rsa4096done = true;
			break;
		}
		msleep(50);
		retries--;
	}

	if(retries == 0)
		return -EBUSY;

	status = ioread32(xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);

	return status;
}

static uint32_t dna_capability(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	u32 capability = 0;

	if (!xlnx_dna)
		return capability;

	capability = ioread32(xlnx_dna->base+XLNX_DNA_CAPABILITY_REGISTER_OFFSET);

	return capability;
}

static struct xocl_dna_funcs dna_ops = {
	.status			= dna_status,
	.capability = dna_capability,
};


static void mgmt_sysfs_destroy_xlnx_dna(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna;

	xlnx_dna = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &xlnx_dna_attrgroup);

}

static int mgmt_sysfs_create_xlnx_dna(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna;
	struct xocl_dev_core *core;
	int err;

	xlnx_dna = platform_get_drvdata(pdev);
	core = XDEV(xocl_get_xdev(pdev));

	err = sysfs_create_group(&pdev->dev.kobj, &xlnx_dna_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create pw group failed: 0x%x", err);
		goto create_grp_failed;
	}

	return 0;

create_grp_failed:
	return err;
}

static int xlnx_dna_probe(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna;
	struct resource *res;
	int err;

	xlnx_dna = devm_kzalloc(&pdev->dev, sizeof(*xlnx_dna), GFP_KERNEL);
	if (!xlnx_dna)
		return -ENOMEM;
	
	xlnx_dna->base = devm_kzalloc(&pdev->dev, sizeof(void __iomem *), GFP_KERNEL);
	if (!xlnx_dna->base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "resource is NULL");
		return -EINVAL;
	}
	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	xlnx_dna->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!xlnx_dna->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	platform_set_drvdata(pdev, xlnx_dna);

	err = mgmt_sysfs_create_xlnx_dna(pdev);
	if (err) {
		goto create_xlnx_dna_failed;
	}
	xocl_subdev_register(pdev, XOCL_SUBDEV_DNA, &dna_ops);

	return 0;

create_xlnx_dna_failed:
	platform_set_drvdata(pdev, NULL);
failed:
	return err;
}


static int xlnx_dna_remove(struct platform_device *pdev)
{
	struct xocl_xlnx_dna	*xlnx_dna;

	xlnx_dna = platform_get_drvdata(pdev);
	if (!xlnx_dna) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	mgmt_sysfs_destroy_xlnx_dna(pdev);

	if (xlnx_dna->base)
		iounmap(xlnx_dna->base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, xlnx_dna);

	return 0;
}

struct platform_device_id xlnx_dna_id_table[] = {
	{ XOCL_DNA, 0 },
	{ },
};

static struct platform_driver	xlnx_dna_driver = {
	.probe		= xlnx_dna_probe,
	.remove		= xlnx_dna_remove,
	.driver		= {
		.name = "xocl_dna",
	},
	.id_table = xlnx_dna_id_table,
};

int __init xocl_init_dna(void)
{
	return platform_driver_register(&xlnx_dna_driver);
}

void xocl_fini_dna(void)
{
	platform_driver_unregister(&xlnx_dna_driver);
}