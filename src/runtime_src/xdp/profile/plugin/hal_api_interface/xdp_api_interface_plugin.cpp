/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_SOURCE

#include "xdp/profile/plugin/hal_api_interface/xdp_api_interface_plugin.h"
#include "xdp/profile/plugin/hal_api_interface/xdp_api_interface.h"
#include "xclperf.h"

namespace xdp {

  // A single object created when the library is loaded.
  static HALAPIInterface APIInterface ;

  static void start_device_profiling_from_hal(void* payload)
  {
    // HAL pointer
    xclDeviceHandle handle = ((CBPayload*)payload)->deviceHandle ;
    
    APIInterface.startProfiling(handle) ;
  }

  static void create_profile_results(void* payload)
  {
    ProfileResultsCBPayload* payld = (ProfileResultsCBPayload*)payload ;
    xclDeviceHandle handle = payld->basePayload.deviceHandle ; // HAL pointer

    APIInterface.createProfileResults(handle, payld->results) ;
  }

  static void get_profile_results(void* payload)
  {
    ProfileResultsCBPayload* payld = (ProfileResultsCBPayload*)payload ;
    xclDeviceHandle handle = payld->basePayload.deviceHandle ; // HAL pointer

    APIInterface.getProfileResults(handle, payld->results) ;
  }

  static void destroy_profile_results(void* payload)
  {
    ProfileResultsCBPayload* payld = (ProfileResultsCBPayload*)payload ;
    xclDeviceHandle handle = payld->basePayload.deviceHandle ; // HAL pointer

    APIInterface.destroyProfileResults(handle, payld->results) ;
  }

} // namespace xdp

// Interface function visible from main XRT code
void hal_api_interface_cb_func(HalInterfaceCallbackType cb_type, void* payload)
{
  switch (cb_type)
  {
  case HalInterfaceCallbackType::START_DEVICE_PROFILING:
    xdp::start_device_profiling_from_hal(payload) ;
    break ;
  case HalInterfaceCallbackType::CREATE_PROFILE_RESULTS:
    xdp::create_profile_results(payload) ;
    break ;
  case HalInterfaceCallbackType::GET_PROFILE_RESULTS:
    xdp::get_profile_results(payload) ;
    break ;
  case HalInterfaceCallbackType::DESTROY_PROFILE_RESULTS:
    xdp::destroy_profile_results(payload) ;
    break ;
  default:
    break ;
  }
}
