/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <drm/drm_file.h>
#include <linux/errno.h>
#include "zocl_drv.h"
#include "zocl_xclbin.h"
#include "zocl_error.h"
#include "zocl_hwctx.h"
#include "xrt_xclbin.h"

static bool
is_aie_only(struct axlf *axlf)
{
        if ((axlf->m_header.m_actionMask & AM_LOAD_AIE))
                return true;

        return false;
}

static bool
is_pl_only(struct axlf *axlf)
{
        if (xrt_xclbin_get_section_num(axlf, IP_LAYOUT) &&
                !xrt_xclbin_get_section_num(axlf, AIE_METADATA))
                return true;

        return false;
}

static int
get_legacy_slot(struct drm_zocl_dev *zdev, struct axlf *axlf, int* slot_id)
{
        struct drm_zocl_slot *slot = NULL;
        uint32_t s_id = 0;
        uint32_t zocl_xclbin_type = 0;

        /* Slots need to be decided based on interface ID. But currently this
         * functionality is not yet ready. Hence we are hard coding the Slots
         * based on the type of XCLBIN. This logic need to be updated in future.
         */
        /* Right now the hard coded logic as follows:
         * Slot - 0 : FULL XCLBIN (Both PL and AIE) / PL Only XCLBIN
         * Slot - 1 : AIE Only XCLBIN
         */
        if (is_aie_only(axlf)) {
                s_id = ZOCL_AIE_ONLY_XCLBIN_SLOT;
                zocl_xclbin_type = ZOCL_XCLBIN_TYPE_AIE_ONLY;
        }
        else {
                s_id = ZOCL_DEFAULT_XCLBIN_SLOT;
                if (is_pl_only(axlf))
                        zocl_xclbin_type = ZOCL_XCLBIN_TYPE_PL_ONLY;
                else
                        zocl_xclbin_type = ZOCL_XCLBIN_TYPE_FULL;
        }
        slot = zdev->pr_slot[s_id];
        if (!slot) {
                DRM_ERROR("Slot[%d] doesn't exists or Invaid slot", s_id);
                return -EINVAL;
        }

        mutex_lock(&slot->slot_xclbin_lock);
        slot->xclbin_type = zocl_xclbin_type;
        mutex_unlock(&slot->slot_xclbin_lock);
	*slot_id = s_id;
	DRM_DEBUG("Found free Slot-%d is selected for xclbin \n", s_id);
        return 0;
}

static int
get_free_slot(struct drm_zocl_dev *zdev, struct axlf *axlf, int* slot_id)
{
	int s_id = -1;
	/* xclbin contains PL section use fixed slot */
	if (xrt_xclbin_get_section_num(axlf, IP_LAYOUT)) {
                DRM_WARN("Xclbin contains PL section Using Slot-0");
		*slot_id = ZOCL_DEFAULT_XCLBIN_SLOT;
		return 0;
	}

        struct drm_zocl_slot *slot = NULL;
	for (int i = 1; i < MAX_PR_SLOT_NUM; i++) {
		if (zdev->slot_mask & (1 << i)) {
			slot = zdev->pr_slot[i];
			if (slot) {
				mutex_lock(&slot->slot_xclbin_lock);
				if (zocl_xclbin_same_uuid(slot, &axlf->m_header.uuid)) {
					// xclbin already downloaded to ith slot
					DRM_INFO("The XCLBIN %pUb already loaded to slot %d", &axlf->m_header.uuid, i);
					*slot_id = i;
					mutex_unlock(&slot->slot_xclbin_lock);
					return 0;
				}
				mutex_unlock(&slot->slot_xclbin_lock);
			}
		}
		else if (!(zdev->slot_mask & (1 << i)) && (s_id < 0)) {
			// found a free slot
			s_id = i;
		}
	}

	if (s_id > 0) {
		slot = zdev->pr_slot[s_id];
		if (!slot) {
			DRM_ERROR("%s: slot %d doesn't exists or invalid", __func__, s_id);
			return -EINVAL;
		}
		DRM_DEBUG("Found a free slot %d for XCLBIN %pUb", s_id, &axlf->m_header.uuid);
		// acquiring the free slot
		zdev->slot_mask |= 1 << s_id;
		*slot_id = s_id;
		return 0;

	}
	return -ENOMEM; // All bits are set
}

