/*
 * sysfs for the device attributes.
 *
 * Copyright (C) 2016-2017 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Umang Parekh <umang.parekh@xilinx.com>
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

#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/signal.h>
#include <linux/init_task.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/types.h>
#include "mgmt-core.h"

static ssize_t instance_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	struct awsmgmt_dev *lro = (struct awsmgmt_dev *)dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", lro->instance);
}
static DEVICE_ATTR_RO(instance);

static ssize_t ratelimit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct awsmgmt_dev *lro  = dev_get_drvdata(dev);
	u32 val = 0;

	if (!lro) {
		return -EINVAL;
	}
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_CONFIG);
	return sprintf(buf, "0x%x\n", val);
}
static ssize_t ratelimit_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct awsmgmt_dev *lro  = dev_get_drvdata(dev);
	u32     val, enable;

	if (!lro || kstrtou32(buf, 16, &val) == -EINVAL) {
		return -EINVAL;
	}

	enable = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);
	iowrite32(0, lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);
	iowrite32(val & 0xffff, lro->bar[AWSMGMT_MAIN_BAR] +
		RATE_LIMITER_CONFIG);
	iowrite32(enable, lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);

	return count;
}
static DEVICE_ATTR_RW(ratelimit);

static ssize_t enable_ratelimit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct awsmgmt_dev *lro  = dev_get_drvdata(dev);
	u32 val = 0;

	if (!lro) {
		return -EINVAL;
	}
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);
	return sprintf(buf, "%u\n", val);
}
static ssize_t enable_ratelimit_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct awsmgmt_dev *lro  = dev_get_drvdata(dev);
	u32     val;

	if (!lro || kstrtou32(buf, 10, &val) == -EINVAL || val > 1) {
		return -EINVAL;
	}

	iowrite32(val, lro->bar[AWSMGMT_MAIN_BAR] + RATE_LIMITER_ENABLE);

	return count;
}
static DEVICE_ATTR_RW(enable_ratelimit);

static ssize_t ddr_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct awsmgmt_dev *lro  = dev_get_drvdata(dev);
	u32 val = 0;

	if (!lro) {
		return -EINVAL;
	}
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDR_STATUS_OFFSET);
	return sprintf(buf, "0x%x\n", val);
}
static DEVICE_ATTR_RO(ddr_status);

static ssize_t ddr_config_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct awsmgmt_dev *lro  = dev_get_drvdata(dev);
	u32 val = 0;

	if (!lro) {
		return -EINVAL;
	}
	val = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + DDR_CONFIG_OFFSET);
	return sprintf(buf, "0x%x\n", val);
}

static ssize_t ddr_config_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct awsmgmt_dev *lro  = dev_get_drvdata(dev);
	u32     val;

	if (!lro || kstrtou32(buf, 16, &val) == -EINVAL) {
		return -EINVAL;
	}

	iowrite32(val, lro->bar[AWSMGMT_MAIN_BAR] + DDR_CONFIG_OFFSET);

	return count;
}
static DEVICE_ATTR_RW(ddr_config);

int mgmt_init_sysfs(struct device *dev)
{
	struct awsmgmt_dev *lro = (struct awsmgmt_dev *)dev_get_drvdata(dev);
	int result = device_create_file(dev, &dev_attr_instance);

	device_create_file(dev, &dev_attr_ratelimit);
	device_create_file(dev, &dev_attr_enable_ratelimit);
	device_create_file(dev, &dev_attr_ddr_status);
	device_create_file(dev, &dev_attr_ddr_config);

	return result;
}

void mgmt_fini_sysfs(struct device *dev)
{
	struct awsmgmt_dev *lro = (struct awsmgmt_dev *)dev_get_drvdata(dev);
	device_remove_file(dev, &dev_attr_instance);

	device_remove_file(dev, &dev_attr_ratelimit);
	device_remove_file(dev, &dev_attr_enable_ratelimit);
	device_remove_file(dev, &dev_attr_ddr_status);
	device_remove_file(dev, &dev_attr_ddr_config);
}
