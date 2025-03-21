/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
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
#include <linux/delay.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#include <linux/hashtable.h>
#endif
#include "version.h"
#include "common.h"
#include "mailbox_proto.h"

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

	ret = xocl_client_ioctl(drm_p->xdev, DRM_XOCL_EXECBUF, data, filp);

	return ret;
}

int xocl_hw_ctx_execbuf_ioctl(struct drm_device *dev,
	void *data, struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	int ret = 0;

	ret = xocl_hw_ctx_command(drm_p->xdev, data, filp);

	return ret;
}

int xocl_execbuf_callback_ioctl(struct drm_device *dev,
			  void *data,
			  struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	int ret = 0;

	ret = xocl_client_ioctl(drm_p->xdev, DRM_XOCL_EXECBUF_CB, data, filp);

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

	ret = xocl_client_ioctl(drm_p->xdev, DRM_XOCL_CTX, data, filp);

	return ret;
}

/* 
 * Create a hw context on a Slot. First Load the given xclbin to a slot and take 
 * a lock on xclbin if it has not been acquired before. Also return the hw_context 
 * once loaded succfully. Shared the same context for all context requests 
 * for that process if loded into same slot. 
 */ 
int xocl_create_hw_ctx_ioctl(struct drm_device *dev, void *data, 
        struct drm_file *filp) 
{ 
        struct drm_xocl_create_hw_ctx *drm_hw_ctx = 
                (struct drm_xocl_create_hw_ctx *)data; 
        struct xocl_drm *drm_p = dev->dev_private; 
        struct xocl_dev *xdev = drm_p->xdev; 
        struct drm_xocl_axlf axlf_obj_ptr = {}; 
        uint32_t slot_id = 0; 
        int ret = 0; 
 
        if (copy_from_user(&axlf_obj_ptr, drm_hw_ctx->axlf_ptr, sizeof(struct drm_xocl_axlf))) 
                return -EFAULT; 
 
        /* Download the XCLBIN to the device first */ 
        mutex_lock(&xdev->dev_lock); 
        ret = xocl_read_axlf_helper(drm_p, &axlf_obj_ptr, drm_hw_ctx->qos, &slot_id); 
        mutex_unlock(&xdev->dev_lock); 
        if (ret) 
                return ret; 
 
        xdev->is_legacy_ctx = false; 
 
        /* Create the HW Context and lock the bitstream */ 
        /* Slot id is 0 for now */ 
        return xocl_create_hw_context(xdev, filp, drm_hw_ctx, 0);
} 
 
/* 
 * Destroy the given hw context. unlock the slot. 
 */ 
int xocl_destroy_hw_ctx_ioctl(struct drm_device *dev, void *data, 
        struct drm_file *filp) 
{ 
        struct drm_xocl_destroy_hw_ctx *drm_hw_ctx = 
                (struct drm_xocl_destroy_hw_ctx *)data; 
        struct xocl_drm *drm_p = dev->dev_private; 
        struct xocl_dev *xdev = drm_p->xdev; 
 
        if (!drm_hw_ctx) 
                return -EINVAL; 
 
        return xocl_destroy_hw_context(xdev, filp, drm_hw_ctx); 
}

/*
 * Open a context (only shared supported today) on a CU under the given hw_context.
 * Return the acquired cu index for further reference.
 */
int xocl_open_cu_ctx_ioctl(struct drm_device *dev, void *data,
        struct drm_file *filp)
{
        struct drm_xocl_open_cu_ctx *drm_cu_ctx =
                (struct drm_xocl_open_cu_ctx *)data;
        struct xocl_drm *drm_p = dev->dev_private;
        struct xocl_dev *xdev = drm_p->xdev;

        if (!drm_cu_ctx)
                return -EINVAL;

        return xocl_open_cu_context(xdev, filp, drm_cu_ctx);
}

/*
 * Close the context (only shared supported today) on a CU under the given hw_context.
 */
int xocl_close_cu_ctx_ioctl(struct drm_device *dev, void *data,
        struct drm_file *filp)
{
        struct drm_xocl_close_cu_ctx *drm_cu_ctx =
                (struct drm_xocl_close_cu_ctx *)data;
        struct xocl_drm *drm_p = dev->dev_private;
        struct xocl_dev *xdev = drm_p->xdev;

