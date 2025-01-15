/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
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

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

#define	TEMP		0x400		// TEMPERATURE REGISTER ADDRESS
#define	VCCINT		0x404		// VCCINT REGISTER OFFSET
#define	VCCAUX		0x408		// VCCAUX REGISTER OFFSET
#define	VCCBRAM		0x418		// VCCBRAM REGISTER OFFSET
#define	TEMP_MAX	0x480
#define	VCCINT_MAX	0x484
#define	VCCAUX_MAX	0x488
#define	VCCBRAM_MAX	0x48c
#define	TEMP_MIN	0x490
#define	VCCINT_MIN	0x494
#define	VCCAUX_MIN	0x498
#define	VCCBRAM_MIN	0x49c
#define	OT_UPPER_ALARM_REG          0x54c // Over Temperature upper alarm register
#define	OT_UPPER_ALARM_REG_OVERRIDE 0x3
/*
 * Measured 12bit ADC code for temperature of 110 degree celcius using
 * equation 4-2 from UG580 document
 */
#define	ADC_CODE_TEMP_110           0xC36

#define	SYSMON_TO_MILLVOLT(val)			\
	((val) * 1000 * 3 >> 16)

#define	READ_REG32(sysmon, off)		\
	XOCL_READ_REG32(sysmon->base + off)
#define	WRITE_REG32(sysmon, val, off)	\
	XOCL_WRITE_REG32(val, sysmon->base + off)

struct xocl_sysmon {
	void __iomem		*base;
	struct device		*hwmon_dev;
	struct xocl_sysmon_privdata *priv_data;
};

/* For ultrascale+ cards use sysmon4 equation 2-11 from UG580 doc
 * Also, sysmon register will have all F's once mgmtpf bar goes offline
 * during card shutdown sequence, so ignoring all F's.
 */
static int32_t SYSMON_TO_MILLDEGREE(u32 val)
{
	if (val == 0xFFFFFFFF)
		return 0;

	return (((int64_t)(val) * 509314 >> 16) - 280230);
}

static int get_prop(struct platform_device *pdev, u32 prop, void *val)
{
	struct xocl_sysmon	*sysmon;
	u32			tmp;

	sysmon = platform_get_drvdata(pdev);
	BUG_ON(!sysmon);

	switch (prop) {
	case XOCL_SYSMON_PROP_TEMP:
		tmp = READ_REG32(sysmon, TEMP);
		*(u32 *)val = SYSMON_TO_MILLDEGREE(tmp)/1000;
		break;
	case XOCL_SYSMON_PROP_TEMP_MAX:
		tmp = READ_REG32(sysmon, TEMP_MAX);
		*(u32 *)val = SYSMON_TO_MILLDEGREE(tmp);
		break;
	case XOCL_SYSMON_PROP_TEMP_MIN:
		tmp = READ_REG32(sysmon, TEMP_MIN);
		*(u32 *)val = SYSMON_TO_MILLDEGREE(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_INT:
		tmp = READ_REG32(sysmon, VCCINT);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_INT_MAX:
		tmp = READ_REG32(sysmon, VCCINT_MAX);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_INT_MIN:
		tmp = READ_REG32(sysmon, VCCINT_MIN);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_AUX:
		tmp = READ_REG32(sysmon, VCCAUX);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_AUX_MAX:
		tmp = READ_REG32(sysmon, VCCAUX_MAX);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_AUX_MIN:
		tmp = READ_REG32(sysmon, VCCAUX_MIN);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_BRAM:
		tmp = READ_REG32(sysmon, VCCBRAM);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_BRAM_MAX:
		tmp = READ_REG32(sysmon, VCCBRAM_MAX);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	case XOCL_SYSMON_PROP_VCC_BRAM_MIN:
		tmp = READ_REG32(sysmon, VCCBRAM_MIN);
		*(u32 *)val = SYSMON_TO_MILLVOLT(tmp);
		break;
	default:
		xocl_err(&pdev->dev, "Invalid prop");
		return -EINVAL;
	}

	return 0;
}

static struct xocl_sysmon_funcs sysmon_ops = {
	.get_prop	= get_prop,
};

static ssize_t show_sysmon(struct platform_device *pdev, u32 prop, char *buf)
{
	u32 val;

	(void) get_prop(pdev, prop, &val);
	return sprintf(buf, "%d\n", val);
}

/* sysfs support */
static ssize_t show_hwmon(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct platform_device *pdev = dev_get_drvdata(dev);

	return show_sysmon(pdev, attr->index, buf);
}

static ssize_t show_name(struct device *dev, struct device_attribute *da,
	char *buf)
{
	return sprintf(buf, "%s\n", "xclmgmt_sysmon");
}

static SENSOR_DEVICE_ATTR(temp1_input, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_TEMP);
static SENSOR_DEVICE_ATTR(temp1_highest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_TEMP_MAX);
static SENSOR_DEVICE_ATTR(temp1_lowest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_TEMP_MIN);

static SENSOR_DEVICE_ATTR(in0_input, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_INT);
static SENSOR_DEVICE_ATTR(in0_highest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_INT_MAX);
static SENSOR_DEVICE_ATTR(in0_lowest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_INT_MIN);

static SENSOR_DEVICE_ATTR(in1_input, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_AUX);
static SENSOR_DEVICE_ATTR(in1_highest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_AUX_MAX);
static SENSOR_DEVICE_ATTR(in1_lowest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_AUX_MIN);

static SENSOR_DEVICE_ATTR(in2_input, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_BRAM);
static SENSOR_DEVICE_ATTR(in2_highest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_BRAM_MAX);
static SENSOR_DEVICE_ATTR(in2_lowest, 0444, show_hwmon, NULL,
	XOCL_SYSMON_PROP_VCC_BRAM_MIN);

static struct attribute *hwmon_sysmon_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_highest.dev_attr.attr,
	&sensor_dev_attr_temp1_lowest.dev_attr.attr,
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_highest.dev_attr.attr,
	&sensor_dev_attr_in0_lowest.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_highest.dev_attr.attr,
	&sensor_dev_attr_in1_lowest.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_highest.dev_attr.attr,
	&sensor_dev_attr_in2_lowest.dev_attr.attr,
	NULL
};

