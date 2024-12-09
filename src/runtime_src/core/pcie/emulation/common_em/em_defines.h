/* 
 * SPDX-License-Identifier: Apache-2.0
   Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */

/*
 * ==== ====================================== =========================================
 * #    Functionality                                  data format
 * ==== ====================================== =========================================
 * 1    Allocate buffer on device                  xocl_create_bo
 * 2    Allocate buffer on device with             xocl_userptr_bo
 *      userptr
 * 3    Prepare bo for mapping into user's         xocl_map_bo
 *      address space
 * 4    Synchronize (DMA) buffer contents in       xocl_sync_bo
 *      requested direction
 * 5    Obtain information about buffer            xocl_info_bo
 *      object
 * 6    Update bo backing storage with user's      xocl_pwrite_bo
 *      data
 * 7    Read back data in bo backing storage       xocl_pread_bo
 * 8    Unprotected write to device memory         xocl_pwrite_unmgd
 * 9    Unprotected read from device memory        xocl_pread_unmgd
 * 10   Obtain device usage statistics             xocl_usage_stat
 * 11   Register eventfd handle for MSIX           xocl_user_intr
 *      interrupt
 * ==== ====================================== =========================================
 */

#ifndef __EM_DEFINES_H__
#define __EM_DEFINES_H__

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <map>
#include "xrt/detail/xrt_mem.h"
#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#if defined (__GNUC__) && !defined(SHIM_UNUSED)
# define SHIM_UNUSED __attribute__((unused))
#elif !defined(SHIM_USED)
# define SHIM_UNUSED
#endif

#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif
const uint64_t mNullBO = 0xffffffff;


#ifdef _WINDOWS
#define __func__ __FUNCTION__
#define MAP_FAILED (void *)-1
#endif

#define STR_MAX_LEN 106

namespace xclemulation {

#define XOCL_BO_DDR0 (1 << 0)
#define XOCL_BO_DDR1 (1 << 1)
#define XOCL_BO_DDR2 (1 << 2)
#define XOCL_BO_DDR3 (1 << 3)

#define XOCL_MEM_BANK_MSK (0xFFFFFF)
#define XOCL_BO_ARE  (1 << 26)

  /**
   * struct xocl_create_bo - Create buffer object
   * used with IOCTL_XOCL_CREATE_BO ioctl
   *
   * @size:       Requested size of the buffer object
   * @handle:     bo handle returned by the driver
   * @flags:      XOCL_BO_XXX flags
   */
  struct xocl_create_bo
  {
    uint64_t size;
    uint32_t handle;
    uint32_t flags;
  };

  /**
   * struct xocl_userptr_bo - Create buffer object with user's pointer
   * used with IOCTL_XOCL_USERPTR_BO ioctl
   *
   * @addr:       Address of buffer allocated by user
   * @size:       Requested size of the buffer object
   * @handle:     bo handle returned by the driver
   * @flags:      XOCL_BO_XXX flags
   */
  struct xocl_userptr_bo
  {
    uint64_t addr;
    uint64_t size;
    uint32_t handle;
    uint32_t flags;
  };

  enum xocl_execbuf_state
  {
    XOCL_EXECBUF_STATE_COMPLETE = 0,
    XOCL_EXECBUF_STATE_RUNNING,
    XOCL_EXECBUF_STATE_SUBMITTED,
    XOCL_EXECBUF_STATE_QUEUED,
    XOCL_EXECBUF_STATE_ERROR,
    XOCL_EXECBUF_STATE_ABORT,
  };

  struct drm_xocl_exec_metadata
  {
    enum xocl_execbuf_state state;
    unsigned int                index;
  };

  struct drm_xocl_bo
  {
    struct drm_xocl_exec_metadata metadata;
    uint64_t              base;
    uint64_t              size;
    void*                 buf;
    void*                 userptr;
    unsigned              flags;
    uint32_t              handle;
    uint32_t              topology;
    std::string           filename;
    int                   fd;
    std::map<uint64_t,uint64_t> chunks;
  };

  //we should not create a memory in default bank for hw_emu. As sw_emu doesnt have rtd information, we are not doing any error check
  static inline unsigned xocl_bo_ddr_idx(unsigned flags, bool is_sw_emu = true)
  {
    unsigned flag = flags & 0xFFFFFFLL;
    //unsigned type = flags & 0xFF000000LL ;

    if(flag == 0 || ((flag == 0xFFFFFFLL) && is_sw_emu))
      return 0;

    return flag;
  }

  static inline bool xocl_bo_p2p(const struct drm_xocl_bo *bo)
  {
    return (bo->flags & XCL_BO_FLAGS_P2P);
  }
  
  static inline bool xocl_bo_dev_only(const struct drm_xocl_bo *bo)
  {
    return (bo->flags & XCL_BO_FLAGS_DEV_ONLY);
  }

  static inline bool xocl_bo_host_only(const struct drm_xocl_bo *bo)
  {
    return (bo->flags & XCL_BO_FLAGS_HOST_ONLY);
  }  

  static inline bool no_host_memory(const struct drm_xocl_bo *bo)
  {
    return xocl_bo_dev_only(bo) || xocl_bo_p2p(bo) ;
  }

  static inline bool is_cacheable(const struct drm_xocl_bo *bo) {
    return (bo->flags & XCL_BO_FLAGS_CACHEABLE);
  }

  //API which denotes whether the sync of data is required or not
  static inline bool is_zero_copy(const struct drm_xocl_bo *bo) {
    bool isCacheable = xclemulation::is_cacheable(bo);
    bool memCheck = xclemulation::no_host_memory(bo) || xclemulation::xocl_bo_host_only(bo);
    bool zeroCopy = (memCheck || !isCacheable) ? true : false;
    return zeroCopy;
  }
}

/**
 * xclMemoryDomains is for support of legacy APIs
 * It is not used in BO APIs where we instead use xclBOKind
 */
enum xclMemoryDomains {
    XCL_MEM_HOST_RAM =    0x00000000,
    XCL_MEM_DEVICE_RAM =  0x00000001,
    XCL_MEM_DEVICE_BRAM = 0x00000002,
    XCL_MEM_SVM =         0x00000003,
    XCL_MEM_CMA =         0x00000004,
    XCL_MEM_DEVICE_REG  = 0x00000005
};

#endif
