/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2021 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "zocl_aie.h"
#include "sched_exec.h"
#include "zocl_xclbin.h"
#include "xclbin.h"
#include "zocl_sk.h"
#include "kds_core.h"

extern int kds_mode;
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

	/* TODO: this should be as simple as */
	/* return stroe_kds_echo(&zdev->kds, buf, count); */
	return store_kds_echo(&zdev->kds, buf, count,
			      kds_mode, 0, &kds_echo);
}
static DEVICE_ATTR(kds_echo, 0644, kds_echo_show, kds_echo_store);

static ssize_t
kds_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kds_mode);
}
static DEVICE_ATTR_RO(kds_mode);

static ssize_t
kds_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	return show_kds_stat(&zdev->kds, buf);
}
static DEVICE_ATTR_RO(kds_stat);

static ssize_t
kds_custat_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	return show_kds_custat_raw(&zdev->kds, buf);
}
static DEVICE_ATTR_RO(kds_custat_raw);

static ssize_t xclbinid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	ssize_t size;

	if (!zdev)
		return 0;

	size = sprintf(buf, "%pUb\n", zdev->zdev_xclbin->zx_uuid);

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
	struct zocl_cu *zcu = NULL;
	ssize_t size = 0;
	phys_addr_t paddr;
	u32 usage;
	int status;
	int i;

	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->exec) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	for (i = 0; i < zdev->exec->num_cus; i++) {
		zcu = &zdev->exec->zcu[i];
		if (!zcu) {
			read_unlock(&zdev->attr_rwlock);
			return 0;
		}

		paddr = zocl_cu_get_paddr(zcu);
		usage = zcu->usage;
		status = zocl_cu_status_get(zcu);
		/* Use %x for now. Needs to use a better approach when support
		 * CU at higher than 4GB address range.
		 */
		size += sprintf(buf + size, "CU[@0x%llx] : %d status : %d\n",
		    (uint64_t)paddr, usage, status);
	}

	read_unlock(&zdev->attr_rwlock);

	return size;
}
static DEVICE_ATTR_RO(kds_custat);

static ssize_t kds_stats_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	struct sched_exec_core *exec = zdev->exec;
	int pending, running;
	struct list_head *pos, *next;
	struct sched_cmd *cmd;
	struct ert_packet *pkg;
	ssize_t sz;
	int i;

	if (!zdev || !zdev->exec)
		return 0;

	pending = atomic_read(&exec->scheduler->num_pending);
	running = atomic_read(&exec->scheduler->num_running);
	sz  = sprintf(buf,    "num_pending %d\n", atomic_read(&exec->scheduler->num_pending));
	sz += sprintf(buf+sz, "num_running %d\n", atomic_read(&exec->scheduler->num_running));
	sz += sprintf(buf+sz, "num_received %d\n", atomic_read(&exec->scheduler->num_received));
	sz += sprintf(buf+sz, "num_notified %d\n", atomic_read(&exec->scheduler->num_notified));

	if (running == 0)
		return sz;

	sz += sprintf(buf+sz, "running commands:\n");
	list_for_each_safe(pos, next, &exec->scheduler->cq) {
		cmd = list_entry(pos, struct sched_cmd, list);
		pkg = cmd->packet;
		sz += sprintf(buf+sz, " opcode %d\n", pkg->opcode);
		sz += sprintf(buf+sz, " count %d\n", pkg->count);
		sz += sprintf(buf+sz, " cu idx %d\n", pkg->data[0]);
		for (i = 1; i < pkg->count; i++) {
			sz += sprintf(buf+sz, " data: 0x%x\n", pkg->data[i]);
		}
	}

	return sz;
}
static DEVICE_ATTR_RO(kds_stats);

