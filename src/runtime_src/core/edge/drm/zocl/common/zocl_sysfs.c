/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "zocl_aie.h"
#include "zocl_xclbin.h"
#include "xclbin.h"
#include "zocl_sk.h"
#include "kds_core.h"

extern int kds_echo;

/* -KDS sysfs-- */
static ssize_t
kds_echo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kds_echo);
}

static ssize_t
kds_echo_store(struct device *dev, struct device_attribute *da,
	       const char *buf, size_t count)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	return store_kds_echo(&zdev->kds, buf, count, &kds_echo);
}
static DEVICE_ATTR(kds_echo, 0644, kds_echo_show, kds_echo_store);

static ssize_t
kds_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	return show_kds_stat(&zdev->kds, buf);
}
static DEVICE_ATTR_RO(kds_stat);

static ssize_t
kds_custat_raw_show(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));

	return show_kds_custat_raw(&zdev->kds, buffer, count, offset);
}

static ssize_t xclbinid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	struct drm_zocl_slot *zocl_slot = NULL;
	const char *raw_fmt = "%d %pUb\n";
	ssize_t size = 0;
	ssize_t count = 0;
	int i = 0;

	read_lock(&zdev->attr_rwlock);
	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (!zocl_slot || !zocl_slot->slot_xclbin ||
		    !zocl_slot->slot_xclbin->zx_uuid)
			continue;

		count = sprintf(buf+size, raw_fmt, zocl_slot->slot_idx,
				zocl_slot->slot_xclbin->zx_uuid);
		size += count;
	}

	read_unlock(&zdev->attr_rwlock);
	return size;
}
static DEVICE_ATTR_RO(xclbinid);

static ssize_t dtbo_path_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	struct drm_zocl_slot *zocl_slot = NULL;
	const char *raw_fmt = "%d %s\n";
	ssize_t size = 0;
	ssize_t count = 0;
	int i = 0;

	read_lock(&zdev->attr_rwlock);
	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (!zocl_slot || !zocl_slot->slot_xclbin ||
		    !zocl_slot->slot_xclbin->zx_dtbo_path)
			continue;

		count = sprintf(buf+size, raw_fmt, zocl_slot->slot_idx,
				zocl_slot->slot_xclbin->zx_dtbo_path);
		size += count;
	}

	read_unlock(&zdev->attr_rwlock);
	return size;
}
static DEVICE_ATTR_RO(dtbo_path);

static ssize_t kds_numcus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	struct kds_sched *kds = &zdev->kds;
	ssize_t size;

	if (!zdev || !kds)
		return 0;

	size = sprintf(buf, "%d\n", kds->cu_mgmt.num_cus);

	return size;
}
static DEVICE_ATTR_RO(kds_numcus);

static ssize_t
kds_interval_store(struct device *dev, struct device_attribute *da,
	       const char *buf, size_t count)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	u32 interval;

	if (kstrtou32(buf, 10, &interval) == -EINVAL)
		return -EINVAL;

	zdev->kds.interval = interval;

	return count;
}

static ssize_t
kds_interval_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", zdev->kds.interval);
}
static DEVICE_ATTR(kds_interval, 0644, kds_interval_show, kds_interval_store);

static ssize_t kds_xrt_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	ssize_t sz = 0;

	if (!zdev || !zdev->soft_kernel)
		return 0;

	sz += sprintf(buf+sz, "XRT GIT BRANCH: %s\n", XRT_BRANCH);
	sz += sprintf(buf+sz, "XRT GIT HASH: %s\n", XRT_HASH);
	sz += sprintf(buf+sz, "XRT GIT HASH DATE: %s\n", XRT_HASH_DATE);
	sz += sprintf(buf+sz, "XRT GIT Modified Files: %s\n", XRT_MODIFIED_FILES);

	return sz;
}
static DEVICE_ATTR_RO(kds_xrt_version);

static ssize_t zocl_get_memstat(struct device *dev, char *buf, bool raw)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	struct zocl_mem *memp = NULL;
	ssize_t size = 0;
	ssize_t count = 0;
	size_t memory_usage = 0;
	unsigned int bo_count = 0;
	const char *txt_fmt = "[%s] 0x%012llx\t(%4lluMB):\t%lluKB\t%dBOs\n";
	const char *raw_fmt = "%llu %d %llu\n";

	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

        list_for_each_entry(memp, &zdev->zm_list_head, link) {
		if (memp->zm_type == ZOCL_MEM_TYPE_STREAMING)
			continue;

		memory_usage = memp->zm_stat.memory_usage;
		bo_count = memp->zm_stat.bo_count;

		if (raw)
			count = sprintf(buf, raw_fmt, memory_usage, bo_count, 0);
		else {
			count = sprintf(buf, txt_fmt,
			    memp->zm_used ? "IN-USE" : "UNUSED",
			    memp->zm_base_addr,
			    memp->zm_size / 1024,
			    memory_usage / 1024,
			    bo_count);
		}
		buf += count;
		size += count;
	}

	read_unlock(&zdev->attr_rwlock);

	return size;
}

