/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
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
#include "kds_core.h"

extern int kds_mode;
extern int kds_echo;

/* Attributes followed by bin_attributes. */
/* -Attributes -- */

/* -xclbinuuid-- (supersedes xclbinid) */
static ssize_t xclbinuuid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	xuid_t *xclbin_id = NULL;
	ssize_t cnt = 0;
	int err = 0;

	err = XOCL_GET_XCLBIN_ID(xdev, xclbin_id);
	if (err)
		return cnt;

	cnt = sprintf(buf, "%pUb\n", xclbin_id ? xclbin_id : 0);
	XOCL_PUT_XCLBIN_ID(xdev);
	return cnt;
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

static ssize_t board_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct xocl_dev *xdev = dev_get_drvdata(dev);

    return sprintf(buf, "%s\n",
		xdev->core.priv.board_name ? xdev->core.priv.board_name : "");
}
static DEVICE_ATTR_RO(board_name);

/* -live client contexts-- */
static ssize_t kdsstat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	int size = 0, err;
	xuid_t *xclbin_id = NULL;
	pid_t *plist = NULL;
	u32 clients, i;

	err = XOCL_GET_XCLBIN_ID(xdev, xclbin_id);
	if (err) {
		size += sprintf(buf + size, "unable to give xclbin id");
		return size;
	}

	size += sprintf(buf + size, "xclbin:\t\t\t%pUb\n",
		xclbin_id ? xclbin_id : 0);
	size += sprintf(buf + size, "outstanding execs:\t%d\n",
		atomic_read(&xdev->outstanding_execs));
	size += sprintf(buf + size, "total execs:\t\t%lld\n",
		(s64)atomic64_read(&xdev->total_execs));

	clients = get_live_clients(xdev, &plist);
	size += sprintf(buf + size, "contexts:\t\t%d\n", clients);
	size += sprintf(buf + size, "client pid:\n");
	for (i = 0; i < clients; i++)
		size += sprintf(buf + size, "\t\t\t%d\n", plist[i]);
	vfree(plist);
	XOCL_PUT_XCLBIN_ID(xdev);
	return size;
}
static DEVICE_ATTR_RO(kdsstat);

/* -live memory usage-- */
static ssize_t xocl_mm_stat(struct xocl_dev *xdev, char *buf, bool raw)
{
	int i, err;
	ssize_t count = 0;
	ssize_t size = 0;
	size_t memory_usage = 0;
	unsigned int bo_count = 0;
	const char *txt_fmt = "[%s] %s@0x%012llx (%lluMB): %lluKB %dBOs\n";
	const char *raw_fmt = "%llu %d %llu\n";
	struct mem_topology *topo = NULL;
	struct drm_xocl_mm_stat stat;

	mutex_lock(&xdev->dev_lock);

	err = XOCL_GET_GROUP_TOPOLOGY(xdev, topo);
	if (err) {
		mutex_unlock(&xdev->dev_lock);
		return err;
	}

	if (!topo) {
		size = -EINVAL;
		goto done;
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
				bo_count, 0);
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

done:
	XOCL_PUT_GROUP_TOPOLOGY(xdev);
	mutex_unlock(&xdev->dev_lock);
	return size;
}

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

/* -- KDS sysfs start -- */
static ssize_t
kds_echo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kds_echo);
}

static ssize_t
kds_echo_store(struct device *dev, struct device_attribute *da,
	       const char *buf, size_t count)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	u32 clients = 0;

	/* TODO: this should be as simple as */
	/* return stroe_kds_echo(&XDEV(xdev)->kds, buf, count); */

	if (!kds_mode)
		clients = get_live_clients(xdev, NULL);

	return store_kds_echo(&XDEV(xdev)->kds, buf, count,
			      kds_mode, clients, &kds_echo);
}
static DEVICE_ATTR(kds_echo, 0644, kds_echo_show, kds_echo_store);

static ssize_t
kds_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", kds_mode);
}
static DEVICE_ATTR_RO(kds_mode);

