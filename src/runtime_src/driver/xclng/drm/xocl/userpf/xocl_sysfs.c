/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@xilinx.com
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
#include "common.h"

/* Attributes followed by bin_attributes. */
/* -Attributes -- */

/* -xclbinuuid-- (supersedes xclbinid) */
static ssize_t xclbinuuid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	xuid_t *xclbin_id;

	xclbin_id = XOCL_XCLBIN_ID(xdev);
	return sprintf(buf, "%pUb\n", xclbin_id ? xclbin_id : 0);
}

static DEVICE_ATTR_RO(xclbinuuid);

/* -userbar-- */
static ssize_t userbar_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", xdev->core.bar_idx);
}

static DEVICE_ATTR_RO(userbar);

static ssize_t user_pf_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* The existence of entry indicates user function. */
	return sprintf(buf, "%s", "");
}
static DEVICE_ATTR_RO(user_pf);

/* -live client contexts-- */
static ssize_t kdsstat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	int size = 0;
	xuid_t *xclbin_id;
	pid_t *plist = NULL;
	u32 clients, i;

	xclbin_id = XOCL_XCLBIN_ID(xdev);
	size += sprintf(buf + size, "xclbin:\t\t\t%pUb\n",
		xclbin_id ? xclbin_id : 0);
	size += sprintf(buf + size, "outstanding execs:\t%d\n",
		atomic_read(&xdev->outstanding_execs));
	size += sprintf(buf + size, "total execs:\t\t%ld\n",
		atomic64_read(&xdev->total_execs));

	clients = get_live_clients(xdev, &plist);
	size += sprintf(buf + size, "contexts:\t\t%d\n", clients);
	size += sprintf(buf + size, "client pid:\n");
	for (i = 0; i < clients; i++)
		size += sprintf(buf + size, "\t\t\t%d\n", plist[i]);
	vfree(plist);
	return size;
}
static DEVICE_ATTR_RO(kdsstat);

static ssize_t xocl_mm_stat(struct xocl_dev *xdev, char *buf, bool raw)
{
	int i;
	ssize_t count = 0;
	ssize_t size = 0;
	size_t memory_usage = 0;
	unsigned int bo_count = 0;
	const char *txt_fmt = "[%s] %s@0x%012llx (%lluMB): %lluKB %dBOs\n";
	const char *raw_fmt = "%llu %d\n";
	struct mem_topology *topo = NULL;
	struct drm_xocl_mm_stat stat;

	mutex_lock(&xdev->dev_lock);

	topo = XOCL_MEM_TOPOLOGY(xdev);
	if (!topo) {
		mutex_unlock(&xdev->dev_lock);
		return -EINVAL;
	}

	for (i = 0; i < topo->m_count; i++) {
		xocl_mm_get_usage_stat(XOCL_DRM(xdev), i, &stat);

		if (raw) {
			memory_usage = 0;
			bo_count = 0;
			memory_usage = stat.memory_usage;
			bo_count = stat.bo_count;

			count = sprintf(buf, raw_fmt,
				memory_usage,
				bo_count);
		} else {
			count = sprintf(buf, txt_fmt,
				topo->m_mem_data[i].m_used ?
				"IN-USE" : "UNUSED",
				topo->m_mem_data[i].m_tag,
				topo->m_mem_data[i].m_base_address,
				topo->m_mem_data[i].m_size / 1024,
				stat.memory_usage / 1024,
				stat.bo_count);
		}
		buf += count;
		size += count;
	}
	mutex_unlock(&xdev->dev_lock);
	return size;
}

/* -live memory usage-- */
static ssize_t memstat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return xocl_mm_stat(xdev, buf, false);
}
static DEVICE_ATTR_RO(memstat);

static ssize_t memstat_raw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return xocl_mm_stat(xdev, buf, true);
}
static DEVICE_ATTR_RO(memstat_raw);

static ssize_t p2p_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	u64 size;

	if (xdev->p2p_bar_addr)
		return sprintf(buf, "1\n");
	else if (xocl_get_p2p_bar(xdev, &size) >= 0 &&
			size > (1 << XOCL_PA_SECTION_SHIFT))
		return sprintf(buf, "2\n");

	return sprintf(buf, "0\n");
}

static ssize_t p2p_enable_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	struct pci_dev *pdev = xdev->core.pdev;
	int ret, p2p_bar;
	u32 enable;
	u64 size;


	if (kstrtou32(buf, 10, &enable) == -EINVAL || enable > 1)
		return -EINVAL;

	p2p_bar = xocl_get_p2p_bar(xdev, NULL);
	if (p2p_bar < 0) {
		xocl_err(&pdev->dev, "p2p bar is not configurable");
		return -EACCES;
	}

	size = xocl_get_ddr_channel_size(xdev) *
		xocl_get_ddr_channel_count(xdev); /* GB */
	size = (ffs(size) == fls(size)) ? (fls(size) - 1) : fls(size);
	size = enable ? (size + 10) : (XOCL_PA_SECTION_SHIFT - 20);
	xocl_info(&pdev->dev, "Resize p2p bar %d to %d M ", p2p_bar,
			(1 << size));
	xocl_p2p_mem_release(xdev, false);

	ret = xocl_pci_resize_resource(pdev, p2p_bar, size);
	if (ret) {
		xocl_err(&pdev->dev, "Failed to resize p2p BAR %d", ret);
		goto failed;
	}

	xdev->p2p_bar_idx = p2p_bar;
	xdev->p2p_bar_len = pci_resource_len(pdev, p2p_bar);

	if (enable) {
		ret = xocl_p2p_mem_reserve(xdev);
		if (ret) {
			xocl_err(&pdev->dev, "Failed to reserve p2p memory %d",
					ret);
		}
	}

	return count;

