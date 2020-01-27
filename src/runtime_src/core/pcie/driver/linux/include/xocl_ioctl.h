/*
 *  Copyright (C) 2017, Xilinx, Inc. All rights reserved.
 *  Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later version.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/**
 * DOC: A GEM style driver for Xilinx PCIe based accelerators
 * This file defines ioctl command codes and associated structures for interacting with
 * *xocl* PCI driver for Xilinx FPGA platforms.
 *
 * Device memory allocation is modeled as buffer objects (bo). For each bo driver tracks the host pointer
 * backed by scatter gather list -- which provides backing storage on host -- and the corresponding device
 * side allocation of contiguous buffer in one of the memory mapped DDRs/BRAMs, etc.
 *
 * Exection model is asynchronous where execute commands are submitted using command buffers and POSIX poll
 * is used to wait for finished commands. Commands for a compute unit can only be submitted after an explicit
 * context has been opened by the client.
 *
 * "xocl" driver allows user land to perform mmap on multiple entities distinguished by offset:
 * - page offset == 0: whole user BAR is mapped
 * - page offset > 0 and <= 128: one CU reg space is mapped, offset is used as CU index
 * - page offset >= (4G >> PAGE_SHIFT): one BO is mapped, offset should be obtained from drm_xocl_map_bo()
 *
 * *xocl* driver functionality is described in the following table. All the APIs are multi-threading and
 * multi-process safe.
 *
 * ==== ====================================== ============================== ==================================
 * #    Functionality                          ioctl request code             data format
 * ==== ====================================== ============================== ==================================
 * 1    Allocate buffer on device              DRM_IOCTL_XOCL_CREATE_BO       drm_xocl_create_bo
 * 2    Allocate buffer on device with         DRM_IOCTL_XOCL_USERPTR_BO      drm_xocl_userptr_bo
 *      userptr
 * 3    Prepare bo for mapping into user's     DRM_IOCTL_XOCL_MAP_BO          drm_xocl_map_bo
 *      address space
 * 4    Synchronize (DMA) buffer contents in   DRM_IOCTL_XOCL_SYNC_BO         drm_xocl_sync_bo
 *      requested direction
 * 5    Obtain information about buffer        DRM_IOCTL_XOCL_INFO_BO         drm_xocl_info_bo
 *      object
 * 6    Update bo backing storage with user's  DRM_IOCTL_XOCL_PWRITE_BO       drm_xocl_pwrite_bo
 *      data
 * 7    Read back data in bo backing storage   DRM_IOCTL_XOCL_PREAD_BO        drm_xocl_pread_bo
 * 8    Open/close a context on a compute unit DRM_XOCL_CTX                   drm_xocl_ctx
 *      on the device
 * 9    Unprotected write to device memory     DRM_IOCTL_XOCL_PWRITE_UNMGD    drm_xocl_pwrite_unmgd
 * 10   Unprotected read from device memory    DRM_IOCTL_XOCL_PREAD_UNMGD     drm_xocl_pread_unmgd
 * 11   Send an execute job to a compute unit  DRM_IOCTL_XOCL_EXECBUF         drm_xocl_execbuf
 * 12   Register eventfd handle for MSIX       DRM_IOCTL_XOCL_USER_INTR       drm_xocl_user_intr
 *      interrupt
 * 13   Update device view with a specific     DRM_XOCL_READ_AXLF             drm_xocl_axlf
 *      xclbin image
 * ==== ====================================== ============================== ==================================
 */

#ifndef _XCL_XOCL_IOCTL_H_
#define _XCL_XOCL_IOCTL_H_

#if defined(__KERNEL__)
#include <linux/types.h>
#include <linux/uuid.h>
#include <linux/version.h>
#elif defined(__cplusplus)
#include <cstdlib>
#include <cstdint>
#include <uuid/uuid.h>
#else
#include <stdlib.h>
#include <stdint.h>
#include <uuid/uuid.h>
#endif

#if defined(__KERNEL__)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
typedef uuid_t xuid_t;
#elif defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7,4)
typedef uuid_t xuid_t;
#endif
#else
typedef uuid_le xuid_t;
#endif
#else
typedef uuid_t xuid_t;
#endif
/*
 * enum drm_xocl_ops - ioctl command code enumerations
 */
