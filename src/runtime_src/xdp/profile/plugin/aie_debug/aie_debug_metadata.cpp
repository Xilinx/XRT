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

#define XDP_PLUGIN_SOURCE

#include "aie_debug_metadata.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;

  /****************************************************************************
   * Constructor
   ***************************************************************************/
  AieDebugMetadata::AieDebugMetadata(uint64_t deviceID, void* handle) :
      deviceID(deviceID), handle(handle)
  {
    xrt_core::message::send(severity_level::info,
                            "XRT", "!!!!! Parsing AIE Debug Metadata.");
    VPDatabase* db = VPDatabase::Instance();

    metadataReader = (db->getStaticInfo()).getAIEmetadataReader();
    if (!metadataReader)
      return;

    // Process all module types
    for (int module = 0; module < NUM_MODULES; ++module) {
      auto type = moduleTypes[module];

      std::vector<tile_type> tiles;
      if (type == module_type::shim)
        tiles = metadataReader->getInterfaceTiles("all", "all", "input_output");
      else
        tiles = metadataReader->getTiles("all", type, "all");

      std::map<tile_type, std::string> m;
      for (auto& t : tiles){
        //configMetrics[module][t] = "tiles_debug"; Old code TODO delete
        //configMetrics.push_back(std::make_pair(t,"tiles_debug"));//  Old code TODO delete
        m[t]= "tiles_debug";
        }
      configMetrics.push_back(m);
    }

    xrt_core::message::send(severity_level::info,
                            "XRT", "!!!! Finished Parsing AIE Debug Metadata.");
  }

  /****************************************************************************
   * Get driver configuration
   ***************************************************************************/
  aie::driver_config
  AieDebugMetadata::getAIEConfigMetadata()
  {
    return metadataReader->getDriverConfig();
  }

}  // namespace xdp
