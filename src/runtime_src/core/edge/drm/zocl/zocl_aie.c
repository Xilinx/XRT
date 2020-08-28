/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinc.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "zocl_aie.h"
#include "xrt_xclbin.h"
#include "xclbin.h"

#ifndef __NONE_PETALINUX__
#include <linux/xlnx-ai-engine.h>
#endif

int
zocl_aie_request_part_fd(struct drm_zocl_dev *zdev, void *data)
{
	struct drm_zocl_aie_fd *args = data;
	int fd;
	int rval = 0;

	mutex_lock(&zdev->aie_lock);

	if (!zdev->aie) {
		DRM_ERROR("AIE image is not loaded.\n");
		rval = -ENODEV;
		goto done;
	}

	if (!zdev->aie->aie_dev) {
		DRM_ERROR("No available AIE partition.\n");
		rval = -ENODEV;
		goto done;
	}

	if (zdev->aie->partition_id != args->partition_id) {
		DRM_ERROR("AIE partition %d does not exist.\n",
		    args->partition_id);
		rval = -ENODEV;
		goto done;
	}

	fd = aie_partition_get_fd(zdev->aie->aie_dev);
	if (fd < 0) {
		DRM_ERROR("Get AIE partition %d fd: %d\n",
		    args->partition_id, fd);
		rval = fd;
		goto done;
	}

	args->fd = fd;

done:
	mutex_unlock(&zdev->aie_lock);

	return rval;
}

int
zocl_create_aie(struct drm_zocl_dev *zdev, struct axlf *axlf)
{
	uint64_t offset;
	uint64_t size;
	struct aie_partition_req req;
	int rval;

	rval = xrt_xclbin_section_info(axlf, AIE_METADATA, &offset, &size);
	if (rval)
		return rval;

	mutex_lock(&zdev->aie_lock);

	if (zdev->aie) {
		DRM_ERROR("AIE already created.\n");
		rval = -EAGAIN;
		goto fail_aie;
	}

	zdev->aie = vzalloc(sizeof (struct zocl_aie));

	/* TODO figure out the partition id and uid from xclbin or PDI */
	req.partition_id = 1;
	req.uid = 0;
	zdev->aie->aie_dev = aie_partition_request(&req);

	if (IS_ERR(zdev->aie->aie_dev)) {
		rval = PTR_ERR(zdev->aie->aie_dev);
		DRM_ERROR("Request AIE partition %d, %d\n",
		    req.partition_id, rval);
		goto fail_part;
	}

	zdev->aie->partition_id = req.partition_id;
	zdev->aie->uid = req.uid;
	mutex_unlock(&zdev->aie_lock);

	return 0;

fail_part:
	vfree(zdev->aie);
	zdev->aie = NULL;

fail_aie:
	mutex_unlock(&zdev->aie_lock);

	return rval;
}

void
zocl_destroy_aie(struct drm_zocl_dev *zdev)
{
	mutex_lock(&zdev->aie_lock);

	if (!zdev->aie) {
		mutex_unlock(&zdev->aie_lock);
		return;
	}

	if (zdev->aie->aie_dev)
		aie_partition_release(zdev->aie->aie_dev);

	vfree(zdev->aie);
	zdev->aie = NULL;
	mutex_unlock(&zdev->aie_lock);
}