enum drm_xocl_ops {
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
	/* Open/close a context */
	DRM_XOCL_CTX,
	/* Get information from device */
	DRM_XOCL_INFO,
	/* Unmanaged DMA from/to device */
	DRM_XOCL_PREAD_UNMGD,
	DRM_XOCL_PWRITE_UNMGD,
	/* Various usage metrics */
	DRM_XOCL_USAGE_STAT,
	/* Hardware debug command */
	DRM_XOCL_DEBUG,
	/* Command to run on one or more CUs */
	DRM_XOCL_EXECBUF,
	/* Register eventfd for user interrupts */
	DRM_XOCL_USER_INTR,
	/* Read xclbin/axlf */
	DRM_XOCL_READ_AXLF,
	/* Hot reset request */
	DRM_XOCL_HOT_RESET,
	/* Reclock through userpf*/
	DRM_XOCL_RECLOCK,
	/* Pre-Alloc CMA through userpf*/
	DRM_XOCL_ALLOC_CMA,
	/* Free allocated CMA chunk through userpf*/
	DRM_XOCL_FREE_CMA,
	DRM_XOCL_NUM_IOCTLS
};

enum drm_xocl_sync_bo_dir {
	DRM_XOCL_SYNC_BO_TO_DEVICE = 0,
	DRM_XOCL_SYNC_BO_FROM_DEVICE
};

/*
 * Higher 4 bits are for DDR, one for each DDR
 * LSB bit for execbuf
 */
#define DRM_XOCL_BO_BANK0   (0x1)
#define DRM_XOCL_BO_BANK1   (0x1 << 1)
#define DRM_XOCL_BO_BANK2   (0x1 << 2)
#define DRM_XOCL_BO_BANK3   (0x1 << 3)

#define DRM_XOCL_CTX_FLAG_EXCLUSIVE (0x1)


#define DRM_XOCL_NUM_SUPPORTED_CLOCKS	4

#define DRM_XOCL_CMA_CHUNK_MAX		4
/**
 * struct drm_xocl_create_bo - Create buffer object
 * used with DRM_IOCTL_XOCL_CREATE_BO ioctl
 *
 * @size:       Requested size of the buffer object
 * @handle:     bo handle returned by the driver
 * @flags:      DRM_XOCL_BO_XXX flags
 * @type:       The type of bo
 */
struct drm_xocl_create_bo {
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
	uint32_t type;
};

/**
 * struct drm_xocl_userptr_bo - Create buffer object with user's pointer
 * used with DRM_IOCTL_XOCL_USERPTR_BO ioctl
 *
 * @addr:       Address of buffer allocated by user
 * @size:       Requested size of the buffer object
 * @handle:     bo handle returned by the driver
 * @flags:      DRM_XOCL_BO_XXX flags
 * @type:       The type of bo
 */
struct drm_xocl_userptr_bo {
	uint64_t addr;
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
	uint32_t type;
};

/**
 * struct drm_xocl_map_bo - Prepare a buffer object for mmap
 * used with DRM_IOCTL_XOCL_MAP_BO ioctl
 *
 * @handle:     bo handle
 * @pad:        Unused
 * @offset:     'Fake' offset returned by the driver which can be used with POSIX mmap
 */
struct drm_xocl_map_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

/**
 * struct drm_xocl_sync_bo - Synchronize the buffer in the requested direction
 * between device and host
 * used with DRM_IOCTL_XOCL_SYNC_BO ioctl
 *
 * @handle:	bo handle
 * @flags:	Unused
 * @size:	Number of bytes to synchronize
 * @offset:	Offset into the object to synchronize
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
 * struct drm_xocl_info_bo - Obtain information about an allocated buffer obbject
 * used with DRM_IOCTL_XOCL_INFO_BO IOCTL
 *
 * @handle:	bo handle
 * @flags:      Unused
 * @size:	Size of buffer object (out)
 * @paddr:	Physical address (out)
 */
struct drm_xocl_info_bo {
	uint32_t handle;
	uint32_t flags;
	uint64_t size;
	uint64_t paddr;
};

/**
 * struct drm_xocl_axlf - load xclbin (AXLF) device image
 * used with DRM_IOCTL_XOCL_READ_AXLF ioctl
 * NOTE: This ioctl will be removed in next release
 *
 * @xclbin:	Pointer to user's xclbin structure in memory
 */
struct drm_xocl_axlf {
	struct axlf *xclbin;
};

/**
 * struct drm_xocl_pwrite_bo - Update bo with user's data
 * used with DRM_IOCTL_XOCL_PWRITE_BO ioctl
 *
 * @handle:	bo handle
 * @pad:	Unused
 * @offset:	Offset into the buffer object to write to
 * @size:	Length of data to write
 * @data_ptr:	User's pointer to read the data from
 */
