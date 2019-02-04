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

  static bool gActive = false;
  static bool gDead = false;

  bool
  active() {
    return gActive;
}

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

  gActive = true;
  };

  RTSingleton::~RTSingleton() {
    gActive = false;
    gDead = true;
    // Destruct in reverse order of construction
    delete DebugMgr;
  }

} // xdp
