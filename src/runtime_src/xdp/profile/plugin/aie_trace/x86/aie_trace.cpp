/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "aie_trace.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <memory>

#include "aie_trace_kernel_config.h"
#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

constexpr uint32_t MAX_TILES = 400;
constexpr uint64_t ALIGNMENT_SIZE = 4096;

namespace xdp {
using severity_level = xrt_core::message::severity_level;
using TraceInputConfiguration = xdp::built_in::TraceInputConfiguration;
using TraceOutputConfiguration = xdp::built_in::TraceOutputConfiguration;
using TraceTileType = xdp::built_in::TraceTileType;
using MessageConfiguration = xdp::built_in::MessageConfiguration;
using Messages = xdp::built_in::Messages;

constexpr uint64_t OUTPUT_SIZE =
    ALIGNMENT_SIZE * 38; // Calculated maximum output size for all 400 tiles
constexpr uint64_t INPUT_SIZE =
    ALIGNMENT_SIZE; // input/output must be aligned to 4096
constexpr uint64_t MSG_OUTPUT_SIZE =
    ALIGNMENT_SIZE * ((sizeof(MessageConfiguration) % ALIGNMENT_SIZE) > 0
                          ? (sizeof(MessageConfiguration) / ALIGNMENT_SIZE) + 1
                          : (sizeof(MessageConfiguration) % ALIGNMENT_SIZE));

AieTrace_x86Impl::AieTrace_x86Impl(VPDatabase *database,
                                   std::shared_ptr<AieTraceMetadata> metadata)
    : AieTraceImpl(database, metadata) {
  auto spdevice = xrt_core::get_userpf_device(metadata->getHandle());
  device = xrt::device(spdevice);

  auto uuid = device.get_xclbin_uuid();

  if (metadata->getHardwareGen() == 1)
    aie_trace_kernel = xrt::kernel(device, uuid.get(), "aie_trace_config");
  else
    aie_trace_kernel = xrt::kernel(device, uuid.get(), "aie2_trace_config");
}

void AieTrace_x86Impl::updateDevice() {
  // Make sure compiler trace option is available as runtime
  if (!metadata->getRuntimeMetrics()) {
    return;
  }

  // Set metrics for counters and trace events
  if (!setMetricsSettings(metadata->getDeviceID(), metadata->getHandle())) {
    std::string msg("Unable to configure AIE trace control and events. No "
                    "trace will be generated.");
    xrt_core::message::send(severity_level::warning, "XRT", msg);
    return;
  }
}

// No CMA checks on x86
uint64_t AieTrace_x86Impl::checkTraceBufSize(uint64_t size) { return size; }

module_type AieTrace_x86Impl::getTileType(uint16_t absRow) {
  if (absRow == 0)
    return module_type::shim;

  if (absRow < metadata->getRowOffset())
    return module_type::mem_tile;

  return module_type::core;
}

void AieTrace_x86Impl::pollTimers(uint64_t index, void *handle) {
  // TBD: poll AIE timers similar to Edge implementation
}

bool AieTrace_x86Impl::setMetricsSettings(uint64_t deviceId, void *handle) {
  // Gather data to send to PS Kernel
  if (!metadata->getIsValidMetrics()) {
    std::string msg("AIE trace metrics were not specified in xrt.ini. AIE "
                    "event trace will not be available.");
    xrt_core::message::send(severity_level::warning, "XRT", msg);
    return false;
  }

  std::string counterScheme = metadata->getCounterScheme();
  uint8_t counterSchemeInt;

  if (counterScheme.compare("es1")) {
    counterSchemeInt = static_cast<uint8_t>(xdp::built_in::CounterScheme::ES1);
  } else {
    counterSchemeInt = static_cast<uint8_t>(xdp::built_in::CounterScheme::ES2);
  }

  auto configMetrics = metadata->getConfigMetrics();
  int numTiles = configMetrics.size();
  // Build input struct
  std::size_t total_size =
      sizeof(TraceInputConfiguration) + sizeof(TraceTileType[numTiles - 1]);
  TraceInputConfiguration *input_params =
      (TraceInputConfiguration *)malloc(total_size);
  input_params->numTiles = numTiles;
  input_params->delayCycles = metadata->getDelay();
  input_params->iterationCount = metadata->getIterationCount();
  input_params->useUserControl = metadata->getUseUserControl();
  input_params->useDelay = metadata->getUseDelay();
  input_params->useGraphIterator = metadata->getUseGraphIterator();
  input_params->useOneDelayCounter = metadata->getUseOneDelayCounter();
  input_params->counterScheme = counterSchemeInt;
  input_params->hwGen = metadata->getHardwareGen();
  input_params->offset = metadata->getRowOffset();

  TraceTileType traceTiles[numTiles];

  // Copy ConfigMetrics to inputConfiguration Struct
  int tile_idx = 0;
  auto configChannel0 = metadata->getConfigChannel0();
  auto configChannel1 = metadata->getConfigChannel1();

  for (auto &tileMetric : configMetrics) {
    traceTiles[tile_idx].col = tileMetric.first.col;
    traceTiles[tile_idx].row = tileMetric.first.row;
    traceTiles[tile_idx].metricSet =
        metadata->getMetricSetIndex(tileMetric.second);

    // If the tile is a memtile, check if any channel specification is present
    if (configChannel0.count(tileMetric.first))
      traceTiles[tile_idx].channel0 = configChannel0[tileMetric.first];

    if (configChannel1.count(tileMetric.first))
      traceTiles[tile_idx].channel1 = configChannel1[tileMetric.first];

    input_params->tiles[tile_idx] = traceTiles[tile_idx];
    tile_idx++;
  }

  // Cast struct to uint8_t pointer and pass this data
  uint8_t *input = reinterpret_cast<uint8_t *>(input_params);

  // Attempt to schedule the kernel and parse the tile configuration output
  try {
    // input bo
    auto inbo = xrt::bo(device, INPUT_SIZE, 2);
    auto inbo_map = inbo.map<uint8_t *>();
    std::fill(inbo_map, inbo_map + INPUT_SIZE, 0);

    // output bo
    auto outbo = xrt::bo(device, OUTPUT_SIZE, 2);
    auto outbo_map = outbo.map<uint8_t *>();
    memset(outbo_map, 0, OUTPUT_SIZE);

    // message_output_bo
    auto messagebo = xrt::bo(device, MSG_OUTPUT_SIZE, 2);
    auto messagebo_map = messagebo.map<uint8_t *>();
    memset(messagebo_map, 0, MSG_OUTPUT_SIZE);

    std::memcpy(inbo_map, input, total_size);
    inbo.sync(XCL_BO_SYNC_BO_TO_DEVICE, INPUT_SIZE, 0);

    auto run = aie_trace_kernel(inbo, outbo, messagebo, 0 /*setup iteration*/);
    run.wait();

    outbo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, OUTPUT_SIZE, 0);
    TraceOutputConfiguration *cfg =
        reinterpret_cast<TraceOutputConfiguration *>(outbo_map);

    messagebo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, MSG_OUTPUT_SIZE, 0);
    uint8_t *msgStruct = reinterpret_cast<uint8_t *>(messagebo_map);
    parseMessages(msgStruct);

