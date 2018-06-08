/*
 * sysfs for the device attributes.
 *
 * Copyright (C) 2016-2017 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Lizhi Hou <lizhih@xilinx.com>
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

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#include "mgmt-core.h"

static ssize_t instance_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", lro->instance);
}
static DEVICE_ATTR_RO(instance);

static ssize_t error_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	ssize_t count;
	count = sprintf(buf, "%s\n", lro->core.ebuf);
	lro->core.ebuf[0] = 0;
	return count;
}
static DEVICE_ATTR_RO(error);

static ssize_t userbar_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
        struct xclmgmt_dev *lro = dev_get_drvdata(dev);
        return sprintf(buf, "%d\n", lro->core.priv.user_bar);
}
static DEVICE_ATTR_RO(userbar);

static struct attribute *mgmt_attrs[] = {
	&dev_attr_instance.attr,
	&dev_attr_error.attr,
	&dev_attr_userbar.attr,
	NULL,
};

static struct attribute_group mgmt_attr_group = {
	.attrs = mgmt_attrs,
};

int mgmt_init_sysfs(struct device *dev)
{
	int err;

	err = sysfs_create_group(&dev->kobj, &mgmt_attr_group);
	if (err)
		xocl_err(dev, "create mgmt attrs failed: %d", err);

	return err;
}

void mgmt_fini_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &mgmt_attr_group);
}


