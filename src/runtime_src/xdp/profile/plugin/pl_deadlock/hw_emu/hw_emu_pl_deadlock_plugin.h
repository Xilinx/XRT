/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef HW_EMU_PL_DEADLOCK_PLUGIN_DOT_H
#define HW_EMU_PL_DEADLOCK_PLUGIN_DOT_H

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

class HwEmuPLDeadlockPlugin : public XDPPlugin {

  public:
    HwEmuPLDeadlockPlugin();
    ~HwEmuPLDeadlockPlugin();
    virtual void updateDevice(void* handle);
    virtual void writeAll(bool openNewFiles);
  };

}

#endif