    // Update the config tiles
    for (int i = 0; i < numTiles; ++i) {
      module_type type = getTileType(cfg->tiles[i].row);
      auto cfgTile = std::make_unique<aie_cfg_tile>(cfg->tiles[i].column,
                                                    cfg->tiles[i].row, type);
      cfgTile->trace_metric_set =
          metadata->getMetricString(cfg->tiles[i].trace_metric_set);

      for (uint32_t corePC = 0; corePC < NUM_TRACE_PCS; ++corePC) {
        auto &cfgData = cfgTile->core_trace_config.pc[corePC];
        cfgData.start_event =
            cfg->tiles[i].core_trace_config.pc[corePC].start_event;
        cfgData.stop_event =
            cfg->tiles[i].core_trace_config.pc[corePC].stop_event;
        cfgData.reset_event =
            cfg->tiles[i].core_trace_config.pc[corePC].reset_event;
        cfgData.event_value =
            cfg->tiles[i].core_trace_config.pc[corePC].event_value;
      }

      // update mem pcs
      for (uint32_t memPC = 0; memPC < NUM_MEM_TRACE_PCS; ++memPC) {
        auto &cfgData = cfgTile->memory_trace_config.pc[memPC];
        cfgData.start_event =
            cfg->tiles[i].memory_trace_config.pc[memPC].start_event;
        cfgData.stop_event =
            cfg->tiles[i].memory_trace_config.pc[memPC].stop_event;
        cfgData.reset_event =
            cfg->tiles[i].memory_trace_config.pc[memPC].reset_event;
        cfgData.event_value =
            cfg->tiles[i].memory_trace_config.pc[memPC].event_value;
      }

      // Updated Traced Events for all Tiles
      for (uint32_t tracedEvent = 0; tracedEvent < NUM_TRACE_EVENTS;
           tracedEvent++) {
        cfgTile->core_trace_config.traced_events[tracedEvent] =
            cfg->tiles[i].core_trace_config.traced_events[tracedEvent];
        cfgTile->memory_trace_config.traced_events[tracedEvent] =
            cfg->tiles[i].memory_trace_config.traced_events[tracedEvent];
        cfgTile->memory_tile_trace_config.traced_events[tracedEvent] =
            cfg->tiles[i].memory_tile_trace_config.traced_events[tracedEvent];
      }

      // Update the broadcast events for the core module
      for (uint32_t bcEvent = 0; bcEvent < NUM_BROADCAST_EVENTS; bcEvent++) {
        cfgTile->core_trace_config.internal_events_broadcast[bcEvent] =
            cfg->tiles[i].core_trace_config.internal_events_broadcast[bcEvent];
      }

      // Update Start and Stop Events for Each Tile
      cfgTile->core_trace_config.start_event =
          cfg->tiles[i].core_trace_config.start_event;
      cfgTile->core_trace_config.stop_event =
          cfg->tiles[i].core_trace_config.stop_event;

      cfgTile->memory_trace_config.start_event =
          cfg->tiles[i].memory_trace_config.start_event;
      cfgTile->memory_trace_config.stop_event =
          cfg->tiles[i].memory_trace_config.stop_event;

      cfgTile->memory_tile_trace_config.start_event =
          cfg->tiles[i].memory_tile_trace_config.start_event;
      cfgTile->memory_tile_trace_config.stop_event =
          cfg->tiles[i].memory_tile_trace_config.stop_event;

      // Update broadcast Masks for core
      cfgTile->core_trace_config.broadcast_mask_east =
          cfg->tiles[i].core_trace_config.broadcast_mask_east;
      cfgTile->core_trace_config.broadcast_mask_west =
          cfg->tiles[i].core_trace_config.broadcast_mask_west;

      // Update Packet type for memory modules or memtiles
      cfgTile->memory_trace_config.packet_type =
          cfg->tiles[i].memory_trace_config.packet_type;
      cfgTile->memory_tile_trace_config.packet_type =
          cfg->tiles[i].memory_tile_trace_config.packet_type;

      // Add Mem-tile specific metrics
      for (uint32_t channel = 0; channel < NUM_CHANNEL_SELECTS; channel++) {
        cfgTile->memory_tile_trace_config.port_trace_ids[channel] =
            cfg->tiles[i].memory_tile_trace_config.port_trace_ids[channel];
        cfgTile->memory_tile_trace_config.port_trace_is_master[channel] =
            cfg->tiles[i]
                        .memory_tile_trace_config
                        .port_trace_is_master[channel] == 1
                ? true
                : false;

        cfgTile->memory_tile_trace_config.s2mm_channels[channel] =
            cfg->tiles[i].memory_tile_trace_config.s2mm_channels[channel];
        cfgTile->memory_tile_trace_config.mm2s_channels[channel] =
            cfg->tiles[i].memory_tile_trace_config.mm2s_channels[channel];
      }

      (db->getStaticInfo()).addAIECfgTile(deviceId, cfgTile);
      // Send Success Message
      std::stringstream msg;
      msg << "Adding tile (" << cfg->tiles[i].column << "," << cfg->tiles[i].row
          << ") to static database";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    // Add Event Resources for Each module
    for (uint32_t event = 0; event < NUM_OUTPUT_TRACE_EVENTS; event++) {
      if (cfg->numTileCoreTraceEvents[event] != 0)
        (db->getStaticInfo())
            .addAIECoreEventResources(deviceId, event,
                                      cfg->numTileCoreTraceEvents[event]);

      if (cfg->numTileMemoryTraceEvents[event] != 0)
        (db->getStaticInfo())
            .addAIEMemoryEventResources(deviceId, event,
                                        cfg->numTileMemoryTraceEvents[event]);

      if (cfg->numTileMemTileTraceEvents[event] != 0)
        (db->getStaticInfo())
            .addAIEMemTileEventResources(deviceId, event,
                                         cfg->numTileMemTileTraceEvents[event]);
    }
  } catch (...) {
    std::string msg = "The aie_trace_config PS kernel was not found.";
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            msg);
    return false;
  }

  std::string msg =
      "The aie_trace_config PS kernel was successfully scheduled.";
  xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);

  free(input_params);
  return true; // placeholder
}

