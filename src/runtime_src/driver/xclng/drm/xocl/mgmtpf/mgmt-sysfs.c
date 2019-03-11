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
#include "version.h"

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

	return sprintf(buf, "%d\n", lro->core.bar_idx);
}
static DEVICE_ATTR_RO(userbar);

static ssize_t flash_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n",
		lro->core.priv.flash_type ? lro->core.priv.flash_type : "");
}
static DEVICE_ATTR_RO(flash_type);

static ssize_t board_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n",
		lro->core.priv.board_name ? lro->core.priv.board_name : "");
}
static DEVICE_ATTR_RO(board_name);

static ssize_t mfg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", (lro->core.priv.flags & XOCL_DSAFLAG_MFG) != 0);
}
static DEVICE_ATTR_RO(mfg);

static ssize_t feature_rom_offset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%llu\n", lro->core.feature_rom_offset);
}
static DEVICE_ATTR_RO(feature_rom_offset);

static ssize_t mgmt_pf_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* The existence of entry indicates mgmt function. */
	return sprintf(buf, "%s", "");
}
static DEVICE_ATTR_RO(mgmt_pf);

static ssize_t version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 major, minor, patch;

	sscanf(XRT_DRIVER_VERSION, "%d.%d.%d", &major, &minor, &patch);
	return sprintf(buf, "%d\n", XOCL_DRV_VER_NUM(major, minor, patch));
}
static DEVICE_ATTR_RO(version);

static ssize_t slot_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", PCI_SLOT(lro->core.pdev->devfn));
}
static DEVICE_ATTR_RO(slot);

static ssize_t link_speed_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	get_pcie_link_info(lro, &width, &speed, false);
	return sprintf(buf, "%d\n", speed);
}
static DEVICE_ATTR_RO(link_speed);

static ssize_t link_width_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	get_pcie_link_info(lro, &width, &speed, false);
	return sprintf(buf, "%d\n", width);
}
static DEVICE_ATTR_RO(link_width);

static ssize_t link_speed_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	get_pcie_link_info(lro, &width, &speed, true);
	return sprintf(buf, "%d\n", speed);
}
static DEVICE_ATTR_RO(link_speed_max);

static ssize_t link_width_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned short speed, width;
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	get_pcie_link_info(lro, &width, &speed, true);
	return sprintf(buf, "%d\n", width);
}
static DEVICE_ATTR_RO(link_width_max);

static ssize_t mig_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		lro->ready ? MGMT_READ_REG32(lro, GENERAL_STATUS_BASE) : 0);
}
static DEVICE_ATTR_RO(mig_calibration);

static ssize_t xpr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", XOCL_DSA_XPR_ON(lro));
}
static DEVICE_ATTR_RO(xpr);

static ssize_t ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lro->ready);
}
static DEVICE_ATTR_RO(ready);

static ssize_t dev_offline_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	int val = lro->core.offline ? 1 : 0;

	return sprintf(buf, "%d\n", val);
}

static ssize_t dev_offline_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	int ret;
	u32 offline;

	if (kstrtou32(buf, 10, &offline) == -EINVAL || offline > 1)
		return -EINVAL;

	device_lock(dev);
	if (offline) {
		ret = health_thread_stop(lro);
		if (ret) {
			xocl_err(dev, "stop health thread failed");
			return -EIO;
		}
		xocl_subdev_destroy_all(lro);
		lro->core.offline = true;
	} else {
		ret = xocl_subdev_create_all(lro, lro->core.priv.subdev_info,
			lro->core.priv.subdev_num);
		if (ret) {
			xocl_err(dev, "Online subdevices failed");
			return -EIO;
		}
		ret = health_thread_start(lro);
		if (ret) {
			xocl_err(dev, "start health thread failed");
			return -EIO;
		}
		lro->core.offline = false;
	}
	device_unlock(dev);

	return count;
}

static DEVICE_ATTR(dev_offline, 0644, dev_offline_show, dev_offline_store);

static ssize_t subdev_online_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	int ret;
	char *name = (char *)buf;

	device_lock(dev);
	ret = xocl_subdev_create_by_name(lro, name);
	if (ret)
		xocl_err(dev, "create subdev by name failed");
	else
		ret = count;
	device_unlock(dev);

	return ret;
}

static DEVICE_ATTR(subdev_online, 0200, NULL, subdev_online_store);

static ssize_t subdev_offline_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	int ret;
	char *name = (char *)buf;

	device_lock(dev);
	ret = xocl_subdev_destroy_by_name(lro, name);
	if (ret)
		xocl_err(dev, "destroy subdev by name failed");
	else
		ret = count;
	device_unlock(dev);

	return ret;
}

static DEVICE_ATTR(subdev_offline, 0200, NULL, subdev_offline_store);

static ssize_t sw_chan_en_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	uint64_t val, ch_switch;

	if (kstrtoull(buf, 0, &val) < 0)
		return -EINVAL;

	if (val & 0x1) {
		mgmt_err(lro, "can only set BIT1 to BIT63");
		return -EINVAL;
	}
	xocl_mailbox_get(lro, CHAN_SWITCH, &ch_switch);
	ch_switch |= val;
	xocl_mailbox_set(lro, CHAN_SWITCH, &ch_switch);
	xclmgmt_chan_switch_notify(lro);

	return count;
}
static ssize_t sw_chan_en_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	uint64_t ch_switch = 0;

	xocl_mailbox_get(lro, CHAN_SWITCH, &ch_switch);

	return sprintf(buf, "0x%llx\n", ch_switch);
}

static DEVICE_ATTR(sw_chan_en, 0644, sw_chan_en_show, sw_chan_en_store);

static ssize_t sw_chan_reset_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	xocl_mailbox_set(lro, CH_SWITCH_RST, NULL);
	xclmgmt_chan_switch_notify(lro);
	return count;
}

static DEVICE_ATTR(sw_chan_reset, 0200, NULL, sw_chan_reset_store);

static struct attribute *mgmt_attrs[] = {
	&dev_attr_instance.attr,
	&dev_attr_error.attr,
	&dev_attr_userbar.attr,
	&dev_attr_version.attr,
	&dev_attr_slot.attr,
	&dev_attr_link_speed.attr,
	&dev_attr_link_width.attr,
	&dev_attr_link_speed_max.attr,
	&dev_attr_link_width_max.attr,
	&dev_attr_mig_calibration.attr,
	&dev_attr_xpr.attr,
	&dev_attr_ready.attr,
	&dev_attr_mfg.attr,
	&dev_attr_mgmt_pf.attr,
	&dev_attr_flash_type.attr,
	&dev_attr_board_name.attr,
	&dev_attr_feature_rom_offset.attr,
	&dev_attr_dev_offline.attr,
	&dev_attr_subdev_online.attr,
	&dev_attr_subdev_offline.attr,
	&dev_attr_sw_chan_en.attr,
	&dev_attr_sw_chan_reset.attr,
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
