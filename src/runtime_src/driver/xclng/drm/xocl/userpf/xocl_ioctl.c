/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Sonal Santan
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 0, 0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <linux/eventfd.h>
#include <linux/uuid.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#include <linux/hashtable.h>
#endif
#include "version.h"
#include "common.h"

#if defined(XOCL_UUID)
xuid_t uuid_null = NULL_UUID_LE;
#endif

int xocl_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_xocl_info *obj = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	struct pci_dev *pdev = xdev->core.pdev;
	u32 major, minor, patch;

	userpf_info(xdev, "INFO IOCTL");

	sscanf(XRT_DRIVER_VERSION, "%d.%d.%d", &major, &minor, &patch);

	obj->vendor = pdev->vendor;
	obj->device = pdev->device;
	obj->subsystem_vendor = pdev->subsystem_vendor;
	obj->subsystem_device = pdev->subsystem_device;
	obj->driver_version = XOCL_DRV_VER_NUM(major, minor, patch);
	obj->pci_slot = PCI_SLOT(pdev->devfn);

	return 0;
}

int xocl_execbuf_ioctl(struct drm_device *dev,
	void *data, struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	int ret = 0;

	ret = xocl_exec_client_ioctl(drm_p->xdev,
		       DRM_XOCL_EXECBUF, data, filp);

	return ret;
}

/*
 * Create a context (only shared supported today) on a CU. Take a lock on xclbin if
 * it has not been acquired before. Shared the same lock for all context requests
 * for that process
 */
int xocl_ctx_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	int ret = 0;

	ret = xocl_exec_client_ioctl(drm_p->xdev,
		       DRM_XOCL_CTX, data, filp);

	return ret;
}

int xocl_user_intr_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	struct drm_xocl_user_intr *args = data;
	int	ret = 0;

	xocl_info(dev->dev, "USER INTR ioctl");

	if (args->fd < 0)
		return -EINVAL;

	xocl_dma_intr_register(xdev, args->msix, NULL, NULL, args->fd);
	xocl_dma_intr_config(xdev, args->msix, true);

	return ret;
}

char *kind_to_string(enum axlf_section_kind kind)
{
	switch (kind) {
	case 0:  return "BITSTREAM";
	case 1:  return "CLEARING_BITSTREAM";
	case 2:  return "EMBEDDED_METADATA";
	case 3:  return "FIRMWARE";
	case 4:  return "DEBUG_DATA";
	case 5:  return "SCHED_FIRMWARE";
	case 6:  return "MEM_TOPOLOGY";
	case 7:  return "CONNECTIVITY";
	case 8:  return "IP_LAYOUT";
	case 9:  return "DEBUG_IP_LAYOUT";
	case 10: return "DESIGN_CHECK_POINT";
	case 11: return "CLOCK_FREQ_TOPOLOGY";
	default: return "UNKNOWN";
	}
}

/* should be obsoleted after mailbox implememted */
static const struct axlf_section_header *
get_axlf_section(const struct axlf *top, enum axlf_section_kind kind)
{
	int i = 0;

	DRM_INFO("Finding %s section header", kind_to_string(kind));
	for (i = 0; i < top->m_header.m_numSections; i++) {
		if (top->m_sections[i].m_sectionKind == kind)
			return &top->m_sections[i];
	}
	DRM_INFO("Did not find AXLF section %s", kind_to_string(kind));
	return NULL;
}

static int
xocl_check_section(const struct axlf_section_header *header, uint64_t len,
		enum axlf_section_kind kind)
{
	uint64_t offset;
	uint64_t size;

	DRM_INFO("Section %s details:", kind_to_string(kind));
	DRM_INFO("  offset = 0x%llx", header->m_sectionOffset);
	DRM_INFO("  size = 0x%llx", header->m_sectionSize);

	offset = header->m_sectionOffset;
	size = header->m_sectionSize;
	if (offset + size <= len)
		return 0;

	DRM_INFO("Section %s extends beyond xclbin boundary 0x%llx\n",
			kind_to_string(kind), len);
	return -EINVAL;
}

