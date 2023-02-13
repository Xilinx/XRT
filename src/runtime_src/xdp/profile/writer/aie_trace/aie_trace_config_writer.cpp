/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include <vector>

#include "aie_trace_config_writer.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  AieTraceConfigWriter::AieTraceConfigWriter(const char* filename,
               uint64_t index) :
    VPWriter(filename), deviceIndex(index)
  {
  }

  AieTraceConfigWriter::~AieTraceConfigWriter()
  {    
  }

  bool AieTraceConfigWriter::write(bool openNewFile)
  {
    bpt::ptree pt;
    bpt::ptree EventTraceConfigs_C, EventTraceConfigs;

    EventTraceConfigs_C.put("datacorrelation", 1);
    EventTraceConfigs_C.put("date", getCurrentDateTime());
    EventTraceConfigs_C.put("timestamp", getMsecSinceEpoch());

    bpt::ptree TraceConfig;
    bpt::ptree AieTileTraceConfig;
    bpt::ptree MemTileTraceConfig;
    bpt::ptree ShimTileTraceConfig;

    auto tiles = (db->getStaticInfo()).getAIECfgTiles(deviceIndex);
    if (tiles != nullptr) {
      for (auto& tile: *tiles) {  
        if (tile->type == module_type::core) {

          bpt::ptree AieTileTraceConfig_C;
          bpt::ptree core_trace_config;
          bpt::ptree memory_trace_config;

          AieTileTraceConfig_C.put("column", tile->column);
          AieTileTraceConfig_C.put("row", tile->row);
          AieTileTraceConfig_C.put("event_trace_name", tile->trace_metric_set);

          /*
          * Core Trace
          */
          core_trace_config.put("packet_type", tile->core_trace_config.packet_type);
          core_trace_config.put("packet_id",   tile->core_trace_config.packet_id);
          core_trace_config.put("trace_mode",  tile->core_trace_config.trace_mode);
          core_trace_config.put("start_event", tile->core_trace_config.start_event);
          core_trace_config.put("stop_event", tile->core_trace_config.stop_event);

          {
            bpt::ptree traced_events;
            for (auto& e : tile->core_trace_config.traced_events) {
              bpt::ptree event;
              event.put("", e);
              traced_events.push_back(std::make_pair("", event));
            }
            core_trace_config.add_child("traced_events", traced_events);
          }

          {
            bpt::ptree group_event_config;
            auto& map = tile->core_trace_config.group_event_config;

            if (!map.empty()) {
              for (const auto& kv : map) {
                group_event_config.put(std::to_string(kv.first), kv.second);
              }
            } else {
              // Dummy Value
              group_event_config.put("123","0");
            }

            core_trace_config.add_child("group_event_config", group_event_config);
          }

          {
            bpt::ptree combo_event_config;
            bpt::ptree combo_input, combo_control;
            for (auto& e : tile->core_trace_config.combo_event_input) {
              bpt::ptree event;
              event.put("", e);
              combo_input.push_back(std::make_pair("", event));
            }
            for (auto& e : tile->core_trace_config.combo_event_control) {
              bpt::ptree event;
              event.put("", e);
              combo_control.push_back(std::make_pair("", event));
            }
            combo_event_config.add_child("combo_input", combo_input);
            combo_event_config.add_child("combo_control", combo_control);
            core_trace_config.add_child("combo_event_config", combo_event_config);
          }

          {
            bpt::ptree performance_counter_config;
            for (unsigned int i=0; i <tile->core_trace_config.pc.size(); i++) {
              bpt::ptree counter;
              auto& ctr = tile->core_trace_config.pc[i];
              counter.put("start_event",   ctr.start_event);
              counter.put("stop_event",    ctr.stop_event);
              counter.put("reset_event",   ctr.reset_event);
              counter.put("event_value",   ctr.event_value);
              counter.put("counter_value", ctr.counter_value);
              std::string id = "counter_" + std::to_string(i);
              performance_counter_config.add_child(id, counter);
            }
            core_trace_config.add_child("performance_counter_config", performance_counter_config);
          }

          {
            core_trace_config.put("PortTraceConfig", tile->core_trace_config.port_trace);
          }

          {
            bpt::ptree BroadcastTraceConfig;
            BroadcastTraceConfig.put("broadcast_mask_south", tile->core_trace_config.broadcast_mask_south);
            BroadcastTraceConfig.put("broadcast_mask_north", tile->core_trace_config.broadcast_mask_north);
            BroadcastTraceConfig.put("broadcast_mask_west",  tile->core_trace_config.broadcast_mask_west);
            BroadcastTraceConfig.put("broadcast_mask_east",  tile->core_trace_config.broadcast_mask_east);
            bpt::ptree internal_events_broadcast;
            for (auto& e : tile->core_trace_config.internal_events_broadcast) {
              bpt::ptree event;
              event.put("", e);
              internal_events_broadcast.push_back(std::make_pair("", event));
            }
            BroadcastTraceConfig.add_child("internal_events_broadcast",  internal_events_broadcast);
            core_trace_config.add_child("BroadcastTraceConfig", BroadcastTraceConfig);
          }

          /*
          * Memory Trace
          */

          memory_trace_config.put("packet_type", tile->memory_trace_config.packet_type);
          memory_trace_config.put("packet_id",   tile->memory_trace_config.packet_id);
          memory_trace_config.put("start_event", tile->memory_trace_config.start_event);
          memory_trace_config.put("stop_event", tile->memory_trace_config.stop_event);

          {
            bpt::ptree traced_events;
            for (auto& e : tile->memory_trace_config.traced_events) {
              bpt::ptree event;
              event.put("", e);
              traced_events.push_back(std::make_pair("", event));
            }
            memory_trace_config.add_child("traced_events", traced_events);
          }

          {
            bpt::ptree group_event_config;
            auto& map = tile->memory_trace_config.group_event_config;

            if (!map.empty()) {
              for (const auto& kv : map) {
                group_event_config.put(std::to_string(kv.first), kv.second);
              }
            } else {
              // Dummy Value
              group_event_config.put("123","0");
            }

            memory_trace_config.add_child("group_event_config", group_event_config);
          }

          {
            bpt::ptree combo_event_config;
            bpt::ptree combo_input, combo_control;
            for (auto& e : tile->memory_trace_config.combo_event_input) {
              bpt::ptree event;
              event.put("", e);
              combo_input.push_back(std::make_pair("", event));
            }
            for (auto& e : tile->memory_trace_config.combo_event_control) {
              bpt::ptree event;
              event.put("", e);
              combo_control.push_back(std::make_pair("", event));
            }
            combo_event_config.add_child("combo_input", combo_input);
            combo_event_config.add_child("combo_control", combo_control);
            memory_trace_config.add_child("combo_event_config", combo_event_config);
          }

          {
            bpt::ptree performance_counter_config;
            for (unsigned int i=0; i <tile->memory_trace_config.pc.size(); i++) {
              bpt::ptree counter;
              auto& ctr = tile->memory_trace_config.pc[i];
              counter.put("start_event",   ctr.start_event);
              counter.put("stop_event",    ctr.stop_event);
              counter.put("reset_event",   ctr.reset_event);
              counter.put("event_value",   ctr.event_value);
              counter.put("counter_value", ctr.counter_value);
              std::string id = "counter_" + std::to_string(i);
              performance_counter_config.add_child(id, counter);
            }
            memory_trace_config.add_child("performance_counter_config", performance_counter_config);
          }

          {
            bpt::ptree BroadcastTraceConfig;
            BroadcastTraceConfig.put("broadcast_mask_south", tile->memory_trace_config.broadcast_mask_south);
            BroadcastTraceConfig.put("broadcast_mask_north", tile->memory_trace_config.broadcast_mask_north);
            BroadcastTraceConfig.put("broadcast_mask_west",  tile->memory_trace_config.broadcast_mask_west);
            BroadcastTraceConfig.put("broadcast_mask_east",  tile->memory_trace_config.broadcast_mask_east);
            bpt::ptree internal_events_broadcast;
            for (auto& e : tile->memory_trace_config.internal_events_broadcast) {
              bpt::ptree event;
              event.put("", e);
              internal_events_broadcast.push_back(std::make_pair("", event));
            }
            BroadcastTraceConfig.add_child("internal_events_broadcast",  internal_events_broadcast);
            memory_trace_config.add_child("BroadcastTraceConfig", BroadcastTraceConfig);
          }

          /*
          * End Tile
          */

          AieTileTraceConfig_C.add_child("core_trace_config", core_trace_config);
          AieTileTraceConfig_C.add_child("memory_trace_config", memory_trace_config);
          AieTileTraceConfig.push_back(std::make_pair("", AieTileTraceConfig_C));
        }
        else if (tile->type == module_type::mem_tile) {
          bpt::ptree MemTileTraceConfig_C;
          MemTileTraceConfig_C.put("column", tile->column);
          MemTileTraceConfig_C.put("row", tile->row);
          MemTileTraceConfig_C.put("event_trace_name", tile->trace_metric_set);

          MemTileTraceConfig_C.put("packet_type", tile->mem_tile_trace_config.packet_type);
          MemTileTraceConfig_C.put("packet_id",   tile->mem_tile_trace_config.packet_id);
          MemTileTraceConfig_C.put("start_event", tile->mem_tile_trace_config.start_event);
          MemTileTraceConfig_C.put("stop_event", tile->mem_tile_trace_config.stop_event);

          {
            bpt::ptree traced_events;
            for (auto& e : tile->mem_tile_trace_config.traced_events) {
              bpt::ptree event;
              event.put("", e);
              traced_events.push_back(std::make_pair("", event));
            }
            MemTileTraceConfig_C.add_child("traced_events", traced_events);
          }

          {
            bpt::ptree port_trace_config;
            bpt::ptree port_trace_ids;
            bpt::ptree port_trace_is_master;

            for (uint32_t i=0; i < NUM_MEM_TILE_PORTS; ++i) {
              bpt::ptree port1;
              bpt::ptree port2;
              port1.put("", tile->mem_tile_trace_config.port_trace_ids[i]);
              port2.put("", tile->mem_tile_trace_config.port_trace_is_master[i]);
              port_trace_ids.push_back(std::make_pair("", port1));
              port_trace_is_master.push_back(std::make_pair("", port2));
            }

            port_trace_config.add_child("traced_port_ids", port_trace_ids);
            port_trace_config.add_child("master_str", port_trace_is_master);
            MemTileTraceConfig_C.add_child("PortTraceConfig", port_trace_config);
          }

          {
            bpt::ptree sel_trace_config;
            bpt::ptree s2mm_channels;
            bpt::ptree mm2s_channels;

            for (uint32_t i=0; i < NUM_MEM_TILE_CHAN_SEL; ++i) {
              bpt::ptree chan1;
              bpt::ptree chan2;
              chan1.put("", tile->mem_tile_trace_config.s2mm_channels[i]);
              chan2.put("", tile->mem_tile_trace_config.mm2s_channels[i]);
              s2mm_channels.push_back(std::make_pair("", chan1));
              mm2s_channels.push_back(std::make_pair("", chan2));
            }
            
            sel_trace_config.add_child("s2mm_channels", s2mm_channels);
            sel_trace_config.add_child("mm2s_channels", mm2s_channels);
            MemTileTraceConfig_C.add_child("SelTraceConfig", sel_trace_config);
          }

          MemTileTraceConfig.push_back(std::make_pair("", MemTileTraceConfig_C));
        }
      }
    }

    // Write tile trace configs
    // NOTE: TileTraceConfig and ShimTraceConfig are required
    //       MemTileTraceConfig is optional based on family
    if (AieTileTraceConfig.empty()) {
      bpt::ptree dummy;
      AieTileTraceConfig.push_back(std::make_pair("", dummy));
    }
    TraceConfig.add_child("TileTraceConfig", AieTileTraceConfig);
    if (!MemTileTraceConfig.empty()) {
      TraceConfig.add_child("MemTileTraceConfig", MemTileTraceConfig);
    }
    if (ShimTileTraceConfig.empty()) {
      bpt::ptree dummy;
      ShimTileTraceConfig.push_back(std::make_pair("", dummy));
    }
    TraceConfig.add_child("ShimTraceConfig", ShimTileTraceConfig);
    EventTraceConfigs_C.add_child("TraceConfig", TraceConfig);

    EventTraceConfigs.push_back(std::make_pair("", EventTraceConfigs_C));
    pt.add_child("EventTraceConfigs", EventTraceConfigs);
    //bpt::write_json(std::cout, pt);
    write_jsonEx(getcurrentFileName(), pt);
    return true;
  }

}