struct drm_xocl_pwrite_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_xocl_pread_bo - Read data from bo
 * used with DRM_IOCTL_XOCL_PREAD_BO ioctl
 *
 * @handle:	bo handle
 * @pad:	Unused
 * @offset:	Offset into the buffer object to read from
 * @size:	Length of data to read
 * @data_ptr:	User's pointer to write the data into
 */
struct drm_xocl_pread_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

enum drm_xocl_ctx_code {
	XOCL_CTX_OP_ALLOC_CTX = 0,
	XOCL_CTX_OP_FREE_CTX
};

#define	XOCL_CTX_SHARED		0x0
#define	XOCL_CTX_EXCLUSIVE	0x1
#define	XOCL_CTX_VIRT_CU_INDEX	0xffffffff
/**
 * struct drm_xocl_ctx - Open or close a context on a compute unit on device
 * used with DRM_XOCL_CTX ioctl
 *
 * @op:            Alloc or free a context (XOCL_CTX_OP_ALLOC_CTX/XOCL_CTX_OP_FREE_CTX)
 * @xclbin_id:	   UUID of the device image (xclbin)
 * @cu_index:	   Index of the compute unit in the device inage for which
 *                 the request is being made
 * @flags:	   Shared or exclusive context (XOCL_CTX_SHARED/XOCL_CTX_EXCLUSIVE)
 * @handle:	   Unused
 */
struct drm_xocl_ctx {
	enum drm_xocl_ctx_code op;
	xuid_t   xclbin_id;
	uint32_t cu_index;
	uint32_t flags;
	// unused, in future it would return context id
	uint32_t handle;
};

struct drm_xocl_info {
	unsigned short vendor;
	unsigned short device;
	unsigned short subsystem_vendor;
	unsigned short subsystem_device;
	unsigned int dma_engine_version;
	unsigned int driver_version;
	unsigned int pci_slot;
	char reserved[64];
};


/**
 * struct drm_xocl_pwrite_unmgd - unprotected write to device memory
 * used with DRM_IOCTL_XOCL_PWRITE_UNMGD ioctl
 *
 * @address_space: Address space in the DSA; currently only 0 is suported
 * @pad:	   Unused
 * @paddr:	   Physical address in the specified address space
 * @size:	   Length of data to write
 * @data_ptr:	   User's pointer to read the data from
 */
