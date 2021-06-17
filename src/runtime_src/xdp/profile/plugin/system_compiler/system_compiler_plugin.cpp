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

#define XDP_SOURCE

#include "xdp/profile/plugin/system_compiler/system_compiler_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"

namespace xdp {

  SystemCompilerPlugin::SystemCompilerPlugin() : XDPPlugin()
  {
    db->registerPlugin(this);
    db->registerInfo(info::system_compiler);

    db->getStaticInfo().addOpenedFile("sc_host_summary.csv", "PROFILE_SUMMARY");
    db->getStaticInfo().addOpenedFile("sc_trace.csv", "VP_TRACE");
  }

  SystemCompilerPlugin::~SystemCompilerPlugin()
  {
  }


} // end namespace xdp
