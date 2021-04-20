/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "sc_profile.h"
#include "core/common/module_loader.h"

namespace xdp {
namespace sc {
namespace profile {

  void load()
  {
    static xrt_core::module_loader xdp_sc_loader("xdp_system_compiler_plugin",
                                                 register_callbacks,
                                                 warning_callbacks) ;
  }

  void register_callbacks(void* /* handle*/)
  {
    // No callbacks in System Compiler plugin
  }

  void warning_callbacks()
  {
    // No warnings in System Compiler plugin
  }

} // end namespace profile
} // end namespace sc
} // end namespace xdp
