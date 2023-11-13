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

#include <boost/format.hpp>
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
#include "pscontext.h"
#include "xclbin.h"
#include "xclhal2_mpsoc.h"
#include "xrt/xrt_device.h"

typedef pscontext* (* kernel_init_t)(xclDeviceHandle device, const uuid_t &uuid);
typedef int (* kernel_fini_t)(pscontext *xrtHandles);

namespace xrt {

  struct ps_arg {
    size_t paddr;
    size_t psize;
    size_t bo_offset;
    void *vaddr;
    int bo_handle;
  };

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
  skd(const xclDeviceHandle handle, const int sk_meta_bohdl, const int sk_bohdl,
      const std::string &kname, const uint32_t cu_index, unsigned char *uuid_in,
      const int parent_mem_bo_in, const uint64_t mem_start_paddr_in, const uint64_t mem_size_in);
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
  void report_ready() const;
  void report_crash() const;
  void report_fini() const;

 private:
    xclDeviceHandle m_parent_devhdl = 0;
    // XRT Device Handle for the child process
    xclDeviceHandle m_devhdl = 0;
    xrtDeviceHandle m_xrtdhdl = 0;
    uuid m_xclbin_uuid;
    // Path of PS kernel object file constructed from PS kernel path and PS kernel name
    const std::filesystem::path m_sk_path;
    // PS Kernel instance name
    std::string m_sk_name = "";
    // PS Kernel CU Index assigned from host
    uint32_t m_cu_idx = 0;
    pscontext* m_xrtHandle = nullptr;

    // BO handle for PS Kernel Object and PS kernel metadata - only used in kernel initialization
    int m_sk_bo = 0;
    int m_sk_meta_bo = 0;

    // Member variables used when mapping the entire DDR space
    int m_parent_bo_handle = 0;
    uint64_t m_mem_start_paddr = 0;
    uint64_t m_mem_size = 0;
    void* m_mem_start_vaddr = nullptr;

    // Signal value from signal handler
    int signal = 0;

    // Handle to PS kernel file and kernel symbol
    void* m_sk_handle = nullptr;
    void* m_kernel = nullptr;

    // Arguments from host and return value offset
    std::vector<xrt_core::xclbin::kernel_argument> m_kernel_args;
    int m_cmd_boh = -1;
    uint32_t *m_args_from_host = nullptr;
    std::vector<ffi_type*> m_ffi_args;
    ffi_cif m_cif = {};
    bool m_pass_xrtHandles = false;
    int m_return_offset = 1;
    
    int wait_next_cmd() const;
    int create_softkernelfile(const xclDeviceHandle handle, const int bohdl) const;
    int delete_softkernelfile() const;
    int create_softkernel(int *boh);
    int get_return_offset(const std::vector<xrt_core::xclbin::kernel_argument> &args) const;
    ffi_type* convert_to_ffitype(const xrt_core::xclbin::kernel_argument &arg) const;
};

}

#endif