        if (!drm_cu_ctx)
                return -EINVAL;

        return xocl_close_cu_context(xdev, filp, drm_cu_ctx);
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

static char *kind_to_string(enum axlf_section_kind kind)
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
	/* Check for overflow and boundary conditions*/
	if (size > len || offset > len || offset > len - size) {
		DRM_INFO("Section %s extends beyond xclbin boundary 0x%llx\n",
				kind_to_string(kind), len);
		return -EINVAL;
	}
	return 0;
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

	c = xocl_kds_live_clients(xdev, plist);

	return c;
}

static bool ps_xclbin_downloaded(struct xocl_dev *xdev, xuid_t *xclbin_id, uint32_t *slot_id)
{
	bool ret = false;
	int err = 0;
	int i = 0;
	xuid_t *downloaded_xclbin =  NULL;

	for (i = 0; i < MAX_SLOT_SUPPORT; i++) {
		if (i == DEFAULT_PL_PS_SLOT)
			continue; 

		err = XOCL_GET_XCLBIN_ID(xdev, downloaded_xclbin, i);
		if (err)
			return ret;

		if (downloaded_xclbin && uuid_equal(downloaded_xclbin, xclbin_id)) {
			ret = true;
			*slot_id = i;
			userpf_info(xdev, "xclbin is already downloaded to slot %d\n", i);
			break;
		}

		XOCL_PUT_XCLBIN_ID(xdev, i);
	}

	return ret;
}

static bool xclbin_downloaded(struct xocl_dev *xdev, xuid_t *xclbin_id,
		uint32_t slot_id)
{
	bool ret = false;
	int err = 0;
	xuid_t *downloaded_xclbin =  NULL;
	bool changed = false;

	xocl_p2p_conf_status(xdev, &changed);
	if (changed) {
		userpf_info(xdev, "p2p configure changed\n");
		return false;
	}

	err = XOCL_GET_XCLBIN_ID(xdev, downloaded_xclbin, slot_id);
	if (err)
		return ret;

	if (downloaded_xclbin && uuid_equal(downloaded_xclbin, xclbin_id)) {
		ret = true;
		userpf_info(xdev, "xclbin is already downloaded\n");
	}

	XOCL_PUT_XCLBIN_ID(xdev, slot_id);

	return ret;
}

static int xocl_preserve_memcmp(struct mem_topology *new_topo, struct mem_topology *mem_topo, size_t size)
{
	int ret = -1;
	size_t i, j = 0;

	if (mem_topo->m_count != new_topo->m_count)
		return ret;

	for (i = 0; i < mem_topo->m_count; ++i) {
		if (convert_mem_tag(mem_topo->m_mem_data[i].m_tag) == MEM_TAG_HOST)
			continue;
		for (j = 0; j < new_topo->m_count; ++j) {
			if (memcmp(mem_topo->m_mem_data[i].m_tag, 
				new_topo->m_mem_data[j].m_tag, 16))
				continue;
			if (memcmp(&mem_topo->m_mem_data[i], &new_topo->m_mem_data[j],
				 sizeof(struct mem_data))) {
				ret = -1;
			} else {
				ret = 0;
				break;
			}

		}
		if (ret)
			break;
	}

	return ret;
}

