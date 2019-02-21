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

//Attributes followed by bin_attributes.
//
/* -Attributes -- */

/* -xclbinuuid-- (supersedes xclbinid) */
static ssize_t xclbinuuid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	return sprintf(buf, "%pUb\n", &xdev->xclbin_id);
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
	// The existence of entry indicates user function.
	return sprintf(buf, "%s", "");
}
static DEVICE_ATTR_RO(user_pf);

/* -live client contects-- */
static ssize_t kdsstat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	int size = sprintf(buf,
			   "xclbin:\t\t\t%pUb\noutstanding execs:\t%d\ntotal execs:\t\t%ld\ncontexts:\t\t%d\n",
			   &xdev->xclbin_id,
			   atomic_read(&xdev->outstanding_execs),
			   atomic64_read(&xdev->total_execs),
			   get_live_client_size(xdev));
#if 0
	buf += size;
	if (xdev->layout == NULL)
		return size;
	// Enable in 2019.1
	for (i = 0; i < xdev->layout->m_count; i++) {
		if (xdev->layout->m_ip_data[i].m_type != IP_KERNEL)
			continue;
		size += sprintf(buf, "\t%s:\t%d\n", xdev->layout->m_ip_data[i].m_name,
				xdev->ip_reference[i]);
		buf += size;
	}
#endif
	return size;
}
static DEVICE_ATTR_RO(kdsstat);

static ssize_t xocl_mm_stat(struct xocl_dev *xdev, char *buf, bool raw)
{
	int i;
	ssize_t count = 0;
	ssize_t size = 0;
	size_t memory_usage = 0;
	unsigned bo_count = 0;
	const char *txt_fmt = "[%s] %s@0x%012llx (%lluMB): %lluKB %dBOs\n";
	const char *raw_fmt = "%llu %d\n";
	struct mem_topology *topo = XOCL_MEM_TOPOLOGY(xdev);
	struct drm_xocl_mm_stat stat;
	void *drm_hdl;

	drm_hdl = xocl_dma_get_drm_handle(xdev);
	if (!drm_hdl)
		return -EINVAL;

	if (!topo)
		return -EINVAL;
										        mutex_lock(&xdev->ctx_list_lock);

	for (i = 0; i < topo->m_count; i++) {
		xocl_mm_get_usage_stat(drm_hdl, i, &stat);

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
	mutex_unlock(&xdev->ctx_list_lock);
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


	if (kstrtou32(buf, 10, &enable) == -EINVAL || enable > 1) {
		return -EINVAL;
	}

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
	int val = xdev->core.offline ? 1 : 0;

	return sprintf(buf, "%d\n", val);
}
static ssize_t dev_offline_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	int ret;
	u32 offline;


	if (kstrtou32(buf, 10, &offline) == -EINVAL || offline > 1) {
		return -EINVAL;
	}

	device_lock(dev);
	if (offline) {
		xocl_subdev_destroy_all(xdev);
		xdev->core.offline = true;
	} else {
		ret = xocl_subdev_create_all(xdev, xdev->core.priv.subdev_info,
				xdev->core.priv.subdev_num);
		if (ret) {
			xocl_err(dev, "Online subdevices failed");
			return -EIO;
		}
		xdev->core.offline = false;
	}
	device_unlock(dev);

	return count;
}

static DEVICE_ATTR(dev_offline, 0644, dev_offline_show, dev_offline_store);

static ssize_t mig_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
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
	NULL,
};

static struct attribute_group xocl_attr_group = {
	.attrs = xocl_attrs,
};

//---
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