static int zocl_identify_slot(struct drm_zocl_dev *zdev, struct drm_zocl_axlf *axlf_obj,
        struct kds_client *client, int* slot_id, bool hw_ctx_flow)
{
        struct axlf axlf_head;
        struct axlf *axlf = NULL;
        long axlf_size;
        char __user *xclbin = NULL;
        size_t size_of_header;
        size_t num_of_sections;
        int ret = 0;

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
        num_of_sections = axlf_head.m_header.m_numSections - 1;
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
	if( hw_ctx_flow)
		ret = get_free_slot(zdev, axlf, slot_id);
	else
		ret = get_legacy_slot(zdev, axlf, slot_id);

        vfree(axlf);
        return ret;
}

/*
 * read_axlf and ctx should be protected by slot_xclbin_lock exclusively.
 */
int
zocl_read_axlf_ioctl(struct drm_device *ddev, void *data, struct drm_file *filp)
{
	struct drm_zocl_axlf *axlf_obj = data;
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
	struct kds_client *client = filp->driver_priv;
	int slot_id = -1;
	int ret = 0;
	ret = zocl_identify_slot(zdev, axlf_obj, client, &slot_id, false /*hw_ctx_flow*/);
	if (ret < 0 || slot_id < 0) {
		DRM_WARN("Unable to allocate slot for xclbin.");
		return ret;
	}
	DRM_DEBUG("Allocated slot %d to load xclbin in device.\n", slot_id);

	return zocl_xclbin_read_axlf(zdev, axlf_obj, client, slot_id);
}

/*
 * IOCTL to create hw context on a slot on device for a xclbin.
 */
int zocl_create_hw_ctx_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_create_hw_ctx *drm_hw_ctx = data;
	struct kds_client *client = filp->driver_priv;
	struct drm_zocl_axlf axlf_obj = {};
	int slot_id = -1;
	int ret = 0;

	if (copy_from_user(&axlf_obj, drm_hw_ctx->axlf_ptr, sizeof(struct drm_zocl_axlf))) {
		DRM_WARN("copy_from_user failed for axlf_ptr");
		return -EFAULT;
	}
	ret = zocl_identify_slot(zdev, &axlf_obj, client, &slot_id, true /*hw_ctx_flow*/);
	if (ret < 0 || slot_id < 0) {
		DRM_WARN("Unable to allocate slot for xclbin.");
		return ret;
	}
	DRM_DEBUG("Allocated slot %d to load xclbin in hw_context.\n", slot_id);

	ret = zocl_xclbin_read_axlf(zdev, &axlf_obj, client, slot_id);
	if (ret) {
		DRM_WARN("xclbin download FAILED.");
		return ret;
	}

	return zocl_create_hw_ctx(zdev, drm_hw_ctx, filp, slot_id);
}

/*
 * IOCTL to destroy hw context on a slot on device
 */
int zocl_destroy_hw_ctx_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_destroy_hw_ctx *drm_hw_ctx = (struct drm_zocl_destroy_hw_ctx *)data;

	return zocl_destroy_hw_ctx(zdev, drm_hw_ctx, filp);
}

/*
 * IOCTL to open a cu context under the given hw context
 */
int zocl_open_cu_ctx_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_open_cu_ctx *drm_cu_ctx = (struct drm_zocl_open_cu_ctx *)data;

	return zocl_open_cu_ctx(zdev, drm_cu_ctx, filp);
}

/*
 * IOCTL to close a opened cu context under the given hw context
 */
int zocl_close_cu_ctx_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_close_cu_ctx *drm_cu_ctx = (struct drm_zocl_close_cu_ctx *)data;

	return zocl_close_cu_ctx(zdev, drm_cu_ctx, filp);
}