static ssize_t kds_skstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);
	struct soft_krnl *sk;
	struct soft_cu *scu;
	ssize_t sz = 0;
	int i;

	if (!zdev || !zdev->soft_kernel)
		return 0;

	sk = zdev->soft_kernel;
	mutex_lock(&sk->sk_lock);
	for (i = 0; i < sk->sk_ncus; i++) {
		scu = sk->sk_cu[i];
		if (scu) {
			sz += sprintf(buf+sz, "SK %d\n", i);
			sz += sprintf(buf+sz, " flags %d\n", scu->sc_flags);
			sz += sprintf(buf+sz, " usage %lld\n", scu->usage);
			sz += sprintf(buf+sz, " vregs[0] 0x%x\n", ((u32*)scu->sc_vregs)[0]);
		} else {
			sz += sprintf(buf+sz, "SK %d is released\n", i);
		}
	}
	mutex_unlock(&sk->sk_lock);

	return sz;
}
static DEVICE_ATTR_RO(kds_skstat);


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
	struct zocl_mem *memp;
	struct mem_topology *topo;
	ssize_t size = 0;
	ssize_t count;
	size_t memory_usage;
	unsigned int bo_count;
	const char *txt_fmt = "[%s] %s@0x%012llx\t(%4lluMB):\t%lluKB\t%dBOs\n";
	const char *raw_fmt = "%llu %d\n";
	int i;

	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->topology || !zdev->mem) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	memp = zdev->mem;
	topo = zdev->topology;

	for (i = 0; i < topo->m_count; i++) {
		if (topo->m_mem_data[i].m_type == MEM_STREAMING)
			continue;

		memory_usage = memp[i].zm_stat.memory_usage;
		bo_count = memp[i].zm_stat.bo_count;

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

static ssize_t graph_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = NULL;
	struct aie_info *aie;
	struct aie_info_cmd *acmd;
	struct aie_info_packet *aiec_packet;
	ssize_t nread = 0;
	zdev = dev_get_drvdata(dev);
	if (!zdev)
		return 0;

	aie = zdev->aie_information;
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
	struct drm_zocl_dev *zdev;
	size_t size;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	size = zdev->aie_data.size;

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

	memcpy(buf, (char *)zdev->aie_data.data + off, nread);

	read_unlock(&zdev->attr_rwlock);

	return nread;
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

static struct attribute *zocl_attrs[] = {
	&dev_attr_xclbinid.attr,
	&dev_attr_kds_numcus.attr,
	&dev_attr_kds_custat.attr,
	&dev_attr_kds_stats.attr,
	&dev_attr_kds_skstat.attr,
	&dev_attr_kds_xrt_version.attr,
	&dev_attr_kds_echo.attr,
	&dev_attr_kds_mode.attr,
	&dev_attr_kds_stat.attr,
	&dev_attr_kds_custat_raw.attr,
	&dev_attr_memstat.attr,
	&dev_attr_memstat_raw.attr,
	&dev_attr_errors.attr,
	&dev_attr_graph_status.attr,
	NULL,
};

static ssize_t read_debug_ip_layout(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev;
	size_t size;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->debug_ip) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	size = sizeof_section(zdev->debug_ip, m_debug_ip_data);

	if (off >= size) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

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
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->ip) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	size = sizeof_section(zdev->ip, m_ip_data);

	if (off >= size) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

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
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->connectivity) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	size = sizeof_section(zdev->connectivity, m_connection);

	if (off >= size) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

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
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->topology) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	size = sizeof_section(zdev->topology, m_mem_data);

	if (off >= size) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	if (count < size - off)
		nread = count;
	else
		nread = size - off;

	memcpy(buf, ((char *)zdev->topology + off), nread);

	read_unlock(&zdev->attr_rwlock);

	return nread;
}

static ssize_t read_xclbin_full(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct drm_zocl_dev *zdev;
	size_t size;
	u32 nread = 0;

	zdev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (!zdev)
		return 0;

	read_lock(&zdev->attr_rwlock);

	if (!zdev->axlf) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	size = zdev->axlf_size;

	if (off >= size) {
		read_unlock(&zdev->attr_rwlock);
		return 0;
	}

	if (count < size - off)
		nread = count;
	else
		nread = size - off;

	memcpy(buf, ((char *)zdev->axlf + off), nread);

	read_unlock(&zdev->attr_rwlock);

	return nread;
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
	&debug_ip_layout_attr,
	&ip_layout_attr,
	&connectivity_attr,
	&mem_topology_attr,
	&aie_metadata_attr,
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
