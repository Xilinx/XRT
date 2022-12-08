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

#define XDP_SOURCE 

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <memory>
#include <cstring>

#include "aie_profile.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile_new/aie_profile_metadata.h"
#include "xdp/profile/plugin/aie_profile_new/x86/aie_profile_kernel_config.h"

namespace xdp {

  using severity_level = xrt_core::message::severity_level;

  AieProfile_x86Impl::AieProfile_x86Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata) {}

  void AieProfile_x86Impl::updateDevice()
  {
    bool runtimeCounters = setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());

    // @ TODO CHECK COMPILE TIME COUNTERS!
    // if (!runtimeCounters) {
    //     std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(metadata->getHandle());
    //     auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

    //     if (counters.empty()) {
    //       xrt_core::message::send(severity_level::warning, "XRT", 
    //         "AIE Profile Counters were not found for this design. Please specify tile_based_[aie|aie_memory|interface_tile]_metrics under \"AIE_profile_settings\" section in your xrt.ini.");
    //       (db->getStaticInfo()).setIsAIECounterRead(metadata->getDeviceID(),true);
    //       return;
    //     }
    //     else {
    //       XAie_DevInst* aieDevInst =
    //         static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, metadata->getHandle()));

    //       for (auto& counter : counters) {
    //         tile_type tile;
    //         auto payload = getCounterPayload(aieDevInst, tile, counter.column, counter.row, 
    //                                          counter.startEvent);

    //         (db->getStaticInfo()).addAIECounter(metadata->getDeviceID(), counter.id, counter.column,
    //             counter.row + 1, counter.counterNumber, counter.startEvent, counter.endEvent,
    //             counter.resetEvent, payload, counter.clockFreqMhz, counter.module, counter.name);
    //       }
    //     }
    //   }
  }

  bool AieProfile_x86Impl::setMetricsSettings(uint64_t deviceId, void* handle){
    int counterId = 0;
    bool runtimeCounters = false;

    // Get AIE clock frequency
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    // auto clockFreqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());

    // Currently supporting Core, Memory, Interface Tile metrics only. Need to add Memory Tile metrics
    constexpr int NUM_MODULES = 3;

    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};
    std::string defaultSets[NUM_MODULES] = {"all:heat_map", "all:conflicts", "all:input_bandwidths"};

    int numCountersMod[NUM_MODULES] =
        {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS};
    module_type moduleTypes[NUM_MODULES] = 
        {module_type::core, module_type::dma, module_type::shim};

    // Get the metrics settings
    std::vector<std::string> metricsConfig;

    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_memory_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_interface_tile_metrics());
    //metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_mem_tile_metrics());

    // Get the graph metrics settings
    std::vector<std::string> graphmetricsConfig;

    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_metrics());
    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_memory_metrics());
