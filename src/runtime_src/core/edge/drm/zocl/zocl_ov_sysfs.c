/*
 * A GEM style (optionally CMA backed) device manager for ZynQ base
 * OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu   <yliu@xilinx.com>
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

#include "zocl_drv.h"
#include "zocl_ospi_versal.h"

static ssize_t pdi_done_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct zocl_ov_dev *ov = dev_get_drvdata(dev);
	u8 val;

	if (!ov)
		return 0;

	if (kstrtou8(buf, 16, &val) == -EINVAL)
		return -EINVAL;

	write_lock(&ov->att_rwlock);
	ov->pdi_done = val;
	write_unlock(&ov->att_rwlock);

	return count;
}
static DEVICE_ATTR_WO(pdi_done);

static ssize_t pdi_ready_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zocl_ov_dev *ov = dev_get_drvdata(dev);
	ssize_t size = 0;

	if (!ov)
		return 0;

	read_lock(&ov->att_rwlock);
	size += sprintf(buf, "%d\n", ov->pdi_ready);
	read_unlock(&ov->att_rwlock);

	return size;
}
static DEVICE_ATTR_RO(pdi_ready);

static struct attribute *zocl_ov_attrs[] = {
	&dev_attr_pdi_ready.attr,
	&dev_attr_pdi_done.attr,
	NULL,
};

static ssize_t read_versal_pdi(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct zocl_ov_dev *ov;
	struct zocl_ov_pkt_node *node;
	size_t pre_size = 0, size = 0, cp_size;
	size_t rem_count = count;
	loff_t cp_start;
	u32 nread = 0;

	ov = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!ov)
		return 0;

	read_lock(&ov->att_rwlock);
	node = ov->head;
	if (!node) {
		read_unlock(&ov->att_rwlock);
		return 0;
	}

	while (node) {
		size += node->zn_size;
		if (off > size) {
			node = node->zn_next;
			pre_size = size;
			continue;
		}

		cp_start = off >= pre_size ? off - pre_size : 0;
		if (node->zn_size - cp_start >= rem_count)
			cp_size = rem_count;
		else
			cp_size = node->zn_size - cp_start;
		rem_count -= cp_size;

		memcpy(buf + nread, ((char *)node->zn_datap) + cp_start,
		    cp_size);
		nread += cp_size;
		if (rem_count == 0)
			break;

		pre_size = size;
		node = node->zn_next;
	}

	read_unlock(&ov->att_rwlock);
	return nread;
}

static struct bin_attribute versal_pdi_attr = {
	.attr = {
		.name = "versal_pdi",
		.mode = 0444
	},
	.read = read_versal_pdi,
	.write = NULL,
	.size = 0
};

static struct bin_attribute *zocl_ov_bin_attrs[] = {
	&versal_pdi_attr,
	NULL,
};

static struct attribute_group zocl_ov_attr_group = {
	.attrs = zocl_ov_attrs,
	.bin_attrs = zocl_ov_bin_attrs,
};

int zocl_ov_init_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &zocl_ov_attr_group);
	if (ret)
		DRM_ERROR("Create zocl attrs failed: %d\n", ret);

	return ret;
}

void zocl_ov_fini_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &zocl_ov_attr_group);
}
