/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.HOu@xilinx.com
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

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/vmalloc.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

#define MAX_RETRY       50
#define RETRY_INTERVAL  100       //ms

#define	MAX_IMAGE_LEN	0x20000

#define	REG_VERSION		0
#define	REG_ID			0x4
#define	REG_STATUS		0x8
#define	REG_ERR			0xC
#define	REG_CAP			0x10
#define	REG_CTL			0x18
#define	REG_STOP_CONFIRM	0x1C
#define	REG_CURR_BASE		0x20
#define	REG_POWER_CHECKSUM	0x1A4

#define	VALID_ID		0x74736574

#define	GPIO_RESET		0x0
#define	GPIO_ENABLED		0x1

#define	SELF_JUMP(ins)		(((ins) & 0xfc00ffff) == 0xb8000000)

enum ctl_mask {
	CTL_MASK_CLEAR_POW	= 0x1,
	CTL_MASK_CLEAR_ERR	= 0x2,
	CTL_MASK_PAUSE		= 0x4,
	CTL_MASK_STOP		= 0x8,
};

enum status_mask {
	STATUS_MASK_INIT_DONE		= 0x1,
	STATUS_MASK_STOPPED		= 0x2,
	STATUS_MASK_PAUSE		= 0x4,
};

enum cap_mask {
	CAP_MASK_PM			= 0x1,
};

enum {
	MB_STATE_INIT = 0,
	MB_STATE_RUN,
	MB_STATE_RESET,
};

enum {
	IO_REG,
	IO_GPIO,
	IO_IMAGE_MGMT,
	IO_IMAGE_SCHE,
	NUM_IOADDR
};

#define	READ_REG32(mb, off)		\
	XOCL_READ_REG32(mb->base_addrs[IO_REG] + off)
#define	WRITE_REG32(mb, val, off)	\
	XOCL_WRITE_REG32(val, mb->base_addrs[IO_REG] + off)

#define	READ_GPIO(mb, off)		\
	XOCL_READ_REG32(mb->base_addrs[IO_GPIO] + off)
#define	WRITE_GPIO(mb, val, off)	\
	XOCL_WRITE_REG32(val, mb->base_addrs[IO_GPIO] + off)

#define	READ_IMAGE_MGMT(mb, off)		\
	XOCL_READ_REG32(mb->base_addrs[IO_IMAGE_MGMT] + off)

#define	COPY_MGMT(mb, buf, len)		\
	xocl_memcpy_toio(mb->base_addrs[IO_IMAGE_MGMT], buf, len)
#define	COPY_SCHE(mb, buf, len)		\
	xocl_memcpy_toio(mb->base_addrs[IO_IMAGE_SCHE], buf, len)

struct xocl_mb {
	struct platform_device	*pdev;
	void __iomem		*base_addrs[NUM_IOADDR];

	struct device		*hwmon_dev;
	bool			enabled;
	u32			state;
	u32			cap;
	struct mutex		mb_lock;

	char			*sche_binary;
	u32			sche_binary_length;
	char			*mgmt_binary;
	u32			mgmt_binary_length;
};

static int mb_stop(struct xocl_mb *mb);
static int mb_start(struct xocl_mb *mb);

/* sysfs support */
static void safe_read32(struct xocl_mb *mb, u32 reg, u32 *val)
{
	mutex_lock(&mb->mb_lock);
	if (mb->enabled && mb->state == MB_STATE_RUN)
		*val = READ_REG32(mb, reg);
	else
		*val = 0;
	mutex_unlock(&mb->mb_lock);
}

static void safe_write32(struct xocl_mb *mb, u32 reg, u32 val)
{
	mutex_lock(&mb->mb_lock);
	if (mb->enabled && mb->state == MB_STATE_RUN)
		WRITE_REG32(mb, val, reg);
	mutex_unlock(&mb->mb_lock);
}

