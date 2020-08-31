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

#include "plugin/xdp/power_profile.h"
#include "core/common/module_loader.h"

namespace xdppowerprofile {

  void load_xdp_power_plugin()
  {
    static xrt_core::module_loader xdp_power_loader("xdp_power_plugin",
						    register_power_callbacks,
						    warning_power_callbacks) ;
  }

  void register_power_callbacks(void* /*handle*/)
  {
    // No callbacks in power profiling.  The plugin is always active
  }

  void warning_power_callbacks()
  {
    // No warnings for power profiling
  }

} // end namespace xdppowerprofile
