/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2019-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
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
#define XRT_DRV_BO_SGL		(1 << 22)
#define XRT_DRV_BO_KERN_BUF	(1 << 21)

/* Versal transfer cache packet definition */

#define	XRT_XFR_VER			1

/*
 * Note: we should keep the existing PKT status and flags
 *       stable to not breaking the old versal platform
 */
#define	XRT_XFR_PKT_STATUS_IDLE		0
#define	XRT_XFR_PKT_STATUS_NEW		1
#define	XRT_XFR_PKT_STATUS_DONE		2
#define	XRT_XFR_PKT_STATUS_FAIL		3

#define	XRT_XFR_PKT_TYPE_SHIFT		1
#define	XRT_XFR_PKT_TYPE_MASK		7

#define	XRT_XFR_PKT_VER_SHIFT		4
#define	XRT_XFR_PKT_VER_MASK		3

#define	XRT_XFR_PKT_TYPE_PDI		0
#define	XRT_XFR_PKT_TYPE_XCLBIN		1

#define	XRT_XFR_PKT_FLAGS_LAST		(1 << 0)
#define	XRT_XFR_PKT_FLAGS_PDI		(XRT_XFR_PKT_TYPE_PDI << \
	XRT_XFR_PKT_TYPE_SHIFT)
#define	XRT_XFR_PKT_FLAGS_XCLBIN	(XRT_XFR_PKT_TYPE_XCLBIN << \
	XRT_XFR_PKT_TYPE_SHIFT)
#define	XRT_XFR_PKT_FLAGS_VER		(XRT_XFR_VER << XRT_XFR_PKT_VER_SHIFT)

struct pdi_packet {
	union {
		struct {
			u8	pkt_status;
			u8	pkt_flags;
			u16	pkt_size;
		};
		u32 header;
	};
} __packed;

#endif
