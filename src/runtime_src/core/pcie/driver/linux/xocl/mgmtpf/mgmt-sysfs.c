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

static ssize_t mfg_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", MGMT_READ_REG32(lro, _GOLDEN_VER));
}
static DEVICE_ATTR_RO(mfg_ver);

static ssize_t recovery_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", xocl_subdev_is_vsec_recovery(lro));
}
static DEVICE_ATTR_RO(recovery);

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
	void __iomem *memcalib;

	memcalib = xocl_iores_get_base(lro, IORES_MEMCALIB);

	return sprintf(buf, "%d\n",
		(memcalib && lro->ready) ? XOCL_READ_REG32(memcalib) : 0);
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
	bool offline;
	int val;

	val = xocl_drvinst_get_offline(lro, &offline);
	if (!val)
		val = offline ? 1 : 0;

	return sprintf(buf, "%d\n", val);
}

static DEVICE_ATTR(dev_offline, 0444, dev_offline_show, NULL);

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
	char *id = (char *)vzalloc(XCL_COMM_ID_SIZE);

	if (!id)
		return -ENOMEM;

	if (count > XCL_COMM_ID_SIZE)
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
	return XCL_COMM_ID_SIZE;
}
static DEVICE_ATTR(config_mailbox_comm_id, 0644,
	config_mailbox_comm_id_show,
	config_mailbox_comm_id_store);

static ssize_t rp_program_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lro->rp_program);
}

static ssize_t rp_program_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	u32 val = 0;
	ssize_t ret;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;
	else if (val == 1) {
		if (lro->rp_program != 0)
			return -EBUSY;
		lro->rp_program = XOCL_RP_PROGRAM_REQ;
		ret = xocl_icap_download_rp(lro, XOCL_SUBDEV_LEVEL_PRP,
				RP_DOWNLOAD_NORMAL);
	} else if (val == 2) {
		ret = xclmgmt_program_shell(lro);
		(void) xocl_peer_listen(lro, xclmgmt_mailbox_srv,
				(void *)lro);
	} else if (val == 3) {
		ret = xocl_icap_download_rp(lro, XOCL_SUBDEV_LEVEL_PRP,
				RP_DOWNLOAD_CLEAR);
	} else
		return -EINVAL;

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(rp_program);

static ssize_t interface_uuids_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	const void *uuid;
	int node = -1, off = 0;

	if (!lro->core.fdt_blob && xocl_get_timestamp(lro) == 0)
		xclmgmt_load_fdt(lro);

	if (!lro->core.fdt_blob)
		return -EINVAL;

	node = xocl_fdt_get_next_prop_by_name(lro, lro->core.blp_blob,
		-1, PROP_INTERFACE_UUID, &uuid, NULL);
	if (!uuid || node < 0)
		return -EINVAL;

	off += sprintf(buf + off, "%s\n", (char *)uuid);

	for (node = xocl_fdt_get_next_prop_by_name(lro, lro->core.fdt_blob,
		-1, PROP_INTERFACE_UUID, &uuid, NULL);
	    uuid && node > 0;
	    node = xocl_fdt_get_next_prop_by_name(lro, lro->core.fdt_blob,
		node, PROP_INTERFACE_UUID, &uuid, NULL))
		off += sprintf(buf + off, "%s\n", (char *)uuid);

	return off;
}

static DEVICE_ATTR_RO(interface_uuids);

static ssize_t logic_uuids_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
        struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	const void *uuid = NULL, *blp_uuid = NULL;
	int node = -1, off = 0;

	if (!lro->core.fdt_blob && xocl_get_timestamp(lro) == 0)
		xclmgmt_load_fdt(lro);

	if (!lro->core.blp_blob)
		return -EINVAL;

	node = xocl_fdt_get_next_prop_by_name(lro, lro->core.blp_blob,
		-1, PROP_LOGIC_UUID, &blp_uuid, NULL);
	if (blp_uuid && node >= 0)
		off += sprintf(buf + off, "%s\n", (char *)blp_uuid);
	else
		return -EINVAL;

	node = xocl_fdt_get_next_prop_by_name(lro, lro->core.fdt_blob,
		-1, PROP_LOGIC_UUID, &uuid, NULL);
	if (uuid && node >= 0 && strcmp(uuid, blp_uuid))
		off += sprintf(buf + off, "%s\n", (char *)uuid);

	return off;
}

static DEVICE_ATTR_RO(logic_uuids);

static ssize_t mgmt_reset_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xclmgmt_dev *lro = dev_get_drvdata(dev);
	u32 val;
	int ret;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 4)
		return -EINVAL;

	/*
	 * Supported reset types:
	 * 1. Hot reset (reset the whole boad)
	 * 2. OCL dynamic region reset
	 * 3. ERT reset
	 * 4. Soft Kernel reset
	 */
	switch(val) {
	case 1:
		ret = (int) xclmgmt_hot_reset(lro, true);
		if (ret < 0)
			return ret;
		break;
	case 2:
		xclmgmt_ocl_reset(lro);
		break;
	case 3:
		xclmgmt_ert_reset(lro);
		break;
	case 4:
		xclmgmt_softkernel_reset(lro);
		break;
	}

	return count;
}
static DEVICE_ATTR_WO(mgmt_reset);

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
	&dev_attr_mfg_ver.attr,
	&dev_attr_recovery.attr,
	&dev_attr_mgmt_pf.attr,
	&dev_attr_flash_type.attr,
	&dev_attr_board_name.attr,
	&dev_attr_dev_offline.attr,
	&dev_attr_config_mailbox_channel_switch.attr,
	&dev_attr_config_mailbox_comm_id.attr,
	&dev_attr_rp_program.attr,
	&dev_attr_interface_uuids.attr,
	&dev_attr_logic_uuids.attr,
	&dev_attr_mgmt_reset.attr,
	NULL,
};

static ssize_t fdt_blob_output(struct file *filp, struct kobject *kobj,
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

static struct bin_attribute fdt_blob_attr = {
	.attr = {
		.name = "fdt_blob",
		.mode = 0400
	},
	.read = fdt_blob_output,
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
	&userpf_blob_attr,
	&fdt_blob_attr,
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

	err = sysfs_create_link(&dev->kobj, &dev->parent->kobj, "dparent");
	if (err) {
		xocl_err(dev, "create parent link failed");
		sysfs_remove_group(&dev->kobj, &mgmt_attr_group);
	}

	return err;
}

void mgmt_fini_sysfs(struct device *dev)
{
	sysfs_remove_link(&dev->kobj, "dparent");
	sysfs_remove_group(&dev->kobj, &mgmt_attr_group);
}