static ssize_t version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(mb, REG_VERSION, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(version);

static ssize_t id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(mb, REG_ID, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(id);

static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(mb, REG_STATUS, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(status);

static ssize_t error_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(mb, REG_ERR, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(error);

static ssize_t capability_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(mb, REG_CAP, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(capability);

static ssize_t power_checksum_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(mb, REG_POWER_CHECKSUM, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(power_checksum);

static ssize_t pause_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(mb, REG_CTL, &val);

	return sprintf(buf, "%d\n", !!(val & CTL_MASK_PAUSE));
}

static ssize_t pause_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 1)
		return -EINVAL;

	val = val ? CTL_MASK_PAUSE : 0;
	safe_write32(mb, REG_CTL, val);

	return count;
}
static DEVICE_ATTR_RW(pause);

static ssize_t reset_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_mb *mb = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 1)
		return -EINVAL;

	if (val) {
		mb_stop(mb);
		mb_start(mb);
	}

	return count;
}
static DEVICE_ATTR_WO(reset);

static struct attribute *mb_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_id.attr,
	&dev_attr_status.attr,
	&dev_attr_error.attr,
	&dev_attr_capability.attr,
	&dev_attr_power_checksum.attr,
	&dev_attr_pause.attr,
	&dev_attr_reset.attr,
	NULL,
};
static struct attribute_group mb_attr_group = {
	.attrs = mb_attrs,
};

static ssize_t show_mb_pw(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct xocl_mb *mb = dev_get_drvdata(dev);
	u32 val;

	safe_read32(mb, REG_CURR_BASE + attr->index * sizeof(u32), &val);

	return sprintf(buf, "%d\n", val);
}

static SENSOR_DEVICE_ATTR(curr1_highest, 0444, show_mb_pw, NULL, 0);
static SENSOR_DEVICE_ATTR(curr1_average, 0444, show_mb_pw, NULL, 1);
static SENSOR_DEVICE_ATTR(curr1_input, 0444, show_mb_pw, NULL, 2);
static SENSOR_DEVICE_ATTR(curr2_highest, 0444, show_mb_pw, NULL, 3);
static SENSOR_DEVICE_ATTR(curr2_average, 0444, show_mb_pw, NULL, 4);
static SENSOR_DEVICE_ATTR(curr2_input, 0444, show_mb_pw, NULL, 5);
static SENSOR_DEVICE_ATTR(curr3_highest, 0444, show_mb_pw, NULL, 6);
static SENSOR_DEVICE_ATTR(curr3_average, 0444, show_mb_pw, NULL, 7);
static SENSOR_DEVICE_ATTR(curr3_input, 0444, show_mb_pw, NULL, 8);
static SENSOR_DEVICE_ATTR(curr4_highest, 0444, show_mb_pw, NULL, 9);
static SENSOR_DEVICE_ATTR(curr4_average, 0444, show_mb_pw, NULL, 10);
static SENSOR_DEVICE_ATTR(curr4_input, 0444, show_mb_pw, NULL, 11);
static SENSOR_DEVICE_ATTR(curr5_highest, 0444, show_mb_pw, NULL, 12);
static SENSOR_DEVICE_ATTR(curr5_average, 0444, show_mb_pw, NULL, 13);
static SENSOR_DEVICE_ATTR(curr5_input, 0444, show_mb_pw, NULL, 14);
static SENSOR_DEVICE_ATTR(curr6_highest, 0444, show_mb_pw, NULL, 15);
static SENSOR_DEVICE_ATTR(curr6_average, 0444, show_mb_pw, NULL, 16);
static SENSOR_DEVICE_ATTR(curr6_input, 0444, show_mb_pw, NULL, 17);

static struct attribute *hwmon_mb_attributes[] = {
	&sensor_dev_attr_curr1_highest.dev_attr.attr,
	&sensor_dev_attr_curr1_average.dev_attr.attr,
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_curr2_highest.dev_attr.attr,
	&sensor_dev_attr_curr2_average.dev_attr.attr,
	&sensor_dev_attr_curr2_input.dev_attr.attr,
	&sensor_dev_attr_curr3_highest.dev_attr.attr,
	&sensor_dev_attr_curr3_average.dev_attr.attr,
	&sensor_dev_attr_curr3_input.dev_attr.attr,
	&sensor_dev_attr_curr4_highest.dev_attr.attr,
	&sensor_dev_attr_curr4_average.dev_attr.attr,
	&sensor_dev_attr_curr4_input.dev_attr.attr,
	&sensor_dev_attr_curr5_highest.dev_attr.attr,
	&sensor_dev_attr_curr5_average.dev_attr.attr,
	&sensor_dev_attr_curr5_input.dev_attr.attr,
	&sensor_dev_attr_curr6_highest.dev_attr.attr,
	&sensor_dev_attr_curr6_average.dev_attr.attr,
	&sensor_dev_attr_curr6_input.dev_attr.attr,
	NULL
};

