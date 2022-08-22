/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/edge/user/shim.h"
#include "core/include/experimental/xrt-next.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/aie_profile/aie_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_profile/aie_writer.h"

constexpr unsigned int NUM_CORE_COUNTERS   = 4;
constexpr unsigned int NUM_MEMORY_COUNTERS = 2;
constexpr unsigned int NUM_SHIM_COUNTERS  =  2;
constexpr unsigned int BASE_MEMORY_COUNTER = 128;
constexpr unsigned int BASE_SHIM_COUNTER =   256;

constexpr uint32_t GROUP_DMA_MASK                   = 0x0000f000;
constexpr uint32_t GROUP_LOCK_MASK                  = 0x55555555;
constexpr uint32_t GROUP_CONFLICT_MASK              = 0x000000ff;
constexpr uint32_t GROUP_ERROR_MASK                 = 0x00003fff;
constexpr uint32_t GROUP_STREAM_SWITCH_IDLE_MASK    = 0x11111111;
constexpr uint32_t GROUP_STREAM_SWITCH_RUNNING_MASK = 0x22222222;
constexpr uint32_t GROUP_STREAM_SWITCH_STALLED_MASK = 0x44444444;
constexpr uint32_t GROUP_STREAM_SWITCH_TLAST_MASK   = 0x88888888;
constexpr uint32_t GROUP_CORE_PROGRAM_FLOW_MASK     = 0x00001FE0;
constexpr uint32_t GROUP_CORE_STALL_MASK            = 0x0000000F;

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->getAieArray() ;
    if (!aieArray)
      return nullptr ;
    return aieArray->getDevInst() ;
  }

  static void* allocateAieDevice(void* devHandle)
  {
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle)) ;
    if (!aieDevInst)
      return nullptr ;
    return new xaiefal::XAieDev(aieDevInst, false) ;
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    xaiefal::XAieDev* object = static_cast<xaiefal::XAieDev*>(aieDevice) ;
    if (object != nullptr)
      delete object ;
  }

} // end anonymous namespace

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  bool AIEProfilingPlugin::live = false;

  AIEProfilingPlugin::AIEProfilingPlugin() 
      : XDPPlugin()
  {
    AIEProfilingPlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_profile);
    db->getStaticInfo().setAieApplication();
    getPollingInterval();

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

  AIEProfilingPlugin::~AIEProfilingPlugin()
  {
    // Stop the polling thread
    endPoll();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }

      db->unregisterPlugin(this);
    }
    AIEProfilingPlugin::live = false;
  }

  bool AIEProfilingPlugin::alive()
  {
    return AIEProfilingPlugin::live;
  }

  void AIEProfilingPlugin::getPollingInterval()
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

  void AIEProfilingPlugin::printTileModStats(xaiefal::XAieDev* aieDevice, 
      const tile_type& tile, XAie_ModuleType mod)
  {
    auto col = tile.col;
    auto row = tile.row + 1;
    auto loc = XAie_TileLoc(col, row);
    std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
                           : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
                           : "interface_tile");
    const std::string groups[3] = {
      XAIEDEV_DEFAULT_GROUP_GENERIC,
      XAIEDEV_DEFAULT_GROUP_STATIC,
      XAIEDEV_DEFAULT_GROUP_AVAIL
    };

    std::stringstream msg;
    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : " << moduleName << std::endl;
    for (auto&g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);
      auto ts = stats.getNumRsc(loc, mod, xaiefal::XAIE_TRACE_EVENTS_RSC);
      auto bc = stats.getNumRsc(loc, mod, XAIE_BCAST_CHANNEL_RSC);
      msg << "Resource Group : " << std::left <<  std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " "
          << std::endl;
    }

    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  uint32_t AIEProfilingPlugin::getNumFreeCtr(xaiefal::XAieDev* aieDevice,
                                             const std::vector<tile_type>& tiles,
                                             const XAie_ModuleType mod,
                                             const std::string& metricSet)
  {
    uint32_t numFreeCtr = 0;
    uint32_t tileId = 0;
    std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
                           : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
                           : "interface_tile");
    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);

    // Calculate number of free counters based on minimum available across tiles
    for (unsigned int i=0; i < tiles.size(); i++) {
      auto row = (mod == XAIE_PL_MOD) ? tiles[i].row : tiles[i].row + 1;
      auto loc = XAie_TileLoc(tiles[i].col, row);
      auto avail = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);
      if (i == 0) {
        numFreeCtr = avail;
      } else {
        if (avail < numFreeCtr) {
          numFreeCtr = avail;
          tileId = i;
        }
      }
    }

    auto requestedEvents = (mod == XAIE_CORE_MOD) ? mCoreStartEvents[metricSet]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryStartEvents[metricSet] 
                         : mShimStartEvents[metricSet]);
    auto eventStrings    = (mod == XAIE_CORE_MOD) ? mCoreEventStrings[metricSet]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryEventStrings[metricSet] 
                         : mShimEventStrings[metricSet]);

    auto numTotalEvents = requestedEvents.size();
    if (numFreeCtr < numTotalEvents) {
      std::stringstream msg;
      msg << "Only " << numFreeCtr << " out of " << numTotalEvents
          << " metrics were available for "
          << moduleName << " profiling due to resource constraints. "
          << "AIE profiling uses performance counters which could be already used by AIE trace, ECC, etc."
          << std::endl;

      msg << "Available metrics : ";
      for (unsigned int i=0; i < numFreeCtr; i++) {
        msg << eventStrings[i] << " ";
      }
      msg << std::endl;

      msg << "Unavailable metrics : ";
      for (unsigned int i=numFreeCtr; i < numTotalEvents; i++) {
        msg << eventStrings[i] << " ";
      }

      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      
      if (tiles.size() > 0)
        printTileModStats(aieDevice, tiles[tileId], mod);
    }

    return numFreeCtr;
  }

  std::string AIEProfilingPlugin::getMetricSet(const XAie_ModuleType mod, const std::string& metricsStr, bool ignoreOldConfig)
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
  AIEProfilingPlugin::getTilesForProfiling(const XAie_ModuleType mod, 
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
              || (!isMaster && (metricsStr == "output_bandwidths")))
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

  void AIEProfilingPlugin::configGroupEvents(XAie_DevInst* aieDevInst,
                                             const XAie_LocType loc,
                                             const XAie_ModuleType mod,
                                             const XAie_Events event,
                                             const std::string metricSet)
  {
    // Set masks for group events
    // NOTE: Group error enable register is blocked, so ignoring
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_DMA_MASK);
    else if (event == XAIE_EVENT_GROUP_LOCK_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_LOCK_MASK);
    else if (event == XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CONFLICT_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_PROGRAM_FLOW_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_STALL_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_STALL_MASK);
  }

  // Configure stream switch ports for monitoring purposes
  void AIEProfilingPlugin::configStreamSwitchPorts(XAie_DevInst* aieDevInst,
                                                   const tile_type& tile,
                                                   xaiefal::XAieTile& xaieTile,
                                                   const XAie_LocType loc,
                                                   const XAie_Events event,
                                                   const std::string metricSet)
  {
    // Currently only used to monitor trace and PL stream
    if ((metricSet != "aie_trace") && (metricSet != "input_bandwidths")
        && (metricSet != "output_bandwidths") && (metricSet != "packets"))
      return;

    if (metricSet == "aie_trace") {
      auto switchPortRsc = xaieTile.sswitchPort();
      auto ret = switchPortRsc->reserve();
      if (ret != AieRC::XAIE_OK)
        return;

      uint32_t rscId = 0;
      XAie_LocType tmpLoc;
      XAie_ModuleType tmpMod;
      switchPortRsc->getRscId(tmpLoc, tmpMod, rscId);
      uint8_t traceSelect = (event == XAIE_EVENT_PORT_RUNNING_0_CORE) ? 0 : 1;
      
      // Define stream switch port to monitor core or memory trace
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, XAIE_STRMSW_SLAVE, TRACE, traceSelect);
      return;
    }

    // Rest is support for PL/shim tiles
    auto switchPortRsc = xaieTile.sswitchPort();
    auto ret = switchPortRsc->reserve();
    if (ret != AieRC::XAIE_OK)
      return;

    uint32_t rscId = 0;
    XAie_LocType tmpLoc;
    XAie_ModuleType tmpMod;
    switchPortRsc->getRscId(tmpLoc, tmpMod, rscId);

    // Grab slave/master and stream ID
    // NOTE: stored in getTilesForProfiling() above
    auto slaveOrMaster = (tile.itr_mem_col == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
    auto streamPortId  = static_cast<uint8_t>(tile.itr_mem_row);

    // Define stream switch port to monitor PLIO 
    XAie_EventSelectStrmPort(aieDevInst, loc, rscId, slaveOrMaster, SOUTH, streamPortId);
  }

  // Get reportable payload specific for this tile and/or counter
  uint32_t AIEProfilingPlugin::getCounterPayload(XAie_DevInst* aieDevInst, 
      const tile_type& tile, uint16_t column, uint16_t row, uint16_t startEvent)
  {
    // First, catch stream ID for PLIO metrics
    // NOTE: value = ((master or slave) << 8) & (stream ID)
    if ((startEvent == XAIE_EVENT_PORT_RUNNING_0_PL)
        || (startEvent == XAIE_EVENT_PORT_TLAST_0_PL)
        || (startEvent == XAIE_EVENT_PORT_IDLE_0_PL)
        || (startEvent == XAIE_EVENT_PORT_STALLED_0_PL))
      return ((tile.itr_mem_col << 8) | tile.itr_mem_row);

    // Second, send DMA BD sizes
    if ((startEvent != XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM))
      return 0;

    uint32_t payloadValue = 0;

    constexpr int NUM_BDS = 8;
    constexpr uint32_t BYTES_PER_WORD = 4;
    constexpr uint32_t ACTUAL_OFFSET = 1;
    uint64_t offsets[NUM_BDS] = {XAIEGBL_MEM_DMABD0CTRL,            XAIEGBL_MEM_DMABD1CTRL,
                                 XAIEGBL_MEM_DMABD2CTRL,            XAIEGBL_MEM_DMABD3CTRL,
                                 XAIEGBL_MEM_DMABD4CTRL,            XAIEGBL_MEM_DMABD5CTRL,
                                 XAIEGBL_MEM_DMABD6CTRL,            XAIEGBL_MEM_DMABD7CTRL};
    uint32_t lsbs[NUM_BDS]    = {XAIEGBL_MEM_DMABD0CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD1CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD2CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD3CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD4CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD5CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD6CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD7CTRL_LEN_LSB};
    uint32_t masks[NUM_BDS]   = {XAIEGBL_MEM_DMABD0CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD1CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD2CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD3CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD4CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD5CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD6CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD7CTRL_LEN_MASK};
    uint32_t valids[NUM_BDS]  = {XAIEGBL_MEM_DMABD0CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD1CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD2CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD3CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD4CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD5CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD6CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD7CTRL_VALBD_MASK};

    auto tileOffset = _XAie_GetTileAddr(aieDevInst, row + 1, column);
    for (int bd = 0; bd < NUM_BDS; ++bd) {
      uint32_t regValue = 0;
      XAie_Read32(aieDevInst, tileOffset + offsets[bd], &regValue);
      
      if (regValue & valids[bd]) {
        uint32_t bdBytes = BYTES_PER_WORD * (((regValue >> lsbs[bd]) & masks[bd]) + ACTUAL_OFFSET);
        payloadValue = std::max(bdBytes, payloadValue);
      }
    }

    return payloadValue;
  }

  // Set metrics for all specified AIE counters on this device
  bool AIEProfilingPlugin::setMetrics(uint64_t deviceId, void* handle)
  {
    int counterId = 0;
    bool runtimeCounters = false;
    constexpr int NUM_MODULES = 3;

    // Get AIE clock frequency
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto clockFreqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());

    auto interfaceMetricStr = xrt_core::config::get_aie_profile_interface_metrics();
    std::vector<std::string> interfaceVec;
    boost::split(interfaceVec, interfaceMetricStr, boost::is_any_of(":"));
    auto interfaceMetric = interfaceVec.at(0);
    try {mChannelId = std::stoi(interfaceVec.at(1));} catch (...) {mChannelId = -1;}

    int numCounters[NUM_MODULES] =
        {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS};
    XAie_ModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};
    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};
    std::string metricSettings[NUM_MODULES] = 
        {xrt_core::config::get_aie_profile_core_metrics(),
         xrt_core::config::get_aie_profile_memory_metrics(),
         interfaceMetric};

    // Configure core, memory, and shim counters
    for (int module=0; module < NUM_MODULES; ++module) {
      std::string metricsStr = metricSettings[module];
      if (metricsStr.empty()){
        std::string metricMsg = "No metric set specified for " + moduleNames[module]
                              + ". Please specify tile_based_" + moduleNames[module] 
                              + "_metrics under \"AIE_profile_settings\" section in your xrt.ini.";
        xrt_core::message::send(severity_level::warning, "XRT", metricMsg);
        continue;
      } else {
        std::string oldModName[NUM_MODULES] = {"core", "memory", "interface"};
        std::string depMsg  = "The xrt.ini flag \"aie_profile_" + oldModName[module] + "_metrics\" is deprecated "
                              + " and will be removed in future release. Please use"
                              + " tile_based_" + moduleNames[module] + "_metrics"
                              + " under \"AIE_profile_settings\" section.";
        xrt_core::message::send(severity_level::warning, "XRT", depMsg);
      }
      int NUM_COUNTERS       = numCounters[module];
      XAie_ModuleType mod    = falModuleTypes[module];
      std::string moduleName = moduleNames[module];
      auto metricSet         = getMetricSet(mod, metricsStr);
      auto tiles             = getTilesForProfiling(mod, metricsStr, handle);

      // Ask Resource manager for resource availability
      auto numFreeCounters   = getNumFreeCtr(aieDevice, tiles, mod, metricSet);
      if (numFreeCounters == 0)
        continue;

      // Get vector of pre-defined metrics for this set
      uint8_t resetEvent = 0;
      auto startEvents = (mod == XAIE_CORE_MOD) ? mCoreStartEvents[metricSet]
                       : ((mod == XAIE_MEM_MOD) ? mMemoryStartEvents[metricSet] 
                       : mShimStartEvents[metricSet]);
      auto endEvents   = (mod == XAIE_CORE_MOD) ? mCoreEndEvents[metricSet]
                       : ((mod == XAIE_MEM_MOD) ? mMemoryEndEvents[metricSet] 
                       : mShimEndEvents[metricSet]);

      int numTileCounters[NUM_COUNTERS+1] = {0};
      
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tile : tiles) {
        int numCounters = 0;
        auto col = tile.col;
        auto row = tile.row;
        
        // NOTE: resource manager requires absolute row number
        auto loc        = (mod == XAIE_PL_MOD) ? XAie_TileLoc(col, 0) 
                        : XAie_TileLoc(col, row + 1);
        auto& xaieTile  = (mod == XAIE_PL_MOD) ? aieDevice->tile(col, 0) 
                        : aieDevice->tile(col, row + 1);
        auto xaieModule = (mod == XAIE_CORE_MOD) ? xaieTile.core()
                        : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
                        : xaieTile.pl());
        
        for (int i=0; i < numFreeCounters; ++i) {
          auto startEvent = startEvents.at(i);
          auto endEvent   = endEvents.at(i);

          // Request counter from resource manager
          auto perfCounter = xaieModule.perfCounter();
          auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
          if (ret != XAIE_OK) break;
          ret = perfCounter->reserve();
          if (ret != XAIE_OK) break;
          
          configGroupEvents(aieDevInst, loc, mod, startEvent, metricSet);
          configStreamSwitchPorts(aieDevInst, tile, xaieTile, loc, startEvent, metricSet);
          
          // Start the counters after group events have been configured
          ret = perfCounter->start();
          if (ret != XAIE_OK) break;
          mPerfCounters.push_back(perfCounter);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = (mod == XAIE_CORE_MOD) ? tmpStart
                                 : ((mod == XAIE_MEM_MOD) ? (tmpStart + BASE_MEMORY_COUNTER)
                                 : (tmpStart + BASE_SHIM_COUNTER));
          uint16_t phyEndEvent   = (mod == XAIE_CORE_MOD) ? tmpEnd
                                 : ((mod == XAIE_MEM_MOD) ? (tmpEnd + BASE_MEMORY_COUNTER)
                                 : (tmpEnd + BASE_SHIM_COUNTER));

          auto payload = getCounterPayload(aieDevInst, tile, col, row, startEvent);

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
              phyStartEvent, phyEndEvent, resetEvent, payload, clockFreqMhz, 
              moduleName, counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," << row << ").";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      }

      // Report counters reserved per tile
      {
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << moduleName << " - ";
        for (int n=0; n <= NUM_COUNTERS; ++n) {
          if (numTileCounters[n] == 0) continue;
          msg << n << ": " << numTileCounters[n] << " tiles";
          if (n != NUM_COUNTERS) msg << ", ";

          (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
        }
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }

      runtimeCounters = true;
    } // for module

    return runtimeCounters;
  }
  
  void AIEProfilingPlugin::pollAIECounters(uint32_t index, void* handle)
  {
    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;

    auto& should_continue = it->second;
    while (should_continue) {
      // Wait until xclbin has been loaded and device has been updated in database
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;
      XAie_DevInst* aieDevInst =
        static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
      if (!aieDevInst)
        continue;

      uint32_t prevColumn = 0;
      uint32_t prevRow = 0;
      uint64_t timerValue = 0;

      // Iterate over all AIE Counters & Timers
      auto numCounters = db->getStaticInfo().getNumAIECounter(index);
      for (uint64_t c=0; c < numCounters; c++) {
        auto aie = db->getStaticInfo().getAIECounter(index, c);
        if (!aie)
          continue;

        std::vector<uint64_t> values;
        values.push_back(aie->column);
        values.push_back(aie->row);
        values.push_back(aie->startEvent);
        values.push_back(aie->endEvent);
        values.push_back(aie->resetEvent);

        // Read counter value from device
        uint32_t counterValue;
        if (mPerfCounters.empty()) {
          // Compiler-defined counters
          XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
          XAie_PerfCounterGet(aieDevInst, tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
        }
        else {
          // Runtime-defined counters
          auto perfCounter = mPerfCounters.at(c);
          perfCounter->readResult(counterValue);
        }
        values.push_back(counterValue);

        // Read tile timer (once per tile to minimize overhead)
        if ((aie->column != prevColumn) || (aie->row != prevRow)) {
          prevColumn = aie->column;
          prevRow = aie->row;
          XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row + 1);
          XAie_ReadTimer(aieDevInst, tileLocation, XAIE_CORE_MOD, &timerValue);
        }
        values.push_back(timerValue);
        values.push_back(aie->payload);

        // Get timestamp in milliseconds
        double timestamp = xrt_core::time_ns() / 1.0e6;
        db->getDynamicInfo().addAIESample(index, timestamp, values);
      }

      std::this_thread::sleep_for(std::chrono::microseconds(mPollingInterval));     
    }
  }

  bool AIEProfilingPlugin::checkAieDevice(uint64_t deviceId, void* handle)
  {
    aieDevInst = static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    aieDevice  = static_cast<xaiefal::XAieDev*>(db->getStaticInfo().getAieDevice(allocateAieDevice, deallocateAieDevice, handle)) ;
    if (!aieDevInst || !aieDevice) {
      xrt_core::message::send(severity_level::warning, "XRT", 
          "Unable to get AIE device. There will be no AIE profiling.");
      return false;
    }
    return true;
  }

  void AIEProfilingPlugin::updateAIEDevice(void* handle)
  {
    // Don't update if no profiling is requested
    if (!xrt_core::config::get_aie_profile())
      return;

    std::array<char, sysfs_max_path_length> pathBuf = {0};
    xclGetDebugIPlayoutPath(handle, pathBuf.data(), (sysfs_max_path_length-1) ) ;
    std::string sysfspath(pathBuf.data());
    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

    if (!(db->getStaticInfo()).isDeviceReady(deviceId)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceId, handle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(handle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
        }
      }
    }

    // Ensure we only read/configure once per xclbin
    if (!(db->getStaticInfo()).isAIECounterRead(deviceId)) {
      // Update the AIE specific portion of the device
      // When new xclbin is loaded, the xclbin specific datastructure is already recreated

      // 1. Runtime-defined counters
      // NOTE: these take precedence

      if(!checkAieDevice(deviceId, handle))
        return;

      bool runtimeCounters = setMetricsSettings(deviceId, handle);
      if(!runtimeCounters) 
        runtimeCounters = setMetrics(deviceId, handle);

      // 2. Compiler-defined counters
      if (!runtimeCounters) {
        std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
        auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

        if (counters.empty()) {
          xrt_core::message::send(severity_level::warning, "XRT", 
            "AIE Profile Counters were not found for this design. Please specify tile_based_[aie|aie_memory|interface_tile]_metrics under \"AIE_profile_settings\" section in your xrt.ini.");
          (db->getStaticInfo()).setIsAIECounterRead(deviceId,true);
          return;
        }
        else {
          XAie_DevInst* aieDevInst =
            static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));

          for (auto& counter : counters) {
            tile_type tile;
            auto payload = getCounterPayload(aieDevInst, tile, counter.column, counter.row, 
                                             counter.startEvent);

            (db->getStaticInfo()).addAIECounter(deviceId, counter.id, counter.column,
                counter.row + 1, counter.counterNumber, counter.startEvent, counter.endEvent,
                counter.resetEvent, payload, counter.clockFreqMhz, counter.module, counter.name);
          }
        }
      }

      (db->getStaticInfo()).setIsAIECounterRead(deviceId, true);
    }

    // Open the writer for this device
    struct xclDeviceInfo2 info;
    xclGetDeviceInfo2(handle, &info);
    std::string deviceName = std::string(info.mName);
    // Create and register writer and file
    std::string core_str = (mCoreMetricSet.empty())   ? "" : "_" + mCoreMetricSet;
    std::string mem_str  = (mMemoryMetricSet.empty()) ? "" : "_" + mMemoryMetricSet;
    std::string shim_str = (mShimMetricSet.empty())   ? "" : "_" + mShimMetricSet;    
    std::string chan_str = (mChannelId < 0)           ? "" : "_chan" + std::to_string(mChannelId);

    std::string outputFile = "aie_profile_" + deviceName + core_str + mem_str 
        + shim_str + chan_str + ".csv";

    VPWriter* writer = new AIEProfilingWriter(outputFile.c_str(),
                                              deviceName.c_str(), mIndex);
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_PROFILE");

    // Start the AIE profiling thread
    mThreadCtrlMap[handle] = true;
    auto device_thread = std::thread(&AIEProfilingPlugin::pollAIECounters, this, mIndex, handle);
    mThreadMap[handle] = std::move(device_thread);

    ++mIndex;
  }

  void AIEProfilingPlugin::endPollforDevice(void* handle)
  {
    // Ask thread to stop
    mThreadCtrlMap[handle] = false;

    auto it = mThreadMap.find(handle);
    if (it != mThreadMap.end()) {
      it->second.join();
      mThreadMap.erase(it);
      mThreadCtrlMap.erase(handle);
    }
  }

  void AIEProfilingPlugin::endPoll()
  {
    // Ask all threads to end
    for (auto& p : mThreadCtrlMap)
      p.second = false;

    for (auto& t : mThreadMap)
      t.second.join();

    mThreadCtrlMap.clear();
    mThreadMap.clear();
  }

  std::vector<tile_type>
  AIEProfilingPlugin::getAllTilesForCoreMemoryProfiling(const XAie_ModuleType mod,
                                                        const std::string &graph,
                                                        void* handle)
  {
    std::vector<tile_type> tiles;
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    tiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
                                 xrt_core::edge::aie::module_type::core);
    if (mod == XAIE_MEM_MOD) {
      auto dmaTiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph,
          xrt_core::edge::aie::module_type::dma);
      std::move(dmaTiles.begin(), dmaTiles.end(), back_inserter(tiles));
    }
    return tiles;
  }

  std::vector<tile_type>
  AIEProfilingPlugin::getAllTilesForShimProfiling(void* handle, 
                        const std::string &metricsStr, 
                        int16_t channelId,
                        bool useColumn, uint32_t minCol, uint32_t maxCol)
  {
    std::vector<tile_type> tiles;

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    int plioCount = 0;
    auto plios = xrt_core::edge::aie::get_plios(device.get());
    for (auto& plio : plios) {
      auto isMaster = plio.second.slaveOrMaster;
      auto streamId = plio.second.streamId;
      auto shimCol  = plio.second.shimColumn;

      // If looking for specific ID, make sure it matches
      if ((channelId >= 0) && (channelId != streamId))
        continue;

      // Make sure it's desired polarity
      // NOTE: input = slave (data flowing from PLIO)
      //       output = master (data flowing to PLIO)
      if ((isMaster && (metricsStr == "input_bandwidths"))
          || (!isMaster && (metricsStr == "output_bandwidths")))
        continue;

      plioCount++;

      if (useColumn 
          && !( (minCol <= shimCol) && (shimCol <= maxCol) )) {
        // shimCol is not within minCol:maxCol range. So skip.
        continue;
      }

      xrt_core::edge::aie::tile_type tile;
      tile.col = shimCol;
      tile.row = 0;
      // Grab stream ID and slave/master (used in configStreamSwitchPorts())
      tile.itr_mem_col = isMaster;
      tile.itr_mem_row = streamId;
      tiles.push_back(tile);
    }
          
    if ((0 == plioCount) && (0 <= channelId)) {
      std::string msg = "No tiles used channel ID " + std::to_string(channelId)
                        + ". Please specify a valid channel ID.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }
    return tiles;
  }

  // Resolve Processor and Memory metrics on all tiles
  // Mem tile metrics is not supported now.
  void
  AIEProfilingPlugin::getConfigMetricsForTiles(int moduleIdx,
                                               std::vector<std::string> metricsSettings,
                                               std::vector<std::string> graphmetricsSettings,
                                               const XAie_ModuleType mod,
                                               void* handle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    bool allGraphsDone = false;

    // STEP 1 : Parse per-graph or per-kernel settings

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * "graphmetricsSettings" contains each metric value
     * graph_based_aie_metrics = <graph name|all>:<kernel name|all>:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>
     * graph_based_aie_memory_metrics = <graph name|all>:<kernel name|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths>
     * graph_based_mem_tile_metrics = <graph name|all>:<kernel name|all>:<off|input_channels|output_channels|memory_stats>[:<channel>]
     */

    std::vector<std::vector<std::string>> graphmetrics(graphmetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < graphmetricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(graphmetrics[i], graphmetricsSettings[i], boost::is_any_of(":"));
      if (3 != graphmetrics[i].size()) {
        /* Note : only graph_mem_tile_metrics can have more than 3 items in a metric value.
         * But it is not supported now.
         */
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Expected three \":\" separated fields for graph_based_aie_[memory_]metrics not found. Hence ignored.");
        continue;
      }

      std::vector<tile_type> tiles;
      /*
       * Core profiling uses all unique core tiles in aie control
       * Memory profiling uses all unique core + dma tiles in aie control
       */
      if (XAIE_CORE_MOD == mod || XAIE_MEM_MOD == mod) {
        if (0 == graphmetrics[i][0].compare("all")) {

          // Check kernel-name field
          if (0 != graphmetrics[i][1].compare("all")) {
            xrt_core::message::send(severity_level::warning, "XRT", 
              "Only \"all\" is supported in kernel-name field for graph_based_aie_[memory_]metrics. Any other specification is replaced with \"all\".");
          }
          // Capture all tiles across all graphs
          auto graphs = xrt_core::edge::aie::get_graphs(device.get());
          for (auto& graph : graphs) {
            std::vector<tile_type> nwTiles = getAllTilesForCoreMemoryProfiling(mod, graph, handle);
            tiles.insert(tiles.end(), nwTiles.begin(), nwTiles.end());
          } 
          allGraphsDone = true;
        } // "all" 
      }
      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = graphmetrics[i][2];
      }
    }  // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting 
    for (size_t i = 0; i < graphmetricsSettings.size(); ++i) {
      if (3 != graphmetrics[i].size()) {
        // Warning must be already generated in Graph Pass 1. So continue silently here.
        continue;
      }
      std::vector<tile_type> tiles;
      /*
       * Core profiling uses all unique core tiles in aie control
       * Memory profiling uses all unique core + dma tiles in aie control
       */
      if (XAIE_CORE_MOD == mod || XAIE_MEM_MOD == mod) {
        if (0 != graphmetrics[i][0].compare("all")) {
          // Check kernel-name field
          if (0 != graphmetrics[i][1].compare("all")) {
            xrt_core::message::send(severity_level::warning, "XRT", 
              "Only \"all\" is supported in kernel-name field for graph_based_aie_[memory_]metrics. Any other specification is replaced with \"all\".");
          }
          // Capture all tiles in the given graph
          tiles = getAllTilesForCoreMemoryProfiling(mod, graphmetrics[i][0] /*graph name*/, handle);
        }
      }
      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = graphmetrics[i][2];
      }
    }  // Graph Pass 2

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * tile_based_aie_metrics = [[{<column>,<row>}|all>:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>]; [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>]]
     *
     * tile_based_aie_memory_metrics = [[<{<column>,<row>}|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths> ]; [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths>]]
     *
     * tile_based_mem_tile_metrics = [[<{<column>,<row>}|all>:<off|input_channels|output_channels|memory_stats>[:<channel>]] ; [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|input_channels|output_channels|memory_stats>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (0 == metrics[i][0].compare("all")) {
        std::vector<tile_type> tiles;
        if (!allGraphsDone) {
          if (XAIE_CORE_MOD == mod || XAIE_MEM_MOD == mod) {
            // Capture all tiles across all graphs
            auto graphs = xrt_core::edge::aie::get_graphs(device.get());
            for (auto& graph : graphs) {
              std::vector<tile_type> nwTiles = getAllTilesForCoreMemoryProfiling(mod, graph, handle);
              tiles.insert(tiles.end(), nwTiles.begin(), nwTiles.end());
            } 
            allGraphsDone = true;
          }
        } // allGraphsDone
        for (auto &e : tiles) {
          mConfigMetrics[moduleIdx][e] = metrics[i][1];
        }
      }
    } // Pass 1 

    // Pass 2 : process only range of tiles metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      std::vector<tile_type> tiles;

      if (3 != metrics[i].size()) {
        continue;
      }

      for (size_t j = 0; j < metrics[i].size(); ++j) {
        boost::replace_all(metrics[i][j], "{", "");
        boost::replace_all(metrics[i][j], "}", "");
      }

      uint32_t minRow = 0, minCol = 0;
      uint32_t maxRow = 0, maxCol = 0;

      std::vector<std::string> minTile, maxTile;

      try {
        boost::split(minTile, metrics[i][0], boost::is_any_of(","));
        minCol = std::stoi(minTile[0]);
        minRow = std::stoi(minTile[1]);

        std::vector<std::string> maxTile;
        boost::split(maxTile, metrics[i][1], boost::is_any_of(","));
        maxCol = std::stoi(maxTile[0]);
        maxRow = std::stoi(maxTile[1]);
      } catch (...) {
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Tile range specification in tile_based_aie_[memory}_metrics is not of valid format and hence skipped.");
        continue;
      }

      for (uint32_t col = minCol; col <= maxCol; ++col) {
        for (uint32_t row = minRow; row <= maxRow; ++row) {
          xrt_core::edge::aie::tile_type tile;
          tile.col = col;
          tile.row = row;
          tiles.push_back(tile);
        }
      }
      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = metrics[i][2];
      }
    } // Pass 2 

    // Pass 3 : process only single tile metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {

      std::vector<tile_type> tiles;
      if (2 != metrics[i].size()) {
        continue;
      }
      if (0 == metrics[i][0].compare("all")) {
        continue;
      }
      xrt_core::edge::aie::tile_type tile;
      std::vector<std::string> tilePos;

      try {
        boost::replace_all(metrics[i][0], "{", "");
        boost::replace_all(metrics[i][0], "}", "");

        boost::split(tilePos, metrics[i][0], boost::is_any_of(","));

        tile.col = std::stoi(tilePos[0]);
        tile.row = std::stoi(tilePos[1]);
        tiles.push_back(tile);
      } catch (...) {
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Tile specification in tile_based_aie_[memory}_metrics is not of valid format and hence skipped.");
        continue;
      }

      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = metrics[i][1];
      }
    } // Pass 3 

    // check validity, set default and remove "off" tiles
    std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" : "aie_memory";

    std::vector<tile_type> offTiles;

    for (auto &tileMetric : mConfigMetrics[moduleIdx]) {

      // save list of "off" tiles
      if (tileMetric.second.empty() || 0 == tileMetric.second.compare("off")) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      if (((mod == XAIE_CORE_MOD) && (mCoreStartEvents.find(tileMetric.second) == mCoreStartEvents.end()))
          || ((mod == XAIE_MEM_MOD) && (mMemoryStartEvents.find(tileMetric.second) == mMemoryStartEvents.end()))) {
        std::string defaultSet = (mod == XAIE_CORE_MOD) ? "heat_map" : "conflicts";
        std::stringstream msg;
        msg << "Unable to find " << moduleName << " metric set " << tileMetric.second
            << ". Using default of " << defaultSet << "."
            << " As new AIE_profile_settings section is given, old style metric configurations, if any, are ignored.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        tileMetric.second = defaultSet;
      }
    }

    // remove all the "off" tiles
    for (auto &t : offTiles) {
      mConfigMetrics[moduleIdx].erase(t);
    }
  }

  // Resolve Interface metrics 
  void
  AIEProfilingPlugin::getInterfaceConfigMetricsForTiles(int moduleIdx,
                                               std::vector<std::string> metricsSettings,
                                               std::vector<std::string> graphmetricsSettings,
                                               void* handle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    bool allGraphsDone = false;

    // STEP 1 : Parse per-graph or per-kernel settings

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * "graphmetricsSettings" contains each metric value
     * graph_based_interface_tile_metrics = <graph name|all>:<port name|all>:<off|input_bandwidths|output_bandwidths|packets>
     */

    std::vector<std::vector<std::string>> graphmetrics(graphmetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < graphmetricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(graphmetrics[i], graphmetricsSettings[i], boost::is_any_of(":"));
      if (3 > graphmetrics[i].size()) {
        // Add unexpected format warning
        continue;
      }
      if (0 != graphmetrics[i][0].compare("all")) {
        continue;
      }
#if 0
      if (0 != graphmetrics[i][1].compare("all")) {
        xrt_core::message::send(severity_level::warning, "XRT",
           "Specific port name is not yet supported in \"graph_based_interface_tile_metrics\" configuration. This will be ignored. Please use \"all\" in port name field.");
      }
#endif
      /*
       * Shim profiling uses all tiles utilized by PLIOs
       */
      std::vector<tile_type> tiles;
      tiles = getAllTilesForShimProfiling(handle, graphmetrics[i][2]);
      allGraphsDone = true;

      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = graphmetrics[i][2];
      }
    }  // Graph Pass 1