/* Return value: Negative for error, or the size in bytes has been copied */
static int
xocl_read_sect(enum axlf_section_kind kind, void **sect, struct axlf *axlf_full)
{
	const struct axlf_section_header *memHeader;
	uint64_t xclbin_len;
	uint64_t offset;
	uint64_t size;
	int err = 0;

	memHeader = get_axlf_section(axlf_full, kind);
	if (!memHeader)
		return 0;

	xclbin_len = axlf_full->m_header.m_length;
	err = xocl_check_section(memHeader, xclbin_len, kind);
	if (err)
		return err;

	offset = memHeader->m_sectionOffset;
	size = memHeader->m_sectionSize;
	*sect = &((char *)axlf_full)[offset];

	return size;
}

/*
 * Return number of client with open ("live") contexts on CUs.
 * If this number > 0, xclbin is locked down.
 * If plist is non-NULL, the list of PIDs of live clients will also be returned.
 * Note that plist should be freed by caller.
 */
static uint live_clients(struct xocl_dev *xdev, pid_t **plist)
{
	const struct list_head *ptr;
	const struct client_ctx *entry;
	uint count = 0;
	uint i = 0;
	pid_t *pl = NULL;

	BUG_ON(!mutex_is_locked(&xdev->dev_lock));

	/* Find out number of active client */
	list_for_each(ptr, &xdev->ctx_list) {
		entry = list_entry(ptr, struct client_ctx, link);
		if (CLIENT_NUM_CU_CTX(entry) > 0)
			count++;
	}
	if (count == 0 || plist == NULL)
		goto out;

	/* Collect list of PIDs of active client */
	pl = (pid_t *)vmalloc(sizeof(pid_t) * count);
	if (pl == NULL)
		goto out;

	list_for_each(ptr, &xdev->ctx_list) {
		entry = list_entry(ptr, struct client_ctx, link);
		if (CLIENT_NUM_CU_CTX(entry) > 0) {
			pl[i] = pid_nr(entry->pid);
			i++;
		}
	}

	*plist = pl;

out:
	return count;
}

u32 get_live_clients(struct xocl_dev *xdev, pid_t **plist)
{
	u32 c;

	mutex_lock(&xdev->dev_lock);
	c = live_clients(xdev, plist);
	mutex_unlock(&xdev->dev_lock);

	return c;
}

