/*
 * A GEM style CMA backed memory manager for ZynQ based OpenCL accelerators.
 *
 * Copyright (C) 2016 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Min Ma       <min.ma@xilinx.com>
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

#ifndef __ZYNQ_IOCTL_H__
#define __ZYNQ_IOCTL_H__

#ifndef __KERNEL__
#include "drm_mode.h"
#else
#include <uapi/drm/drm_mode.h>
#endif /* !__KERNEL__ */


//#define XCLBIN_DOWNLOAD

enum {
	DRM_ZOCL_CREATE_BO = 0,
	DRM_ZOCL_USERPTR_BO,
	DRM_ZOCL_MAP_BO,
	DRM_ZOCL_SYNC_BO,
	DRM_ZOCL_INFO_BO,
	DRM_ZOCL_PWRITE_BO,
	DRM_ZOCL_PREAD_BO,
	DRM_ZOCL_PCAP_DOWNLOAD,
  DRM_ZOCL_EXECBUF,
	DRM_ZOCL_READ_AXLF,
	DRM_ZOCL_NUM_IOCTLS
};

enum drm_zocl_sync_bo_dir {
	DRM_ZOCL_SYNC_BO_TO_DEVICE,
	DRM_ZOCL_SYNC_BO_FROM_DEVICE
};

#define DRM_ZOCL_BO_FLAGS_COHERENT   (0x1 << 27)
#define DRM_ZOCL_BO_FLAGS_CMA        (0x1 << 28)
#define DRM_ZOCL_BO_FLAGS_SVM        (0x1 << 29)
#define DRM_ZOCL_BO_FLAGS_USERPTR    (0x1 << 30)
#define DRM_ZOCL_BO_FLAGS_EXECBUF    (0x1 << 31)

struct drm_zocl_create_bo {
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
};

struct drm_zocl_userptr_bo {
	uint64_t addr;
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
};

struct drm_zocl_map_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

/**
 * struct drm_zocl_sync_bo - used for SYNQ_BO IOCTL
 * @handle:	GEM object handle
 * @dir:	DRM_ZOCL_SYNC_DIR_XXX
 * @offset:	Offset into the object to write to
 * @size:	Length of data to write
 */
struct drm_zocl_sync_bo {
	uint32_t handle;
	enum drm_zocl_sync_bo_dir dir;
	uint64_t offset;
	uint64_t size;
};

/**
 * struct drm_zocl_info_bo - used for INFO_BO IOCTL
 * @handle:	GEM object handle
 * @size:	Size of BO
 * @paddr:	physical address
 */
struct drm_zocl_info_bo {
	uint32_t	handle;
	uint64_t	size;
	uint64_t	paddr;
};

/**
 * struct drm_zocl_pwrite_bo - used for PWRITE_BO IOCTL
 * @handle:	GEM object handle
 * @pad:	Padding
 * @offset:	Offset into the object to write to
 * @size:	Length of data to write
 * @data_ptr:	Pointer to read the data from (pointers not 32/64 compatible)
 */
struct drm_zocl_pwrite_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_zocl_pread_bo - used for PREAD_BO IOCTL
 * @handle:	GEM object handle
 * @pad:	Padding
 * @offset:	Offset into the object to read from
 * @size:	Length of data to wrreadite
 * @data_ptr:	Pointer to write the data into (pointers not 32/64 compatible)
 */
struct drm_zocl_pread_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_zocl_pcap_download - used for PCAP_DOWNLOAD
 * @xclbin:	Pointer to xclbin object
 */
struct drm_zocl_pcap_download {
	struct axlf *xclbin;
};

/**
 * Opcodes for the embedded scheduler provided by the client to the driver
 */
enum drm_zocl_execbuf_code {
  DRM_ZOCL_EXECBUF_RUN_KERNEL = 0,
  DRM_ZOCL_EXECBUF_RUN_KERNEL_XYZ,
  DRM_ZOCL_EXECBUF_PING,
  DRM_ZOCL_EXECBUF_DEBUG,
};

/**
 * State of exec request managed by the kernel driver
 */
enum drm_zocl_execbuf_state {
  DRM_ZOCL_EXECBUF_STATE_COMPLETE = 0,
  DRM_ZOCL_EXECBUF_STATE_RUNNING,
  DRM_ZOCL_EXECBUF_STATE_SUBMITTED,
  DRM_ZOCL_EXECBUF_STATE_QUEUED,
  DRM_ZOCL_EXECBUF_STATE_ERROR,
  DRM_ZOCL_EXECBUF_STATE_ABORT,
};

struct drm_zocl_execbuf {
  uint32_t ctx_id;
  uint32_t exec_bo_handle;
};

/**
 * struct drm_xocl_axlf - load xclbin (AXLF) device image
 * used with DRM_IOCTL_ZOCL_READ_AXLF ioctl
 *
 * @axlf
 **/
struct drm_zocl_axlf {
	struct axlf *xclbin;
};

#define DRM_IOCTL_ZOCL_CREATE_BO       DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_CREATE_BO,     \
                                       struct drm_zocl_create_bo)
#define DRM_IOCTL_ZOCL_USERPTR_BO      DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_USERPTR_BO,     \
                                       struct drm_zocl_userptr_bo)
#define DRM_IOCTL_ZOCL_MAP_BO          DRM_IOWR(DRM_COMMAND_BASE +          \
                                       DRM_ZOCL_MAP_BO, struct drm_zocl_map_bo)
#define DRM_IOCTL_ZOCL_SYNC_BO         DRM_IOWR(DRM_COMMAND_BASE +          \
                                       DRM_ZOCL_SYNC_BO, struct drm_zocl_sync_bo)
#define DRM_IOCTL_ZOCL_INFO_BO         DRM_IOWR(DRM_COMMAND_BASE +          \
                                       DRM_ZOCL_INFO_BO, struct drm_zocl_info_bo)
#define DRM_IOCTL_ZOCL_PWRITE_BO       DRM_IOWR(DRM_COMMAND_BASE +  \
                                       DRM_ZOCL_PWRITE_BO, \
                                       struct drm_zocl_pwrite_bo)
#define DRM_IOCTL_ZOCL_PREAD_BO        DRM_IOWR(DRM_COMMAND_BASE +      \
                                       DRM_ZOCL_PREAD_BO, struct drm_zocl_pread_bo)
#define DRM_IOCTL_ZOCL_PCAP_DOWNLOAD   DRM_IOWR(DRM_COMMAND_BASE +      \
                                       DRM_ZOCL_PCAP_DOWNLOAD, struct drm_zocl_pcap_download)
#define DRM_IOCTL_ZOCL_EXECBUF         DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_EXECBUF, struct drm_zocl_execbuf)
#define DRM_IOCTL_ZOCL_READ_AXLF       DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_READ_AXLF, struct drm_zocl_axlf)

#endif