static const struct attribute_group hwmon_sysmon_attrgroup = {
	.attrs = hwmon_sysmon_attributes,
};

static struct sensor_device_attribute sysmon_name_attr =
	SENSOR_ATTR(name, 0444, show_name, NULL, 0);

static ssize_t temp_show(struct device *dev, struct device_attribute *da,
    char *buf)
{
    return show_sysmon(to_platform_device(dev), XOCL_SYSMON_PROP_TEMP, buf);
}
static DEVICE_ATTR_RO(temp);

static ssize_t vcc_int_show(struct device *dev, struct device_attribute *da,
    char *buf)
{
    return show_sysmon(to_platform_device(dev), XOCL_SYSMON_PROP_VCC_INT, buf);
}
static DEVICE_ATTR_RO(vcc_int);

static ssize_t vcc_aux_show(struct device *dev, struct device_attribute *da,
    char *buf)
{
    return show_sysmon(to_platform_device(dev), XOCL_SYSMON_PROP_VCC_AUX, buf);
}
static DEVICE_ATTR_RO(vcc_aux);

static ssize_t vcc_bram_show(struct device *dev, struct device_attribute *da,
    char *buf)
{
    return show_sysmon(to_platform_device(dev), XOCL_SYSMON_PROP_VCC_BRAM, buf);
}
static DEVICE_ATTR_RO(vcc_bram);

static struct attribute *sysmon_attributes[] = {
	&dev_attr_temp.attr,
	&dev_attr_vcc_int.attr,
	&dev_attr_vcc_aux.attr,
	&dev_attr_vcc_bram.attr,
	NULL,
};

static const struct attribute_group sysmon_attrgroup = {
	.attrs = sysmon_attributes,
};

static void mgmt_sysfs_destroy_sysmon(struct platform_device *pdev)
{
	struct xocl_sysmon *sysmon;

	sysmon = platform_get_drvdata(pdev);

	device_remove_file(sysmon->hwmon_dev, &sysmon_name_attr.dev_attr);
	sysfs_remove_group(&sysmon->hwmon_dev->kobj, &hwmon_sysmon_attrgroup);
	hwmon_device_unregister(sysmon->hwmon_dev);
	sysmon->hwmon_dev = NULL;

	sysfs_remove_group(&pdev->dev.kobj, &sysmon_attrgroup);
}

