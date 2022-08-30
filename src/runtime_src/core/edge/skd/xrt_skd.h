/*
 * Copyright (C) 2022, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _XRT_SKD_H_
#define _XRT_SKD_H_

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <dlfcn.h>
#include <execinfo.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <vector>


#include "core/common/xclbin_parser.h"
#include "ffi.h"
#include "ps_kernel.h"
#include "sk_types.h"
#include "xclbin.h"
#include "xclhal2_mpsoc.h"
#include "xrt/xrt_device.h"

namespace xrt {

class skd
{
 public:
  /**
   * skd() - Construct for empty skd
   */
  skd() = default;


  /**
   * skd() - Constructor from uuid and soft kernel section
   *
   * @param soft kernel metadata buffer handle
   *
   * @param soft kernel image buffer handle
   *
   * @param soft kernel name
   *
   * @param soft kernel CU index
   *
   */
  skd(xclDeviceHandle handle, int sk_meta_bohdl, int sk_bohdl, char *kname, uint32_t cu_index, unsigned char *uuid);
  ~skd();

  XCL_DRIVER_DLLESPEC
  int
  init();

  XCL_DRIVER_DLLESPEC
  void
  run();

  XCL_DRIVER_DLLESPEC
  void
  fini();

  void report_ready();
  void report_crash();
  void report_fini();

 private:
    xclDeviceHandle parent_devHdl;
    xclDeviceHandle devHdl;
    xrtDeviceHandle xrtdHdl;
    char sk_path[XRT_MAX_PATH_LENGTH];
    uint32_t cu_idx;
    char sk_name[PS_KERNEL_NAME_LENGTH];
    pscontext* xrtHandle = NULL;
    int sk_bo;
    int sk_meta_bo;
    unsigned char xclbin_uuid[16];

    void* sk_handle;
    void* kernel;
    std::vector<xrt_core::xclbin::kernel_argument> args;
    int cmd_boh = -1;
    uint32_t *args_from_host;
    ffi_type** ffi_args;
    ffi_cif cif;
    bool pass_xrtHandles = false;
    int num_args;
    int return_offset;
    
    int waitNextCmd();
    int createSoftKernelFile(xclDeviceHandle handle, int bohdl);
    int deleteSoftKernelFile();
    int createSoftKernel(int *boh);
    ffi_type* convert_to_ffitype(xrt_core::xclbin::kernel_argument arg);
};

}

#endif