static int xocl_preserve_mem(struct xocl_drm *drm_p, struct mem_topology *new_topology, size_t size)
{
	int ret = 0;
	struct mem_topology *topology = NULL;
	struct xocl_dev *xdev = drm_p->xdev;
	uint32_t legacy_slot_id = DEFAULT_PL_PS_SLOT;

	ret = XOCL_GET_MEM_TOPOLOGY(xdev, topology, legacy_slot_id);
	if (ret)
		return ret;

	if (!topology) {
		XOCL_PUT_MEM_TOPOLOGY(xdev, legacy_slot_id);
		return 0;
	}

	/*
	 * Compare MEM_TOPOLOGY previous vs new.
	 * Ignore this and keep disable preserve_mem if not for aws.
	 */
	if (xocl_icap_get_data(xdev, DATA_RETAIN) && (topology != NULL) &&
		drm_p->xocl_mm->mm) {
		if ((size == sizeof_sect(topology, m_mem_data)) &&
		    !xocl_preserve_memcmp(new_topology, topology, size)) {
			userpf_info(xdev, "preserving mem_topology.");
			ret = 1;
		} else {
			userpf_info(xdev, "not preserving mem_topology.");
		}
	}

	XOCL_PUT_MEM_TOPOLOGY(xdev, legacy_slot_id);
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
xocl_resolver(struct xocl_dev *xdev, struct axlf *axlf, xuid_t *xclbin_id,
		uint32_t qos,	uint32_t *slot_id)
{
	uint32_t s_id = DEFAULT_PL_PS_SLOT;
	int ret = 0;

	if (xocl_axlf_section_header(xdev, axlf, BITSTREAM) ||
		xocl_axlf_section_header(xdev, axlf, BITSTREAM_PARTIAL_PDI) ||
		!xocl_axlf_section_header(xdev, axlf, SOFT_KERNEL)) {
		s_id = DEFAULT_PL_PS_SLOT;
		if (xclbin_downloaded(xdev, xclbin_id, s_id)) {
			if (qos & XOCL_AXLF_FORCE_PROGRAM) {
				// We come here if user sets force_xclbin_program
				// option "true" in xrt.ini under [Runtime] section
				// and we check if current xclbin is in-use or not
                                if (xocl_icap_bitstream_is_locked(xdev, s_id)) {
                                        DRM_WARN("%s current xclbin in-use", __func__);
                                        ret = -EEXIST;
                                } else {
                                        DRM_WARN("%s Force xclbin download", __func__);
				        *slot_id = s_id;
                                } 
			} else {
				*slot_id = s_id;
				ret = -EEXIST;
				goto done;
			}
		}
	}
	else {
		int ps_slot_id = DEFAULT_PL_PS_SLOT;
		uint32_t existing_slot_id = 0;

		if (ps_xclbin_downloaded(xdev, xclbin_id, &existing_slot_id)) {
			if (qos & XOCL_AXLF_FORCE_PROGRAM) {
				s_id = ps_slot_id;
				DRM_WARN("%s Force xclbin download to slot %d", __func__, s_id);
			} else {
				*slot_id = existing_slot_id;
				ret = -EEXIST;
				goto done;
			}
		}
        xdev->ps_slot_id = ps_slot_id;
	}	

	*slot_id = s_id;
done:
	userpf_info(xdev, "Loading xclbin %pUb to slot %d", xclbin_id, *slot_id);
	return ret;
}

/* This is a Workaround function for AWS F2 to reset the clock registers.
 * This function also incurs a delay of 10seconds to work around AWS ocl timeout issue.
 * These changes will be removed once the issue is addressed in AWS F2 instance.
 */
static void aws_reset_clock_registers(xdev_handle_t xdev)
{
	struct xocl_dev_core *core = XDEV(xdev);
	resource_size_t bar0_clk1, bar0_clk2;
	void __iomem *vbar0_clk1, *vbar0_clk2;

	userpf_info(xdev, "AWS F2 WA, waiting to reset clock registers after Load ");
	msleep(10000);

	bar0_clk1 = pci_resource_start(core->pdev, 0) + 0x4058014;
	bar0_clk2 = pci_resource_start(core->pdev, 0) + 0x4058010;
	vbar0_clk1 = ioremap_nocache(bar0_clk1, 32);
	vbar0_clk2 = ioremap_nocache(bar0_clk2, 32);

	iowrite32(0, vbar0_clk1);
	iowrite32(0, vbar0_clk2);

	iounmap(vbar0_clk1);
	iounmap(vbar0_clk2);
	return;
}

int
xocl_read_axlf_helper(struct xocl_drm *drm_p, struct drm_xocl_axlf *axlf_ptr,
	       uint32_t qos, uint32_t *slot)
{
	long err = 0;
	struct axlf *axlf = NULL;
	struct axlf bin_obj;
	size_t size = 0;
	uint32_t slot_id = DEFAULT_PL_PS_SLOT;
	int preserve_mem = 0;
	struct mem_topology *new_topology = NULL;
	struct xocl_dev *xdev = drm_p->xdev;
	struct xocl_axlf_obj_cache *axlf_obj = NULL;
	const struct axlf_section_header * dtbHeader = NULL;
	void *ulp_blob;
	void *kernels;
	int rc = 0;
	struct xocl_dev_core *core = XDEV(drm_p->xdev);

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

	if (is_bad_state(&XDEV(xdev)->kds))
		return -EDEADLK;

	/* Really need to download, sanity check xclbin, first. */
	if (xocl_xrt_version_check(xdev, &bin_obj, true)) {
		userpf_err(xdev, "Xclbin isn't supported by current XRT\n");
		return -EINVAL;
	}

	if (!xocl_verify_timestamp(xdev,
		bin_obj.m_header.m_featureRomTimeStamp)) {
		userpf_err(xdev, "TimeStamp of ROM did not match Xclbin\n");
		return -EOPNOTSUPP;
	}

	/* Validate the length of the data */
        if (bin_obj.m_header.m_length < sizeof(struct axlf)) {
                userpf_err(xdev, "invalid xclbin length\n");
                return -EINVAL;
        }

	/* Copy bitstream from user space and proceed. */
	axlf = vmalloc(bin_obj.m_header.m_length);
	if (!axlf) {
		userpf_err(xdev, "Unable to alloc mem for xclbin, size=%llu\n",
			bin_obj.m_header.m_length);
		return -ENOMEM;
	}
	if (copy_from_user(axlf, axlf_ptr->xclbin, bin_obj.m_header.m_length)) {
		err = -EFAULT;
		goto out_done;
	}

	/* TODO : qos need to define */
	qos |= axlf_ptr->flags;
	rc = xocl_resolver(xdev, axlf, &bin_obj.m_header.uuid, qos, &slot_id);
	if (rc) {
		if (rc == -EEXIST)
			goto out_done;

		userpf_err(xdev, "Download xclbin failed\n");
		err = -EINVAL;
		goto out_done;
	}

	/*
	 * 1. We locked &xdev->dev_lock so no new contexts can be opened
	 *    and no contexts can be closed
	 * 2. A opened context would lock bitstream and hold it. Directly
	 *    ask icap if bitstream is locked
	 */
	if (xocl_icap_bitstream_is_locked(xdev, slot_id)) {
		err = -EBUSY;
		goto out_done;
	}

	/* Populating MEM_TOPOLOGY sections. */
	size = xocl_read_sect(MEM_TOPOLOGY, (void **)&new_topology, axlf);
	if (size <= 0) {
		if (size != 0)
			goto out_done;
	} else if (sizeof_sect(new_topology, m_mem_data) != size) {
		err = -EINVAL;
		goto out_done;
	}

	preserve_mem = xocl_preserve_mem(drm_p, new_topology, size);

	/* Switching the xclbin, make sure none of the buffers are used. */
	if (!preserve_mem) {
		err = xocl_cleanup_mem(drm_p, slot_id);
		if (err)
			goto out_done;
	}

	/* All contexts are closed. No outstanding commands */
	axlf_obj = XDEV(xdev)->axlf_obj[slot_id];
	if (axlf_obj != NULL) {
		if (axlf_obj->ulp_blob)
			vfree(axlf_obj->ulp_blob);

		if (axlf_obj->kernels) 
			vfree(axlf_obj->kernels);

		axlf_obj->kernels = NULL;
		axlf_obj->ksize = 0;

		vfree(axlf_obj);
		XDEV(xdev)->axlf_obj[slot_id] = NULL;
	}

	/* Cache some axlf data which shared in ioctl */
	axlf_obj = vzalloc(sizeof(struct xocl_axlf_obj_cache));
	if (!axlf_obj) {
		err = -ENOMEM;
		goto done;
	}

	axlf_obj->idx = slot_id;
	axlf_obj->flags = axlf_ptr->flags;
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

		axlf_obj->ulp_blob = vmalloc(fdt_totalsize(ulp_blob));
		if (!axlf_obj->ulp_blob) {
			err = -ENOMEM;
			goto done;
		}
		memcpy(axlf_obj->ulp_blob, ulp_blob, fdt_totalsize(ulp_blob));

		/*
		 * don't check uuid if the xclbin is a lite one
		 * the lite xclbin will not have BITSTREAM 
		 */
		if (xocl_axlf_section_header(xdev, axlf, BITSTREAM)) {
			xocl_xdev_info(xdev, "check interface uuid");
			err = xocl_fdt_check_uuids(xdev,
				(const void *)XDEV(xdev)->fdt_blob,
				(const void *)((char*)axlf_obj->ulp_blob));
			if (err) {
				userpf_err(xdev, "interface uuids do not match");
				err = -EINVAL;
				goto done;
			}
		}
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
		axlf_obj->ksize = axlf_ptr->ksize;
		axlf_obj->kernels = kernels;
	}

	memcpy(&axlf_obj->kds_cfg, &axlf_ptr->kds_cfg, sizeof(struct drm_xocl_kds));

	XDEV(xdev)->axlf_obj[slot_id] = axlf_obj;
	err = xocl_icap_download_axlf(xdev, axlf, slot_id);
	/*
	 * Don't just bail out here, always recreate drm mem
	 * since we have cleaned it up before download.
	 */

	if (!err && !preserve_mem)
		err = xocl_init_mem(drm_p, slot_id);

	/*
	 * This is a workaround for u280 only
	 */
	if (!err &&  size >=0)
		xocl_p2p_refresh_rbar(xdev);

	/* The final step is to update KDS configuration */
	if (!err) {
		err = xocl_kds_update(xdev, XDEV(xdev)->axlf_obj[slot_id]->kds_cfg);
		if (err) {
			xocl_icap_clean_bitstream(xdev, slot_id);
		}
	}

done:
	if (size < 0)
		err = size;
	if (err) {
		if (axlf_obj) {
			if (axlf_obj->kernels)
				vfree(axlf_obj->kernels);

			if (axlf_obj->ulp_blob)
				vfree(axlf_obj->ulp_blob);

			vfree(axlf_obj);
			XDEV(xdev)->axlf_obj[slot_id] = NULL;
		}

		userpf_err(xdev, "Failed to download xclbin, err: %ld\n", err);
	}
	else {
		userpf_info(xdev, "Loaded xclbin %pUb", &bin_obj.m_header.uuid);
		/* Work around added for AWS F2 Instance to perform delay and reset clock registers */
		if(core->pdev->device == 0xf010)
		{
			aws_reset_clock_registers(xdev);
		}
	}

out_done:
	/* Update the slot */
	*slot = slot_id;
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
	uint32_t slot_id = 0;
	int err = 0;

	mutex_lock(&xdev->dev_lock);
	err = xocl_read_axlf_helper(drm_p, axlf_obj_ptr, 0, &slot_id); // QOS legacy
	xdev->is_legacy_ctx = true;
	mutex_unlock(&xdev->dev_lock);
	return err;
}

int xocl_hot_reset_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp)
{
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	uint64_t chan_disable = 0;

	/*
	 * if the reset mailbox opcode is disabled, we don't allow
	 * user run 'xbutil reset'
	 */
	xocl_mailbox_get(xdev, CHAN_DISABLE, &chan_disable);
	if (chan_disable & (1 << XCL_MAILBOX_REQ_HOT_RESET))
		return -EOPNOTSUPP;

	xdev->ps_slot_id = 0;  // Clear PS kernel xclbin slots after reset
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

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

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

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	mutex_lock(&xdev->dev_lock);

	if (xocl_xclbin_in_use(xdev) || xocl_check_topology(drm_p))
		err = -EBUSY;
	else
		xocl_cma_bank_free(xdev);
	mutex_unlock(&xdev->dev_lock);

	return err;
}

int xocl_set_cu_read_only_range_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *filp)
{
	struct drm_xocl_set_cu_range *info = data;
	struct xocl_drm *drm_p = dev->dev_private;
	struct xocl_dev *xdev = drm_p->xdev;
	int ret = 0;

	ret = xocl_kds_set_cu_read_range(xdev, info->cu_index, info->start, info->size);
	return ret;
}
