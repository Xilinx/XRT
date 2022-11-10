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

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace_new/aie_trace_metadata.h"

#include "core/include/xrt/xrt_kernel.h"

#include "aie_trace_kernel_config.h"
#include "aie_trace.h"

constexpr uint32_t MAX_TILES = 400;
constexpr uint32_t ALIGNMENT_SIZE = 4096;

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  void AieTrace_x86Impl::updateDevice() {
    // Set metrics for counters and trace events 
    if (!setMetrics(metadata->getDeviceID(), metadata->getHandle())) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }
  }

  // No CMA checks on x86
  uint64_t AieTrace_x86Impl::checkTraceBufSize(uint64_t size) {
    return size;
  }

  bool AieTrace_x86Impl::setMetrics(uint64_t deviceId, void* handle) {
      
    constexpr uint64_t OUTPUT_SIZE = ALIGNMENT_SIZE * 38; //Calculated maximum output size for all 400 tiles
    constexpr uint64_t INPUT_SIZE = ALIGNMENT_SIZE; // input/output must be aligned to 4096
    constexpr uint64_t MSG_OUTPUT_SIZE = ALIGNMENT_SIZE * ((sizeof(xdp::built_in::MessageConfiguration)%ALIGNMENT_SIZE) > 0 ? (sizeof(xdp::built_in::MessageConfiguration)/ALIGNMENT_SIZE) + 1 : (sizeof(xdp::built_in::MessageConfiguration)%ALIGNMENT_SIZE));

    //Gather data to send to PS Kernel
    std::string counterScheme = xrt_core::config::get_aie_trace_settings_counter_scheme();
    std::string metricsStr = xrt_core::config::get_aie_trace_metrics();
    std::string metricSet = metadata->getMetricSet(metricsStr);
    uint8_t counterSchemeInt;
    uint8_t metricSetInt;

    auto tiles = metadata->getTilesForTracing();

    metadata->setTraceStartControl();
    uint32_t delayCycles = static_cast<uint32_t>(metadata->getDelay());
    bool userControl = metadata->getUseUserControl();//xrt_core::config::get_aie_trace_settings_start_type() == "kernel_event0";
    bool useDelay = metadata->getUseDelay();

    uint16_t rows[MAX_TILES];
    uint16_t cols[MAX_TILES];

    uint16_t numTiles = 0;

    for (auto& tile : tiles) {
      rows[numTiles] = tile.row;
      cols[numTiles] = tile.col;
      numTiles++;
    }

    if (counterScheme.compare("es1")) {
      counterSchemeInt = static_cast<uint8_t>(xdp::built_in::CounterScheme::ES1);
    } else {
      counterSchemeInt = static_cast<uint8_t>(xdp::built_in::CounterScheme::ES2);
    }

    if (metricSet.compare("functions") == 0) {
      metricSetInt = static_cast<uint8_t>(xdp::built_in::MetricSet::FUNCTIONS);
    } else if (metricSet.compare("functions_partial_stalls") == 0) {
      metricSetInt = static_cast<uint8_t>(xdp::built_in::MetricSet::PARTIAL_STALLS);
    } else if (metricSet.compare("functions_all_stalls") == 0) {  
      metricSetInt = static_cast<uint8_t>(xdp::built_in::MetricSet::ALL_STALLS);
    } else {
      metricSetInt = static_cast<uint8_t>(xdp::built_in::MetricSet::ALL);
    }

    //Build input struct
    std::size_t total_size = sizeof(xdp::built_in::InputConfiguration) + sizeof(uint16_t[(numTiles * 2) - 1]);
    xdp::built_in::InputConfiguration* input_params = (xdp::built_in::InputConfiguration*)malloc(total_size);
    input_params->delayCycles = delayCycles;
    input_params->counterScheme = counterSchemeInt;
    input_params->metricSet = metricSetInt; 
    input_params->numTiles = numTiles;
    input_params->useDelay = useDelay;
    input_params->userControl = userControl;

    //tile pairs are consecutive in the array
    int tileIdx = 0;
    for (uint16_t i = 0; i < numTiles * 2; i +=2) {
      input_params->tiles[i] = rows[tileIdx];
      input_params->tiles[i+1] = cols[tileIdx];
      tileIdx += 1;
    }

    total_size = sizeof(xdp::built_in::InputConfiguration) + sizeof(uint16_t[(numTiles * 2) - 1]);

    //Cast struct to uint8_t pointer and pass this data
    uint8_t* input = reinterpret_cast<uint8_t*>(input_params);

    //Attempt to schedule the kernel and parse the tile configuration output
    try {
      
      auto spdevice = xrt_core::get_userpf_device(handle);
      auto device = xrt::device(spdevice);
    
      auto uuid = device.get_xclbin_uuid();
      auto aie_trace_kernel = xrt::kernel(device, uuid.get(), "aie_trace_config");

      //input bo  
      auto bo0 = xrt::bo(device, INPUT_SIZE, 2);
      auto bo0_map = bo0.map<uint8_t*>();
      std::fill(bo0_map, bo0_map + INPUT_SIZE, 0);
   
      //output bo
      auto outTileConfigbo = xrt::bo(device, OUTPUT_SIZE, 2);
      auto outTileConfigbomapped = outTileConfigbo.map<uint8_t*>();
      memset(outTileConfigbomapped, 0, OUTPUT_SIZE);

      //message_output_bo
      auto messagebo = xrt::bo(device, MSG_OUTPUT_SIZE, 2);
      auto messagebomapped = messagebo.map<uint8_t*>();
      memset(messagebomapped, 0, MSG_OUTPUT_SIZE);

      std::memcpy(bo0_map, input, total_size);
      bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, INPUT_SIZE, 0);

      auto run = aie_trace_kernel(bo0, outTileConfigbo, messagebo);
      run.wait();

      outTileConfigbo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, OUTPUT_SIZE, 0);
      xdp::built_in::OutputConfiguration* cfg = reinterpret_cast<xdp::built_in::OutputConfiguration*>(outTileConfigbomapped);

      messagebo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, MSG_OUTPUT_SIZE, 0);
      uint8_t* msgStruct = reinterpret_cast<uint8_t*>(messagebomapped);
      parseMessages(msgStruct);

      // Update the config tiles
      for (uint32_t i = 0; i < cfg->numTiles; ++i) {
        auto cfgTile = std::make_unique<aie_cfg_tile>(cfg->tiles[i].column, cfg->tiles[i].row);
        cfgTile->trace_metric_set = metricSet;
    
        for (uint32_t corePC = 0; corePC < NUM_TRACE_PCS; ++corePC) {
          auto& cfgData = cfgTile->core_trace_config.pc[corePC];
          cfgData.start_event = cfg->tiles[i].core_trace_config.pc[corePC].start_event;
          cfgData.stop_event  = cfg->tiles[i].core_trace_config.pc[corePC].stop_event;
          cfgData.reset_event = cfg->tiles[i].core_trace_config.pc[corePC].reset_event;
          cfgData.event_value = cfg->tiles[i].core_trace_config.pc[corePC].event_value;
        } 
        //update mem pcs  
        for (uint32_t memPC = 0; memPC < NUM_MEM_TRACE_PCS; ++memPC) {
          auto& cfgData = cfgTile->memory_trace_config.pc[memPC];
          cfgData.start_event = cfg->tiles[i].memory_trace_config.pc[memPC].start_event;
          cfgData.stop_event  = cfg->tiles[i].memory_trace_config.pc[memPC].stop_event;
          cfgData.reset_event = cfg->tiles[i].memory_trace_config.pc[memPC].reset_event;
          cfgData.event_value = cfg->tiles[i].memory_trace_config.pc[memPC].event_value;
        } 
        
        for (uint32_t tracedEvent = 0; tracedEvent < NUM_TRACE_EVENTS; tracedEvent++) {
          cfgTile->core_trace_config.traced_events[tracedEvent] = cfg->tiles[i].core_trace_config.traced_events[tracedEvent];
        }

        cfgTile->core_trace_config.start_event = cfg->tiles[i].core_trace_config.start_event;
        cfgTile->core_trace_config.stop_event = cfg->tiles[i].core_trace_config.stop_event;
      
      
        for (uint32_t tracedEvent = 0; tracedEvent < NUM_TRACE_EVENTS; tracedEvent++) {
          cfgTile->memory_trace_config.traced_events[tracedEvent] = cfg->tiles[i].memory_trace_config.traced_events[tracedEvent];
        }

        for (uint32_t bcEvent = 0; bcEvent < NUM_BROADCAST_EVENTS; bcEvent++) {
          cfgTile->core_trace_config.internal_events_broadcast[bcEvent] = cfg->tiles[i].core_trace_config.internal_events_broadcast[bcEvent];
        }
        
        cfgTile->memory_trace_config.start_event = cfg->tiles[i].memory_trace_config.start_event;
        cfgTile->memory_trace_config.stop_event = cfg->tiles[i].memory_trace_config.stop_event;
        cfgTile->core_trace_config.broadcast_mask_east = cfg->tiles[i].core_trace_config.broadcast_mask_east;   
        cfgTile->core_trace_config.broadcast_mask_west = cfg->tiles[i].core_trace_config.broadcast_mask_west; 
        cfgTile->memory_trace_config.packet_type = cfg->tiles[i].memory_trace_config.packet_type;

        (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile); 
        //Send Success Message
        std::stringstream msg;
        msg << "Adding tile (" << cfg->tiles[i].column << "," << cfg->tiles[i].row << ") to static database";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());      
    
      }
    
      for (uint32_t event = 0; event < NUM_OUTPUT_TRACE_EVENTS; event ++) {
        if (cfg->numTileCoreTraceEvents[event] != 0)
          (db->getStaticInfo()).addAIECoreEventResources(deviceId, event, cfg->numTileCoreTraceEvents[event]);

        if (cfg->numTileMemoryTraceEvents[event] != 0)
          (db->getStaticInfo()).addAIEMemoryEventResources(deviceId, event, cfg->numTileMemoryTraceEvents[event]);
      } 

    } catch (...) {
      std::string msg = "The aie_trace_config PS kernel was not found.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return false;
    } 

    std::string msg = "The aie_trace_config PS kernel was successfully scheduled.";
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
    
    free(input_params);
    return true; //placeholder
  }


  void AieTrace_x86Impl::parseMessages(uint8_t* messageStruct) {
      xdp::built_in::MessageConfiguration* messages = reinterpret_cast<xdp::built_in::MessageConfiguration*>(messageStruct);
    for (uint32_t i = 0; i < messages->numMessages; i++) {
      auto packet = messages->packets[i];
      auto messageCode = static_cast<xdp::built_in::Messages>(packet.messageCode);
      
      std::stringstream msg;
      switch (messageCode) {
     
        case xdp::built_in::Messages::NO_CORE_MODULE_PCS:
          msg << "Available core module performance counters for aie trace : " << packet.params[0] << std::endl
              << "Required core module performance counters for aie trace : "  << packet.params[1];
          xrt_core::message::send(severity_level::info, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::NO_CORE_MODULE_TRACE_SLOTS:
          msg << "Available core module trace slots for aie trace : " << packet.params[0] << std::endl
              << "Required core module trace slots for aie trace : "  << packet.params[1];
          xrt_core::message::send(severity_level::info, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::NO_CORE_MODULE_BROADCAST_CHANNELS:
          msg << "Available core module broadcast channels for aie trace : " << packet.params[0] << std::endl
              << "Required core module broadcast channels for aie trace : "  << packet.params[1];
          xrt_core::message::send(severity_level::info, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::NO_MEM_MODULE_PCS:
          msg << "Available memory module performance counters for aie trace : " << packet.params[0] << std::endl
              << "Required memory module performance counters for aie trace : "  << packet.params[1];
          xrt_core::message::send(severity_level::info, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::NO_MEM_MODULE_TRACE_SLOTS:
          msg << "Available memory module trace slots for aie trace : " << packet.params[0] << std::endl
              << "Required memory module trace slots for aie trace : "  << packet.params[1];
          xrt_core::message::send(severity_level::info, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::NO_RESOURCES:
          xrt_core::message::send(severity_level::warning, "XRT", "Tile doesn't have enough free resources for trace. Aborting trace configuration.");
          break;
        case xdp::built_in::Messages::COUNTERS_NOT_RESERVED:
          msg << "Unable to reserve " << packet.params[0] << " core counters"
              << " and " << packet.params[1] << " memory counters"
              << " for AIE tile (" << packet.params[2] << "," << packet.params[3] << ") required for trace.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::CORE_MODULE_TRACE_NOT_RESERVED:
          msg << "Unable to reserve core module trace control for AIE tile (" 
              << packet.params[0] << "," << packet.params[1] << ").";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::CORE_TRACE_EVENTS_RESERVED:
          msg << "Reserved " << packet.params[0] << " core trace events for AIE tile (" << packet.params[1] << "," << packet.params[2] << ").";
          xrt_core::message::send(severity_level::debug, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::MEMORY_MODULE_TRACE_NOT_RESERVED:
          msg << "Unable to reserve memory module trace control for AIE tile (" 
              << packet.params[0] << "," << packet.params[1] << ").";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
          break;
        case xdp::built_in::Messages::MEMORY_TRACE_EVENTS_RESERVED: 
          msg << "Reserved " << packet.params[0] << " memory trace events for AIE tile (" << packet.params[1] << "," << packet.params[2] << ").";
          xrt_core::message::send(severity_level::debug, "XRT", msg.str());
          break; 
      }
    }
  }
}
