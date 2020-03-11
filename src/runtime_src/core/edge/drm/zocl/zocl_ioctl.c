/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
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

#include "sched_exec.h"
#include "zocl_xclbin.h"
#include "zocl_generic_cu.h"

/*
 * read_axlf and ctx should be protected by zdev_xclbin_lock exclusively.
 */
int
zocl_read_axlf_ioctl(struct drm_device *ddev, void *data, struct drm_file *filp)
{
	struct drm_zocl_axlf *axlf_obj = data;
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
	int ret;

	mutex_lock(&zdev->zdev_xclbin_lock);
	ret = zocl_xclbin_read_axlf(zdev, axlf_obj);
	mutex_unlock(&zdev->zdev_xclbin_lock);

	return ret;
}

/*
 * Block comment for context switch.
 * The read_axlf_ioctl can happen without calling open context, we need to use mutex
 * lock to exclude access between read_axlf_ioctl and zocl_ctx_ioctl. At one
 * time, only one operation can be accessed.
 *
 * When swaping xclbin, first call read_axlf_ioctl to download new xclbin, the
 * following conditions have to be true:
 *   -  When we lock the zdev_xclbin_lock, no more zocl_ctx/read_axlf
 *   -  If still have live context, we cannot swap xclbin
 *   -  If no live contexts, but still live cmds from previous closed context,
 *      we cannot swap xclbin.
 * If all the above conditions is cleared, we start changing to new xclbin.
 */
int
zocl_ctx_ioctl(struct drm_device *ddev, void *data, struct drm_file *filp)
{
	struct drm_zocl_ctx *args = data;
	struct drm_zocl_dev *zdev = ZOCL_GET_ZDEV(ddev);
	int ret = 0;

	if (args->op == ZOCL_CTX_OP_OPEN_GCU_FD) {
		ret = zocl_open_gcu(zdev, args, filp->driver_priv);
		return ret;
	}

	mutex_lock(&zdev->zdev_xclbin_lock);
	ret = zocl_xclbin_ctx(zdev, args, filp->driver_priv);
	mutex_unlock(&zdev->zdev_xclbin_lock);

	return ret;
}

/* IOCTL to get CU index in aperture list
 * used for recognizing BO and CU in mmap
 */
int
zocl_info_cu_ioctl(struct drm_device *ddev, void *data, struct drm_file *filp)
{
	struct drm_zocl_info_cu *args = data;
	struct drm_zocl_dev *zdev = ddev->dev_private;
	struct sched_exec_core *exec = zdev->exec;
	struct addr_aperture *apts = zdev->apertures;
	int apt_idx = args->apt_idx;
	int cu_idx = args->cu_idx;
	phys_addr_t addr = args->paddr;

	if (!exec->configured) {
		DRM_ERROR("Schduler is not configured\n");
		return -EINVAL;
	}

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
	return 0;
}
