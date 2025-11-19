// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_PLUGIN_SOURCE

#include "aie_profile_npu3.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "core/common/api/bo_int.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "core/include/xrt/xrt_kernel.h"

#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/experimental/xrt_module.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_base/aie_base_util.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/plugin/vp_base/info.h"

// XRT headers
#include "xrt/xrt_bo.h"
#include "core/common/shim/hwctx_handle.h"

#ifdef _WIN32
#include <windows.h> 
#endif

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;

  AieProfile_NPU3Impl::AieProfile_NPU3Impl(VPDatabase* database,
    std::shared_ptr<AieProfileMetadata> metadata
  )
    : AieProfileImpl(database, metadata)
  {
    auto hwGen = metadata->getHardwareGen();

    coreStartEvents = aie::profile::getCoreEventSets(hwGen);
    coreEndEvents = coreStartEvents;

    memoryStartEvents = aie::profile::getMemoryEventSets(hwGen);
    memoryEndEvents = memoryStartEvents;

    shimStartEvents = aie::profile::getInterfaceTileEventSets(hwGen);
    shimEndEvents = shimStartEvents;

    memTileStartEvents = aie::profile::getMemoryTileEventSets(hwGen);
    memTileEndEvents = memTileStartEvents;

    tranxHandler = std::make_unique<aie::NPU3Transaction>();

    // Create debug buffer for AIE Profile results
    auto context = metadata->getHwContext();
    uint32_t* output = nullptr;
    std::map<uint32_t, size_t> activeUCsegmentMap;
    activeUCsegmentMap[0] = 0x20000;
    try {
      //resultBO = xrt_core::bo_int::create_debug_bo(context, 0x20000);
      resultBO = xrt_core::bo_int::create_bo(context, 0x20000, xrt_core::bo_int::use_type::uc_debug);
      xrt_core::bo_int::config_bo(resultBO, activeUCsegmentMap);
      output = resultBO.map<uint32_t*>();
      memset(output, 0, 0x20000);
    } 
    catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to create 128KB buffer for AIE Profile results. Cannot get AIE Profile info. " << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
    }
  }

  void AieProfile_NPU3Impl::updateDevice()
  {
    setMetricsSettings(metadata->getDeviceID());
    generatePollElf();
  }

  bool AieProfile_NPU3Impl::setMetricsSettings(const uint64_t deviceId)
  {
    xrt_core::message::send(severity_level::info, "XRT", "Setting AIE Profile Metrics Settings.");

    int counterId = 0;
    bool runtimeCounters = false;

    xdp::aie::driver_config meta_config = metadata->getAIEConfigMetadata();

    XAie_Config cfg {
      meta_config.hw_gen,
      meta_config.base_address,
      meta_config.column_shift,
      meta_config.row_shift,
      meta_config.num_rows,
      meta_config.num_columns,
      meta_config.shim_row,
      0,
      1,
      meta_config.mem_row_start,
      meta_config.mem_num_rows,
      meta_config.aie_tile_row_start,
      meta_config.aie_tile_num_rows,
      {0} // PartProp
    };

    auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);
    if (RC != XAIE_OK) {
      xrt_core::message::send(severity_level::warning, "XRT", "AIE Driver Initialization Failed.");
      return false;
    }

    std::string tranxName = "AieProfileMetrics";

    // Get partition columns
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(metadata->getHandle());
    // Currently, assuming only one Hw Context is alive at a time
    //uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));
    uint8_t startCol = 0;

    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
      "Starting transaction " + tranxName);

    // Initialize transaction    
    if (!tranxHandler->initializeTransaction(&aieDevInst, tranxName))
      return false;

    auto configChannel0 = metadata->getConfigChannel0();
    auto configChannel1 = metadata->getConfigChannel1();

    for (int module = 0; module < metadata->getNumModules(); ++module) {
      std::cout << "Configuring profiling for module " << module << std::endl;

      XAie_ModuleType mod = aie::profile::getFalModuleType(module);
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : metadata->getConfigMetrics(module)) {
        int numCounters  = 0;

        auto& metricSet  = tileMetric.second;
        auto tile = tileMetric.first;
        auto row  = tile.row;
        auto col  = tile.col;
        auto subtype = tile.subtype;
        auto type = aie::getModuleType(row, metadata->getAIETileRowOffset());
        
        std::cout << "Configuring profiling for tile (" << +col << "," << +row 
                  << ") using metric set " << metricSet << std::endl;
        
        // Ignore invalid types and inactive modules
        // NOTE: Inactive core modules are configured when utilizing
        //       stream switch monitor ports to profile DMA channels
        if (!aie::profile::isValidType(type, mod))
          continue;
        if ((type == module_type::dma) && !tile.active_memory)
          continue;
        if ((type == module_type::core) && !tile.active_core) {
          if (metadata->getPairModuleIndex(metricSet, type) < 0)
            continue;
        }

        //std::cout << "Getting sets and modifying events..." << std::endl;

        auto loc         = XAie_TileLoc(col, row);
        auto startEvents = (type  == module_type::core) ? coreStartEvents[metricSet]
                         : ((type == module_type::dma)  ? memoryStartEvents[metricSet]
                         : ((type == module_type::shim) ? shimStartEvents[metricSet]
                         : memTileStartEvents[metricSet]));
        auto endEvents   = (type  == module_type::core) ? coreEndEvents[metricSet]
                         : ((type == module_type::dma)  ? memoryEndEvents[metricSet]
                         : ((type == module_type::shim) ? shimEndEvents[metricSet]
                         : memTileEndEvents[metricSet]));

        uint8_t numFreeCtr = static_cast<uint8_t>(startEvents.size());
        std::vector<uint64_t> Regs = regValues.at(type);

        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        // TODO: for now, hard-code channels 2 and 3
        std::vector<uint8_t> channels = {channel0, channel1, 2, 3};

        // Modify events as needed
        aie::profile::modifyEvents(type, subtype, channel0, startEvents, metadata->getHardwareGen());
        endEvents = startEvents;

        configEventSelections(loc, type, metricSet, channels);

        // Request and configure all available counters for this tile
        for (uint8_t i = 0; i < numFreeCtr; i++) {
          //std::cout << "Configuring counter " << +i << std::endl;

          auto startEvent    = startEvents.at(i);
          auto endEvent      = endEvents.at(i);
          uint8_t resetEvent = 0;

          // No resource manager, so manually manage the counters
          RC = XAie_PerfCounterReset(&aieDevInst, loc, mod, i);
          if (RC != XAIE_OK) {
            xrt_core::message::send(severity_level::error, "XRT", "AIE Performance Counter Reset Failed.");
            break;
          }
          RC = XAie_PerfCounterControlSet(&aieDevInst, loc, mod, i, startEvent, endEvent);
          if (RC != XAIE_OK) {
            xrt_core::message::send(severity_level::error, "XRT", "AIE Performance Counter Set Failed.");
            break;
          }

          aie::profile::configGroupEvents(&aieDevInst, loc, mod, type, metricSet, startEvent, channel0);
          if (aie::isStreamSwitchPortEvent(startEvent))
           configStreamSwitchPorts(tileMetric.first, loc, type, metricSet, channel0, startEvent);

          // Convert enums to physical event IDs for reporting purposes
          uint16_t tmpStart;
          uint16_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = tmpStart + aie::profile::getCounterBase(type);
          uint16_t phyEndEvent   = tmpEnd   + aie::profile::getCounterBase(type);
          // auto payload = getCounterPayload(tileMetric.first, type, col, row, 
          //                                  startEvent, metricSet, channel0);
          auto payload = channel0;

          // Store counter info in database
          std::string counterName = "AIE Counter" + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
                phyStartEvent, phyEndEvent, resetEvent, payload, metadata->getClockFreqMhz(), 
                metadata->getModuleName(module), counterName);

          // NOTE: NPU3 has unique addressing, so get offsets from driver 
          auto tileOffset = XAie_GetTileAddr(&aieDevInst, row, col);
          op_profile_data.emplace_back((u32)(Regs[i] + tileOffset));

          std::vector<uint64_t> values;
          uint8_t absCol = col + startCol;
          values.insert(values.end(), {absCol, row, phyStartEvent, phyEndEvent, 
                        resetEvent, 0, 0, payload});
          outputValues.push_back(values);

          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << +col << "," 
            << +row << ") using metric set " << metricSet << " and channel " << +channel0 << ".";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        // numTileCounters[numCounters]++;
      }
      runtimeCounters = true;
    } // modules

    //xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(hwCtxImpl);
    auto hwContext = metadata->getHwContext();
    tranxHandler->submitTransaction(&aieDevInst, hwContext);

    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE Profiling.");
    return runtimeCounters;
  } // setMetricsSettings

  /****************************************************************************
  * Configure selection index to monitor channel numbers
  * NOTE: In NPU3, this is required in memory and interface tiles
  ***************************************************************************/
  void
  AieProfile_NPU3Impl::configEventSelections(const XAie_LocType loc, const module_type type,
                                             const std::string metricSet, std::vector<uint8_t>& channels)
  {
    if ((type != module_type::mem_tile) && (type != module_type::shim))
      return;

    XAie_DmaDirection dmaDir = aie::isInputSet(type, metricSet) ? DMA_S2MM : DMA_MM2S;
    uint8_t numChannels = ((type == module_type::shim) && (dmaDir == DMA_MM2S))
                        ? NUM_CHANNEL_SELECTS_SHIM_NPU3 : NUM_CHANNEL_SELECTS;

    if (aie::isDebugVerbosity()) {
      std::string tileType = (type == module_type::shim) ? "interface" : "memory";
      std::string dmaType  = (dmaDir == DMA_S2MM) ? "S2MM" : "MM2S";
      std::stringstream channelsStr;
      std::copy(channels.begin(), channels.end(), std::ostream_iterator<uint8_t>(channelsStr, ", "));
      
      std::string msg = "Configuring event selections for " + tileType + " tile DMA " 
                      + dmaType + " channels " + channelsStr.str();
      xrt_core::message::send(severity_level::debug, "XRT", msg);
    }

    for (uint8_t c = 0; c < numChannels; ++c)
      XAie_EventSelectDmaChannel(&aieDevInst, loc, c, dmaDir, channels.at(c));
  }

  /****************************************************************************
  * Configure stream switch ports for monitoring purposes
  * NOTE: Used to monitor streams: trace, interfaces, and memory tiles
  ***************************************************************************/
  void
  AieProfile_NPU3Impl::configStreamSwitchPorts(const tile_type& tile, const XAie_LocType& loc,
                                               const module_type& type, const std::string& metricSet, 
                                               const uint8_t channel, const XAie_Events startEvent)
  {
    // Hardcoded
    uint8_t rscId = 0;
    uint8_t portnum = aie::getPortNumberFromEvent(startEvent);
    // AIE Tiles (e.g., trace streams)
    if (type == module_type::core) {
      auto slaveOrMaster = (metricSet.find("mm2s") != std::string::npos) ?
        XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      XAie_EventSelectStrmPort(&aieDevInst, loc, rscId, slaveOrMaster, DMA, channel);
      std::stringstream msg;
      msg << "Configured core tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") 
          << " stream switch ports for metricset " << metricSet << " and channel " << (int)channel << ".";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      return;
    }

    // Interface tiles (e.g., PLIO, GMIO)
    if (type == module_type::shim) {
      // NOTE: skip configuration of extra ports for tile if stream_ids are not available.
      if (portnum >= tile.stream_ids.size())
        return;
      // Grab slave/master and stream ID
      // NOTE: stored in getTilesForProfiling() above
      auto slaveOrMaster = (tile.is_master_vec.at(portnum) == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      uint8_t streamPortId = static_cast<uint8_t>(tile.stream_ids.at(portnum));
      
      // auto streamPortId  = tile.stream_id;
      // Define stream switch port to monitor interface 
      XAie_EventSelectStrmPort(&aieDevInst, loc, rscId, slaveOrMaster, SOUTH, streamPortId);
      std::stringstream msg;
      msg << "Configured shim tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") << " stream switch ports for metricset " << metricSet << " and stream port id " << (int)streamPortId << ".";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      return;
    }

    if (type == module_type::mem_tile) {
      auto slaveOrMaster = (metricSet.find("mm2s") != std::string::npos) ?
        XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      XAie_EventSelectStrmPort(&aieDevInst, loc, rscId, slaveOrMaster, DMA, channel);
      std::stringstream msg;
      msg << "Configured mem tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") << " stream switch ports for metricset " << metricSet << " and channel " << (int)channel << ".";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }
  }

  void AieProfile_NPU3Impl::generatePollElf()
  {
    auto context = metadata->getHwContext();

    std::string tranxName = "AieProfilePoll";
    if (!tranxHandler->initializeTransaction(&aieDevInst, tranxName)) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
                              "Unable to initialize transaction for AIE profile polling.");
      return;
    }
    for (u32 i = 0; i < op_profile_data.size(); i++) {
      XAie_SaveRegister(&aieDevInst, op_profile_data[i], i);
    }
    if (!tranxHandler->submitTransaction(&aieDevInst, context))
      return;
  }

  void AieProfile_NPU3Impl::poll(const uint64_t id)
  {
    if (finishedPoll)
      return;

    if (db->infoAvailable(xdp::info::ml_timeline)) {
      db->broadcast(VPDatabase::MessageType::READ_RECORD_TIMESTAMPS, nullptr);
      xrt_core::message::send(severity_level::debug, "XRT", "Done reading recorded timestamps.");
    }

    resultBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    uint32_t* output = resultBO.map<uint32_t*>();

    // Get timestamp in milliseconds
    double timestamp = xrt_core::time_ns() / 1.0e6;

    //**************************TODO: Remove this after testing ***************************
    for (u32 i = 0; i < op_profile_data.size() + 12 * 3; i++) {
      std::stringstream msg;
      msg << "Counter address/values: " << output[2 * i] << " - " << output[2 * i + 1];
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
    }

    // Process counter values and add to database
    for (u32 i = 0; i < op_profile_data.size(); i++) {
      // Update counter value in outputValues and add to database
      std::vector<uint64_t> values = outputValues[i];
      values[5] = static_cast<uint64_t>(output[2 * i + 1]); // Write counter value
      db->getDynamicInfo().addAIESample(id, timestamp, values);
    }

    finishedPoll = true;
  }

  void AieProfile_NPU3Impl::freeResources()
  {
    
  }
}  // namespace xdp
