/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * MPSoC based OpenCL accelerators Compute Units.
 *
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/fpga/fpga-mgr.h>
#include <linux/of.h>
#include "zocl_drv.h"
#include "zocl_xclbin.h"
#include "zocl_aie.h"
#include "zocl_sk.h"
#include "xrt_xclbin.h"
#include "xclbin.h"

static inline u32 xclbin_protocol(u32 prop)
{
	u32 intr_id = prop & IP_CONTROL_MASK;

	return intr_id >> IP_CONTROL_SHIFT;
}

static inline u32 xclbin_intr_enable(u32 prop)
{
	u32 intr_enable = prop & IP_INT_ENABLE_MASK;

	return intr_enable;
}

static inline u32 xclbin_intr_id(u32 prop)
{
	u32 intr_id = prop & IP_INTERRUPT_ID_MASK;

	return intr_id >> IP_INTERRUPT_ID_SHIFT;
}


/*
 * Cache the xclbin blob so that it can be shared by processes.
 *
 * Note: currently, we only cache xclbin blob for AIE only xclbin to
 *       support AIE multi-processes. For AIE only xclbin, we load
 *       the PDI to AIE even it has been loaded. But if a process is
 *       using UUID to load xclbin metatdata, we don't load PDI to AIE.
 *       So that a shared AIE context can load AIE metadata without
 *       reload the hardware and can do non-destructive operations.
 */
static int
zocl_cache_xclbin(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot,
		  struct axlf *axlf, char __user *xclbin_ptr)
{
	int ret = 0;
	struct axlf *slot_axlf = NULL;
	size_t size = axlf->m_header.m_length;

	slot_axlf = vmalloc(size);
	if (!slot_axlf)
		return -ENOMEM;

	ret = copy_from_user(slot_axlf, xclbin_ptr, size);
	if (ret) {
		vfree(slot_axlf);
		return ret;
	}

	write_lock(&zdev->attr_rwlock);
	slot->axlf = slot_axlf;
	slot->axlf_size = size;
	write_unlock(&zdev->attr_rwlock);

	return 0;
}

static bool
is_aie_only(struct axlf *axlf)
{
	if ((axlf->m_header.m_actionMask & AM_LOAD_AIE))
		return true;

	return false;
}

/*
 * This function is the main entry point to load xclbin. It's takes an userspace
 * pointer of xclbin and copy the xclbin data to kernel space. Then load that
 * xclbin to the FPGA. It also initialize other modules like, memory, aie, CUs
 * etc.
 *
 * @param       zdev:		zocl device structure
 * @param       axlf_obj:	xclbin userspace structure
 * @param       client:		user space client attached to device
 *
 * @return      0 on success, Error code on failure.
 */
int
zocl_xclbin_read_axlf(struct drm_zocl_dev *zdev, struct drm_zocl_axlf *axlf_obj,
	struct kds_client *client, int slot_id)
{
	struct axlf axlf_head;
	struct axlf *axlf = NULL;
	long axlf_size;
	char __user *xclbin = NULL;
	size_t size_of_header;
	size_t num_of_sections;
	void *kernels = NULL;
	void *aie_res = 0;
	int ret = 0;
	struct drm_zocl_slot *slot = NULL;
	uint32_t qos = 0;
	uint8_t hw_gen = axlf_obj->hw_gen;

	/* Download the XCLBIN from user space to kernel space and validate */
	if (copy_from_user(&axlf_head, axlf_obj->za_xclbin_ptr,
	    sizeof(struct axlf))) {
		DRM_WARN("copy_from_user failed for za_xclbin_ptr");
		return -EFAULT;
	}

	if (memcmp(axlf_head.m_magic, "xclbin2", 8)) {
		DRM_WARN("xclbin magic is invalid %s", axlf_head.m_magic);
		return -EINVAL;
	}

	/* Get full axlf header */
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = axlf_head.m_header.m_numSections;
	axlf_size = sizeof(struct axlf) + size_of_header * num_of_sections;
	axlf = vmalloc(axlf_size);
	if (!axlf) {
		DRM_WARN("read xclbin fails: no memory");
		return -ENOMEM;
	}

	if (copy_from_user(axlf, axlf_obj->za_xclbin_ptr, axlf_size)) {
		DRM_WARN("read xclbin: fail copy from user memory");
		vfree(axlf);
		return -EFAULT;
	}

	xclbin = (char __user *)axlf_obj->za_xclbin_ptr;
	ret = !ZOCL_ACCESS_OK(VERIFY_READ, xclbin, axlf_head.m_header.m_length);
	if (ret) {
		DRM_WARN("read xclbin: fail the access check");
		vfree(axlf);
		return -EFAULT;
	}

	/* TODO : qos need to define */
	qos |= axlf_obj->za_flags;
	slot = zdev->pr_slot[slot_id];

	mutex_lock(&slot->slot_xclbin_lock);
	if (zocl_xclbin_same_uuid(slot,  &axlf_head.m_header.uuid)) {
		if (qos & DRM_ZOCL_FORCE_PROGRAM) {
			// We come here if user sets force_xclbin_program
			// option "true" in xrt.ini under [Runtime] section
			DRM_WARN("%s Force xclbin download", __func__);
		} else if (!is_aie_only(axlf)) {
			DRM_INFO("xclbin already downloaded to slot=%d", slot_id);
			vfree(axlf);
			mutex_unlock(&slot->slot_xclbin_lock);
			return 0;
		}
	}

