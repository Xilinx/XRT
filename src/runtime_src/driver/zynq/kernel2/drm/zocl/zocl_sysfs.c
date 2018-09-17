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

#include "zocl_drv.h"

static ssize_t xclbinid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	return sprintf(buf, "%llx\n", zdev->unique_id_last_bitstream);
}

static DEVICE_ATTR_RO(xclbinid);

#if 0
static ssize_t dr_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", 0);
}

static DEVICE_ATTR_RO(dr_base_addr);
#endif

static ssize_t mem_topology_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	memcpy(buf, zdev->topology.topology, zdev->topology.size);

	return zdev->topology.size;
}

static DEVICE_ATTR_RO(mem_topology);

static ssize_t connectivity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	memcpy(buf, zdev->connectivity.connections, zdev->connectivity.size);

	return zdev->connectivity.size;
}

static DEVICE_ATTR_RO(connectivity);

static ssize_t ip_layout_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	memcpy(buf, zdev->layout.layout, zdev->layout.size);

	return zdev->layout.size;
}

static DEVICE_ATTR_RO(ip_layout);

static ssize_t read_debug_ip_layout(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (off >= zdev->debug_layout.size)
		return 0;

	if (count < zdev->debug_layout.size - off)
		nread = count;
	else
		nread = zdev->debug_layout.size - off;

	memcpy(buf, ((char *)zdev->debug_layout.layout) + off, nread);

	return nread;
}

static struct bin_attribute debug_ip_layout_attrs = {
	.attr = {
		.name = "debug_ip_layout",
		.mode = 0444
	},
	.read = read_debug_ip_layout,
	.write = NULL,
	.size = 0
};

int zocl_init_sysfs(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_xclbinid);
	if (ret)
		goto out0;

	ret = device_create_file(dev, &dev_attr_connectivity);
	if (ret)
		goto out1;

	ret = device_create_file(dev, &dev_attr_ip_layout);
	if (ret)
		goto out2;

	ret = device_create_file(dev, &dev_attr_mem_topology);
	if (ret)
		goto out3;

	ret = device_create_bin_file(dev, &debug_ip_layout_attrs);
	if (ret)
		goto out4;

	return ret;

out4:
	device_remove_file(dev, &dev_attr_mem_topology);
out3:
	device_remove_file(dev, &dev_attr_ip_layout);
out2:
	device_remove_file(dev, &dev_attr_connectivity);
out1:
	device_remove_file(dev, &dev_attr_xclbinid);
out0:
	return ret;
}

void zocl_fini_sysfs(struct device *dev)
{
	device_remove_file(dev, &dev_attr_xclbinid);
	device_remove_file(dev, &dev_attr_mem_topology);
	device_remove_file(dev, &dev_attr_connectivity);
	device_remove_file(dev, &dev_attr_ip_layout);
	device_remove_bin_file(dev, &debug_ip_layout_attrs);
}