static const struct attribute_group hwmon_mb_attrgroup = {
	.attrs = hwmon_mb_attributes,
};

static ssize_t show_name(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	return sprintf(buf, "%s\n", "xclmgmt_microblaze");
}

static struct sensor_device_attribute name_attr =
	SENSOR_ATTR(name, 0444, show_name, NULL, 0);

static void mgmt_sysfs_destroy_mb(struct platform_device *pdev)
{
	struct xocl_mb *mb;

	mb = platform_get_drvdata(pdev);

	if (!mb->enabled)
		return;

	if (mb->hwmon_dev) {
		device_remove_file(mb->hwmon_dev, &name_attr.dev_attr);
		sysfs_remove_group(&mb->hwmon_dev->kobj,
			&hwmon_mb_attrgroup);
		hwmon_device_unregister(mb->hwmon_dev);
		mb->hwmon_dev = NULL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &mb_attr_group);
}

static int mgmt_sysfs_create_mb(struct platform_device *pdev)
{
	struct xocl_mb *mb;
	struct xocl_dev_core *core;
	int err;

	mb = platform_get_drvdata(pdev);
	core = XDEV(xocl_get_xdev(pdev));

	if (!mb->enabled)
		return 0;
	err = sysfs_create_group(&pdev->dev.kobj, &mb_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create mb attrs failed: 0x%x", err);
		goto create_attr_failed;
	}
	mb->hwmon_dev = hwmon_device_register(&core->pdev->dev);
	if (IS_ERR(mb->hwmon_dev)) {
		err = PTR_ERR(mb->hwmon_dev);
		xocl_err(&pdev->dev, "register mb hwmon failed: 0x%x", err);
		goto hwmon_reg_failed;
	}

	dev_set_drvdata(mb->hwmon_dev, mb);

	err = device_create_file(mb->hwmon_dev, &name_attr.dev_attr);
	if (err) {
		xocl_err(&pdev->dev, "create attr name failed: 0x%x", err);
		goto create_name_failed;
	}

	err = sysfs_create_group(&mb->hwmon_dev->kobj,
		&hwmon_mb_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create pw group failed: 0x%x", err);
		goto create_pw_failed;
	}

	return 0;

create_pw_failed:
	device_remove_file(mb->hwmon_dev, &name_attr.dev_attr);
create_name_failed:
	hwmon_device_unregister(mb->hwmon_dev);
	mb->hwmon_dev = NULL;
hwmon_reg_failed:
	sysfs_remove_group(&pdev->dev.kobj, &mb_attr_group);
create_attr_failed:
	return err;
}

static int mb_stop(struct xocl_mb *mb)
{
	int retry = 0;
	int ret = 0;
	u32 reg_val = 0;

	if (!mb->enabled)
		return 0;

	mutex_lock(&mb->mb_lock);
	reg_val = READ_GPIO(mb, 0);
	xocl_info(&mb->pdev->dev, "Reset GPIO 0x%x", reg_val);
	if (reg_val == GPIO_RESET) {
		/* MB in reset status */
		mb->state = MB_STATE_RESET;
		goto out;
	}

	xocl_info(&mb->pdev->dev,
		"MGMT Image magic word, 0x%x, status 0x%x, id 0x%x",
		READ_IMAGE_MGMT(mb, 0),
		READ_REG32(mb, REG_STATUS),
		READ_REG32(mb, REG_ID));

	if (!SELF_JUMP(READ_IMAGE_MGMT(mb, 0))) {
		/* non cold boot */
		reg_val = READ_REG32(mb, REG_STATUS);
		if (!(reg_val & STATUS_MASK_STOPPED)) {
			// need to stop microblaze
			xocl_info(&mb->pdev->dev, "stopping microblaze...");
			WRITE_REG32(mb, CTL_MASK_STOP, REG_CTL);
			WRITE_REG32(mb, 1, REG_STOP_CONFIRM);
			while (retry++ < MAX_RETRY &&
				!(READ_REG32(mb, REG_STATUS) &
				STATUS_MASK_STOPPED)) {
				msleep(RETRY_INTERVAL);
			}
			if (retry >= MAX_RETRY) {
				xocl_err(&mb->pdev->dev,
					"Failed to stop microblaze");
				xocl_err(&mb->pdev->dev,
					"Error Reg 0x%x",
					READ_REG32(mb, REG_ERR));
				ret = -EIO;
				goto out;
			}
		}
		xocl_info(&mb->pdev->dev, "Microblaze Stopped, retry %d",
			retry);
	}

	/* hold reset */
	WRITE_GPIO(mb, GPIO_RESET, 0);
	mb->state = MB_STATE_RESET;
out:
	mutex_unlock(&mb->mb_lock);

	return ret;
}

