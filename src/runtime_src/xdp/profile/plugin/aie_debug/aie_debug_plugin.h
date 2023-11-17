/**
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

#ifndef XDP_AIE_DEBUG_PLUGIN_DOT_H
#define XDP_AIE_DEBUG_PLUGIN_DOT_H

#include <boost/property_tree/ptree.hpp>
#include <memory>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"


extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

  class AieDebugPlugin : public XDPPlugin
  {
  public:
    XDP_PLUGIN_EXPORT AieDebugPlugin();
    XDP_PLUGIN_EXPORT ~AieDebugPlugin();
    XDP_PLUGIN_EXPORT void updateAIEDevice(void* handle);
    XDP_PLUGIN_EXPORT void endAIEDebugRead(void* handle);
    XDP_PLUGIN_EXPORT static bool alive();

  private:
    uint64_t getDeviceIDFromHandle(void* handle);
    void endPoll();
    std::vector<std::string> getSettingsVector(std::string settingsString);
    std::map<module_type, std::vector<uint64_t>> parseMetrics();
    aie::driver_config getAIEConfigMetadata();

  private:
    static constexpr int NUM_MODULES = 4;
    uint32_t mIndex = 0;
    xrt::kernel mKernel;
    xrt::bo input_bo; 
    XAie_DevInst aieDevInst = {0};
    boost::property_tree::ptree aie_meta;
    std::unique_ptr<xdp::aie::BaseFiletypeImpl> filetype;
    aie_profile_op_t* op;
    std::size_t op_size;
    xrt::hw_context context;

    static bool live;
    struct AIEData {
      uint64_t deviceID;
      std::atomic<bool> threadCtrlBool;
      std::thread thread;

    };
    std::map<void*, AIEData>  handleToAIEData;

  };

} // end namespace xdp

#endif
