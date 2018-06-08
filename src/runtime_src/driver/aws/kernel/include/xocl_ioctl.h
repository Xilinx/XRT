/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
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

#ifndef _XCL_XOCL_IOCTL_H_
#define _XCL_XOCL_IOCTL_H_

#if defined(__KERNEL__)
#include <linux/types.h>
#elif defined(__cplusplus)
#include <cstdlib>
#include <cstdint>
#else
#include <stdlib.h>
#include <stdint.h>
#endif

enum {
	/* GEM core ioctls */
        /* Buffer creation */
	DRM_XOCL_CREATE_BO = 0,
        /* Buffer creation from user provided pointer */
	DRM_XOCL_USERPTR_BO,
        /* Map buffer into application user space (no DMA is performed) */
	DRM_XOCL_MAP_BO,
        /* Sync buffer (like fsync) in the desired direction by using DMA */
	DRM_XOCL_SYNC_BO,
        /* Get information about the buffer such as physical address in the device, etc */
	DRM_XOCL_INFO_BO,
        /* Update host cached copy of buffer wih user's data */
	DRM_XOCL_PWRITE_BO,
        /* Update user's data with host cached copy of buffer */
	DRM_XOCL_PREAD_BO,
	/* Other ioctls */
	DRM_XOCL_OCL_RESET,
        /* Currently unused */
	DRM_XOCL_CREATE_CTX,
        /* Get information from device */
	DRM_XOCL_INFO,
        /* Unmanaged DMA from/to device */
	DRM_XOCL_PREAD_UNMGD,
	DRM_XOCL_PWRITE_UNMGD,
	DRM_XOCL_NUM_IOCTLS
};

enum drm_xocl_sync_bo_dir {
	DRM_XOCL_SYNC_BO_TO_DEVICE = 0,
	DRM_XOCL_SYNC_BO_FROM_DEVICE
};

struct drm_xocl_create_bo {
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
};

struct drm_xocl_userptr_bo {
	uint64_t addr;
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
};

struct drm_xocl_map_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

/**
 * struct drm_xocl_sync_bo - used for SYNQ_BO IOCTL
 * @handle:	GEM object handle
 * @flags:	Unused
 * @size:	Number of bytes to migrate
 * @offset:	Offset into the object to write to
 * @dir:	DRM_XOCL_SYNC_DIR_XXX
 */
struct drm_xocl_sync_bo {
	uint32_t handle;
	uint32_t flags;
	uint64_t size;
	uint64_t offset;
	enum drm_xocl_sync_bo_dir dir;
};

/**
 * struct drm_xocl_info_bo - used for INFO_BO IOCTL
 * @handle:	GEM object handle
 * @size:	Size of buffer object in bytes
 * @paddr:	physical address (out)
 */
struct drm_xocl_info_bo {
	uint32_t handle;
	uint32_t flags;
	uint64_t size;
	uint64_t paddr;
};

/**
 * struct drm_xocl_pwrite_bo - used for PWRITE_BO IOCTL
 * @handle:	GEM object handle
 * @pad:	Padding
 * @offset:	Offset into the buffer object to write to
 * @size:	Length of data to write
 * @data_ptr:	Pointer to read the data from
 */
struct drm_xocl_pwrite_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_xocl_pread_bo - used for PREAD_BO IOCTL
 * @handle:	GEM object handle
 * @pad:	Padding
 * @offset:	Offset into the buffer object to read from
 * @size:	Length of data to read
 * @data_ptr:	Pointer to write the data into
 */
struct drm_xocl_pread_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

struct drm_xocl_info {
	unsigned short	     vendor;
	unsigned short	     device;
	unsigned short	     subsystem_vendor;
	unsigned short	     subsystem_device;
	unsigned	     dma_engine_version;
	unsigned	     driver_version;
	unsigned	     pci_slot;
	char reserved[64];
};

/**
 * struct drm_xocl_pwrite_unmgd (used with PWRITE_UNMGD IOCTL)
 * @address_space: Address space in the DSA; currently only 0 is suported
 * @pad:	   Padding
 * @offset:	   Physical address in the specified address space
 * @size:	   Length of data to write
 * @data_ptr:	   Pointer to read the data from
 */
struct drm_xocl_pwrite_unmgd {
	uint32_t address_space;
	uint32_t pad;
	uint64_t paddr;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_xocl_pread_unmgd (used for PREAD_UNMGD IOCTL)
 * @address_space: Address space in the DSA; currently only 0 is valid
 * @pad:	   Padding
 * @offset:	   Physical address in the specified address space
 * @size:	   Length of data to write
 * @data_ptr:	   Pointer to write the data to
 */
struct drm_xocl_pread_unmgd {
	uint32_t address_space;
	uint32_t pad;
	uint64_t paddr;
	uint64_t size;
	uint64_t data_ptr;
};


#define DRM_IOCTL_XOCL_CREATE_BO      DRM_IOWR(DRM_COMMAND_BASE +	   \
					       DRM_XOCL_CREATE_BO, struct drm_xocl_create_bo)
#define DRM_IOCTL_XOCL_USERPTR_BO     DRM_IOWR(DRM_COMMAND_BASE +	\
					       DRM_XOCL_USERPTR_BO, struct drm_xocl_userptr_bo)
#define DRM_IOCTL_XOCL_MAP_BO	      DRM_IOWR(DRM_COMMAND_BASE +		\
					       DRM_XOCL_MAP_BO, struct drm_xocl_map_bo)
#define DRM_IOCTL_XOCL_SYNC_BO	      DRM_IOW (DRM_COMMAND_BASE +		\
					       DRM_XOCL_SYNC_BO, struct drm_xocl_sync_bo)
#define DRM_IOCTL_XOCL_INFO_BO	      DRM_IOWR(DRM_COMMAND_BASE +		\
					       DRM_XOCL_INFO_BO, struct drm_xocl_info_bo)
#define DRM_IOCTL_XOCL_PWRITE_BO      DRM_IOW (DRM_COMMAND_BASE +	  \
					       DRM_XOCL_PWRITE_BO, struct drm_xocl_pwrite_bo)
#define DRM_IOCTL_XOCL_PREAD_BO	      DRM_IOWR(DRM_COMMAND_BASE +		\
					       DRM_XOCL_PREAD_BO, struct drm_xocl_pread_bo)
#define DRM_IOCTL_XOCL_CREATE_CTX     DRM_IO  (DRM_COMMAND_BASE +		\
					       DRM_XOCL_CREATE_CTX)
#define DRM_IOCTL_XOCL_INFO	      DRM_IOR(DRM_COMMAND_BASE +		\
					      DRM_XOCL_INFO, struct drm_xocl_info)
#define DRM_IOCTL_XOCL_PWRITE_UNMGD   DRM_IOW (DRM_COMMAND_BASE +	\
					       DRM_XOCL_PWRITE_UNMGD, struct drm_xocl_pwrite_unmgd)
#define DRM_IOCTL_XOCL_PREAD_UNMGD    DRM_IOWR(DRM_COMMAND_BASE +	\
					       DRM_XOCL_PREAD_UNMGD, struct drm_xocl_pread_unmgd)

#endif