static ssize_t
kds_numcdma_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	struct kds_sched *kds = &XDEV(xdev)->kds;
	return sprintf(buf, "%d\n", kds->cu_mgmt.num_cdma);
}
static DEVICE_ATTR_RO(kds_numcdma);

static ssize_t
kds_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return show_kds_stat(&XDEV(xdev)->kds, buf);
}
static DEVICE_ATTR_RO(kds_stat);

static ssize_t
kds_custat_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return show_kds_custat_raw(&XDEV(xdev)->kds, buf);
}
static DEVICE_ATTR_RO(kds_custat_raw);

static ssize_t
kds_scustat_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return show_kds_scustat_raw(&XDEV(xdev)->kds, buf);
}
static DEVICE_ATTR_RO(kds_scustat_raw);

static ssize_t
kds_interrupt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	if (XDEV(xdev)->kds.cu_intr)
		return sprintf(buf, "%s\n", "cu");
	else
		return sprintf(buf, "%s\n", "ert");
}

static ssize_t
kds_interrupt_store(struct device *dev, struct device_attribute *da,
		  const char *buf, size_t count)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	struct kds_sched *kds = &XDEV(xdev)->kds;
	u32 live_clients;
	u32 cu_intr = 0;

	if (XDEV(xdev)->kds.bad_state)
		return -ENODEV;

	mutex_lock(&XDEV(xdev)->kds.lock);
	if (kds_mode)
		live_clients = kds_live_clients_nolock(&XDEV(xdev)->kds, NULL);
	else
		live_clients = get_live_clients(xdev, NULL);

	if (live_clients > 0) {
		mutex_unlock(&XDEV(xdev)->kds.lock);
		return -EBUSY;
	}

	if (!kds->cu_intr_cap)
		goto done;

	/* The last character of buf is '\n' */
	if (!strncmp(buf, "ert", count-1))
		cu_intr = 0;
	else if (!strncmp(buf, "cu", count-1))
		cu_intr = 1;
	else
		goto done;

	if (kds->cu_intr == cu_intr)
		goto done;

	if (cu_intr) {
		xocl_ert_user_mb_sleep(xdev);
		xocl_ert_user_cu_intr_cfg(xdev);
	} else {
		xocl_ert_user_mb_wakeup(xdev);
		xocl_ert_user_ert_intr_cfg(xdev);
	}

	kds->cu_intr = cu_intr;
	kds_cfg_update(&XDEV(xdev)->kds);

done:
	mutex_unlock(&XDEV(xdev)->kds.lock);

	return count;
}
static DEVICE_ATTR(kds_interrupt, 0644, kds_interrupt_show, kds_interrupt_store);

static ssize_t
ert_disable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", XDEV(xdev)->kds.ert_disable);
}

static ssize_t
ert_disable_store(struct device *dev, struct device_attribute *da,
	       const char *buf, size_t count)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	u32 live_clients;
	u32 disable;

	/* Switch KDS/ERT mode is a fundamental change on the hardware.
	 * We should only allow it when hardware is good and there is no
	 * live clients exist.
	 * Below sanity check is similar to kds_echo, maybe we should have
	 * an API to check the status of the hardware and client.
	 *
	 * Ideally, ICAP should implement the API. Since it knows if bitstream
	 * is locked. It could know if hardware is in bad state.
	 * When a client exit, if KDS is in bad state, notice ICAP before
	 * unlock bitstream.
	 */
	if (XDEV(xdev)->kds.bad_state)
		return -ENODEV;

	mutex_lock(&XDEV(xdev)->kds.lock);
	if (kds_mode)
		live_clients = kds_live_clients_nolock(&XDEV(xdev)->kds, NULL);
	else
		live_clients = get_live_clients(xdev, NULL);

	if (live_clients > 0) {
		mutex_unlock(&XDEV(xdev)->kds.lock);
		return -EBUSY;
	}

	if (kstrtou32(buf, 10, &disable) == -EINVAL || disable > 1) {
		mutex_unlock(&XDEV(xdev)->kds.lock);
		return -EINVAL;
	}

	/* If ERT subdev doesn't present, cound not enable ERT */
	if (kds_mode && !XDEV(xdev)->kds.ert)
		disable = 1;

	/* once ini_disable set to true, xrt.ini could not
	 * enable/disable ert.
	 */
	XDEV(xdev)->kds.ini_disable = true;
	XDEV(xdev)->kds.ert_disable = disable;
	mutex_unlock(&XDEV(xdev)->kds.lock);

	return count;
}
static DEVICE_ATTR(ert_disable, 0644, ert_disable_show, ert_disable_store);
/* -- KDS sysfs end -- */

