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


constexpr uint32_t ALIGNMENT_SIZE = 4096;

constexpr uint64_t OUTPUT_SIZE = ALIGNMENT_SIZE * 22; //Calculated maximum output size for all 400 tiles
constexpr uint64_t INPUT_SIZE = ALIGNMENT_SIZE * 2; // input/output must be aligned to 4096

namespace xdp {
  using ProfileInputConfiguration = xdp::built_in::ProfileInputConfiguration;
  using ProfileOutputConfiguration = xdp::built_in::ProfileOutputConfiguration;
  using ProfileTileType = xdp::built_in::ProfileTileType;
  using severity_level = xrt_core::message::severity_level;

  AieProfile_x86Impl::AieProfile_x86Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata) {}

  void AieProfile_x86Impl::updateDevice()
  {
    bool runtimeCounters = setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());

    if (!runtimeCounters){
      //start compile time counter checks
    }
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

    int NUM_MODULES = metadata->getNumModules();

    //Create the Configuration PS kernel 
    // Calculate number of tiles per module
    int numTiles = 0;
    for(int module = 0; module < NUM_MODULES; ++module) {
      numTiles += metadata->getConfigMetrics(module).size();
    }

    std::size_t total_size = sizeof(ProfileInputConfiguration) + sizeof(ProfileTileType[numTiles-1]);
    ProfileInputConfiguration* input_params = (ProfileInputConfiguration*)malloc(total_size);
    input_params->numTiles = numTiles;
    
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
        profileTiles[tile_idx].metricSet = metadata->getMetricSetIndex(tileMetric.second, metadata->getModuleType(module));
        profileTiles[tile_idx].tile_mod = module;
        input_params->tiles[tile_idx] = profileTiles[tile_idx];
        tile_idx++;
      }
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

      std::memcpy(inbo_map, input, total_size);
      inbo.sync(XCL_BO_SYNC_BO_TO_DEVICE, INPUT_SIZE, 0);

      auto run = aie_profile_kernel(inbo, outbo, 0 /*setup iteration*/);
      run.wait();

      outbo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, OUTPUT_SIZE, 0);
      ProfileOutputConfiguration* cfg = reinterpret_cast<ProfileOutputConfiguration*>(outbo_map);
    
      for (uint32_t i = 0; i < cfg->numCounters; i++){
        // Store counter info in database
        auto& counter = cfg->counters[i];
        std::string counterName = "AIE Counter " + std::to_string(counter.counterId);
        (db->getStaticInfo()).addAIECounter(deviceId, counter.counterId, counter.col, counter.row, counter.counterNum,
        counter.startEvent, counter.endEvent, counter.resetEvent, counter.payload, metadata->getClockFreqMhz() , 
        metadata->getModuleName(counter.moduleName), counterName);
      }
    } catch (...) {
      std::string msg = "The aie_profile_config PS kernel was not found.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return false;
    }

    std::string msg = "The aie_profile_config PS kernel was successfully scheduled.";
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);

    free(input_params);

    return true;
  }

  void AieProfile_x86Impl::poll(uint32_t index, void* handle)
  {

    try {
      auto spdevice = xrt_core::get_userpf_device(handle);
      auto device = xrt::device(spdevice);
    
      auto uuid = device.get_xclbin_uuid();
      auto aie_profile_kernel = xrt::kernel(device, uuid.get(), "aie_profile_config");

      //input bo  
      // We Don't need to pass data from the db for polling since
      // the counters are stored locally in PS memory after setup
      auto inbo = xrt::bo(device, INPUT_SIZE, 2);
      auto inbo_map = inbo.map<uint8_t*>();
      memset(inbo_map, 0, INPUT_SIZE); 
   
      //output bo
      auto outbo = xrt::bo(device, OUTPUT_SIZE, 2);
      auto outbo_map = outbo.map<uint8_t*>();
      memset(outbo_map, 0, OUTPUT_SIZE);

      auto run = aie_profile_kernel(inbo, outbo, 1 /*poll iteration*/);
      run.wait();
      outbo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, OUTPUT_SIZE, 0);
      ProfileOutputConfiguration* cfg = reinterpret_cast<ProfileOutputConfiguration*>(outbo_map);

      for (uint32_t i = 0; i < cfg->numCounters; i++){
        std::vector<uint64_t> values;
        auto& counter = cfg->counters[i];
        values.push_back(counter.col);
        values.push_back(counter.row);
        values.push_back(counter.startEvent);
        values.push_back(counter.endEvent);
        values.push_back(counter.resetEvent);
        values.push_back(counter.counterValue);
        values.push_back(counter.timerValue);
        values.push_back(counter.payload);
        double timestamp = xrt_core::time_ns() / 1.0e6;
        db->getDynamicInfo().addAIESample(index, timestamp, values);
      } 

    } catch (...) {
      std::string msg = "The aie_profile polling failed.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }

  }
}