struct drm_xocl_pwrite_unmgd {
	uint32_t address_space;
	uint32_t pad;
	uint64_t paddr;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_xocl_pread_unmgd - unprotected read from device memory
 * used with DRM_IOCTL_XOCL_PREAD_UNMGD ioctl
 *
 * @address_space: Address space in the DSA; currently only 0 is valid
 * @pad:	   Unused
 * @paddr:	   Physical address in the specified address space
 * @size:	   Length of data to write
 * @data_ptr:	   User's pointer to write the data to
 */
struct drm_xocl_pread_unmgd {
	uint32_t address_space;
	uint32_t pad;
	uint64_t paddr;
	uint64_t size;
	uint64_t data_ptr;
};


struct drm_xocl_mm_stat {
	size_t memory_usage;
	unsigned int bo_count;
};

/**
 * struct drm_xocl_stats - obtain device memory usage and DMA statistics
 * used with DRM_IOCTL_XOCL_USAGE_STAT ioctl
 *
 * @dma_channel_count: How many DMA channels are present
 * @mm_channel_count:  How many storage banks (DDR) are present
 * @h2c:	       Total data transferred from host to device by a DMA channel
 * @c2h:	       Total data transferred from device to host by a DMA channel
 * @mm:	               BO statistics for a storage bank (DDR)
 */
struct drm_xocl_usage_stat {
	unsigned dma_channel_count;
	unsigned mm_channel_count;
	uint64_t h2c[8];
	uint64_t c2h[8];
	struct drm_xocl_mm_stat mm[8];
};

enum drm_xocl_debug_code {
	DRM_XOCL_DEBUG_ACQUIRE_CU = 0,
	DRM_XOCL_DEBUG_RELEASE_CU,
	DRM_XOCL_DEBUG_NIFD_RD,
	DRM_XOCL_DEBUG_NIFD_WR,
};

struct drm_xocl_debug {
	uint32_t ctx_id;
	enum drm_xocl_debug_code code;
	unsigned int code_size;
	uint64_t code_ptr;
};

enum drm_xocl_execbuf_state {
	DRM_XOCL_EXECBUF_STATE_COMPLETE = 0,
	DRM_XOCL_EXECBUF_STATE_RUNNING,
	DRM_XOCL_EXECBUF_STATE_SUBMITTED,
	DRM_XOCL_EXECBUF_STATE_QUEUED,
	DRM_XOCL_EXECBUF_STATE_ERROR,
	DRM_XOCL_EXECBUF_STATE_ABORT,
};


/**
 * struct drm_xocl_execbuf - Submit a command buffer for execution on a compute unit
 * used with DRM_IOCTL_XOCL_EXECBUF ioctl
 *
 * @ctx_id:         Pass 0
 * @exec_bo_handle: BO handle of command buffer formatted as ERT command
 * @deps:	    Upto 8 dependency command BO handles this command is dependent on
 *                  for automatic event dependency handling by ERT
 */
struct drm_xocl_execbuf {
	uint32_t ctx_id;
	uint32_t exec_bo_handle;
	uint32_t deps[8];
};

/**
 * struct drm_xocl_user_intr - Register user's eventfd for MSIX interrupt
 * used with DRM_IOCTL_XOCL_USER_INTR ioctl
 *
 * @ctx_id:        Pass 0
 * @fd:	           File descriptor created with eventfd system call
 * @msix:	   User interrupt number (0 to 15)
 */
struct drm_xocl_user_intr {
	uint32_t ctx_id;
	int fd;
	int msix;
};

struct drm_xocl_reclock_info {
	unsigned region;
	unsigned short ocl_target_freq[DRM_XOCL_NUM_SUPPORTED_CLOCKS];
};


struct drm_xocl_alloc_cma_info {
	uint64_t page_sz;
	uint64_t user_addr;
	uint64_t chunk_id;
};

struct drm_xocl_free_cma_info {
	uint64_t chunk_id;
};
/*
 * Core ioctls numbers
 */
#define	XOCL_IOC(cmd)		\
	DRM_IO(DRM_COMMAND_BASE + DRM_XOCL_##cmd)
#define	XOCL_IOC_ARG(cmd, type)	\
	DRM_IOWR(DRM_COMMAND_BASE + DRM_XOCL_##cmd, struct drm_xocl_##type)

#define	DRM_IOCTL_XOCL_CREATE_BO	XOCL_IOC_ARG(CREATE_BO, create_bo)
#define	DRM_IOCTL_XOCL_USERPTR_BO	XOCL_IOC_ARG(USERPTR_BO, userptr_bo)
#define	DRM_IOCTL_XOCL_MAP_BO		XOCL_IOC_ARG(MAP_BO, map_bo)
#define	DRM_IOCTL_XOCL_SYNC_BO		XOCL_IOC_ARG(SYNC_BO, sync_bo)
#define	DRM_IOCTL_XOCL_INFO_BO		XOCL_IOC_ARG(INFO_BO, info_bo)
#define	DRM_IOCTL_XOCL_PWRITE_BO	XOCL_IOC_ARG(PWRITE_BO, pwrite_bo)
#define	DRM_IOCTL_XOCL_PREAD_BO		XOCL_IOC_ARG(PREAD_BO, pread_bo)
#define	DRM_IOCTL_XOCL_CTX		XOCL_IOC_ARG(CTX, ctx)
#define	DRM_IOCTL_XOCL_INFO		XOCL_IOC_ARG(INFO, info)
#define	DRM_IOCTL_XOCL_READ_AXLF	XOCL_IOC_ARG(READ_AXLF, axlf)
#define	DRM_IOCTL_XOCL_PWRITE_UNMGD	XOCL_IOC_ARG(PWRITE_UNMGD, pwrite_unmgd)
#define	DRM_IOCTL_XOCL_PREAD_UNMGD	XOCL_IOC_ARG(PREAD_UNMGD, pread_unmgd)
#define	DRM_IOCTL_XOCL_USAGE_STAT	XOCL_IOC_ARG(USAGE_STAT, usage_stat)
#define	DRM_IOCTL_XOCL_DEBUG		XOCL_IOC_ARG(DEBUG, debug)
#define	DRM_IOCTL_XOCL_EXECBUF		XOCL_IOC_ARG(EXECBUF, execbuf)
#define	DRM_IOCTL_XOCL_USER_INTR	XOCL_IOC_ARG(USER_INTR, user_intr)
#define	DRM_IOCTL_XOCL_HOT_RESET	XOCL_IOC(HOT_RESET)
#define	DRM_IOCTL_XOCL_RECLOCK		XOCL_IOC_ARG(RECLOCK, reclock_info)
#define	DRM_IOCTL_XOCL_ALLOC_CMA	XOCL_IOC_ARG(ALLOC_CMA, alloc_cma_info)
#define	DRM_IOCTL_XOCL_FREE_CMA		XOCL_IOC_ARG(FREE_CMA, free_cma_info)
#endif