static ssize_t dev_offline_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	bool offline;
	int val;

	val = xocl_drvinst_get_offline(xdev->core.drm, &offline);
	if (!val)
		val = offline ? 1 : 0;

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR(dev_offline, 0444, dev_offline_show, NULL);

static ssize_t shutdown_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", XDEV(xdev)->shutdown);
}

static ssize_t shutdown_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	u32 val;


	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	if (val == XOCL_SHUTDOWN_WITH_RESET)
		xocl_queue_work(xdev, XOCL_WORK_SHUTDOWN_WITH_RESET, 0);
	else if (val == XOCL_SHUTDOWN_WITHOUT_RESET)
		xocl_queue_work(xdev, XOCL_WORK_SHUTDOWN_WITHOUT_RESET, 0);
	else if (val == XOCL_ONLINE)
		xocl_queue_work(xdev, XOCL_WORK_ONLINE, 0);

	return count;
}
static DEVICE_ATTR(shutdown, 0644, shutdown_show, shutdown_store);

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

static ssize_t config_mailbox_channel_disable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	uint64_t ret = 0;

	xocl_mailbox_get(xdev, CHAN_DISABLE, &ret);
	return sprintf(buf, "0x%llx\n", ret);
}
static DEVICE_ATTR_RO(config_mailbox_channel_disable);

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
	return XCL_COMM_ID_SIZE;
}
static DEVICE_ATTR_RO(config_mailbox_comm_id);

static ssize_t ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	uint64_t ch_state = 0, ret = 0, daemon_state = 0;
	uint64_t ch_disable = 0, ch_switch = 0;

	xocl_mailbox_get(xdev, CHAN_STATE, &ch_state);

	if (ch_state & XCL_MB_PEER_SAME_DOMAIN)
		ret = (ch_state & XCL_MB_PEER_READY) ? 1 : 0;
	else {
		/*
		 * If xocl and xclmgmt are not in the same daemon,
		 * mark the card as ready when
		 *  1. both MB channel and daemon are ready
		 *  This is for case cloud vendor controls the xclbin download,
		 *  like azure, aws F1
		 *  2. MB channel is ready
		 *     and
		 *     all sw channels are off
		 *     and
		 *     some channels are disabled	
		 *  This is for case where msd/mpd(and plugin) are not required,
		 *  like aws V1, download xclbin is not allowed so no need to
		 *  setup mpd & plugin. In this case, admin must disable some
		 *  channels, typically 0x8, otherwise, if user run validate, 
		 *  the xclbins would be loaded through h/w mailbox, and would
		 *  end up whole mailbox being disabled.
		 */
		xocl_mailbox_get(xdev, DAEMON_STATE, &daemon_state);
		xocl_mailbox_get(xdev, CHAN_SWITCH, &ch_switch);
		xocl_mailbox_get(xdev, CHAN_DISABLE, &ch_disable);
		ret = ((ch_state & XCL_MB_PEER_READY) && (daemon_state ||
			(!ch_switch && ch_disable))) ? 1 : 0;
	}

	return sprintf(buf, "0x%llx\n", ret);
}

static DEVICE_ATTR_RO(ready);

static ssize_t interface_uuids_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	const void *uuid;
	int node = -1, off = 0;

	if (!xdev->core.fdt_blob)
		return -EINVAL;

	for (node = xocl_fdt_get_next_prop_by_name(xdev, xdev->core.fdt_blob,
		-1, PROP_INTERFACE_UUID, &uuid, NULL);
		uuid && node > 0;
		node = xocl_fdt_get_next_prop_by_name(xdev, xdev->core.fdt_blob,
		node, PROP_INTERFACE_UUID, &uuid, NULL))
		off += sprintf(buf + off, "%s\n", (char *)uuid);

	return off;
}

