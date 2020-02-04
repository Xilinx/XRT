/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu    <yliu@xilinx.com>
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

#ifndef _ZOCL_BO_H
#define _ZOCL_BO_H

#include "xclhal2_mem.h"

/**
 * XCL BO Flags bits layout
 *
 * bits  0 ~ 15: DDR BANK index
 * bits 16 ~ 31: BO flags
 *
 * TODO These flags are copied from old xclhal2_mem.h. BO flags
 * in current xclhal2_mem.h are modified. To prevent zocl from
 * being broken, we keep a copy of old flags and implement a
 * converter from the new flags to these old ones. These flags
 * will be removed when we implement BO BO BO project at edge
 * side.
 */
#define ZOCL_BO_FLAGS_CACHEABLE		(1 << 24)
#define ZOCL_BO_FLAGS_HOST_BO		(1 << 25)
#define ZOCL_BO_FLAGS_COHERENT		(1 << 26)
#define ZOCL_BO_FLAGS_SVM		(1 << 27)
#define ZOCL_BO_FLAGS_USERPTR		(1 << 28)
#define ZOCL_BO_FLAGS_CMA		(1 << 29)
#define ZOCL_BO_FLAGS_P2P		(1 << 30)
#define ZOCL_BO_FLAGS_EXECBUF		(1 << 31)

/* BO types we support */
#define ZOCL_BO_NORMAL	(XRT_DRV_BO_HOST_MEM | XRT_DRM_SHMEM | \
			XRT_DRV_BO_DRM_ALLOC)
#define ZOCL_BO_EXECBUF	(ZOCL_BO_NORMAL)
#define ZOCL_BO_CACHE	(ZOCL_BO_NORMAL | XRT_CACHEABLE)
#define ZOCL_BO_USERPTR	(XRT_USER_MEM | XRT_DRV_BO_USER_ALLOC)
#define ZOCL_BO_SVM	(XRT_DRV_BO_HOST_MEM | XRT_DRM_SHMEM | \
			XRT_DRV_BO_DRM_ALLOC)
#define ZOCL_BO_PL_DDR	(XRT_DEVICE_MEM)
#define ZOCL_BO_HOST_BO	(XRT_DRV_BO_HOST_MEM)
#define ZOCL_BO_IMPORT	(XRT_DRM_IMPORT | XRT_DRV_BO_HOST_MEM)

static inline uint32_t zocl_convert_bo_uflags(uint32_t uflags)
{
	uint32_t zflags = 0;

	/*
	 * Keep the bank index and remove all flags, except EXECBUF and
	 * CACHEABLE.
	 */
	if (uflags & XCL_BO_FLAGS_EXECBUF)
		zflags |= ZOCL_BO_FLAGS_EXECBUF;

	if (uflags & XCL_BO_FLAGS_CACHEABLE)
		zflags |= ZOCL_BO_FLAGS_CACHEABLE;

	zflags |= (uflags & 0xFFFF);

	return zflags;
}

struct drm_gem_object *
zocl_gem_import(struct drm_device *dev, struct dma_buf *dma_buf);

#endif /* _ZOCL_BO_H */