#if 0
    // Graph Pass 2 : process per graph metric setting 
    /* Currently interfaces cannot be tied to graphs.
     * graph_based_interface_tile_metrics = <graph name>:<port name|all>:<off|input_bandwidths|output_bandwidths|packets>
     * is not supported yet.
     */
#endif

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * tile_based_interface_tile_metrics = [[<column|all>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]] ; [<mincolumn>:<maxcolumn>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (0 == metrics[i][0].compare("all")) {

        int16_t channelId = -1;
        if (3 == metrics[i].size()) {
          channelId = std::stoi(metrics[i][2]);
        }

        std::vector<tile_type> tiles;
        if (!allGraphsDone || (channelId >= 0)) {
          tiles = getAllTilesForShimProfiling(handle, metrics[i][1], channelId);
          allGraphsDone = true;
        } // allGraphsDone

        for (auto &e : tiles) {
          mConfigMetrics[moduleIdx][e] = metrics[i][1];
        }
      }
    } // Pass 1 

    // Pass 2 : process only range of tiles metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      std::vector<tile_type> tiles;

      if (3 > metrics[i].size()) {
        continue;
      }
     /* The following two styles are applicable here 
      * tile_based_interface_tile_metrics = <column|all>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>] 
      * OR
      * tile_based_interface_tile_metrics = <mincolumn>:<maxcolumn>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]]
      * Handle only the 2nd style here.
      */

      uint32_t maxCol = 0;
      try {
        maxCol = std::stoi(metrics[i][1]);
      } catch (std::invalid_argument const &e) {
        // maxColumn is not an integer i.e either 1st style or wrong format, skip for now
        continue;
      }
      uint32_t minCol = 0;
      try {
        minCol = std::stoi(metrics[i][0]);
      } catch (std::invalid_argument const &e) {
        // 2nd style but expected min column is not an integer, give warning and skip 
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Minimum column specification in tile_based_interface_tile_metrics is not an integer and hence skipped.");
        continue;
      }

      int16_t channelId = 0;
      if (4 == metrics[i].size()) {
        try {
          channelId = std::stoi(metrics[i][3]);
        } catch (std::invalid_argument const &e) {
          // Expected channel Id is not an integer, give warning and ignore channelId
          xrt_core::message::send(severity_level::warning, "XRT", 
             "Channel ID specification in tile_based_interface_tile_metrics is not an integer and hence ignored.");
          channelId = -1;
        }
      }
      tiles = getAllTilesForShimProfiling(handle, metrics[i][2], channelId,
                                          true, minCol, maxCol);

      for (auto &t : tiles) {
        mConfigMetrics[moduleIdx][t] = metrics[i][2];
      }
    } // Pass 2 

    // Pass 3 : process only single tile metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      std::vector<tile_type> tiles;

      if (4 == metrics[i].size() /* skip column range specification with channel */
            || 2 > metrics[i].size() /* invalid format */) {
        continue;
      }
      if (0 == metrics[i][0].compare("all")) {
        continue;
      }
      uint32_t col = 0;
      try {
        col = std::stoi(metrics[i][1]);
      } catch (std::invalid_argument const &e) {
        // max column is not a number, so the expected single column specification. Handle this here

        try {
          col = std::stoi(metrics[i][0]);
        } catch (std::invalid_argument const &e) {
          // Expected column specification is not a number. Give warning and skip
          xrt_core::message::send(severity_level::warning, "XRT", 
             "Column specification in tile_based_interface_tile_metrics is not an integer and hence skipped.");
          continue;
        }

        int16_t channelId = -1;
        if (3 == metrics[i].size()) {
          try {
            channelId = std::stoi(metrics[i][2]);
          } catch (std::invalid_argument const &e) {
            // Expected channel Id is not an integer, give warning and ignore channelId
            xrt_core::message::send(severity_level::warning, "XRT", 
               "Channel ID specification in tile_based_interface_tile_metrics is not an integer and hence ignored.");
            channelId = -1;
          }
        }
        tiles = getAllTilesForShimProfiling(handle, metrics[i][1], channelId,
                                            true, col, col);

        for (auto &t : tiles) {
          mConfigMetrics[moduleIdx][t] = metrics[i][1];
        }
      }
    } // Pass 3 

    // check validity, set default and remove "off" tiles
    std::vector<tile_type> offTiles;
    for (auto &tileMetric : mConfigMetrics[moduleIdx]) {

      // save list of "off" tiles
      if (tileMetric.second.empty() || 0 == tileMetric.second.compare("off")) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      if (mShimStartEvents.find(tileMetric.second) == mShimStartEvents.end()) {
        std::string msg = "Unable to find interface_tile metric set " + tileMetric.second
                          + ". Using default of input_bandwidths. "
                          + "As new AIE_profile_settings section is given, old style metric configurations, if any, are ignored.";
        xrt_core::message::send(severity_level::warning, "XRT", msg);
        tileMetric.second = "input_bandwidths" ;
      }
    }

    // remove all the "off" tiles
    for (auto &t : offTiles) {
      mConfigMetrics[moduleIdx].erase(t);
    }
  }

  // Set metrics for all specified AIE counters on this device with configs given in AIE_profile_settings
  bool 
  AIEProfilingPlugin::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    int counterId = 0;
    bool runtimeCounters = false;

    // Get AIE clock frequency
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto clockFreqMhz = xrt_core::edge::aie::get_clock_freq_mhz(device.get());

    // Currently supporting Core, Memory, Interface Tile metrics only. Need to add Memory Tile metrics
    constexpr int NUM_MODULES = 3;

    std::string moduleNames[NUM_MODULES] = {"aie", "aie_memory", "interface_tile"};

    int numCountersMod[NUM_MODULES] =
        {NUM_CORE_COUNTERS, NUM_MEMORY_COUNTERS, NUM_SHIM_COUNTERS};
    XAie_ModuleType falModuleTypes[NUM_MODULES] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD};

    // Get the metrics settings
    std::vector<std::string> metricsConfig;

    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_memory_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_interface_tile_metrics());