static DEVICE_ATTR_RO(interface_uuids);

static ssize_t logic_uuids_show(struct device *dev,
		        struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	const void *uuid = NULL;
	int node = -1, off = 0;

	if (!xdev->core.fdt_blob)
		return -EINVAL;

	node = xocl_fdt_get_next_prop_by_name(xdev, xdev->core.fdt_blob,
		-1, PROP_LOGIC_UUID, &uuid, NULL);
	if (uuid && node >= 0)
		off += sprintf(buf + off, "%s\n", (char *)uuid);

	return off;
}

static DEVICE_ATTR_RO(logic_uuids);

static ssize_t ulp_uuids_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	const void *uuid;
	int node = -1, off = 0;

	if (!xdev->ulp_blob || fdt_check_header(xdev->ulp_blob))
		return -EINVAL;

	for (node = xocl_fdt_get_next_prop_by_name(xdev, xdev->ulp_blob,
		-1, PROP_INTERFACE_UUID, &uuid, NULL);
	    uuid && node > 0;
	    node = xocl_fdt_get_next_prop_by_name(xdev, xdev->ulp_blob,
		node, PROP_INTERFACE_UUID, &uuid, NULL))
		off += sprintf(buf + off, "%s\n", (char *)uuid);

	return off;
}

static DEVICE_ATTR_RO(ulp_uuids);


/* To get the latest ECC status from peer, mig ecc is slightly different from
 * most of the sub device, we ask xocl by touch the sysfs node mig_cache_update 
 * to get the latest ECC status from its peer and save all the data to 
 * each individual mig ecc sub device instead of generate mailbox request by
 * touch mig ecc sysfs node the way likes most of the sub device.
 * It can avoid dmesg overwhelmed by mailbox msg(40x or more)
 */
static ssize_t mig_cache_update_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	xocl_update_mig_cache(xdev);

	return 0;
}
static DEVICE_ATTR_RO(mig_cache_update);

static ssize_t nodma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_dev *xdev = dev_get_drvdata(dev);

	/* A shell without dma subdev and with m2m subdev is nodma shell */
	return sprintf(buf, "%d\n", (!DMA_DEV(xdev) && M2M_DEV(xdev)));
}
static DEVICE_ATTR_RO(nodma);

/* - End attributes-- */
static struct attribute *xocl_attrs[] = {
	&dev_attr_xclbinuuid.attr,
	&dev_attr_userbar.attr,
	&dev_attr_board_name.attr,
	&dev_attr_kdsstat.attr,
	&dev_attr_memstat.attr,
	&dev_attr_memstat_raw.attr,
	&dev_attr_kds_mode.attr,
	&dev_attr_kds_echo.attr,
	&dev_attr_kds_numcdma.attr,
	&dev_attr_kds_stat.attr,
	&dev_attr_kds_custat_raw.attr,
	&dev_attr_kds_scustat_raw.attr,
	&dev_attr_kds_interrupt.attr,
	&dev_attr_ert_disable.attr,
	&dev_attr_dev_offline.attr,
	&dev_attr_mig_calibration.attr,
	&dev_attr_link_width.attr,
	&dev_attr_link_speed.attr,
	&dev_attr_link_speed_max.attr,
	&dev_attr_link_width_max.attr,
	&dev_attr_mailbox_connect_state.attr,
	&dev_attr_config_mailbox_channel_disable.attr,
	&dev_attr_config_mailbox_channel_switch.attr,
	&dev_attr_config_mailbox_comm_id.attr,
	&dev_attr_ready.attr,
	&dev_attr_interface_uuids.attr,
	&dev_attr_logic_uuids.attr,
	&dev_attr_ulp_uuids.attr,
	&dev_attr_mig_cache_update.attr,
	&dev_attr_nodma.attr,
	NULL,
};

