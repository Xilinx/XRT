/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
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

#ifndef _XOCL_DRM_H
#define	_XOCL_DRM_H

void xocl_mm_get_usage_stat(struct xocl_dev *xdev, u32 ddr,
        struct drm_xocl_mm_stat *pstat);
void xocl_mm_update_usage_stat(struct xocl_dev *xdev, u32 ddr,
        u64 size, int count);
int xocl_mm_insert_node(struct xocl_dev *xdev, u32 ddr,
                struct drm_mm_node *node, u64 size);
int xocl_drm_init(struct xocl_dev *xdev);
void xocl_drm_fini(struct xocl_dev *xdev);

void xocl_cleanup_mem(struct xocl_dev *xdev);
void xocl_cleanup_connectivity(struct xocl_dev *xdev);
int xocl_check_topology(struct xocl_dev *xdev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
int xocl_gem_fault(struct vm_fault *vmf);
#else
int xocl_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
#endif

#endif
