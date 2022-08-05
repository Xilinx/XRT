/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/database/database.h"

#include "hw_emu_pl_deadlock_plugin.h"


namespace xdp {

  HwEmuPLDeadlockPlugin::HwEmuPLDeadlockPlugin() : XDPPlugin()
  {
    db->registerPlugin(this);
  }

  HwEmuPLDeadlockPlugin::~HwEmuPLDeadlockPlugin()
  {
    if (VPDatabase::alive())
      db->unregisterPlugin(this);
  }

  void HwEmuPLDeadlockPlugin::updateDevice(void* handle)
  {
    db->getStaticInfo().addOpenedFile("pl_deadlock_diagnosis.txt", "PL_DEADLOCK_DIAGNOSIS");
  }

  void HwEmuPLDeadlockPlugin::writeAll(bool /*openNewFiles*/)
  {
    // Explicitly override to do nothing
  }

} // end namespace xdp
