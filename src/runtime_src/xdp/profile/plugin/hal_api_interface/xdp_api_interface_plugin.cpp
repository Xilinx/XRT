/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

// This file includes the functions that bridge the gap from host code
//  to the dynamically loaded library.  The actual implementations
//  are abstracted in the HALAPIInterface object

#define XDP_PLUGIN_SOURCE

#include "core/include/xdp/hal_api.h"
#include "xdp/profile/plugin/hal_api_interface/xdp_api_interface_plugin.h"
#include "xdp/profile/plugin/hal_api_interface/xdp_api_interface.h"

namespace xdp {

  // A single object created when the library is loaded.
  static HALAPIInterface APIInterface ;

  static void start_device_profiling_from_hal(void* payload)
  {
    // HAL pointer
    xclDeviceHandle handle = reinterpret_cast<xdp::CBPayload*>(payload)->deviceHandle;
    
    APIInterface.startProfiling(handle) ;
  }

  static void create_profile_results_from_hal(void* payload)
  {
    xdp::ProfileResultsCBPayload* payld = reinterpret_cast<xdp::ProfileResultsCBPayload*>(payload) ;
    xclDeviceHandle handle = payld->basePayload.deviceHandle ; // HAL pointer

    APIInterface.createProfileResults(handle, payld->results) ;
  }

  static void get_profile_results_from_hal(void* payload)
  {
    xdp::ProfileResultsCBPayload* payld = reinterpret_cast<xdp::ProfileResultsCBPayload*>(payload) ;
    xclDeviceHandle handle = payld->basePayload.deviceHandle ; // HAL pointer

    APIInterface.getProfileResults(handle, payld->results) ;
  }

  static void destroy_profile_results_from_hal(void* payload)
  {
    xdp::ProfileResultsCBPayload* payld = reinterpret_cast<xdp::ProfileResultsCBPayload*>(payload) ;
    xclDeviceHandle handle = payld->basePayload.deviceHandle ; // HAL pointer

    APIInterface.destroyProfileResults(handle, payld->results) ;
  }

} // namespace xdp

// Interface function visible from main XRT code
void hal_api_interface_cb_func(xdp::HalInterfaceCallbackType cb_type, void* payload)
{
  if (!xdp::HALAPIInterface::alive())
    return;

  switch (cb_type)
  {
  case xdp::HalInterfaceCallbackType::start_device_profiling:
    xdp::start_device_profiling_from_hal(payload) ;
    break ;
  case xdp::HalInterfaceCallbackType::create_profile_results:
    xdp::create_profile_results_from_hal(payload) ;
    break ;
  case xdp::HalInterfaceCallbackType::get_profile_results:
    xdp::get_profile_results_from_hal(payload) ;
    break ;
  case xdp::HalInterfaceCallbackType::destroy_profile_results:
    xdp::destroy_profile_results_from_hal(payload) ;
    break ;
  default:
    break ;
  }
}
