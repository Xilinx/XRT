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

	int val = xocl_drvinst_get_offline(lro) ? 1 : 0;

	return sprintf(buf, "%d\n", val);
}

static DEVICE_ATTR(dev_offline, 0444, dev_offline_show, NULL);

static ssize_t subdev_cmd_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	int ret = 0, i;
	char *name = (char *)buf;
	char cmd[32] = { 0 }, sdev_name[33] = { 0 };

	sscanf(name, "%32s %32s", cmd, sdev_name);

	device_lock(dev);
	if (!strcmp(cmd, "create") && !strcmp(sdev_name, "dynamic") ) {
		xclmgmt_connect_notify(lro, false);
		xocl_subdev_destroy_all(lro);
		ret = xocl_subdev_create_all(lro);
		xclmgmt_update_userpf_blob(lro);

		(void) xocl_peer_listen(lro, xclmgmt_mailbox_srv, (void *)lro);
		xclmgmt_connect_notify(lro, true);
	} else if (!strcmp(cmd, "destroy") &&
			!strcmp(sdev_name, "dynamic")) {
		for (i = XOCL_SUBDEV_LEVEL_URP; i > XOCL_SUBDEV_LEVEL_STATIC;
				i--) {
			xocl_subdev_destroy_by_level(lro, i);
		}
	}else {
		xocl_err(dev, "Invalid command");
		ret = -EINVAL;
	}

	if (!ret || ret == -EAGAIN)
		ret = count;
	else
		xocl_err(dev, "%s %s failed", cmd, sdev_name);
	device_unlock(dev);

	return ret;
}

static DEVICE_ATTR(subdev_cmd, 0200, NULL, subdev_cmd_store);


static ssize_t config_mailbox_channel_switch_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	uint64_t val;

	if (kstrtoull(buf, 0, &val) < 0)
		return -EINVAL;

	(void) xocl_mailbox_set(lro, CHAN_SWITCH, val);
	xclmgmt_connect_notify(lro, true);

	return count;
}
static ssize_t config_mailbox_channel_switch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	uint64_t ch_switch = 0;

	(void) xocl_mailbox_get(lro, CHAN_SWITCH, &ch_switch);
	return sprintf(buf, "0x%llx\n", ch_switch);
}
static DEVICE_ATTR(config_mailbox_channel_switch, 0644,
	config_mailbox_channel_switch_show,
	config_mailbox_channel_switch_store);

static ssize_t config_mailbox_comm_id_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	char *id = (char *)vzalloc(COMM_ID_SIZE);

	if (!id)
		return -ENOMEM;

	if (count > COMM_ID_SIZE)
		return -EINVAL;

	(void) memcpy(id, buf, count);
	(void) xocl_mailbox_set(lro, COMM_ID, (u64)(uintptr_t)id);
	vfree(id);
	xclmgmt_connect_notify(lro, true);

	return count;
}
static ssize_t config_mailbox_comm_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	(void) xocl_mailbox_get(lro, COMM_ID, (u64 *)buf);
	return COMM_ID_SIZE;
}
static DEVICE_ATTR(config_mailbox_comm_id, 0644,
	config_mailbox_comm_id_show,
	config_mailbox_comm_id_store);

static ssize_t blob_clear_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	if (lro->core.fdt_blob) {
		vfree(lro->core.fdt_blob);
		lro->core.fdt_blob = NULL;
	}

	return count;
}

static DEVICE_ATTR(blob_clear, 0200, NULL, blob_clear_store);

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
	&dev_attr_dev_offline.attr,
	&dev_attr_config_mailbox_channel_switch.attr,
	&dev_attr_config_mailbox_comm_id.attr,
	&dev_attr_blob_clear.attr,
	&dev_attr_subdev_cmd.attr,
	NULL,
};

static ssize_t mgmt_blob_input(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	int ret = 0;

	if (off == 0) {
		if (count < sizeof(struct fdt_header)) {
			mgmt_err(lro, "count is too small %ld", count);
			return -EINVAL;
		}

		if (fdt_check_header(buffer)) {
			mgmt_err(lro, "Invalid fdt header");
			return -EINVAL;
		}

		lro->bin_length = fdt_totalsize(buffer);
		lro->bin_buffer = vmalloc(lro->bin_length);
	}

	if (off + count >= lro->bin_length) {
		memcpy(lro->bin_buffer + off, buffer, lro->bin_length - off);
		ret = xocl_fdt_blob_input(lro, lro->bin_buffer);
		vfree(lro->bin_buffer);
		lro->bin_buffer = NULL;
		lro->bin_length = 0;

	} else
		memcpy(lro->bin_buffer + off, buffer, count);

	return ret ? ret : count;
}

static ssize_t mgmt_blob_output(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	unsigned char *blob;
	size_t size;
	ssize_t ret = 0;

	if (!lro->core.fdt_blob)
		goto bail;

	blob = lro->core.fdt_blob;
	size = fdt_totalsize(lro->core.fdt_blob);

	if (off >= size)
		goto bail;

	if (off + count > size)
		count = size - off;
	memcpy(buf, blob + off, count);

	ret = count;
bail:

	return ret;
}

static struct bin_attribute blob_input_attr = {
	.attr = {
		.name = "blob_input",
		.mode = 0600
	},
	.read = mgmt_blob_output,
	.write = mgmt_blob_input,
	.size = 0
};

static ssize_t userpf_blob_output(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	unsigned char *blob;
	size_t size;
	ssize_t ret = 0;

	if (!lro->userpf_blob)
		goto bail;

	blob = lro->userpf_blob;
	size = fdt_totalsize(lro->userpf_blob);

	if (off >= size)
		goto bail;

	if (off + count > size)
		count = size - off;
	memcpy(buf, blob + off, count);

	ret = count;
bail:

	return ret;
}

static struct bin_attribute userpf_blob_attr = {
	.attr = {
		.name = "userpf_blob",
		.mode = 0400
	},
	.read = userpf_blob_output,
	.size = 0
};

static struct bin_attribute  *mgmt_bin_attrs[] = {
	&blob_input_attr,
	&userpf_blob_attr,
	NULL,
};

static struct attribute_group mgmt_attr_group = {
	.attrs = mgmt_attrs,
	.bin_attrs = mgmt_bin_attrs,
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