/*
 * IOCTL to open a graph context under the given hw context
 */
int zocl_open_graph_ctx_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_open_graph_ctx *drm_graph_ctx = (struct drm_zocl_open_graph_ctx *)data;

	return zocl_open_graph_ctx(zdev, drm_graph_ctx, filp);
}

/*
 * IOCTL to close a graph context openned under the given hw context
 */
int zocl_close_graph_ctx_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_close_graph_ctx *drm_graph_ctx = (struct drm_zocl_close_graph_ctx *)data;

	return zocl_close_graph_ctx(zdev, drm_graph_ctx, filp);
}

/*
 * Block comment for context switch.
 * The read_axlf_ioctl can happen without calling open context, we need to use mutex
 * lock to exclude access between read_axlf_ioctl and zocl_ctx_ioctl. At one
 * time, only one operation can be accessed.
 *
 * When swaping xclbin, first call read_axlf_ioctl to download new xclbin, the
 * following conditions have to be true:
 *   -  When we lock the slot_xclbin_lock, no more zocl_ctx/read_axlf
 *   -  If still have live context, we cannot swap xclbin
 *   -  If no live contexts, but still live cmds from previous closed context,
 *      we cannot swap xclbin.
 * If all the above conditions is cleared, we start changing to new xclbin.
 */
int
zocl_ctx_ioctl(struct drm_device *ddev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);

	/* Do not acquire slot_xclbin_lock like sched_xclbin_ctx().
	 * New KDS would lock bitstream when open the fist context.
	 * The lock bitstream would exclude read_axlf_ioctl().
	 */
	return zocl_context_ioctl(zdev, data, filp);
}

/* IOCTL to get CU index in aperture list
 * used for recognizing BO and CU in mmap
 */
int
zocl_info_cu_ioctl(struct drm_device *ddev, void *data, struct drm_file *filp)
{
	struct drm_zocl_info_cu *args = data;
	struct drm_zocl_dev *zdev = ddev->dev_private;
	struct addr_aperture *apts = zdev->cu_subdev.apertures;
	int apt_idx = args->apt_idx;
	int cu_idx = args->cu_idx;
	phys_addr_t addr = args->paddr;

	if (cu_idx != -1) {
		apt_idx = get_apt_index_by_cu_idx(zdev, cu_idx);
		if (apt_idx != -EINVAL) {
			addr = apts[apt_idx].addr;
			goto out;
		}
	}

	apt_idx = get_apt_index_by_addr(zdev, args->paddr);
	if (apt_idx != -EINVAL)
		cu_idx = apts[apt_idx].cu_idx;

out:
	args->paddr = addr;
	args->apt_idx = apt_idx;
	args->cu_idx = cu_idx;
        //update cu size based on the apt_index
        args->cu_size = apts[apt_idx].size;
	return 0;
}

int
zocl_execbuf_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = dev->dev_private;

	return zocl_command_ioctl(zdev, data, filp);
}

int
zocl_hw_ctx_execbuf_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_hw_ctx_execbuf *drm_hw_ctx_execbuf = (struct drm_zocl_hw_ctx_execbuf *)data;

	return zocl_hw_ctx_execbuf(zdev, drm_hw_ctx_execbuf, filp);
}

int
zocl_error_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	int ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	ret = zocl_inject_error(zdev, data, filp);
	return ret;
}

int
zocl_aie_fd_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);

	return zocl_aie_request_part_fd(zdev, data, filp);
}

int
zocl_aie_reset_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);

	return zocl_aie_reset(zdev, data , filp);
}

int
zocl_aie_freqscale_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);

	return zocl_aie_freqscale(zdev, data, filp);
}

int
zocl_set_cu_read_only_range_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(dev);
	struct drm_zocl_set_cu_range *info = data;
	int ret = 0;

	ret = zocl_kds_set_cu_read_range(zdev, info->cu_index, info->start, info->size);
	return ret;
}
