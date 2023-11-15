/**
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

#include "hal_api_interface.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"

#include "core/include/xdp/hal_api.h"

namespace sfs = std::filesystem;

namespace xdphalinterface {

  std::function<void(unsigned int, void*)> cb ;

  std::atomic<unsigned> global_idcode(0);
  
  static bool cb_valid() {
    return cb != nullptr ;
  }

  APIInterfaceLoader::APIInterfaceLoader()
  {
    if (xrt_core::config::get_profile_api())
      load_xdp_hal_interface_plugin_library() ;
  }

  APIInterfaceLoader::~APIInterfaceLoader()
  {
  }
    
  StartDeviceProfilingCls::StartDeviceProfilingCls(xclDeviceHandle handle)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) return;
    xdp::CBPayload payload = {0, handle};
    cb(xdp::HalInterfaceCallbackType::start_device_profiling, &payload);
  }

  StartDeviceProfilingCls::~StartDeviceProfilingCls()
  {}

  CreateProfileResultsCls::CreateProfileResultsCls(xclDeviceHandle handle, ProfileResults** results, int& status)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) { status = (-1); return; }
    
    xdp::ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};   // pass ProfileResults** as void*
    cb(xdp::HalInterfaceCallbackType::create_profile_results, &payload);
    status = 0;
  }

  CreateProfileResultsCls::~CreateProfileResultsCls()
  {}

  GetProfileResultsCls::GetProfileResultsCls(xclDeviceHandle handle, ProfileResults* results, int& status)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) { status = (-1); return; }

    xdp::ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};
    cb(xdp::HalInterfaceCallbackType::get_profile_results, &payload);
    status = 0;
  }

  GetProfileResultsCls::~GetProfileResultsCls()
  {}

  DestroyProfileResultsCls::DestroyProfileResultsCls(xclDeviceHandle handle, ProfileResults* results, int& status)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) { status = (-1); return; }

    xdp::ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};
    cb(xdp::HalInterfaceCallbackType::destroy_profile_results, &payload);
    status = 0;
  }

  DestroyProfileResultsCls::~DestroyProfileResultsCls()
  {}

  void register_hal_interface_callbacks(void* handle)
  {
    typedef void (*ftype)(unsigned int, void*) ;
    cb = (ftype)(xrt_core::dlsym(handle, "hal_api_interface_cb_func")) ;
    if (xrt_core::dlerror() != NULL) cb = nullptr ;
  }

  int error_hal_interface_callbacks()
  {
    return 0 ;
  }

  void load_xdp_hal_interface_plugin_library()
  {
    static xrt_core::module_loader
      xdp_hal_interface_loader("xdp_hal_api_interface_plugin",
			       register_hal_interface_callbacks,
			       nullptr, // warning function
			       error_hal_interface_callbacks) ;
  }

}
