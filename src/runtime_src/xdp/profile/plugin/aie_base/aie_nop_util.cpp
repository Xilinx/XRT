// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#include "xdp/profile/plugin/aie_base/aie_nop_util.h"

#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/experimental/xrt_module.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/message.h"
#include "xrt/xrt_kernel.h"

namespace xdp::aie {

  using severity_level = xrt_core::message::severity_level;

  bool submitNopElf(void* handle)
  {
    xrt_core::message::send(severity_level::debug, "XRT",
              "In submitNopElf, going to load nop code Elf");
    
    std::string inputCtrlCode = "nop.elf";
    
    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);

    xrt::elf nopElf;
    try {
      nopElf = xrt::elf(inputCtrlCode);
    } catch (...) {
      std::string msg = "Failed to load " + inputCtrlCode + " for AIE configuration.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return false;
    }

    xrt::module mod{nopElf};
    xrt::kernel krnl;
    try {
      krnl = xrt::ext::kernel{hwContext, mod, "XDP_KERNEL:{IPUV1CNN}"};
    } catch (...) {
      xrt_core::message::send(severity_level::warning, "XRT",
                "XDP_KERNEL not found in HW Context. Cannot configure nop code.");
      return false;
    }

    xrt_core::message::send(severity_level::debug, "XRT",
              "New Kernel Object for XDP_KERNEL created for running nop code Elf");      

    xrt::run rn{krnl};
    rn.start();
    xrt_core::message::send(severity_level::debug, "XRT",
              "nop code run start, going to wait");  

    rn.wait2();
    xrt_core::message::send(severity_level::debug, "XRT",
              "nop code run wait completed, proceeding to configuration");

    return true;
  }

}  // namespace xdp::aie

