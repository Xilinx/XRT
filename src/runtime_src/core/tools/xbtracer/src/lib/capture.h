// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt_bo_inst.h"
#include "xrt_device_inst.h"
#include "xrt_ext_inst.h"
#include "xrt_hw_context_inst.h"
#include "xrt_kernel_inst.h"
#include "xrt_xclbin_inst.h"
#include "xrt_module_inst.h"
#include "xrt_elf_inst.h"

namespace xrt::tools::xbtracer {

class xrt_ftbl
{
  public:
  xrt_device_ftbl     device;
  xrt_bo_ftbl         bo;
  xrt_kernel_ftbl     kernel;
  xrt_run_ftbl        run;
  xrt_xclbin_ftbl     xclbin;
  xrt_hw_context_ftbl hw_context;
  xrt_ext_ftbl        ext;
  xrt_module_ftbl     module;
  xrt_elf_ftbl        elf;

  static xrt_ftbl& get_instance();

  xrt_ftbl(const xrt_ftbl&) = delete;
  void operator=(const xrt_ftbl&) = delete;

  private:
  xrt_ftbl() {}
};

} // namespace xrt::tools::xbtracer

#ifdef _WIN32
#ifdef __cplusplus
extern "C" {
#endif
  __declspec(dllexport) void idt_fixup( void *dummy );
#ifdef __cplusplus
}
#endif
#endif /* #ifdef _WIN32 */
