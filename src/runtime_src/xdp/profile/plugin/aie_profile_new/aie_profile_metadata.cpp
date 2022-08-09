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

namespace xdp {

    AieProfileMetadata::AieProfileMetadata(uint64_t deviceID, void* handle)
  : deviceID(deviceID)
  , handle(handle)
  {
    //
    // Pre-defined metric sets
    //
    // **** Core Module Counters ****
    mCoreStartEvents = {
      {"heat_map",              {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                                 XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stalls",                {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                                 XAIE_EVENT_LOCK_STALL_CORE,           XAIE_EVENT_CASCADE_STALL_CORE}},
      {"execution",             {XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_INSTR_LOAD_CORE,
                                 XAIE_EVENT_INSTR_STORE_CORE,          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"floating_point",        {XAIE_EVENT_FP_OVERFLOW_CORE,          XAIE_EVENT_FP_UNDERFLOW_CORE,
                                 XAIE_EVENT_FP_INVALID_CORE,           XAIE_EVENT_FP_DIV_BY_ZERO_CORE}},
      {"stream_put_get",        {XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
                                 XAIE_EVENT_INSTR_STREAM_GET_CORE,     XAIE_EVENT_INSTR_STREAM_PUT_CORE}},
      {"write_bandwidths",      {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_PUT_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_PUT_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"read_bandwidths",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_GET_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"aie_trace",             {XAIE_EVENT_PORT_RUNNING_1_CORE,       XAIE_EVENT_PORT_STALLED_1_CORE,
                                 XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"events",                {XAIE_EVENT_INSTR_EVENT_0_CORE,        XAIE_EVENT_INSTR_EVENT_1_CORE,
                                 XAIE_EVENT_USER_EVENT_0_CORE,         XAIE_EVENT_USER_EVENT_1_CORE}}
    };
    mCoreEndEvents = {
      {"heat_map",              {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                                 XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stalls",                {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                                 XAIE_EVENT_LOCK_STALL_CORE,           XAIE_EVENT_CASCADE_STALL_CORE}},
      {"execution",             {XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_INSTR_LOAD_CORE,
                                 XAIE_EVENT_INSTR_STORE_CORE,          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"floating_point",        {XAIE_EVENT_FP_OVERFLOW_CORE,          XAIE_EVENT_FP_UNDERFLOW_CORE,
                                 XAIE_EVENT_FP_INVALID_CORE,           XAIE_EVENT_FP_DIV_BY_ZERO_CORE}},
      {"stream_put_get",        {XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
                                 XAIE_EVENT_INSTR_STREAM_GET_CORE,     XAIE_EVENT_INSTR_STREAM_PUT_CORE}},
      {"write_bandwidths",      {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_PUT_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_PUT_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"read_bandwidths",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_GET_CORE,
                                 XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"aie_trace",             {XAIE_EVENT_PORT_RUNNING_1_CORE,       XAIE_EVENT_PORT_STALLED_1_CORE,
                                 XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"events",                {XAIE_EVENT_INSTR_EVENT_0_CORE,        XAIE_EVENT_INSTR_EVENT_1_CORE,
                                 XAIE_EVENT_USER_EVENT_0_CORE,         XAIE_EVENT_USER_EVENT_1_CORE}}
    };

    // **** Memory Module Counters ****
    mMemoryStartEvents = {
      {"conflicts",             {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {"dma_locks",             {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}},
      {"dma_stalls_s2mm",       {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"dma_stalls_mm2s",       {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"write_bandwidths",      {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}},
      {"read_bandwidths",       {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}}
    };
    mMemoryEndEvents = {
      {"conflicts",             {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {"dma_locks",             {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}}, 
      {"dma_stalls_s2mm",       {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"dma_stalls_mm2s",       {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM}},
      {"write_bandwidths",      {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}},
      {"read_bandwidths",       {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
                                 XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}}
    };

    // **** PL/Shim Counters ****
    mShimStartEvents = {
      {"input_bandwidths",      {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"output_bandwidths",     {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"packets",               {XAIE_EVENT_PORT_TLAST_0_PL,   XAIE_EVENT_PORT_TLAST_1_PL}}
    };
    mShimEndEvents = {
      {"input_bandwidths",      {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"output_bandwidths",     {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"packets",               {XAIE_EVENT_PORT_TLAST_0_PL,   XAIE_EVENT_PORT_TLAST_1_PL}}
    };

    // String event values for guidance and output
    mCoreEventStrings = {
      {"heat_map",              {"ACTIVE_CORE",               "GROUP_CORE_STALL_CORE",
                                 "INSTR_VECTOR_CORE",         "GROUP_CORE_PROGRAM_FLOW"}},
      {"stalls",                {"MEMORY_STALL_CORE",         "STREAM_STALL_CORE",
                                 "LOCK_STALL_CORE",           "CASCADE_STALL_CORE"}},
      {"execution",             {"INSTR_VECTOR_CORE",         "INSTR_LOAD_CORE",
                                 "INSTR_STORE_CORE",          "GROUP_CORE_PROGRAM_FLOW"}},
      {"floating_point",        {"FP_OVERFLOW_CORE",          "FP_UNDERFLOW_CORE",
                                 "FP_INVALID_CORE",           "FP_DIV_BY_ZERO_CORE"}},
      {"stream_put_get",        {"INSTR_CASCADE_GET_CORE",    "INSTR_CASCADE_PUT_CORE",
                                 "INSTR_STREAM_GET_CORE",     "INSTR_STREAM_PUT_CORE"}},
      {"write_bandwidths",      {"ACTIVE_CORE",               "INSTR_STREAM_PUT_CORE",
                                 "INSTR_CASCADE_PUT_CORE",    "EVENT_TRUE_CORE"}},
      {"read_bandwidths",       {"ACTIVE_CORE",               "INSTR_STREAM_GET_CORE",
                                 "INSTR_CASCADE_GET_CORE",    "EVENT_TRUE_CORE"}},
      {"aie_trace",             {"CORE_TRACE_RUNNING",        "CORE_TRACE_STALLED",
                                 "MEMORY_TRACE_RUNNING",      "MEMORY_TRACE_STALLED"}},
      {"events",                {"INSTR_EVENT_0_CORE",        "INSTR_EVENT_1_CORE",
                                 "USER_EVENT_0_CORE",         "USER_EVENT_1_CORE"}}
    };
    mMemoryEventStrings = {
      {"conflicts",             {"GROUP_MEMORY_CONFLICT_MEM", "GROUP_ERRORS_MEM"}},
      {"dma_locks",             {"GROUP_DMA_ACTIVITY_MEM",    "GROUP_LOCK_MEM"}},
      {"dma_stalls_s2mm",       {"DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM",
                                 "DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM"}},
      {"dma_stalls_mm2s",       {"DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM",
                                 "DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM"}},
      {"write_bandwidths",      {"DMA_MM2S_0_FINISHED_BD_MEM",
                                 "DMA_MM2S_1_FINISHED_BD_MEM"}},
      {"read_bandwidths",       {"DMA_S2MM_0_FINISHED_BD_MEM",
                                 "DMA_S2MM_1_FINISHED_BD_MEM"}}
    };
    mShimEventStrings = {
      {"input_bandwidths",      {"PORT_RUNNING_0_PL", "PORT_STALLED_0_PL"}},
      {"output_bandwidths",     {"PORT_RUNNING_0_PL", "PORT_STALLED_0_PL"}},
      {"packets",               {"PORT_TLAST_0_PL",   "PORT_TLAST_1_PL"}}
    };

  }

  void AieProfileMetadata::getPollingInterval()
  {
    // Get polling interval (in usec; minimum is 100)
    mPollingInterval = xrt_core::config::get_aie_profile_settings_interval_us();
    if (1000 == mPollingInterval) {
      // If set to default value, then check for old style config 
      mPollingInterval = xrt_core::config::get_aie_profile_interval_us();
      if (1000 != mPollingInterval) {
        xrt_core::message::send(severity_level::warning, "XRT", 
          "The xrt.ini flag \"aie_profile_interval_us\" is deprecated and will be removed in future release. Please use \"interval_us\" under \"AIE_profile_settings\" section.");
      }
    }
  }

  std::string AieProfileMetadata::getMetricSet(const XAie_ModuleType mod, const std::string& metricsStr, bool ignoreOldConfig)
  {
    std::vector<std::string> vec;

    boost::split(vec, metricsStr, boost::is_any_of(":"));
    for (int i=0; i < vec.size(); ++i) {
      boost::replace_all(vec.at(i), "{", "");
      boost::replace_all(vec.at(i), "}", "");
    }

    // Determine specification type based on vector size:
    //   * Size = 1: All tiles
    //     * aie_profile_core_metrics = <heat_map|stalls|execution>
    //     * aie_profile_memory_metrics = <dma_locks|conflicts>
    //   * Size = 2: Single tile
    //     * aie_profile_core_metrics = {<column>,<row>}:<heat_map|stalls|execution>
    //     * aie_profile_memory_metrics = {<column>,<row>}:<dma_locks|conflicts>
    //   * Size = 3: Range of tiles
    //     * aie_profile_core_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<heat_map|stalls|execution>
    //     * aie_profile_memory_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<dma_locks|conflicts>
    std::string metricSet  = vec.at( vec.size()-1 );
    std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
                           : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
                           : "interface_tile");
    
    // Ensure requested metric set is supported (if not, use default)
    if (((mod == XAIE_CORE_MOD) && (mCoreStartEvents.find(metricSet) == mCoreStartEvents.end()))
        || ((mod == XAIE_MEM_MOD) && (mMemoryStartEvents.find(metricSet) == mMemoryStartEvents.end()))
        || ((mod == XAIE_PL_MOD) && (mShimStartEvents.find(metricSet) == mShimStartEvents.end()))) {
      std::string defaultSet = (mod == XAIE_CORE_MOD) ? "heat_map" 
                             : ((mod == XAIE_MEM_MOD) ? "conflicts" 
                             : "input_bandwidths");
      std::stringstream msg;
      msg << "Unable to find " << moduleName << " metric set " << metricSet
          << ". Using default of " << defaultSet << ".";
      if (ignoreOldConfig) {
        msg << " As new AIE_profile_settings section is given, old style metric configurations, if any, are ignored.";
      }
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      metricSet = defaultSet;
    }

    if (mod == XAIE_CORE_MOD)
      mCoreMetricSet = metricSet;
    else if (mod == XAIE_MEM_MOD)
      mMemoryMetricSet = metricSet;
    else
      mShimMetricSet = metricSet;
    return metricSet;
  }

  std::vector<tile_type> 
  AieProfileMetadata::getTilesForProfiling(const XAie_ModuleType mod, 
                                           const std::string& metricsStr,
                                           void* handle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    std::vector<std::string> vec;
    boost::split(vec, metricsStr, boost::is_any_of(":"));

    // Compile list of tiles based on how its specified in setting
    std::vector<tile_type> tiles;
    std::vector<tile_type> tempTiles;

    if (vec.size() == 1) {
      //aie_profile_core_metrics = <heat_map|stalls|execution>
      /*
       * Core profiling uses all unique core tiles in aie control
       * Memory profiling uses all unique core + dma tiles in aie control
       * Shim profiling uses all tiles utilized by PLIOs
       */
      if ((mod == XAIE_CORE_MOD) || (mod == XAIE_MEM_MOD)) {
        // Capture all tiles across all graphs
        auto graphs = xrt_core::edge::aie::get_graphs(device.get());
        for (auto& graph : graphs) {
          tempTiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
              xrt_core::edge::aie::module_type::core);
          if (mod == XAIE_MEM_MOD) {
            auto dmaTiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
                xrt_core::edge::aie::module_type::dma);
            std::move(dmaTiles.begin(), dmaTiles.end(), back_inserter(tempTiles));
          }
        }
      }
      else { // XAIE_PL_MOD
        int plioCount = 0;
        auto plios = xrt_core::edge::aie::get_plios(device.get());
        for (auto& plio : plios) {
          auto isMaster = plio.second.slaveOrMaster;
          auto streamId = plio.second.streamId;

          // If looking for specific ID, make sure it matches
          if ((mChannelId >= 0) && (mChannelId != streamId))
            continue;

          // Make sure it's desired polarity
          // NOTE: input = slave (data flowing from PLIO)
          //       output = master (data flowing to PLIO)
          if ((isMaster && (metricsStr == "input_bandwidths"))
              || (isMaster && (metricsStr == "input_stalls_idle"))
              || (!isMaster && (metricsStr == "output_bandwidths"))
              || (!isMaster && (metricsStr == "output_stalls_idle")))
            continue;

          tempTiles.push_back(tile_type());
          auto& t = tempTiles.at(plioCount++);
          t.col = plio.second.shimColumn;
          t.row = 0;

          // Grab stream ID and slave/master (used in configStreamSwitchPorts() below)
          // TODO: find better way to store these values
          t.itr_mem_col = isMaster;
          t.itr_mem_row = streamId;
        }
          
        if (plioCount == 0) {
          std::string msg = "No tiles used channel ID " + std::to_string(mChannelId)
                            + ". Please specify a valid channel ID.";
          xrt_core::message::send(severity_level::warning, "XRT", msg);
        }
      } // XAIE_PL_MOD 

      // Sort and unique copy to remove repeated tiles
      std::sort(tempTiles.begin(), tempTiles.end(),
        [](tile_type t1, tile_type t2) {
              if (t1.row == t2.row)
                return t1.col > t2.col;
              else
                return t1.row > t2.row;
          }
        );
        std::unique_copy(tempTiles.begin(), tempTiles.end(), back_inserter(tiles),
          [](tile_type t1, tile_type t2) {
              return ((t1.col == t2.col) && (t1.row == t2.row));
          }
        );
   }
    else if (vec.size() == 2) {
      // aie_profile_core_metrics = {<column>,<row>}:<heat_map|stalls|execution>
      std::vector<std::string> tileVec;
      boost::split(tileVec, vec.at(0), boost::is_any_of(","));

      xrt_core::edge::aie::tile_type tile;
      tile.col = std::stoi(tileVec.at(0));
      tile.row = std::stoi(tileVec.at(1));
      tiles.push_back(tile);
    }
    else if (vec.size() == 3) {
      // aie_profile_core_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<heat_map|stalls|execution>
      std::vector<std::string> minTileVec;
      boost::split(minTileVec, vec.at(0), boost::is_any_of(","));
      uint32_t minCol = std::stoi(minTileVec.at(0));
      uint32_t minRow = std::stoi(minTileVec.at(1));

      std::vector<std::string> maxTileVec;
      boost::split(maxTileVec, vec.at(1), boost::is_any_of(","));
      uint32_t maxCol = std::stoi(maxTileVec.at(0));
      uint32_t maxRow = std::stoi(maxTileVec.at(1));

      for (uint32_t col = minCol; col <= maxCol; ++col) {
        for (uint32_t row = minRow; row <= maxRow; ++row) {
          xrt_core::edge::aie::tile_type tile;
          tile.col = col;
          tile.row = row;
          tiles.push_back(tile);
        }
      }
    }

    // Report tiles (debug only)
    {
      std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
                             : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
                             : "interface_tile");
      std::stringstream msg;
      msg << "Tiles used for " << moduleName << " profile counters: ";
      for (auto& tile : tiles) {
        msg << "(" << tile.col << "," << tile.row << "), ";
      }
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    return tiles;
  }

  std::vector<tile_type>
  AieProfileMetadata::getAllTilesForShimProfiling(void* handle, const std::string &metricsStr)
  {
    std::vector<tile_type> tiles;

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    int plioCount = 0;
    auto plios = xrt_core::edge::aie::get_plios(device.get());
    for (auto& plio : plios) {
      auto isMaster = plio.second.slaveOrMaster;
      auto streamId = plio.second.streamId;

      // If looking for specific ID, make sure it matches
      if ((mChannelId >= 0) && (mChannelId != streamId))
        continue;

      // Make sure it's desired polarity
      // NOTE: input = slave (data flowing from PLIO)
      //       output = master (data flowing to PLIO)
      if ((isMaster && (metricsStr == "input_bandwidths"))
          || (isMaster && (metricsStr == "input_stalls_idle"))
          || (!isMaster && (metricsStr == "output_bandwidths"))
          || (!isMaster && (metricsStr == "output_stalls_idle")))
        continue;

      tiles.push_back(tile_type());
      auto& t = tiles.at(plioCount++);
      t.col = plio.second.shimColumn;
      t.row = 0;

      // Grab stream ID and slave/master (used in configStreamSwitchPorts() below)
      // TODO: find better way to store these values
      t.itr_mem_col = isMaster;
      t.itr_mem_row = streamId;
    }
          
    if (plioCount == 0) {
      std::string msg = "No tiles used channel ID " + std::to_string(mChannelId)
                        + ". Please specify a valid channel ID.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }
    return tiles;
  }


}