static ssize_t graph_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = NULL;
	struct aie_info *aie;
	struct aie_info_cmd *acmd;
	struct aie_info_packet *aiec_packet;
	ssize_t nread = 0;
	struct drm_zocl_slot* slot = NULL;
	zdev = dev_get_drvdata(dev);
	if (!zdev)
		return 0;

	// TODO hardcoding slot to 0
	slot = zdev->pr_slot[0];
	aie = slot->aie_information;
	if (!aie)
		return 0;

	/* create request */
	acmd = kmalloc(sizeof(struct aie_info_cmd), GFP_KERNEL);
	if (!acmd) {
		return -ENOMEM;
	}

	aiec_packet = kmalloc(sizeof(struct aie_info_packet), GFP_KERNEL);
	if (!aiec_packet) {
		return -ENOMEM;
	}

	/* set command */
	aiec_packet->opcode = GRAPH_STATUS;
	acmd->aiec_packet = aiec_packet;

	/* init semaphore */
	sema_init(&acmd->aiec_sem, 0);

	/* caller release the wait aied thread and wait for result */
	mutex_lock(&aie->aie_lock);
	if (waitqueue_active(&aie->aie_wait_queue)) {
		list_add_tail(&acmd->aiec_list, &aie->aie_cmd_list);
		mutex_unlock(&aie->aie_lock);
		wake_up_interruptible(&aie->aie_wait_queue);
		if (down_interruptible(&acmd->aiec_sem)) {
			nread = -ERESTARTSYS;
			goto clean;
		}
	} else {
		mutex_unlock(&aie->aie_lock);
		nread =  -ERESTARTSYS;
		goto clean;
	}

	nread = snprintf(buf, acmd->aiec_packet->size, "%s\n", acmd->aiec_packet->info);

clean:
	kfree(acmd->aiec_packet);
	kfree(acmd);
	return nread;
}
static DEVICE_ATTR_RO(graph_status);

static ssize_t read_aie_metadata(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev = NULL;
	struct drm_zocl_slot *zocl_slot = NULL;
	size_t size = 0;
	u32 nread = 0;
	u32 f_nread = 0;
	int i = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (!zocl_slot || !zocl_slot->aie_data.size)
			continue;

		size = zocl_slot->aie_data.size;

		/* Read offset should be lesser then total size of metadata */
		if (off >= size) {
			read_unlock(&zdev->attr_rwlock);
			return 0;
		}

		/* Buffer size should be greater than the size of metadata to be read */
		if (count < size - off)
			nread = count;
		else
			nread = size - off;

		memcpy(buf, (char *)zocl_slot->aie_data.data + off, nread);

		buf += nread;
		f_nread += nread;
	}
	read_unlock(&zdev->attr_rwlock);

	return f_nread;
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

static ssize_t errors_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	ssize_t size = 0;
	xrtErrorCode error_code;
	u64 timestamp;
	int i;

	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->zdev_error.ze_err) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	for (i = 0; i < zdev->zdev_error.ze_num; i++) {
		error_code = zdev->zdev_error.ze_err[i].zer_err_code;
		timestamp = zdev->zdev_error.ze_err[i].zer_ts;
		size += sprintf(buf + size, "%llu%20llu\n",
		    error_code, timestamp);
	}

	read_unlock(&zdev->attr_rwlock);

	return size;
}
static DEVICE_ATTR_RO(errors);

static ssize_t host_mem_addr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	uint64_t val = 0;

	if (zdev->host_mem)
		val = zdev->host_mem;

	return sprintf(buf, "%lld\n", val);
}
static DEVICE_ATTR_RO(host_mem_addr);

static ssize_t host_mem_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	uint64_t val = 0;

	if (zdev->host_mem_len)
		val = zdev->host_mem_len;

	return sprintf(buf, "%lld\n", val);
}
static DEVICE_ATTR_RO(host_mem_size);

static ssize_t
zocl_reset_store(struct device *dev, struct device_attribute *da,
		const char *buf, size_t count)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
        u32 val = 0;

	if (kstrtou32(buf, 10, &val) < 0 || val != 1)
		return -EINVAL;

	return zocl_reset(zdev, buf, count);
}
static DEVICE_ATTR_WO(zocl_reset);

static struct attribute *zocl_attrs[] = {
	&dev_attr_xclbinid.attr,
	&dev_attr_kds_numcus.attr,
	&dev_attr_kds_xrt_version.attr,
	&dev_attr_kds_echo.attr,
	&dev_attr_kds_stat.attr,
	&dev_attr_kds_interval.attr,
	&dev_attr_memstat.attr,
	&dev_attr_memstat_raw.attr,
	&dev_attr_errors.attr,
	&dev_attr_graph_status.attr,
	&dev_attr_dtbo_path.attr,
	&dev_attr_host_mem_addr.attr,
	&dev_attr_host_mem_size.attr,
	&dev_attr_zocl_reset.attr,
	NULL,
};

