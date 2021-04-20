/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include "noc_profile.h"
#include "core/common/module_loader.h"

namespace xdp {
namespace noc {
namespace profile {

  void load()
  {
    static xrt_core::module_loader xdp_noc_loader("xdp_noc_plugin",
						  register_callbacks,
						  warning_callbacks);
  }

  void register_callbacks(void* /*handle*/)
  {
    // No callbacks in NOC profiling. The plugin is always active.
  }

  void warning_callbacks()
  {
    // No warnings for NOC profiling
  }

} // end namespace profile
} // end namespace noc
} // end namespace xdp
