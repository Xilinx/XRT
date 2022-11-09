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

#include <boost/algorithm/string.hpp>

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "aie_profile_metadata.h"
#include "aie_profile_plugin.h"
// #include "xaiefal/xaiefal.hpp"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

    AieProfileMetadata::AieProfileMetadata(uint64_t deviceID, void* handle)
  : deviceID(deviceID)
  , handle(handle)
  {}

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

  // std::string AieProfileMetadata::getMetricSet(const int mod, const std::string& metricsStr, bool ignoreOldConfig)
  // {
  //   std::vector<std::string> vec;

  //   int XAIE_CORE_MOD  = 0;
  //   int XAIE_MEM_MOD  = 1;
  //   int XAIE_PL_MOD  = 2;

  //   boost::split(vec, metricsStr, boost::is_any_of(":"));
  //   for (int i=0; i < vec.size(); ++i) {
  //     boost::replace_all(vec.at(i), "{", "");
  //     boost::replace_all(vec.at(i), "}", "");
  //   }

  //   // Determine specification type based on vector size:
  //   //   * Size = 1: All tiles
  //   //     * aie_profile_core_metrics = <heat_map|stalls|execution>
  //   //     * aie_profile_memory_metrics = <dma_locks|conflicts>
  //   //   * Size = 2: Single tile
  //   //     * aie_profile_core_metrics = {<column>,<row>}:<heat_map|stalls|execution>
  //   //     * aie_profile_memory_metrics = {<column>,<row>}:<dma_locks|conflicts>
  //   //   * Size = 3: Range of tiles
  //   //     * aie_profile_core_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<heat_map|stalls|execution>
  //   //     * aie_profile_memory_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<dma_locks|conflicts>
  //   std::string metricSet  = vec.at( vec.size()-1 );
  //   std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
  //                          : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
  //                          : "interface_tile");

  //   // Ensure requested metric set is supported (if not, use default)
  //   if (((mod == XAIE_CORE_MOD) && (mCoreStartEvents.find(metricSet) == mCoreStartEvents.end()))
  //       || ((mod == XAIE_MEM_MOD) && (mMemoryStartEvents.find(metricSet) == mMemoryStartEvents.end()))
  //       || ((mod == XAIE_PL_MOD) && (mShimStartEvents.find(metricSet) == mShimStartEvents.end()))) {
  //     std::string defaultSet = (mod == XAIE_CORE_MOD) ? "heat_map" 
  //                            : ((mod == XAIE_MEM_MOD) ? "conflicts" 
  //                            : "input_bandwidths");
  //     std::stringstream msg;
  //     msg << "Unable to find " << moduleName << " metric set " << metricSet
  //         << ". Using default of " << defaultSet << ".";
  //     if (ignoreOldConfig) {
  //       msg << " As new AIE_profile_settings section is given, old style metric configurations, if any, are ignored.";
  //     }
  //     xrt_core::message::send(severity_level::warning, "XRT", msg.str());
  //     metricSet = defaultSet;
  //   }

  //   if (mod == XAIE_CORE_MOD)
  //     mCoreMetricSet = metricSet;
  //   else if (mod == XAIE_MEM_MOD)
  //     mMemoryMetricSet = metricSet;
  //   else
  //     mShimMetricSet = metricSet;
  //   return metricSet;
  // }

  std::vector<tile_type> 
  AieProfileMetadata::getTilesForProfiling(const int mod, 
                                           const std::string& metricsStr,
                                           void* handle)
  {
    int XAIE_CORE_MOD  = 0;
    int XAIE_MEM_MOD  = 1;
    // int XAIE_PL_MOD = 2; 

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