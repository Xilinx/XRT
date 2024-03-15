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

#include "aie_profile.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <memory>

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"
#include "xdp/profile/plugin/aie_profile/x86/aie_profile_kernel_config.h"

constexpr uint32_t ALIGNMENT_SIZE = 4096;

constexpr uint64_t OUTPUT_SIZE = ALIGNMENT_SIZE * 22;  // Calculated maximum output size for all 400 tiles
constexpr uint64_t INPUT_SIZE = ALIGNMENT_SIZE * 2;    // input/output must be aligned to 4096

namespace xdp {
  using ProfileInputConfiguration = xdp::built_in::ProfileInputConfiguration;
  using ProfileOutputConfiguration = xdp::built_in::ProfileOutputConfiguration;
  using PSCounterInfo = xdp::built_in::PSCounterInfo;
  using ProfileTileType = xdp::built_in::ProfileTileType;
  using severity_level = xrt_core::message::severity_level;

  AieProfile_x86Impl::AieProfile_x86Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata)
  {
    auto spdevice = xrt_core::get_userpf_device(metadata->getHandle());
    device = xrt::device(spdevice);

    auto uuid = device.get_xclbin_uuid();

    if (metadata->getHardwareGen() == 1)
      aie_profile_kernel = xrt::kernel(device, uuid.get(), "aie_profile_config");
    else
      aie_profile_kernel = xrt::kernel(device, uuid.get(), "aie2_profile_config");
  }

  void AieProfile_x86Impl::updateDevice()
  {
    setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());
  }

  bool AieProfile_x86Impl::setMetricsSettings(const uint64_t deviceId, void* handle)
  {
    int NUM_MODULES = metadata->getNumModules();

    // Create the Configuration PS kernel
    //  Calculate number of tiles per module
    int numTiles = 0;
    for (int module = 0; module < NUM_MODULES; ++module) {
      numTiles += metadata->getConfigMetrics(module).size();
    }

    std::size_t total_size = sizeof(ProfileInputConfiguration) + sizeof(ProfileTileType[numTiles - 1]);
    ProfileInputConfiguration* input_params = (ProfileInputConfiguration*)malloc(total_size);
    input_params->numTiles = numTiles;
    input_params->offset = metadata->getAIETileRowOffset();

    // Create the Profile Tile Struct with All Tiles
    ProfileTileType profileTiles[numTiles];
    int tile_idx = 0;

    auto configChannel0 = metadata->getConfigChannel0();
    auto configChannel1 = metadata->getConfigChannel1();

    for (int module = 0; module < NUM_MODULES; ++module) {
      auto configMetrics = metadata->getConfigMetrics(module);
      for (auto& tileMetric : configMetrics) {
        profileTiles[tile_idx].col = tileMetric.first.col;
        profileTiles[tile_idx].row = tileMetric.first.row;
        profileTiles[tile_idx].stream_id = tileMetric.first.stream_id;
        profileTiles[tile_idx].is_master = tileMetric.first.is_master;
        profileTiles[tile_idx].itr_mem_addr = tileMetric.first.itr_mem_addr;
        profileTiles[tile_idx].is_trigger = tileMetric.first.is_trigger;
        profileTiles[tile_idx].metricSet =
            metadata->getMetricSetIndex(tileMetric.second, metadata->getModuleType(module));
        profileTiles[tile_idx].tile_mod = module;

        // If the tile is a memtile, check if any channel specification is
        // present
        if (configChannel0.count(tileMetric.first))
          profileTiles[tile_idx].channel0 = configChannel0[tileMetric.first];
        if (configChannel1.count(tileMetric.first))
          profileTiles[tile_idx].channel1 = configChannel1[tileMetric.first];

        input_params->tiles[tile_idx] = profileTiles[tile_idx];
        tile_idx++;
      }
    }

    if (tile_idx == 0) {
      std::string msg = "No tiles were found in the AIE_METADATA section. Profiling is not enabled.";
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
      return false;
    }

    uint8_t* input = reinterpret_cast<uint8_t*>(input_params);

    try {
      // input bo
      auto inbo = xrt::bo(device, INPUT_SIZE, 2);
      auto inbo_map = inbo.map<uint8_t*>();
      std::fill(inbo_map, inbo_map + INPUT_SIZE, 0);

      // output bo
      auto outbo = xrt::bo(device, OUTPUT_SIZE, 2);
      auto outbo_map = outbo.map<uint8_t*>();
      memset(outbo_map, 0, OUTPUT_SIZE);

      std::memcpy(inbo_map, input, total_size);
      inbo.sync(XCL_BO_SYNC_BO_TO_DEVICE, INPUT_SIZE, 0);

      auto run = aie_profile_kernel(inbo, outbo, 0 /*setup iteration*/);
      run.wait();

      outbo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, OUTPUT_SIZE, 0);
      ProfileOutputConfiguration* cfg = reinterpret_cast<ProfileOutputConfiguration*>(outbo_map);

      numCountersConfigured = cfg->numCounters;
      for (uint32_t i = 0; i < cfg->numCounters; i++) {
        // Store counter info in database
        auto& counter = cfg->counters[i];
        std::string counterName = "AIE Counter " + std::to_string(counter.counterId);
        if (!metadata->checkModule(counter.moduleName)) {
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Invalid Module Returned from PS Kernel. Data may be invalid.");
          counter.moduleName = 0;
        }

        (db->getStaticInfo())
            .addAIECounter(deviceId, counter.counterId, counter.col, counter.row, counter.counterNum,
                           counter.startEvent, counter.endEvent, counter.resetEvent, counter.payload,
                           metadata->getClockFreqMhz(), metadata->getModuleName(counter.moduleName), counterName);
      }
    
    }
    catch (...) {
      std::string msg = "The aie_profile_config PS kernel was not found.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      free(input_params);
      return false;
    }

    std::string msg = "The aie_profile_config PS kernel was successfully scheduled.";
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);

    free(input_params);

    return true;
  }

  void AieProfile_x86Impl::poll(const uint32_t index, void* handle)
  {
    try {
      // input bo
      //  We Don't need to pass data from the db for polling since
      //  the counters are stored locally in PS memory after setup

      if (numCountersConfigured == 0) return;

      auto inbo = xrt::bo(device, INPUT_SIZE, 2);
      auto inbo_map = inbo.map<uint8_t*>();
      memset(inbo_map, 0, INPUT_SIZE);

      // output bo
      auto outbo = xrt::bo(device, OUTPUT_SIZE, 2);
      auto outbo_map = outbo.map<uint8_t*>();
      memset(outbo_map, 0, OUTPUT_SIZE);

      auto run = aie_profile_kernel(inbo, outbo, 1 /*poll iteration*/);
      run.wait();
      outbo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, OUTPUT_SIZE, 0);
      ProfileOutputConfiguration* cfg = reinterpret_cast<ProfileOutputConfiguration*>(outbo_map);

      for (uint32_t i = 0; i < numCountersConfigured; i++) {
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
    }
    catch (...) {
      std::string msg = "The aie_profile polling failed.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }
  }

  void AieProfile_x86Impl::freeResources()
  {
    try {
      auto inbo = xrt::bo(device, INPUT_SIZE, 2);
      auto inbo_map = inbo.map<uint8_t*>();
      memset(inbo_map, 0, INPUT_SIZE);

      // output bo
      auto outbo = xrt::bo(device, OUTPUT_SIZE, 2);
      auto outbo_map = outbo.map<uint8_t*>();
      memset(outbo_map, 0, OUTPUT_SIZE);

      auto run = aie_profile_kernel(inbo, outbo, 2 /*cleanup iteration*/);
      run.wait();
    }
    catch (...) {
      std::string msg = "The aie_profile cleanup failed.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }
  }
}  // namespace xdp
