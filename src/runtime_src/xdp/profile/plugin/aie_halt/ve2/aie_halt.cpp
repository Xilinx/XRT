/**
 * Copyright (C) 2025 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include "core/common/api/hw_context_int.h"
#include "core/common/device.h"
#include "core/common/message.h"

#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/experimental/xrt_module.h"

#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/plugin/aie_halt/ve2/aie_halt.h"
#include "xdp/profile/plugin/vp_base/utility.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {


  AIEHaltVE2Impl::AIEHaltVE2Impl(VPDatabase*dB)
    : AIEHaltImpl(dB)
  {
  }

  void AIEHaltVE2Impl::updateDevice(void* hwCtxImpl)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
              "In AIEHaltVE2Impl::updateDevice");

    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(hwCtxImpl);

    xrt::elf haltElf("aiehalt4x4.elf");
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
              "In AIEHaltVE2Impl New Elf Object created for custom Elf");

    xrt::module mod{haltElf};

    xrt::kernel krnl = xrt::ext::kernel{hwContext, mod, "XDP_KERNEL:{IPUV1CNN}"};
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
              "In AIEHaltVE2Impl New Kernel Object for XDP_KERNEL created for running custom Elf");      

    xrt::run rn{krnl};

    rn.start();
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
              "In AIEHaltVE2Impl run start, going to wait");  

    rn.wait2();
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
              "In AIEHaltVE2Impl wait completed");  

  }

  void AIEHaltVE2Impl::finishflushDevice(void* /*hwCtxImpl*/)
  {
  }
}
