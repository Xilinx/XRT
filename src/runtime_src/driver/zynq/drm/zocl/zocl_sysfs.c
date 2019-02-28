/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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
#include "sched_exec.h"
#include "xclbin.h"

static ssize_t xclbinid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	ssize_t size;

	if (!zdev)
		return 0;

	size = sprintf(buf, "%llx\n", zdev->unique_id_last_bitstream);

	return size;
}
static DEVICE_ATTR_RO(xclbinid);

static ssize_t kds_numcus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	ssize_t size;

	if (!zdev || !zdev->exec)
		return 0;

	size = sprintf(buf, "%d\n", zdev->exec->num_cus);

	return size;
}
static DEVICE_ATTR_RO(kds_numcus);

static ssize_t kds_custat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	ssize_t size = 0;
	int i;

	if (!zdev || !zdev->exec)
		return 0;

	read_lock(&zdev->attr_rwlock);

	for (i = 0; i < zdev->exec->num_cus; i++)
		size += sprintf(buf + size, "CU[@0x%x] : %d\n",
		    zdev->exec->cu_addr_phy[i], zdev->exec->cu_usage[i]);

	read_unlock(&zdev->attr_rwlock);

	return size;
}
static DEVICE_ATTR_RO(kds_custat);

static ssize_t zocl_get_memstat(struct device *dev, char *buf, bool raw)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	struct mem_topology *topo;
	ssize_t size = 0;
	ssize_t count;
	size_t memory_usage;
	unsigned int bo_count;
	const char *txt_fmt = "[%s] %s@0x%012llx\t(%4lluMB):\t%lluKB\t%dBOs\n";
	const char *raw_fmt = "%llu %d\n";
	int i;

	if (!zdev || !zdev->topology)
		return 0;

	topo = zdev->topology;
	read_lock(&zdev->attr_rwlock);

	for (i = 0; i < topo->m_count; i++) {
		if (topo->m_mem_data[i].m_type == MEM_STREAMING)
			continue;

		memory_usage = topo->m_mem_data[i].m_used ?
		    zdev->mm_usage.memory_usage : 0;
		bo_count = topo->m_mem_data[i].m_used ?
		    zdev->mm_usage.bo_count : 0;

		if (raw)
			count = sprintf(buf, raw_fmt, memory_usage, bo_count);
		else {
			count = sprintf(buf, txt_fmt,
			    topo->m_mem_data[i].m_used ? "IN-USE" : "UNUSED",
			    topo->m_mem_data[i].m_tag,
			    topo->m_mem_data[i].m_base_address,
			    topo->m_mem_data[i].m_size / 1024,
			    memory_usage / 1024,
			    bo_count);
		}
		buf += count;
		size += count;
	}

	read_unlock(&zdev->attr_rwlock);

	return size;
}

static ssize_t memstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return zocl_get_memstat(dev, buf, false);
}
static DEVICE_ATTR_RO(memstat);

static ssize_t memstat_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return zocl_get_memstat(dev, buf, true);
}
static DEVICE_ATTR_RO(memstat_raw);

static struct attribute *zocl_attrs[] = {
	&dev_attr_xclbinid.attr,
	&dev_attr_kds_numcus.attr,
	&dev_attr_kds_custat.attr,
	&dev_attr_memstat.attr,
	&dev_attr_memstat_raw.attr,
	NULL,
};

static ssize_t read_debug_ip_layout(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev;
	size_t size;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev || !zdev->debug_ip)
		return 0;

	read_lock(&zdev->attr_rwlock);

	size = sizeof_section(zdev->debug_ip, m_debug_ip_data);

	if (off >= size)
		return 0;

	if (count < size - off)
		nread = count;
	else
		nread = size - off;

	memcpy(buf, ((char *)zdev->debug_ip) + off, nread);

	read_unlock(&zdev->attr_rwlock);

	return nread;
}

static ssize_t read_ip_layout(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev;
	size_t size;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev || !zdev->ip)
		return 0;

	read_lock(&zdev->attr_rwlock);

	size = sizeof_section(zdev->ip, m_ip_data);

	if (off > size)
		return 0;

	if (count < size - off)
		nread = count;
	else
		nread = size - off;

	memcpy(buf, ((char *)zdev->ip) + off, nread);

	read_unlock(&zdev->attr_rwlock);

	return nread;
}

static ssize_t read_connectivity(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev;
	size_t size;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev || !zdev->connectivity)
		return 0;

	read_lock(&zdev->attr_rwlock);

	size = sizeof_section(zdev->connectivity, m_connection);

	if (off > size)
		return 0;

	if (count < size - off)
		nread = count;
	else
		nread = size - off;

	memcpy(buf, ((char *)zdev->connectivity + off), nread);

	read_unlock(&zdev->attr_rwlock);

	return nread;
}

static ssize_t read_mem_topology(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev;
	size_t size;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev || !zdev->topology)
		return 0;

	read_lock(&zdev->attr_rwlock);

	size = sizeof_section(zdev->topology, m_mem_data);

	if (off > size)
		return 0;

	if (count < size - off)
		nread = count;
	else
		nread = size - off;

	memcpy(buf, ((char *)zdev->topology + off), nread);

	read_unlock(&zdev->attr_rwlock);

	return nread;
}

static struct bin_attribute debug_ip_layout_attr = {
	.attr = {
		.name = "debug_ip_layout",
		.mode = 0444
	},
	.read = read_debug_ip_layout,
	.write = NULL,
	.size = 0
};

static struct bin_attribute ip_layout_attr = {
	.attr = {
		.name = "ip_layout",
		.mode = 0444
	},
	.read = read_ip_layout,
	.write = NULL,
	.size = 0
};

static struct bin_attribute connectivity_attr = {
	.attr = {
		.name = "connectivity",
		.mode = 0444
	},
	.read = read_connectivity,
	.write = NULL,
	.size = 0
};

static struct bin_attribute mem_topology_attr = {
	.attr = {
		.name = "mem_topology",
		.mode = 0444
	},
	.read = read_mem_topology,
	.write = NULL,
	.size = 0
};


static struct bin_attribute *zocl_bin_attrs[] = {
	&debug_ip_layout_attr,
	&ip_layout_attr,
	&connectivity_attr,
	&mem_topology_attr,
	NULL,
};

static struct attribute_group zocl_attr_group = {
	.attrs = zocl_attrs,
	.bin_attrs = zocl_bin_attrs,
};

int zocl_init_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &zocl_attr_group);
	if (ret)
		DRM_ERROR("Create zocl attrs failed: %d\n", ret);

	return ret;
}

void zocl_fini_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &zocl_attr_group);
}

