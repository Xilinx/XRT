/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
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

#ifndef _XRT_DRV_H
#define _XRT_DRV_H

#define XRT_DRV_BO_DEVICE_MEM 	(1 << 31)
#define XRT_DRV_BO_HOST_MEM	(1 << 30)
#define XRT_DRV_BO_DRV_ALLOC	(1 << 29)
#define XRT_DRV_BO_DRM_IMPORT 	(1 << 28)
#define XRT_DRV_BO_P2P		(1 << 27)
#define XRT_DRV_BO_DRM_SHMEM	(1 << 26)
#define XRT_DRV_BO_USER_ALLOC	(1 << 25)
#define XRT_DRV_BO_CMA		(1 << 24)
#define XRT_DRV_BO_CACHEABLE	(1 << 23)
#endif