/*
 * persist entries will only be created/destroyed by driver attach/detach
 * They will not be removed across hot reset, shutdown, switching PLP etc.
 * So please DO NOT access any subdevice APIs inside store()/show()
 */
static struct attribute *xocl_persist_attrs[] = {
	&dev_attr_shutdown.attr,
	&dev_attr_user_pf.attr,
	NULL,
};

static ssize_t fdt_blob_output(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct xocl_dev *xdev = dev_get_drvdata(dev);
	unsigned char *blob;
	size_t size;
	ssize_t ret = 0;

	if (!xdev->core.fdt_blob)
		goto bail;

	blob = xdev->core.fdt_blob;

	size = fdt_totalsize(xdev->core.fdt_blob);

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
		.mode = 0444
	},
	.read = fdt_blob_output,
	.size = 0
};

static struct bin_attribute  *xocl_bin_attrs[] = {
	&fdt_blob_attr,
	NULL,
};

static struct attribute_group xocl_attr_group = {
	.attrs = xocl_attrs,
	.bin_attrs = xocl_bin_attrs,
};

static struct attribute_group xocl_persist_attr_group = {
	.attrs = xocl_persist_attrs,
};

int xocl_init_persist_sysfs(struct xocl_dev *xdev)
{
	struct device *dev = &xdev->core.pdev->dev;
	int ret = 0;

	if (xdev->flags & XOCL_FLAGS_PERSIST_SYSFS_INITIALIZED) {
		xocl_err(dev, "persist sysfs noded already created");
		return -EINVAL;
	}

	xocl_info(dev, "Creating persist sysfs");
	ret = sysfs_create_group(&dev->kobj, &xocl_persist_attr_group);
	if (ret)
		xocl_err(dev, "create xocl persist attrs failed: %d", ret);

	xdev->flags |= XOCL_FLAGS_PERSIST_SYSFS_INITIALIZED;


	return ret;
}

void xocl_fini_persist_sysfs(struct xocl_dev *xdev)
{
	struct device *dev = &xdev->core.pdev->dev;

	if (!(xdev->flags & XOCL_FLAGS_PERSIST_SYSFS_INITIALIZED)) {
		xocl_err(dev, "persist sysfs nodes already removed");
		return;
	}

	xocl_info(dev, "Removing persist sysfs");
	sysfs_remove_group(&dev->kobj, &xocl_persist_attr_group);
	xdev->flags &= ~XOCL_FLAGS_PERSIST_SYSFS_INITIALIZED;
}

int xocl_init_sysfs(struct xocl_dev *xdev)
{
	int ret;
	struct pci_dev *rdev;
	struct device *dev = &xdev->core.pdev->dev;

	if (xdev->flags & XOCL_FLAGS_SYSFS_INITIALIZED) {
		xocl_info(dev, "Sysfs noded already created");
		return 0;
	}

	xocl_info(dev, "Creating sysfs");
	ret = sysfs_create_group(&dev->kobj, &xocl_attr_group);
	if (ret)
		xocl_err(dev, "create xocl attrs failed: %d", ret);

	xocl_get_root_dev(to_pci_dev(dev), rdev);
	ret = sysfs_create_link(&dev->kobj, &rdev->dev.kobj, "root_dev");
	if (ret) {
		xocl_err(dev, "create root device link failed: %d", ret);
		sysfs_remove_group(&dev->kobj, &xocl_attr_group);
	}

	xdev->flags |= XOCL_FLAGS_SYSFS_INITIALIZED;

	return ret;
}

void xocl_fini_sysfs(struct xocl_dev *xdev)
{
	struct device *dev = &xdev->core.pdev->dev;

	if (!(xdev->flags & XOCL_FLAGS_SYSFS_INITIALIZED)) {
		xocl_info(dev, "Sysfs nodes already removed");
		return;
	}

	xocl_info(dev, "Removing sysfs");
	sysfs_remove_link(&dev->kobj, "root_dev");
	sysfs_remove_group(&dev->kobj, &xocl_attr_group);

	xdev->flags &= ~XOCL_FLAGS_SYSFS_INITIALIZED;
}
