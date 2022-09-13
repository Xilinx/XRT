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
#include <fstream>
#include <functional>
#include <iostream>
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


#include "core/common/api/device_int.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
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
  skd(const xclDeviceHandle handle, const int sk_meta_bohdl, const int sk_bohdl, const std::string kname, const uint32_t cu_index, unsigned char *uuid_in, const int parent_mem_bo_in, const uint64_t mem_start_paddr_in, const uint64_t mem_size_in);
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

  void set_signal(int sig);
  void report_ready();
  void report_crash();
  void report_fini();

 private:
    xclDeviceHandle m_parent_devhdl = 0;
    xclDeviceHandle m_devhdl = 0;
    xrtDeviceHandle m_xrtdhdl = 0;
    uuid m_xclbin_uuid;
    const std::filesystem::path m_sk_path;
    uint32_t m_cu_idx;
    std::string m_sk_name;
    pscontext* m_xrtHandle = nullptr;
    int m_sk_bo;
    int m_sk_meta_bo;
    int m_parent_bo_handle;
    uint64_t m_mem_start_paddr;
    uint64_t m_mem_size;
    void* m_mem_start_vaddr = nullptr;
    int signal = 0;
    
    void* m_sk_handle = nullptr;
    void* m_kernel = nullptr;
    std::vector<xrt_core::xclbin::kernel_argument> m_kernel_args;
    int m_cmd_boh = -1;
    uint32_t *m_args_from_host = nullptr;
    std::vector<ffi_type*> m_ffi_args;
    ffi_cif m_cif = {};
    bool m_pass_xrtHandles = false;
    int m_return_offset = 1;
    
    int wait_next_cmd();
    int create_softkernelfile(xclDeviceHandle handle, int bohdl);
    int delete_softkernelfile();
    int create_softkernel(int *boh);
    int get_return_offset(std::vector<xrt_core::xclbin::kernel_argument> args);
    ffi_type* convert_to_ffitype(xrt_core::xclbin::kernel_argument arg);
};

}

#endif
