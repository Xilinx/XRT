/**
 * Copyright (C) 2021-2022 Xilinx, Inc
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
#include <iostream>
#include <memory>
#include <cstring>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace_new/aie_trace_metadata.h"

#include "xrt/xrt_kernel.h"

#include "aie_trace_kernel_config.h"
#include "aie_trace.h"

constexpr uint32_t MAX_TILES = 400;
constexpr uint32_t MAX_LENGTH = 4096;

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using module_type = xrt_core::edge::aie::module_type;

  void AieTrace_x86Impl::updateDevice(void* handle) {
    if (handle == nullptr)
      return;

    auto deviceID = metadata->HandleToDeviceID[handle];
    // Set metrics for counters and trace events 
    if (!setMetrics(deviceID, handle)) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }
  }

  void AieTrace_x86Impl::flushDevice(void* handle) {
    if (handle == nullptr)
      return;

    auto deviceID = metadata->HandleToDeviceID[handle];
    if (aieOffloaders.find(deviceID) != aieOffloaders.end())
      (std::get<0>(aieOffloaders[deviceID]))->readTrace(true);

  }

  void AieTrace_x86Impl::finishFlushDevice(void* handle){
    if (handle == nullptr)
      return;

    auto deviceID = metadata->HandleToDeviceID[handle];
    if (aieOffloaders.find(deviceID) != aieOffloaders.end()) {
      auto& offloader = std::get<0>(aieOffloaders[deviceID]);

      if (offloader->continuousTrace()) {
        offloader->stopOffload();
        while(offloader->getOffloadStatus() != AIEOffloadThreadStatus::STOPPED);
      }

      offloader->readTrace(true);
      if (offloader->isTraceBufferFull())
        xrt_core::message::send(severity_level::warning, "XRT", AIE_TS2MM_WARN_MSG_BUF_FULL);
      offloader->endReadTrace();

      aieOffloaders.erase(deviceID);
    }
  }

  bool AieTrace_x86Impl::isEdge(){
    return false;
  }

  // No CMA checks on x86
  uint64_t AieTrace_x86Impl::checkTraceBufSize(uint64_t size) {
    return size;
  }

  bool AieTrace_x86Impl::setMetrics(uint64_t deviceId, void* handle) {
    // Create struct to pass to PS kernel
    std::string counterScheme = xrt_core::config::get_aie_trace_counter_scheme();
    std::string metricSet = metadata->getMetricSet(handle);
    uint8_t counterSchemeInt;
    uint8_t metricSetInt;

    auto tiles = metadata->getTilesForTracing(handle);
    uint32_t delayCycles = static_cast<uint32_t>(metadata->getTraceStartDelayCycles(handle));
    bool userControl = xrt_core::config::get_aie_trace_settings_start_type() == "user";
    bool useDelay = metadata->mUseDelay;

    uint16_t rows[MAX_TILES];
    uint16_t cols[MAX_TILES];

    uint16_t numTiles = 0;

    for (auto& tile : tiles){
      rows[numTiles] = tile.row;
      cols[numTiles] = tile.col;
      numTiles++;
    }

    if (counterScheme.compare("es1")) {
      counterSchemeInt = 0;
    } else {
      counterSchemeInt = 1;
    }

    if (metricSet.compare("functions") == 0){
      metricSetInt = 0;
    } else if (metricSet.compare("functions_partial_stalls") == 0){
      metricSetInt = 1;
    } else if (metricSet.compare("functions_all_stalls") == 0){
      metricSetInt = 2;
    } else { //all
      metricSetInt = 3;
    }

    //Build struct
    std::size_t total_size = sizeof(xdp::built_in::InputConfiguration) + sizeof(uint16_t[(numTiles * 2) - 1]);
    xdp::built_in::InputConfiguration* input_params = (xdp::built_in::InputConfiguration*)malloc(total_size);
    input_params->delayCycles = delayCycles;
    input_params->counterScheme = counterSchemeInt;
    input_params->metricSet = metricSetInt; 
    input_params->numTiles = numTiles;
    input_params->useDelay = useDelay;
    input_params->userControl = userControl;

    int counter = 0;
    for (int i = 0; i < numTiles * 2; i +=2){
      input_params->tiles[i] = rows[counter];
      input_params->tiles[i+1] = cols[counter];
      counter += 1;
    }

    total_size = sizeof(xdp::built_in::InputConfiguration) + sizeof(uint16_t[(numTiles * 2) - 1]);

    //Cast struct to uint8_t pointer and pass this data
    uint8_t* input = reinterpret_cast<uint8_t*>(input_params);

    // needs to be modified
    auto device = xrt::device(0);
    auto uuid = device.load_xclbin("runtime_trace_kernel.xclbin");
    auto aie_trace_kernel = xrt::kernel(device, uuid.get(), "aie_trace_kernel");

    auto bo0 = xrt::bo(device, MAX_LENGTH, 2);
    auto bo0_map = bo0.map<uint8_t*>();
    std::fill(bo0_map, bo0_map + MAX_LENGTH, 0);
    

    std::memcpy(bo0_map, input, total_size);
    bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, MAX_LENGTH, 0);
    auto run = aie_trace_kernel(bo0);
    run.wait();

    delete input_params;

  // auto device = xrt::device("0");
  // auto uuid = device.load_xclbin(xclbin_fnm);

  // Create kernel.  
  // auto trace_kernel = xrt::kernel(device, uuid.get(), "trace_kernel");

  // // Create a parent BO for kernel input data
  // auto bo = xrt::bo(device, num*sizeof(uint8_t), trace_kernel.group_id(0));
  // auto bo_mapped = bo.map<int*>();

  // //Populate the input and reference vectors.
  // int reference[DATA_SIZE];
  // for (int i = 0; i < DATA_SIZE; i++) {
  //   bo_mapped[i] = i;
  //   int val = 0;
  //   if(i%4==0)  val = i+2;
  //   if(i%4==1)  val = i+2;
  //   if(i%4==2)  val = i-2;
  //   if(i%4==3)  val = i-2;
  //   reference[i] = val;
  // }

    //Call the SetMetrics PS Kernel (TODO)
    // auto device = xrt::device(device_index);
    // auto uuid = device.load_xclbin(xclbin_fnm);

    // auto loopback = xrt::kernel(device, uuid.get(), "configure_aie_trace");

    //OutputConfiguration = getDataFromPS();
    //return OutputConfiguration->success;
    return true; //placeholder
  }

  
}