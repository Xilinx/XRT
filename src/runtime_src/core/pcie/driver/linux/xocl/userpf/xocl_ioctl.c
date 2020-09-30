/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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
#include <linux/eventfd.h>
#include <linux/uuid.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#include <linux/hashtable.h>
#endif
#include "version.h"
#include "common.h"

extern int kds_mode;

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

	if (kds_mode == 1)
		ret = xocl_client_ioctl(drm_p->xdev,
					DRM_XOCL_EXECBUF, data, filp);
	else
		ret = xocl_exec_client_ioctl(drm_p->xdev,
					     DRM_XOCL_EXECBUF, data, filp);

	return ret;
}

int xocl_execbuf_callback_ioctl(struct drm_device *dev,
			  void *data,
			  struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	int ret = 0;

	if (kds_mode == 1)
		ret = xocl_client_ioctl(drm_p->xdev,
					DRM_XOCL_EXECBUF_CB, data, filp);
	else
		ret = xocl_exec_client_ioctl(drm_p->xdev,
					     DRM_XOCL_EXECBUF_CB, data, filp);

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

	if (kds_mode == 1)
		ret = xocl_client_ioctl(drm_p->xdev,
					DRM_XOCL_CTX, data, filp);
	else
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
	uint count = 0;
	uint i = 0;
	pid_t *pl = NULL;
	const struct client_ctx *entry;

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

/* TODO: Move to xocl_kds.c, when start to create sysfs nodes for new kds. */
u32 get_live_clients(struct xocl_dev *xdev, pid_t **plist)
{
	u32 c;

	if (kds_mode) {
		c = xocl_kds_live_clients(xdev, plist);
	} else {
		mutex_lock(&xdev->dev_lock);
		c = live_clients(xdev, plist);
		mutex_unlock(&xdev->dev_lock);
	}

	return c;
}

static bool xclbin_downloaded(struct xocl_dev *xdev, xuid_t *xclbin_id)
{
	bool ret = false;
	int err = 0;
	xuid_t *downloaded_xclbin =  NULL;
	bool changed;

	xocl_p2p_conf_status(xdev, &changed);
	if (changed) {
		userpf_info(xdev, "p2p configure changed\n");
		return false;
	}

	err = XOCL_GET_XCLBIN_ID(xdev, downloaded_xclbin);
	if (err)
		return ret;

	if (downloaded_xclbin && uuid_equal(downloaded_xclbin, xclbin_id)) {
		ret = true;
		userpf_info(xdev, "xclbin is already downloaded\n");
	}
	XOCL_PUT_XCLBIN_ID(xdev);

	return ret;
}

static int xocl_preserve_mem(struct xocl_drm *drm_p, struct mem_topology *new_topology, size_t size)
{
	int ret = 0;
	struct mem_topology *topology = NULL;
	struct xocl_dev *xdev = drm_p->xdev;

	ret = XOCL_GET_MEM_TOPOLOGY(xdev, topology);
	if (ret)
		return 0;

	/*
	 * Compare MEM_TOPOLOGY previous vs new.
	 * Ignore this and keep disable preserve_mem if not for aws.
	 */
	if (xocl_icap_get_data(xdev, DATA_RETAIN) && (topology != NULL) && drm_p->mm) {
		if ((size == sizeof_sect(topology, m_mem_data)) &&
		    !memcmp(new_topology, topology, size)) {
			userpf_info(xdev, "preserving mem_topology.");
			ret = 1;
		} else {
			userpf_info(xdev, "not preserving mem_topology.");
		}
	}
	XOCL_PUT_MEM_TOPOLOGY(xdev);

	return ret;
}

static bool xocl_xclbin_in_use(struct xocl_dev *xdev)
{
	BUG_ON(!xdev);

	if (live_clients(xdev, NULL) || atomic_read(&xdev->outstanding_execs)) {
		userpf_err(xdev, " Current xclbin is in-use, can't change\n");
		return true;
	}
	return false;
}

static int
xocl_read_axlf_helper(struct xocl_drm *drm_p, struct drm_xocl_axlf *axlf_ptr)
{
	long err = 0;
	struct axlf *axlf = NULL;
	struct axlf bin_obj;
	size_t size = 0;
	int preserve_mem = 0;
	struct mem_topology *new_topology = NULL;
	struct xocl_dev *xdev = drm_p->xdev;
	const struct axlf_section_header * dtbHeader = NULL;
	void *ulp_blob;
	void *kernels;
	int rc;

	if (!xocl_is_unified(xdev)) {
		userpf_err(xdev, "XOCL: not unified Shell\n");
		return -EINVAL;
	}

	if (copy_from_user(&bin_obj, axlf_ptr->xclbin, sizeof(struct axlf)))
		return -EFAULT;
	if (memcmp(bin_obj.m_magic, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2))) {
		userpf_err(xdev, "invalid xclbin magic string\n");
		return -EINVAL;
	}
	if (uuid_is_null(&bin_obj.m_header.uuid)) {
		userpf_err(xdev, "invalid xclbin uuid\n");
		return -EINVAL;
	}

	if (kds_mode) {
		if (is_bad_state(&XDEV(xdev)->kds)) {
			err = -EDEADLK;
			goto done;
		}
	} else {
		if (list_is_singular(&xdev->ctx_list) && atomic_read(&xdev->outstanding_execs)) {
			err = -EDEADLK;
			goto done;
		}
	}

	if (xclbin_downloaded(xdev, &bin_obj.m_header.uuid))
		goto done;

	/*
	 * Coupling scheduler(context, exec BOs) at this place is a bad idea.
	 *
	 * The load xclbin and lock xclbin operation are separated.
	 * The xclbin was locked until a context was opened and unlocked until
	 * all of the contexts were closed.
	 * If some processes are downloading the same xclbin. Only the first
	 * one could reach below lines.
	 * If some processes are downloading different xclbins. The second one
	 * got dev_lock could go here.  Let's see if ICAP could protect the
	 * sequence of download xclbin and open context on downloaded xclbin.
	 *
	 * If open context locked bitstream, xocl_icap_download_axlf() would
	 * failed later on, since the bitstream is busy.
	 * If xocl_icap_download_axlf() successed, open context on prev xclbin
	 * would fail. This is the same as current implementation. This is the
	 * cost that we have to pay if we are not able to lock xclbin after
	 * loaded it.
	 *
	 * After all, ICAP subdevice does everything to protect xclbin. If
	 * scheduler is ready to work on a new xclbin, like all of contexts are
	 * closed, it should only notice ICAP not here.
	 *
	 * "Note that icap subdevice also maintains xclbin ref count, which is
	 * used to lock down xclbin on mgmt pf side."
	 * I found this statement is not correct anymore. The ref count is using
	 * on user pf side.
	 */
	if (kds_mode)
		goto skip1;

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
	if (xocl_xclbin_in_use(xdev)) {
		err = -EBUSY;
		goto done;
	}

skip1:
	/* Really need to download, sanity check xclbin, first. */
	if (xocl_xrt_version_check(xdev, &bin_obj, true)) {
		userpf_err(xdev, "Xclbin isn't supported by current XRT\n");
		err = -EINVAL;
		goto done;
	}

	if (!xocl_verify_timestamp(xdev,
		bin_obj.m_header.m_featureRomTimeStamp)) {
		userpf_err(xdev, "TimeStamp of ROM did not match Xclbin\n");
		err = -EOPNOTSUPP;
		goto done;
	}

	/* Copy bitstream from user space and proceed. */
	axlf = vmalloc(bin_obj.m_header.m_length);
	if (!axlf) {
		userpf_err(xdev, "Unable to alloc mem for xclbin, size=%llu\n",
			bin_obj.m_header.m_length);
		err = -ENOMEM;
		goto done;
	}
	if (copy_from_user(axlf, axlf_ptr->xclbin, bin_obj.m_header.m_length)) {
		err = -EFAULT;
		goto done;
	}

	dtbHeader = xocl_axlf_section_header(xdev, axlf,
		PARTITION_METADATA);
	if (dtbHeader) {
		ulp_blob = (char*)axlf + dtbHeader->m_sectionOffset;
		if (fdt_check_header(ulp_blob) || fdt_totalsize(ulp_blob) >
				dtbHeader->m_sectionSize) {
			userpf_err(xdev, "Invalid PARTITION_METADATA");
			err = -EINVAL;
			goto done;
		}

		if (xdev->ulp_blob)
			vfree(xdev->ulp_blob);

		xdev->ulp_blob = vmalloc(fdt_totalsize(ulp_blob));
		if (!xdev->ulp_blob) {
			err = -ENOMEM;
			goto done;
		}
		memcpy(xdev->ulp_blob, ulp_blob, fdt_totalsize(ulp_blob));

		xocl_xdev_info(xdev, "check interface uuid");
		if (!XDEV(xdev)->fdt_blob) {
			userpf_err(xdev, "did not find platform dtb");
			err = -EINVAL;
			goto done;
		}
		err = xocl_fdt_check_uuids(xdev,
			(const void *)XDEV(xdev)->fdt_blob,
			(const void *)((char*)xdev->ulp_blob));
		if (err) {
			userpf_err(xdev, "interface uuids do not match");
			err = -EINVAL;
			goto done;
		}
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

	preserve_mem = xocl_preserve_mem(drm_p, new_topology, size);

	/* Switching the xclbin, make sure none of the buffers are used. */
	if (!preserve_mem) {
		err = xocl_cleanup_mem(drm_p);
		if (err)
			goto done;
	}

	if (XDEV(xdev)->kernels != NULL) {
		vfree(XDEV(xdev)->kernels);
		XDEV(xdev)->kernels = NULL;
	}

	/* There is a corner case.
	 * A xclbin might only have an ap_ctrl_none kernel in ip_layout and
	 * without any arguments. In this case, ksize would be 0, there is no
	 * kernel information anywhere.
	 */
	if (axlf_ptr->ksize) {
		kernels = vmalloc(axlf_ptr->ksize);
		if (!kernels) {
			userpf_err(xdev, "Unable to alloc mem for kernels, size=%u\n",
				   axlf_ptr->ksize);
			err = -ENOMEM;
			goto done;
		}
		if (copy_from_user(kernels, axlf_ptr->kernels, axlf_ptr->ksize)) {
			vfree(kernels);
			err = -EFAULT;
			goto done;
		}
		XDEV(xdev)->ksize = axlf_ptr->ksize;
		XDEV(xdev)->kernels = kernels;
	}

	err = xocl_icap_download_axlf(xdev, axlf);
	if (err) {
		/* TODO: remove this. Coupling scheduler is a bad idea.
		 */
		/*
		 * We have to clear uuid cached in scheduler here if
		 * download xclbin failed
		 */
		if (kds_mode)
			(void) xocl_kds_reset(xdev, &uuid_null);
		else
			(void) xocl_exec_reset(xdev, &uuid_null);
		/*
		 * Don't just bail out here, always recreate drm mem
		 * since we have cleaned it up before download.
		 */
	}

	/* The finial step is to update KDS configuration */
	if (kds_mode)
		xocl_kds_update(xdev);

	if (!preserve_mem) {
		rc = xocl_init_mem(drm_p);
		if (err == 0)
			err = rc;
	}

	/*
	 * This is a workaround for u280 only
	 */
	if (!err &&  size >=0)
		xocl_p2p_refresh_rbar(xdev);

done:
	if (size < 0)
		err = size;
	if (err) {
		userpf_err(xdev, "Failed to download xclbin, err: %ld\n", err);
	}
	else
		userpf_info(xdev, "Loaded xclbin %pUb", &bin_obj.m_header.uuid);

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

int xocl_hot_reset_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;

	xocl_drvinst_set_offline(xdev->core.drm, true);
	xocl_queue_work(xdev, XOCL_WORK_RESET, XOCL_RESET_DELAY);
	xocl_xdev_info(xdev, "Scheduled reset");

	return 0;
}

int xocl_reclock_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int err;

	xocl_drvinst_set_offline(xdev->core.drm, true);
	err = xocl_reclock(xdev, data);
	xocl_drvinst_set_offline(xdev->core.drm, false);

	userpf_info(xdev, "%s err: %d\n", __func__, err);
	return err;
}

int xocl_alloc_cma_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct drm_xocl_alloc_cma_info *cma_info = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int err = 0;

	mutex_lock(&xdev->dev_lock);

	if (xocl_xclbin_in_use(xdev)) {
		err = -EBUSY;
		goto done;
	}

	err = xocl_cma_bank_alloc(xdev, cma_info);
done:
	mutex_unlock(&xdev->dev_lock);
	return err;
}

int xocl_free_cma_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int err = 0;

	mutex_lock(&xdev->dev_lock);

	if (xocl_xclbin_in_use(xdev) || xocl_check_topology(drm_p))
		err = -EBUSY;
	else
		xocl_cma_bank_free(xdev);
	mutex_unlock(&xdev->dev_lock);

	return err;
}