static int mb_start(struct xocl_mb *mb)
{
	int retry = 0;
	u32 reg_val = 0;
	int ret = 0;
	void *xdev_hdl;

	if (!mb->enabled)
		return 0;

	xdev_hdl = xocl_get_xdev(mb->pdev);

	mutex_lock(&mb->mb_lock);
	reg_val = READ_GPIO(mb, 0);
	xocl_info(&mb->pdev->dev, "Reset GPIO 0x%x", reg_val);
	if (reg_val == GPIO_ENABLED)
		goto out;

	xocl_info(&mb->pdev->dev, "Start Microblaze...");
	xocl_info(&mb->pdev->dev, "MGMT Image magic word, 0x%x",
		READ_IMAGE_MGMT(mb, 0));

	if (xocl_mb_mgmt_on(xdev_hdl)) {
		xocl_info(&mb->pdev->dev, "Copying mgmt image len %d",
			mb->mgmt_binary_length);
		COPY_MGMT(mb, mb->mgmt_binary, mb->mgmt_binary_length);
	}

	if (xocl_mb_sched_on(xdev_hdl)) {
		xocl_info(&mb->pdev->dev, "Copying scheduler image len %d",
			mb->sche_binary_length);
		COPY_SCHE(mb, mb->sche_binary, mb->sche_binary_length);
	}

	WRITE_GPIO(mb, GPIO_ENABLED, 0);
	xocl_info(&mb->pdev->dev,
		"MGMT Image magic word, 0x%x, status 0x%x, id 0x%x",
		READ_IMAGE_MGMT(mb, 0),
		READ_REG32(mb, REG_STATUS),
		READ_REG32(mb, REG_ID));
	do {
		msleep(RETRY_INTERVAL);
	} while (retry++ < MAX_RETRY && (READ_REG32(mb, REG_STATUS) &
		STATUS_MASK_STOPPED));

	/* Extra pulse needed as workaround for axi interconnect issue in DSA */
	if (retry >= MAX_RETRY) {
		retry = 0;
		WRITE_GPIO(mb, GPIO_RESET, 0);
		WRITE_GPIO(mb, GPIO_ENABLED, 0);
		do {
			msleep(RETRY_INTERVAL);
		} while (retry++ < MAX_RETRY && (READ_REG32(mb, REG_STATUS) &
			STATUS_MASK_STOPPED));
	}

	if (retry >= MAX_RETRY) {
		xocl_err(&mb->pdev->dev, "Failed to start microblaze");
		xocl_err(&mb->pdev->dev, "Error Reg 0x%x",
				READ_REG32(mb, REG_ERR));
			ret = -EIO;
	}

	mb->cap = READ_REG32(mb, REG_CAP);
	mb->state = MB_STATE_RUN;
out:
	mutex_unlock(&mb->mb_lock);

	return ret;
}

static int mb_reset(struct platform_device *pdev)
{
	struct xocl_mb *mb;

	xocl_info(&pdev->dev, "Reset Microblaze...");
	mb = platform_get_drvdata(pdev);
	if (!mb)
		return -EINVAL;

	mb_stop(mb);
	mb_start(mb);

	return 0;
}

static int load_mgmt_image(struct platform_device *pdev, const char *image,
	u32 len)
{
	struct xocl_mb *mb;
	char *binary;

	if (len > MAX_IMAGE_LEN)
		return -EINVAL;

	mb = platform_get_drvdata(pdev);
	if (!mb)
		return -EINVAL;

	binary = mb->mgmt_binary;
	mb->mgmt_binary = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
	if (!mb->mgmt_binary)
		return -ENOMEM;

	if (binary)
		devm_kfree(&pdev->dev, binary);
	memcpy(mb->mgmt_binary, image, len);
	mb->mgmt_binary_length = len;

	return 0;
}