failed:
	return ret;

}

static DEVICE_ATTR(p2p_enable, 0644, p2p_enable_show, p2p_enable_store);

static ssize_t dev_offline_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	int val = xocl_drvinst_get_offline(xdev) ? 1 : 0;

	return sprintf(buf, "%d\n", val);
}
static ssize_t dev_offline_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	int ret;
	u32 offline;


	if (kstrtou32(buf, 10, &offline) == -EINVAL || offline > 1)
		return -EINVAL;

	device_lock(dev);
	if (offline) {
		xocl_drvinst_offline(xdev, true);
		xocl_subdev_destroy_all(xdev);
	} else {
		ret = xocl_subdev_create_all(xdev, xdev->core.priv.subdev_info,
				xdev->core.priv.subdev_num);
		if (ret) {
			xocl_err(dev, "Online subdevices failed");
			return -EIO;
		}
		xocl_drvinst_offline(xdev, false);
	}
	device_unlock(dev);

	return count;
}

static DEVICE_ATTR(dev_offline, 0644, dev_offline_show, dev_offline_store);

static ssize_t mig_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	uint64_t ret  = xocl_get_data(xdev, MIG_CALIB);

	return sprintf(buf, "0x%llx\n", ret);
}

static DEVICE_ATTR_RO(mig_calibration);

static ssize_t link_width_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	get_pcie_link_info(xdev, &width, &speed, false);
	return sprintf(buf, "%d\n", width);
}
static DEVICE_ATTR_RO(link_width);

static ssize_t link_speed_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	get_pcie_link_info(xdev, &width, &speed, false);
	return sprintf(buf, "%d\n", speed);
}
static DEVICE_ATTR_RO(link_speed);

static ssize_t link_width_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	get_pcie_link_info(xdev, &width, &speed, true);
	return sprintf(buf, "%d\n", width);
}
static DEVICE_ATTR_RO(link_width_max);

static ssize_t link_speed_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	get_pcie_link_info(xdev, &width, &speed, true);
	return sprintf(buf, "%d\n", speed);
}
static DEVICE_ATTR_RO(link_speed_max);

static ssize_t mailbox_connect_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	uint64_t ret = 0;

	xocl_mailbox_get(xdev, CHAN_STATE, &ret);
	return sprintf(buf, "0x%llx\n", ret);
}
static DEVICE_ATTR_RO(mailbox_connect_state);

static ssize_t config_mailbox_channel_switch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	uint64_t ret = 0;

	xocl_mailbox_get(xdev, CHAN_SWITCH, &ret);
	return sprintf(buf, "0x%llx\n", ret);
}
static DEVICE_ATTR_RO(config_mailbox_channel_switch);

static ssize_t config_mailbox_comm_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	xocl_mailbox_get(xdev, COMM_ID, (u64 *)buf);
	return MB_COMM_ID_LEN;
}
static DEVICE_ATTR_RO(config_mailbox_comm_id);

static ssize_t ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	uint64_t ch_state, ret;

	xocl_mailbox_get(xdev, CHAN_STATE, &ch_state);

	ret = (ch_state & MB_PEER_READY) ? 1 : 0;

	return sprintf(buf, "0x%llx\n", ret);
}

static DEVICE_ATTR_RO(ready);

/* - End attributes-- */
static struct attribute *xocl_attrs[] = {
	&dev_attr_xclbinuuid.attr,
	&dev_attr_userbar.attr,
	&dev_attr_kdsstat.attr,
	&dev_attr_memstat.attr,
	&dev_attr_memstat_raw.attr,
	&dev_attr_user_pf.attr,
	&dev_attr_p2p_enable.attr,
	&dev_attr_dev_offline.attr,
	&dev_attr_mig_calibration.attr,
	&dev_attr_link_width.attr,
	&dev_attr_link_speed.attr,
	&dev_attr_link_speed_max.attr,
	&dev_attr_link_width_max.attr,
	&dev_attr_mailbox_connect_state.attr,
	&dev_attr_config_mailbox_channel_switch.attr,
	&dev_attr_config_mailbox_comm_id.attr,
	&dev_attr_ready.attr,
	NULL,
};

static struct attribute_group xocl_attr_group = {
	.attrs = xocl_attrs,
};

int xocl_init_sysfs(struct device *dev)
{
	int ret;
	struct pci_dev *rdev;

	ret = sysfs_create_group(&dev->kobj, &xocl_attr_group);
	if (ret)
		xocl_err(dev, "create xocl attrs failed: %d", ret);

	xocl_get_root_dev(to_pci_dev(dev), rdev);
	ret = sysfs_create_link(&dev->kobj, &rdev->dev.kobj, "root_dev");
	if (ret)
		xocl_err(dev, "create root device link failed: %d", ret);

	return ret;
}

void xocl_fini_sysfs(struct device *dev)
{
	sysfs_remove_link(&dev->kobj, "root_dev");
	sysfs_remove_group(&dev->kobj, &xocl_attr_group);
}
