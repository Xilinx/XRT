/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "rt_singleton.h"
#include "xdp/appdebug/appdebug.h"

// TODO: remove these dependencies
#include "xdp/profile/plugin/ocl/xocl_profile.h"

namespace xdp {

  static bool gDead = false;

  RTSingleton* 
  RTSingleton::Instance() {
    if (gDead) {
      std::cout << "RTSingleton is dead\n";
      return nullptr;
    }
    
    static RTSingleton singleton;
    return &singleton;
  }

  RTSingleton::RTSingleton()
  : Status( CL_SUCCESS ),
    Platform( nullptr ),
    ProfileMgr( nullptr ),
    DebugMgr( nullptr )
  {
    DebugMgr = new RTDebug();

    // share ownership of the global platform
    Platform = xocl::get_shared_platform();

    if (xrt::config::get_app_debug()) {
      appdebug::register_xocl_appdebug_callbacks();
    }

#ifdef PMD_OCL
    return;
#endif
  };

  RTSingleton::~RTSingleton() {
    gDead = true;
    // Destruct in reverse order of construction
    delete DebugMgr;
  }

  // TODO: Remove these 3 functions once we implement HAL calls in xdp device

  unsigned RTSingleton::getProfileNumberSlots(xclPerfMonType type, std::string& deviceName) {
    unsigned numSlots = xdp::profile::platform::get_profile_num_slots(Platform.get(),
        deviceName, type);
    return numSlots;
  }

  void RTSingleton::getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                                       unsigned slotnum, std::string& slotName) {
    xdp::profile::platform::get_profile_slot_name(Platform.get(), deviceName,
        type, slotnum, slotName);
  }

  unsigned RTSingleton::getProfileSlotProperties(xclPerfMonType type, std::string& deviceName, unsigned slotnum) {
    return xdp::profile::platform::get_profile_slot_properties(Platform.get(), deviceName, type, slotnum);
  }

} // xdp
