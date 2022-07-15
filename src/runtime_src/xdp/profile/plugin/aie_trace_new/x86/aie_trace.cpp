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
#include "core/edge/user/shim.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"
#include "xdp/profile/writer/aie_trace/aie_trace_config_writer.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"
#include "xrt/xrt_kernel.h"

#include "aie_trace_kernel_config.h"
#include "aie_trace.h"

#define MAX_TILES 400
#define MAX_LENGTH 4096

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using module_type = xrt_core::edge::aie::module_type;

  void AieTrace_x86Impl::updateDevice(void* handle) {

    metadata = std::make_unique<AieTraceMetadata>();

    if (handle == nullptr)
      return;

    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);

    metadata->setDeviceId(db->addDevice(sysfspath));
    // uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

    metadata->deviceIdToHandle[metadata->getDeviceId()] = handle;
    // handle is not added to "deviceHandles" as this is user provided handle, not owned by XDP

    if (!(db->getStaticInfo()).isDeviceReady(metadata->getDeviceId())) {
      // first delete the offloader, logger
      // Delete the old offloader as data is already from it
      if(aieOffloaders.find(metadata->getDeviceId()) !=aieOffloaders.end()) {
        auto entry = aieOffloaders[metadata->getDeviceId()];

        auto aieOffloader = std::get<0>(entry);
        auto aieLogger    = std::get<1>(entry);

        delete aieOffloader;
        delete aieLogger;
        // don't delete DeviceIntf

        aieOffloaders.erase(metadata->getDeviceId());
      }

      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(metadata->getDeviceId(), handle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(handle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(metadata->getDeviceId(), std::string(info.mName));
        }
      }
    }  

    // Set metrics for counters and trace events 
    if (!setMetrics(metadata->getDeviceId(), handle)) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }

    if (!(db->getStaticInfo()).isGMIORead(metadata->getDeviceId())) {
      // Update the AIE specific portion of the device
      // When new xclbin is loaded, the xclbin specific datastructure is already recreated
      std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle) ;
      if (device != nullptr) {
        for (auto& gmio : metadata->get_trace_gmios(device.get())) {
          (db->getStaticInfo()).addTraceGMIO(metadata->getDeviceId(), gmio.id, gmio.shimColumn, gmio.channelNum, gmio.streamId, gmio.burstLength) ;
        }
      }
      (db->getStaticInfo()).setIsGMIORead(metadata->getDeviceId(), true);
    }

    metadata->setNumStreams((db->getStaticInfo()).getNumAIETraceStream(metadata->getDeviceId()));
    if (metadata->getNumStreams() == 0) {
      // no AIE Trace Stream to offload trace, so return
      std::string msg("Neither PLIO nor GMIO trace infrastucture is found in the given design. So, AIE event trace will not be available.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }

  }

  void AieTrace_x86Impl::flushDevice(void* handle){
    if (handle == nullptr)
      return;

    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);
    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

    if(aieOffloaders.find(deviceId) != aieOffloaders.end()) {
      (std::get<0>(aieOffloaders[deviceId]))->readTrace(true);
    }

  }

  void AieTrace_x86Impl::finishFlushDevice(void* handle){
    if (handle == nullptr)
      return;
    
    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);

    metadata->setDeviceId(db->addDevice(sysfspath)); // Get the unique device Id

    auto itr =  metadata->deviceIdToHandle.find(metadata->getDeviceId());
    if ((itr == metadata->deviceIdToHandle.end()) || (itr->second != handle))
      return;

    // Set metrics to flush the trace FIFOs
    // NOTE 1: The data mover uses a burst length of 128, so we need dummy packets
    //         to ensure all execution trace gets written to DDR.
    // NOTE 2: This flush mechanism is only valid for runtime event trace
    if (metadata->getRunTimeMetrics() && xrt_core::config::get_aie_trace_flush()) {
      // setFlushMetrics(metadata->getDeviceId(), handle);
      // std::cout << "I Finished The SetFlushMetrics()" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "I Finished Sleep" << std::endl;

    if(aieOffloaders.find(metadata->getDeviceId()) != aieOffloaders.end()) {
      auto offloader = std::get<0>(aieOffloaders[metadata->getDeviceId()]);
      auto logger    = std::get<1>(aieOffloaders[metadata->getDeviceId()]);

      if (offloader->continuousTrace()) {
        offloader->stopOffload() ;
        while(offloader->getOffloadStatus() != AIEOffloadThreadStatus::STOPPED) ;
      }

      offloader->readTrace(true);
      if (offloader->isTraceBufferFull())
        xrt_core::message::send(severity_level::warning, "XRT", AIE_TS2MM_WARN_MSG_BUF_FULL);
      offloader->endReadTrace();

      delete (offloader);
      delete (logger);

      aieOffloaders.erase(metadata->getDeviceId());
    }
    std::cout << "Finished Final Flush!" << std::endl;

  }

   bool AieTrace_x86Impl::isEdge(){
    return false;
  }

  bool AieTrace_x86Impl::setMetrics(uint64_t deviceId, void* handle) {
    // Create struct to pass to PS kernel
    std::string counterScheme = xrt_core::config::get_aie_trace_counter_scheme();
    std::string metricSet = metadata->getMetricSet(handle);
    uint8_t counterSchemeInt;
    uint8_t metricSetInt;

    auto tiles = metadata->getTilesForTracing(handle);
    uint32_t delayCycles = static_cast<uint32_t>(metadata->getTraceStartDelayCycles(handle));
    bool userControl = xrt_core::config::get_aie_trace_user_control();
    bool useDelay = metadata->mUseDelay;

    uint16_t rows[MAX_TILES];
    uint16_t cols[MAX_TILES];

    uint16_t numTiles = 0;

    for (auto& tile : tiles){
      rows[numTiles] = tile.row;
      cols[numTiles] = tile.col;
      numTiles++;
    }

    if (counterScheme.compare("es1")){
      counterSchemeInt = 0;
    }else {
      counterSchemeInt = 1;
    }

    if (metricSet.compare("functions") == 0){
      metricSetInt = 0;
    }else if (metricSet.compare("functions_partial_stalls") == 0){
      metricSetInt = 1;
    }else if (metricSet.compare("functions_all_stalls") == 0){
      metricSetInt = 2;
    }else { //all
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

    std::size_t total_size = sizeof(xdp::built_in::InputConfiguration) + sizeof(uint16_t[(numTiles * 2) - 1]);

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