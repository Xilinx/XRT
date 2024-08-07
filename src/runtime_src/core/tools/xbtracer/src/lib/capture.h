// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt_device_inst.h"

namespace xrt::tools::xbtracer {

class xrt_ftbl
{
  public:
  xrt_device_ftbl     device;

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
