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

#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/device/device_intf.h"

namespace xdp {

  PLInfo::~PLInfo()
  {
    for (auto& i : cus) {
      delete i.second ;
    }
    for (auto& i : memoryInfo) {
      delete i.second ;
    }
    for (auto i : ams) {
      delete i ;
    }
    for (auto i : aims) {
      delete i ;
    }
    for (auto i : asms) {
      delete i ;
    }
  }

  AIEInfo::~AIEInfo()
  {
    for (auto i : nocList) {
      delete i ;
    }
  }

  XclbinInfo::~XclbinInfo()
  {
    if (deviceIntf)
      delete deviceIntf ;
  }

} // end namespace xdp