//    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_interface_tile_metrics());
//    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_mem_tile_metrics());

    // Process AIE_profile_settings metrics
    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::vector<std::string>> metricsSettings(NUM_MODULES);
    std::vector<std::vector<std::string>> graphmetricsSettings(NUM_MODULES);

    bool newConfigUsed = false;
    for(int module = 0; module < NUM_MODULES; ++module) {
      bool findTileMetric = false;
      if (!metricsConfig[module].empty()) {
        boost::replace_all(metricsConfig[module], " ", "");
        boost::split(metricsSettings[module], metricsConfig[module], boost::is_any_of(";"));
        findTileMetric = true;        
      } else {
          std::string modName = moduleNames[module].substr(0, moduleNames[module].find(" "));
          std::string metricMsg = "No metric set specified for " + modName + " module. " +
                                  "Please specify the AIE_profile_settings." + modName + "_metrics setting in your xrt.ini. A default set of " + defaultSets[module] + " has been specified.";
          xrt_core::message::send(severity_level::warning, "XRT", metricMsg);

          metricsConfig[module] = defaultSets[module];
          boost::split(metricsSettings[module], metricsConfig[module], boost::is_any_of(";"));
          findTileMetric = true;

      }
      if ((module < graphmetricsConfig.size()) && !graphmetricsConfig[module].empty()) {
        /* interface_tile metrics is not supported for Graph based metrics.
         * Only aie and aie_memory are supported.
         */
        boost::replace_all(graphmetricsConfig[module], " ", "");
        boost::split(graphmetricsSettings[module], graphmetricsConfig[module], boost::is_any_of(";"));
        findTileMetric = true;        
      }

      if(findTileMetric) {
        newConfigUsed = true;

        if (module_type::shim == moduleTypes[module]) {
          metadata->getInterfaceConfigMetricsForTiles(module, 
                                       metricsSettings[module], 
                                       /* graphmetricsSettings[module], */
                                       handle);
        } else {
          metadata->getConfigMetricsForTiles(module, 
                                   metricsSettings[module], 
                                   graphmetricsSettings[module], 
                                   moduleTypes[module],
                                   handle);
        }
      }
    }

    //Create the PS kernel 
    using ProfileInputConfiguration = xdp::built_in::ProfileInputConfiguration;
    using ProfileOutputConfiguration = xdp::built_in::ProfileOutputConfiguration;
    using ProfileTileType = xdp::built_in::ProfileTileType;

    // Calculate number of tiles per module
    uint16_t tileCounts[NUM_MODULES];
    int numTiles = 0;
    for(int module = 0; module < NUM_MODULES; ++module) {
      auto size = metadata->getConfigMetrics(module).size();
      tileCounts[module] = size;
      numTiles += size;
    }

    std::size_t total_size = sizeof(ProfileInputConfiguration) + sizeof(ProfileTileType[numTiles-1]);
    ProfileInputConfiguration* input_params = (ProfileInputConfiguration*)malloc(total_size);

    //Create the Profile Tile Struct with All Tiles
    xdp::built_in::ProfileTileType profileTiles[numTiles];
    int tile_idx = 0;
    for(int module = 0; module < NUM_MODULES; ++module) {
      auto configMetrics = metadata->getConfigMetrics(module);
      for (auto &tileMetric : configMetrics){
        profileTiles[tile_idx].col = tileMetric.first.col;
        profileTiles[tile_idx].row = tileMetric.first.row;
        profileTiles[tile_idx].itr_mem_row = tileMetric.first.itr_mem_row;
        profileTiles[tile_idx].itr_mem_col = tileMetric.first.itr_mem_col;
        profileTiles[tile_idx].itr_mem_addr = tileMetric.first.itr_mem_addr;
        profileTiles[tile_idx].is_trigger = tileMetric.first.is_trigger;
        profileTiles[tile_idx].metricSet = tileMetric.second;
        input_params->tiles[tile_idx] = profileTiles[tile_idx];
        tile_idx++;
      }

      input_params->numTiles[module] = tileCounts[module];
    }

    uint8_t* input = reinterpret_cast<uint8_t*>(input_params);

    try {
      
      auto spdevice = xrt_core::get_userpf_device(handle);
      auto device = xrt::device(spdevice);
    
      auto uuid = device.get_xclbin_uuid();
      auto aie_profile_kernel = xrt::kernel(device, uuid.get(), "aie_profile_config");

      //input bo  
      auto inbo = xrt::bo(device, INPUT_SIZE, 2);
      auto inbo_map = inbo.map<uint8_t*>();
      std::fill(inbo_map, inbo_map + INPUT_SIZE, 0);
   
      //output bo
      auto outbo = xrt::bo(device, OUTPUT_SIZE, 2);
      auto outbo_map = outbo.map<uint8_t*>();
      memset(outbo_map, 0, OUTPUT_SIZE);

      // //message_output_bo
      // auto messagebo = xrt::bo(device, MSG_OUTPUT_SIZE, 2);
      // auto messagebomapped = messagebo.map<uint8_t*>();
      // memset(messagebomapped, 0, MSG_OUTPUT_SIZE);

      std::memcpy(inbo_map, input, total_size);
      inbo.sync(XCL_BO_SYNC_BO_TO_DEVICE, INPUT_SIZE, 0);

      auto run = aie_profile_kernel(inbo, outbo, 0 /*iteration*/);
      run.wait();
    }
    runtimeCounters = true;

    return runtimeCounters;
  }

  void AieProfile_x86Impl::poll(uint32_t index, void* handle)
  {
    //TODO
  }
}
