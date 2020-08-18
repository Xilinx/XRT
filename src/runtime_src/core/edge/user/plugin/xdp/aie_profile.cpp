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

#include "aie_profile.h"
#include "core/common/module_loader.h"

namespace xdpaieprofile {

  void load_xdp_aie_plugin()
  {
#ifdef XRT_ENABLE_AIE
    static xrt_core::module_loader xdp_aie_loader("xdp_aie_plugin",
						    register_aie_callbacks,
						    warning_aie_callbacks);
#endif
  }

  void register_aie_callbacks(void* /*handle*/)
  {
    // No callbacks in AIE profiling. The plugin is always active.
  }

  void warning_aie_callbacks()
  {
    // No warnings for AIE profiling
  }

} // end namespace xdpaieprofile