static int mgmt_sysfs_create_sysmon(struct platform_device *pdev)
{
	struct xocl_sysmon *sysmon;
	struct xocl_dev_core *core;
	int err;

	sysmon = platform_get_drvdata(pdev);
	core = XDEV(xocl_get_xdev(pdev));

	sysmon->hwmon_dev = hwmon_device_register(&core->pdev->dev);
	if (IS_ERR(sysmon->hwmon_dev)) {
		err = PTR_ERR(sysmon->hwmon_dev);
		xocl_err(&pdev->dev, "register sysmon hwmon failed: 0x%x", err);
		goto hwmon_reg_failed;
	}

	dev_set_drvdata(sysmon->hwmon_dev, pdev);
	err = device_create_file(sysmon->hwmon_dev,
		&sysmon_name_attr.dev_attr);
	if (err) {
		xocl_err(&pdev->dev, "create attr name failed: 0x%x", err);
		goto create_name_failed;
	}

	err = sysfs_create_group(&sysmon->hwmon_dev->kobj,
		&hwmon_sysmon_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create hwmon group failed: 0x%x", err);
		goto create_hwmon_failed;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &sysmon_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create sysmon group failed: 0x%x", err);
		goto create_sysmon_failed;
	}

	return 0;

create_sysmon_failed:
	sysfs_remove_group(&sysmon->hwmon_dev->kobj, &hwmon_sysmon_attrgroup);
create_hwmon_failed:
	device_remove_file(sysmon->hwmon_dev, &sysmon_name_attr.dev_attr);
create_name_failed:
	hwmon_device_unregister(sysmon->hwmon_dev);
	sysmon->hwmon_dev = NULL;
hwmon_reg_failed:
	return err;
}

static int sysmon_probe(struct platform_device *pdev)
{
	struct xocl_sysmon *sysmon;
	struct resource *res;
	int err;

	sysmon = devm_kzalloc(&pdev->dev, sizeof(*sysmon), GFP_KERNEL);
	if (!sysmon)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "resource is NULL");
		return -EINVAL;
	}
	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);
	sysmon->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!sysmon->base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	sysmon->priv_data = XOCL_GET_SUBDEV_PRIV(&pdev->dev);

	platform_set_drvdata(pdev, sysmon);

	err = mgmt_sysfs_create_sysmon(pdev);
	if (err) {
		goto create_sysmon_failed;
	}

	if (sysmon->priv_data &&
		sysmon->priv_data->flags & XOCL_SYSMON_OT_OVERRIDE) {
		xocl_info(&pdev->dev, "Over temperature threshold override is set");
		WRITE_REG32(sysmon, (ADC_CODE_TEMP_110 << 4) |
			OT_UPPER_ALARM_REG_OVERRIDE, OT_UPPER_ALARM_REG);
	}

	return 0;

create_sysmon_failed:
	platform_set_drvdata(pdev, NULL);
failed:
	return err;
}


static int sysmon_remove(struct platform_device *pdev)
{
	struct xocl_sysmon	*sysmon;

	sysmon = platform_get_drvdata(pdev);
	if (!sysmon) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	mgmt_sysfs_destroy_sysmon(pdev);

	if (sysmon->base)
		iounmap(sysmon->base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, sysmon);

	return 0;
}

struct xocl_drv_private sysmon_priv = {
	.ops = &sysmon_ops,
};

struct platform_device_id sysmon_id_table[] = {
	{ XOCL_DEVNAME(XOCL_SYSMON), (kernel_ulong_t)&sysmon_priv },
	{ },
};

static struct platform_driver	sysmon_driver = {
	.probe		= sysmon_probe,
	.remove		= sysmon_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_SYSMON),
	},
	.id_table = sysmon_id_table,
};

int __init xocl_init_sysmon(void)
{
	return platform_driver_register(&sysmon_driver);
}

void xocl_fini_sysmon(void)
{
	platform_driver_unregister(&sysmon_driver);
}