	/*
	 * Read AIE_RESOURCES section. aie_res will be NULL if there is no
	 * such a section.
	 */

	zocl_read_sect(AIE_RESOURCES, &aie_res, axlf, xclbin);

	/* 1. We locked &zdev->slot_xclbin_lock so that no new contexts
	 * can be opened and/or closed
	 * 2. A opened context would lock bitstream and hold it.
	 * 3. If all contexts are closed, new kds would make sure all
	 * relative exec BO are released
	 */
	if (zocl_bitstream_is_locked(zdev, slot)) {
		DRM_ERROR("Current xclbin is in-use, can't change");
		ret = -EBUSY;
		goto out0;
	}

	/* Free sections before load the new xclbin */
	zocl_free_sections(zdev, slot);
	
	/* Cleanup the aie information from slot and create new */
	zocl_cleanup_aie(slot);


#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	if (xrt_xclbin_get_section_num(axlf, PARTITION_METADATA) &&
	    axlf_head.m_header.m_mode != XCLBIN_HW_EMU &&
	    axlf_head.m_header.m_mode != XCLBIN_HW_EMU_PR) {
		/*
		 * Perform dtbo overlay for both static and rm region
		 * axlf should have dtbo in PARTITION_METADATA section and
		 * bitstream in BITSTREAM section.
		 */
		ret = zocl_load_sect(zdev, axlf, xclbin, PARTITION_METADATA,
				    slot);
		if (ret)
			goto out0;

	} else
#endif
	if (is_aie_only(axlf)) {

		ret = zocl_load_aie_only_pdi(zdev, slot, axlf, xclbin, client);
		if (ret)
			goto out0;

		zocl_cache_xclbin(zdev, slot, axlf, xclbin);

	} else if ((axlf_obj->za_flags & DRM_ZOCL_PLATFORM_FLAT) &&
		   axlf_head.m_header.m_mode == XCLBIN_FLAT &&
		   axlf_head.m_header.m_mode != XCLBIN_HW_EMU &&
		   axlf_head.m_header.m_mode != XCLBIN_HW_EMU_PR) {
		/*
		 * Load full bitstream, enabled in xrt runtime config
		 * and xclbin has full bitstream and its not hw emulation
		 */
		ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM, slot);
		if (ret)
			goto out0;
	} else {

		if (!(axlf_obj->za_flags & DRM_ZOCL_PLATFORM_PR)) {
			DRM_INFO("disable partial bitstream download, "
			    "axlf flags is %d", axlf_obj->za_flags);
		} else {
			 /*
			  * cleanup previously loaded xclbin related data
			  * before loading new bitstream/pdi
			  */
			if (zocl_xclbin_get_uuid(slot) != NULL) {
				zocl_destroy_cu_slot(zdev, slot->slot_idx);
				zocl_cleanup_aie(slot);
			}

			/*
			 * Make sure we load PL bitstream first,
			 * if there is one, before loading AIE PDI.
			 */
			ret = zocl_load_sect(zdev, axlf, xclbin, BITSTREAM,
					    slot);
			if (ret)
				goto out0;

			ret = zocl_load_sect(zdev, axlf, xclbin,
			    BITSTREAM_PARTIAL_PDI, slot);
			if (ret)
				goto out0;

			ret = zocl_load_sect(zdev, axlf, xclbin, PDI, slot);
			if (ret)
				goto out0;
		}
	}

	ret = populate_slot_specific_sec(zdev, axlf, xclbin, slot);
	if (ret)
		goto out0;

	/* Kernels are slot specific. */
	if (slot->kernels != NULL) {
		vfree(slot->kernels);
		slot->kernels = NULL;
		slot->ksize = 0;
	}

	if (axlf_obj->za_ksize > 0) {
		kernels = vmalloc(axlf_obj->za_ksize);
		if (!kernels) {
			ret = -ENOMEM;
			goto out0;
		}
		if (copy_from_user(kernels, axlf_obj->za_kernels,
		    axlf_obj->za_ksize)) {
			ret = -EFAULT;
			goto out0;
		}
		slot->ksize = axlf_obj->za_ksize;
		slot->kernels = kernels;
	}

        ret = zocl_update_apertures(zdev, slot);
        if (ret)
                goto out0;

	zocl_clear_mem_slot(zdev, slot->slot_idx);
	/* Initialize the memory for the new xclbin */
	zocl_init_mem(zdev, slot);

	/* Createing AIE Partition */
	zocl_create_aie(slot, axlf, xclbin, aie_res, hw_gen, axlf_obj->partition_id);

	/*
	 * Remember xclbin_uuid for opencontext.
	 */
	if(ZOCL_PLATFORM_ARM64)
		zocl_xclbin_set_dtbo_path(zdev, slot,
			axlf_obj->za_dtbo_path, axlf_obj->za_dtbo_path_len);

	zocl_xclbin_set_uuid(zdev, slot, &axlf_head.m_header.uuid);

	/* Destroy the CUs specific for this slot */
	zocl_destroy_cu_slot(zdev, slot->slot_idx);

	/* Create the CUs for this slot */
	ret = zocl_create_cu(zdev, slot);
	if (ret)
		goto out0;

	ret = zocl_kds_update(zdev, slot, &axlf_obj->kds_cfg);
	if (ret)
		goto out0;

out0:
	vfree(aie_res);
	vfree(axlf);
	DRM_INFO("%s %pUb ret: %d", __func__, zocl_xclbin_get_uuid(slot),
		ret);
	mutex_unlock(&slot->slot_xclbin_lock);
	return ret;
}