//    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_mem_tile_metrics());

    // Get the graph metrics settings
    std::vector<std::string> graphmetricsConfig;

    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_metrics());
    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_memory_metrics());
    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_interface_tile_metrics());
//    graphmetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_mem_tile_metrics());

    // Process AIE_profile_settings metrics
    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::vector<std::string>> metricsSettings(NUM_MODULES);
    std::vector<std::vector<std::string>> graphmetricsSettings(NUM_MODULES);

    mConfigMetrics.resize(NUM_MODULES);

    bool newConfigUsed = false;
    for(int module = 0; module < NUM_MODULES; ++module) {
      bool findTileMetric = false;
      if (!metricsConfig[module].empty()) {
        boost::replace_all(metricsConfig[module], " ", "");
        boost::split(metricsSettings[module], metricsConfig[module], boost::is_any_of(";"));
        findTileMetric = true;        
      } else {
#if 0
// Add warning later
// No need to add the warning message here, as all the tests are using configs under Debug
        std::string modName = moduleNames[module].substr(0, moduleNames[module].find(" "));
        std::string metricMsg = "No metric set specified for " + modName + " module. " +
                                "Please specify the AIE_profile_settings." + modName + "_metrics setting in your xrt.ini.";
        xrt_core::message::send(severity_level::warning, "XRT", metricMsg);
#endif
      }
      if (!graphmetricsConfig[module].empty()) {
        boost::replace_all(graphmetricsConfig[module], " ", "");
        boost::split(graphmetricsSettings[module], graphmetricsConfig[module], boost::is_any_of(";"));
        findTileMetric = true;        
      } else {
#if 0
// Add warning later
// No need to add the warning message here, as all the tests are using configs under Debug
        std::string modName = moduleNames[module].substr(0, moduleNames[module].find(" "));
        std::string metricMsg = "No graph metric set specified for " + modName + " module. " +
                                "Please specify the AIE_profile_settings.graph_" + modName + "_metrics setting in your xrt.ini.";
        xrt_core::message::send(severity_level::warning, "XRT", metricMsg);
#endif
      }
      if(findTileMetric) {
        newConfigUsed = true;

        if (XAIE_PL_MOD == falModuleTypes[module]) {
          getInterfaceConfigMetricsForTiles(module, 
                                       metricsSettings[module], 
                                       graphmetricsSettings[module], 
                                       handle);
        } else {
          getConfigMetricsForTiles(module, 
                                   metricsSettings[module], 
                                   graphmetricsSettings[module], 
                                   falModuleTypes[module],
                                   handle);
        }
      }
    }

    if (!newConfigUsed) {
      // None of the new style AIE profile metrics have been used. So check for old style.
      return false;
    }

    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);

    for(int module = 0; module < NUM_MODULES; ++module) {
      int numTileCounters[numCountersMod[module]+1] = {0};
      XAie_ModuleType mod    = falModuleTypes[module];

      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : mConfigMetrics[module]) {
        int numCounters = 0;
        auto col = tileMetric.first.col;
        auto row = tileMetric.first.row;

        // NOTE: resource manager requires absolute row number
        auto loc        = (mod == XAIE_PL_MOD) ? XAie_TileLoc(col, 0) 
                        : XAie_TileLoc(col, row + 1);
        auto& xaieTile  = (mod == XAIE_PL_MOD) ? aieDevice->tile(col, 0) 
                        : aieDevice->tile(col, row + 1);
        auto xaieModule = (mod == XAIE_CORE_MOD) ? xaieTile.core()
                        : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
                        : xaieTile.pl());

        auto numFreeCtr = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);
        
        auto startEvents = (mod == XAIE_CORE_MOD) ? mCoreStartEvents[tileMetric.second]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryStartEvents[tileMetric.second] 
                         : mShimStartEvents[tileMetric.second]);
        auto endEvents   = (mod == XAIE_CORE_MOD) ? mCoreEndEvents[tileMetric.second]
                         : ((mod == XAIE_MEM_MOD) ? mMemoryEndEvents[tileMetric.second] 
                         : mShimEndEvents[tileMetric.second]);

        auto numTotalReqEvents = startEvents.size();
        if (numFreeCtr < numTotalReqEvents) {
          // warning
 #if 0
          std::stringstream msg;
          msg << "Only " << numFreeCtr << " out of " << numTotalEvents
              << " metrics were available for AIE "
              << moduleName << " profiling due to resource constraints. "
              << "AIE profiling uses performance counters which could be already used by AIE trace, ECC, etc."
              << std::endl;

          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
 #endif
        }
        for (int i=0; i < numFreeCtr; ++i) {

          // Get vector of pre-defined metrics for this set
          uint8_t resetEvent = 0;

          auto startEvent = startEvents.at(i);
          auto endEvent   = endEvents.at(i);

          // Request counter from resource manager
          auto perfCounter = xaieModule.perfCounter();
          auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
          if (ret != XAIE_OK) break;
          ret = perfCounter->reserve();
          if (ret != XAIE_OK) break;
        
          configGroupEvents(aieDevInst, loc, mod, startEvent, tileMetric.second);
          configStreamSwitchPorts(aieDevInst, tileMetric.first, xaieTile, loc, startEvent, tileMetric.second);
        
          // Start the counters after group events have been configured
          ret = perfCounter->start();
          if (ret != XAIE_OK) break;
          mPerfCounters.push_back(perfCounter);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = (mod == XAIE_CORE_MOD) ? tmpStart
                                 : ((mod == XAIE_MEM_MOD) ? (tmpStart + BASE_MEMORY_COUNTER)
                                 : (tmpStart + BASE_SHIM_COUNTER));
          uint16_t phyEndEvent   = (mod == XAIE_CORE_MOD) ? tmpEnd
                                 : ((mod == XAIE_MEM_MOD) ? (tmpEnd + BASE_MEMORY_COUNTER)
                                 : (tmpEnd + BASE_SHIM_COUNTER));

          auto payload = getCounterPayload(aieDevInst, tileMetric.first, col, row, startEvent);

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
                phyStartEvent, phyEndEvent, resetEvent, payload, clockFreqMhz, 
                moduleNames[module], counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," << row << ").";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      }

      // Report counters reserved per tile
      {
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << moduleNames[module] << " - ";
        for (int n=0; n <= numCountersMod[module]; ++n) {
          if (numTileCounters[n] == 0) continue;
          msg << n << ": " << numTileCounters[n] << " tiles";
          if (n != numCountersMod[module]) msg << ", ";

          (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
        }
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }

      runtimeCounters = true;
    } // modules

    return runtimeCounters;
  }
 
} // end namespace xdp
