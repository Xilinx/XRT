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

// Copyright 2014 Xilinx, Inc. All rights reserved.
#ifndef __XDP_RT_SINGLETON_H
#define __XDP_RT_SINGLETON_H

#include <CL/opencl.h>
#include "xdp/debug/rt_debug.h"
#include "xdp/profile/plugin/ocl/xocl_profile.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <iostream>

/*
 * This should be deprecated
 */

namespace xdp {

  /**
   * Check that the rtsingleton is in an active state.
   *
   * This function can be called during static global exit()
   * to check if it is no longer safe to rely on the singleton
   *
   * @return 
   *   true as long as main is running, false after the singleton dtor
   *   has been called during static global destruction.
   */
  bool active();

  class RTSingleton {

  public:
    ~RTSingleton();

  private:
    RTSingleton();

  public:
    // Singleton instance
    static RTSingleton* Instance();
    cl_int getStatus() {
      return Status;
    }

  public:
    // Inline functions: platform ID, profile/debug managers, profile flags
    inline xocl::platform* getcl_platform_id() { return Platform.get(); }
    inline RTDebug* getDebugManager() { return DebugMgr; }

  private:
    // Status of singleton
    cl_int Status;

    // Share ownership of the global platform
    std::shared_ptr<xocl::platform> Platform;

    // Debug manager
    RTDebug* DebugMgr = nullptr;

  };

} // xdp

#endif