static int
xocl_read_axlf_helper(struct xocl_drm *drm_p, struct drm_xocl_axlf *axlf_ptr)
{
	long err = 0;
	struct axlf *axlf = 0;
	struct axlf bin_obj;
	size_t size;
	int preserve_mem = 0;
	struct mem_topology *new_topology = NULL, *topology;
	struct xocl_dev *xdev = drm_p->xdev;
	xuid_t *xclbin_id;

	userpf_info(xdev, "READ_AXLF IOCTL\n");

	if (!xocl_is_unified(xdev)) {
		printk(KERN_INFO "XOCL: not unified dsa");
		return err;
	}

	if (copy_from_user(&bin_obj, axlf_ptr->xclbin, sizeof(struct axlf)))
		return -EFAULT;

	if (memcmp(bin_obj.m_magic, "xclbin2", 8))
		return -EINVAL;

	if (xocl_xrt_version_check(xdev, &bin_obj, true))
		return -EINVAL;

	if (uuid_is_null(&bin_obj.m_header.uuid)) {
		/* Legacy xclbin, convert legacy id to new id */
		memcpy(&bin_obj.m_header.uuid, &bin_obj.m_header.m_timeStamp, 8);
	}

	xclbin_id = XOCL_XCLBIN_ID(xdev);
	if (!xclbin_id)
		return -EINVAL;

	/*
	 * Support for multiple processes
	 * 1. We lock &xdev->dev_lock so no new contexts can be opened and no
	 *    live contexts can be closed
	 * 2. If more than one live context exists, we cannot swap xclbin
	 * 3. If no live contexts exists, there may still be sumbitted exec
	 *    BOs from a previous context (which was subsequently closed, but
	 *    the BOs were stuck). If exec BO count > 0, we cannot swap xclbin
	 *
	 * Note that icap subdevice also maintains xclbin ref count, which is
	 * used to lock down xclbin on mgmt pf side.
	 */
	if (!uuid_equal(xclbin_id, &bin_obj.m_header.uuid)) {
		if (live_clients(xdev, NULL) ||
			atomic_read(&xdev->outstanding_execs)) {
			printk(KERN_ERR "Current xclbin is busy, can't change\n");
			return -EBUSY;
		}
	}

	/* Ignore timestamp matching for AWS platform */
	if (!xocl_is_aws(xdev) && !xocl_verify_timestamp(xdev,
		bin_obj.m_header.m_featureRomTimeStamp)) {
		printk(KERN_ERR "TimeStamp of ROM did not match Xclbin\n");
		return -EINVAL;
	}

	printk(KERN_INFO "XOCL: VBNV and TimeStamps matched\n");

	if (uuid_equal(xclbin_id, &bin_obj.m_header.uuid)) {
		printk(KERN_INFO "Skipping repopulating topology, connectivity,ip_layout data\n");
		goto done;
	}

	/* Copy from user space and proceed. */
	axlf = vmalloc(bin_obj.m_header.m_length);
	if (!axlf) {
		DRM_ERROR("Unable to create axlf\n");
		err = -ENOMEM;
		goto done;
	}

	printk(KERN_INFO "XOCL: Marker 5\n");

	if (copy_from_user(axlf, axlf_ptr->xclbin, bin_obj.m_header.m_length)) {
		err = -EFAULT;
		goto done;
	}

	/* Populating MEM_TOPOLOGY sections. */
	size = xocl_read_sect(MEM_TOPOLOGY, (void **)&new_topology, axlf);
	if (size <= 0) {
		if (size != 0)
			goto done;
	} else if (sizeof_sect(new_topology, m_mem_data) != size) {
		err = -EINVAL;
		goto done;
	}

	topology = XOCL_MEM_TOPOLOGY(xdev);

	/*
	 * Compare MEM_TOPOLOGY previous vs new.
	 * Ignore this and keep disable preserve_mem if not for aws.
	 */
	if (xocl_is_aws(xdev) && (topology != NULL)) {
		if ((size == sizeof_sect(topology, m_mem_data)) &&
		    !memcmp(new_topology, topology, size)) {
			xocl_xdev_info(xdev, "MEM_TOPOLOGY match, preserve mem_topology.");
			preserve_mem = 1;
		} else {
			xocl_xdev_info(xdev, "MEM_TOPOLOGY mismatch, do not preserve mem_topology.");
		}
	}

	/* Switching the xclbin, make sure none of the buffers are used. */
	if (!preserve_mem) {
		err = xocl_cleanup_mem(drm_p);
		if (err)
			goto done;
	}

	err = xocl_icap_download_axlf(xdev, axlf);
	if (err) {
		DRM_ERROR("%s Fail to download\n", __func__);
		/*
		 * Don't just bail out here, always recreate drm mem
		 * since we have cleaned it up before download.
		 */
	}

	if (!preserve_mem) {
		int rc = xocl_init_mem(drm_p);
		if (err == 0)
			err = rc;
	}

done:
	if (size < 0)
		err = size;
	if (err)
		userpf_err(xdev, "err: %ld\n", err);
	else
		userpf_info(xdev, "Loaded xclbin %pUb", xclbin_id);
	vfree(axlf);
	return err;
}

int xocl_read_axlf_ioctl(struct drm_device *dev,
			 void *data,
			 struct drm_file *filp)
{
	struct drm_xocl_axlf *axlf_obj_ptr = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int err = 0;

	mutex_lock(&xdev->dev_lock);
	err = xocl_read_axlf_helper(drm_p, axlf_obj_ptr);
	mutex_unlock(&xdev->dev_lock);
	return err;
}

void reset_notify_client_ctx(struct xocl_dev *xdev)
{
	xdev->needs_reset = false;
	wmb();
}

int xocl_hot_reset_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int delay_jiffies;

	xocl_drvinst_offline(xdev, true);
	delay_jiffies = msecs_to_jiffies(XOCL_RESET_DELAY);
	schedule_delayed_work(&xdev->core.reset_work, delay_jiffies);

	xocl_xdev_info(xdev, "Scheduled reset");

	return 0; 
}

int xocl_reclock_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int err = xocl_reclock(xdev, data);

	printk(KERN_INFO "%s err: %d\n", __func__, err);
	return err;
}

int xocl_sw_mailbox_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	int ret = 0;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;

	struct drm_xocl_sw_mailbox *args;
	args = (struct drm_xocl_sw_mailbox *)data;

	/* 0 is a successful transfer */
	ret = xocl_mailbox_sw_transfer(xdev, args);
	return ret;
}

