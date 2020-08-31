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

#include "plugin/xdp/noc_profile.h"
#include "core/common/module_loader.h"

namespace xdpnocprofile {

  void load_xdp_noc_plugin()
  {
    static xrt_core::module_loader xdp_noc_loader("xdp_noc_plugin",
						    register_noc_callbacks,
						    warning_noc_callbacks);
  }

  void register_noc_callbacks(void* /*handle*/)
  {
    // No callbacks in NOC profiling. The plugin is always active.
  }

  void warning_noc_callbacks()
  {
    // No warnings for NOC profiling
  }

} // end namespace xdpnocprofile