static ssize_t read_debug_ip_layout(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev = NULL;
	struct drm_zocl_slot *zocl_slot = NULL;
	size_t size = 0;
	u32 nread = 0;
	u32 f_nread = 0;
	int i = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (!zocl_slot || !zocl_slot->debug_ip)
			continue;

		size = sizeof_section(zocl_slot->debug_ip, m_debug_ip_data);
		if (off >= size) {
			read_unlock(&zdev->attr_rwlock);
			return 0;
		}

		if (count < size - off)
			nread = count;
		else
			nread = size - off;

		memcpy(buf, ((char *)zocl_slot->debug_ip) + off, nread);
		buf += nread;
		f_nread += nread;
	}
	read_unlock(&zdev->attr_rwlock);

	return f_nread;
}

static ssize_t read_ip_layout(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev = NULL;
	struct drm_zocl_slot *zocl_slot = NULL;
	size_t size = 0;
	u32 nread = 0;
	u32 f_nread = 0;
	int i = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (!zocl_slot || !zocl_slot->ip)
			continue;

		size = sizeof_section(zocl_slot->ip, m_ip_data);
		if (off >= size) {
			read_unlock(&zdev->attr_rwlock);
			return 0;
		}

		if (count < size - off)
			nread = count;
		else
			nread = size - off;

		memcpy(buf, ((char *)zocl_slot->ip) + off, nread);
		buf += nread;
		f_nread += nread;
	}
	read_unlock(&zdev->attr_rwlock);

	return f_nread;
}

static ssize_t read_connectivity(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev = NULL;
	struct drm_zocl_slot *zocl_slot = NULL;
	size_t size = 0;
	u32 nread = 0;
	u32 f_nread = 0;
	int i = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (!zocl_slot || !zocl_slot->connectivity)
			continue;

		size = sizeof_section(zocl_slot->connectivity, m_connection);
		if (off >= size) {
			read_unlock(&zdev->attr_rwlock);
			return 0;
		}

		if (count < size - off)
			nread = count;
		else
			nread = size - off;

		memcpy(buf, ((char *)zocl_slot->connectivity + off), nread);
		buf += nread;
		f_nread += nread;
	}
	read_unlock(&zdev->attr_rwlock);

	return f_nread;
}

static ssize_t read_mem_topology(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev = NULL;
	struct drm_zocl_slot *zocl_slot = NULL;
	size_t size = 0;
	u32 nread = 0;
	u32 f_nread = 0;
	int i = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
		zocl_slot = zdev->pr_slot[i];
		if (!zocl_slot || !zocl_slot->topology)
			continue;

		size = sizeof_section(zocl_slot->topology, m_mem_data);

		if (off >= size) {
			read_unlock(&zdev->attr_rwlock);
			return 0;
		}

		if (count < size - off)
			nread = count;
		else
			nread = size - off;

		memcpy(buf, ((char *)zocl_slot->topology + off), nread);
		buf += nread;
		f_nread += nread;
	}

	read_unlock(&zdev->attr_rwlock);

	return f_nread;
}

static ssize_t read_xclbin_full(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev = NULL;
	struct drm_zocl_slot *zocl_slot = NULL;
	size_t size = 0;
	u32 nread = 0;
	u32 f_nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	// Only read slot 0's xclbin - TODO: extend to multi-slot 
	zocl_slot = zdev->pr_slot[0];
	if (!zocl_slot || !zocl_slot->axlf) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	size = zocl_slot->axlf_size;
	if (off >= size) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	if (count < size - off)
		nread = count;
	else
		nread = size - off;

	memcpy(buf, ((char *)zocl_slot->axlf + off), nread);

	buf += nread;
	f_nread += nread;

	read_unlock(&zdev->attr_rwlock);

	return f_nread;
}

static struct bin_attribute aie_metadata_attr = {
	.attr = {
		.name = "aie_metadata",
		.mode = 0444
	},
	.read = read_aie_metadata,
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

static struct bin_attribute kds_custat_raw_attr = {
	.attr = {
		.name = "kds_custat_raw",
		.mode = 0444
	},
	.read = kds_custat_raw_show,
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

static struct bin_attribute xclbin_full_attr = {
	.attr = {
		.name = "xclbin_full",
		.mode = 0444
	},
	.read = read_xclbin_full,
	.write = NULL,
	.size = 0
};


static struct bin_attribute *zocl_bin_attrs[] = {
	&aie_metadata_attr,
	&connectivity_attr,
	&debug_ip_layout_attr,
	&ip_layout_attr,
	&kds_custat_raw_attr,
	&mem_topology_attr,
	&xclbin_full_attr,
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