void AieTrace_x86Impl::flushTraceModules() {
  try {
    // input bo
    auto inbo = xrt::bo(device, INPUT_SIZE, 2);
    auto inbo_map = inbo.map<uint8_t *>();
    memset(inbo_map, 0, INPUT_SIZE);

    // output bo
    auto outbo = xrt::bo(device, OUTPUT_SIZE, 2);
    auto outbo_map = outbo.map<uint8_t *>();
    memset(outbo_map, 0, OUTPUT_SIZE);

    // message_output_bo
    auto messagebo = xrt::bo(device, MSG_OUTPUT_SIZE, 2);
    auto messagebo_map = messagebo.map<uint8_t *>();
    memset(messagebo_map, 0, MSG_OUTPUT_SIZE);

    auto run = aie_trace_kernel(inbo, outbo, messagebo, 1 /*flush iteration*/);
    run.wait();
  } catch (...) {
    std::string msg = "The aie_trace_config flush failed.";
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            msg);
    return;
  }

  std::string msg = "The aie_trace_config flush was successfully scheduled.";
  xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
}

void AieTrace_x86Impl::parseMessages(uint8_t *messageStruct) {
  MessageConfiguration *messages =
      reinterpret_cast<MessageConfiguration *>(messageStruct);

  for (uint32_t i = 0; i < messages->numMessages; i++) {
    auto packet = messages->packets[i];
    auto messageCode = static_cast<Messages>(packet.messageCode);

    std::stringstream msg;

    switch (messageCode) {
    case Messages::NO_CORE_MODULE_PCS:
      msg << "Available core module performance counters for aie trace : "
          << packet.params[0] << std::endl
          << "Required core module performance counters for aie trace : "
          << packet.params[1];
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      break;

    case Messages::NO_CORE_MODULE_TRACE_SLOTS:
      msg << "Available core module trace slots for aie trace : "
          << packet.params[0] << std::endl
          << "Required core module trace slots for aie trace : "
          << packet.params[1];
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      break;

    case Messages::NO_CORE_MODULE_BROADCAST_CHANNELS:
      msg << "Available core module broadcast channels for aie trace : "
          << packet.params[0] << std::endl
          << "Required core module broadcast channels for aie trace : "
          << packet.params[1];
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      break;

    case Messages::NO_MEM_MODULE_PCS:
      msg << "Available memory module performance counters for aie trace : "
          << packet.params[0] << std::endl
          << "Required memory module performance counters for aie trace : "
          << packet.params[1];
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      break;

    case Messages::NO_MEM_MODULE_TRACE_SLOTS:
      msg << "Available memory module trace slots for aie trace : "
          << packet.params[0] << std::endl
          << "Required memory module trace slots for aie trace : "
          << packet.params[1];
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      break;

    case Messages::NO_RESOURCES:
      xrt_core::message::send(severity_level::warning, "XRT",
                              "Tile doesn't have enough free resources for "
                              "trace. Aborting trace configuration.");
      break;

    case Messages::COUNTERS_NOT_RESERVED:
      msg << "Unable to reserve " << packet.params[0] << " core counters"
          << " and " << packet.params[1] << " memory counters"
          << " for AIE tile (" << packet.params[2] << "," << packet.params[3]
          << ") required for trace.";
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      break;

    case Messages::CORE_MODULE_TRACE_NOT_RESERVED:
      msg << "Unable to reserve core module trace control for AIE tile ("
          << packet.params[0] << "," << packet.params[1] << ").";
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      break;

    case Messages::CORE_TRACE_EVENTS_RESERVED:
      msg << "Reserved " << packet.params[0]
          << " core trace events for AIE tile (" << packet.params[1] << ","
          << packet.params[2] << ").";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      break;

    case Messages::MEMORY_MODULE_TRACE_NOT_RESERVED:
      msg << "Unable to reserve memory module trace control for AIE tile ("
          << packet.params[0] << "," << packet.params[1] << ").";
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      break;

    case Messages::MEMORY_TRACE_EVENTS_RESERVED:
      msg << "Reserved " << packet.params[0]
          << " memory trace events for AIE tile (" << packet.params[1] << ","
          << packet.params[2] << ").";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      break;

    case Messages::ALL_TRACE_EVENTS_RESERVED:
      msg << "Reserved " << packet.params[0] << " core and " << packet.params[1]
          << " memory trace events for AIE tile (" << packet.params[2] << ","
          << packet.params[3] << "). Adding tile to static database.";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      break;

    case Messages::ENABLE_TRACE_FLUSH:
      xrt_core::message::send(severity_level::info, "XRT",
                              "Enabling trace flush");
      break;
    }
  }
}

// Release resources
void AieTrace_x86Impl::freeResources() {
  // TODO: release resources
}
} // namespace xdp