static int load_sche_image(struct platform_device *pdev, const char *image,
	u32 len)
{
	struct xocl_mb *mb;
	char *binary = NULL;

	if (len > MAX_IMAGE_LEN)
		return -EINVAL;

	mb = platform_get_drvdata(pdev);
	if (!mb)
		return -EINVAL;

	binary = mb->sche_binary;
	mb->sche_binary = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
	if (!mb->sche_binary)
		return -ENOMEM;

	if (binary)
		devm_kfree(&pdev->dev, binary);
	memcpy(mb->sche_binary, image, len);
	mb->sche_binary_length = len;

	return 0;
}

//Have a function stub but don't actually do anything when this is called
static int mb_ignore(struct platform_device *pdev)
{
	return 0;
}

static struct xocl_mb_funcs mb_ops = {
	.load_mgmt_image	= load_mgmt_image,
	.load_sche_image	= load_sche_image,
	.reset			= mb_reset,
	.stop			= mb_ignore,
};



static int __mb_remove(struct platform_device *pdev)
{
	struct xocl_mb *mb;
	int	i;

	mb = platform_get_drvdata(pdev);
	if (!mb)
		return 0;

	if (mb->mgmt_binary)
		devm_kfree(&pdev->dev, mb->mgmt_binary);
	if (mb->sche_binary)
		devm_kfree(&pdev->dev, mb->sche_binary);

	/*
	 * It is more secure that MB keeps running even driver is unloaded.
	 * Even user unload our driver and use their own stuff, MB will still
	 * be able to monitor the board unless user stops it explicitly
	 */
	mb_stop(mb);

	mgmt_sysfs_destroy_mb(pdev);

	for (i = 0; i < NUM_IOADDR; i++) {
		if (mb->base_addrs[i])
			iounmap(mb->base_addrs[i]);
	}

	mutex_destroy(&mb->mb_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, mb);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void mb_remove(struct platform_device *pdev)
{
	__mb_remove(pdev);
}
#else
#define mb_remove __mb_remove
#endif

static int mb_probe(struct platform_device *pdev)
{
	struct xocl_mb *mb;
	struct resource *res;
	void	*xdev_hdl;
	int i, err;

	mb = devm_kzalloc(&pdev->dev, sizeof(*mb), GFP_KERNEL);
	if (!mb) {
		xocl_err(&pdev->dev, "out of memory");
		return -ENOMEM;
	}

	mb->pdev = pdev;
	platform_set_drvdata(pdev, mb);

	xdev_hdl = xocl_get_xdev(pdev);
	if (xocl_mb_mgmt_on(xdev_hdl) || xocl_mb_sched_on(xdev_hdl)) {
		xocl_info(&pdev->dev, "Microblaze is supported.");
		mb->enabled = true;
	} else {
		xocl_info(&pdev->dev, "Microblaze is not supported.");
		devm_kfree(&pdev->dev, mb);
		platform_set_drvdata(pdev, NULL);
		return 0;
	}

	for (i = 0; i < NUM_IOADDR; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
			res->start, res->end);
		mb->base_addrs[i] =
			ioremap_nocache(res->start, res->end - res->start + 1);
		if (!mb->base_addrs[i]) {
			err = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}
	}

	err = mgmt_sysfs_create_mb(pdev);
	if (err) {
		xocl_err(&pdev->dev, "Create sysfs failed, err %d", err);
		goto failed;
	}

	mutex_init(&mb->mb_lock);

	return 0;

failed:
	mb_remove(pdev);
	return err;
}

struct xocl_drv_private mb_priv = {
	.ops = &mb_ops,
};

struct platform_device_id mb_id_table[] = {
	{ XOCL_DEVNAME(XOCL_MB), (kernel_ulong_t)&mb_priv },
	{ },
};

static struct platform_driver	mb_driver = {
	.probe		= mb_probe,
	.remove		= mb_remove,
	.driver		= {
		.name = "xocl_mb",
	},
	.id_table = mb_id_table,
};

int __init xocl_init_mb(void)
{
	return platform_driver_register(&mb_driver);
}

void xocl_fini_mb(void)
{
	platform_driver_unregister(&mb_driver);
}
