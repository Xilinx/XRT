/**
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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
#include <boost/property_tree/ptree.hpp>

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "core/common/api/bo_int.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
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

  AieProfile_WinImpl::AieProfile_WinImpl(VPDatabase* database,
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

    auto context = metadata->getHwContext();
    transactionHandler = std::make_unique<aie::ClientTransaction>(context, "AIE Profile Setup");
    
  }

  void
  AieProfile_WinImpl::updateDevice()
  {
    setMetricsSettings(metadata->getDeviceID());
  }

  bool
  AieProfile_WinImpl::setMetricsSettings(const uint64_t deviceId)
  {
    xrt_core::message::send(severity_level::info, "XRT", "Setting AIE Profile Metrics Settings.");

    int counterId = 0;
    bool runtimeCounters = false;
    // inputs to the DPU kernel
    std::vector<register_data_t> op_profile_data;

    xdp::aie::driver_config meta_config = metadata->getAIEConfigMetadata();

    XAie_Config cfg {
      meta_config.hw_gen,
      meta_config.base_address,
      meta_config.column_shift,
      meta_config.row_shift,
      meta_config.num_rows,
      meta_config.num_columns,
      meta_config.shim_row,
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

    // Get partition columns
    boost::property_tree::ptree aiePartitionPt = xdp::aie::getAIEPartitionInfo(metadata->getHandle());
    // Currently, assuming only one Hw Context is alive at a time
    uint8_t startCol = static_cast<uint8_t>(aiePartitionPt.front().second.get<uint64_t>("start_col"));

    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    auto configChannel0 = metadata->getConfigChannel0();
    for (int module = 0; module < metadata->getNumModules(); ++module) {

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

        auto loc         = XAie_TileLoc(col, row);
        auto startEvents = (type  == module_type::core) ? coreStartEvents[metricSet]
                         : ((type == module_type::dma)  ? memoryStartEvents[metricSet]
                         : ((type == module_type::shim) ? shimStartEvents[metricSet]
                         : memTileStartEvents[metricSet]));
        auto endEvents   = (type  == module_type::core) ? coreEndEvents[metricSet]
                         : ((type == module_type::dma)  ? memoryEndEvents[metricSet]
                         : ((type == module_type::shim) ? shimEndEvents[metricSet]
                         : memTileEndEvents[metricSet]));

        uint8_t numFreeCtr = (type == module_type::dma) ? 2 : static_cast<uint8_t>(startEvents.size());

        auto iter0 = configChannel0.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;

        // Modify events as needed
        aie::profile::modifyEvents(type, subtype, channel0, startEvents, metadata->getHardwareGen());
        endEvents = startEvents;

        aie::profile::configEventSelections(&aieDevInst, loc, type, metricSet, channel0);

        // Request and configure all available counters for this tile
        for (uint8_t i = 0; i < numFreeCtr; i++) {
          auto startEvent    = startEvents.at(i);
          auto endEvent      = endEvents.at(i);
          uint8_t resetEvent = 0;

          // No resource manager - manually manage the counters:
          RC = XAie_PerfCounterReset(&aieDevInst, loc, mod, i);
          if(RC != XAIE_OK) {
            xrt_core::message::send(severity_level::error, "XRT", "AIE Performance Counter Reset Failed.");
            break;
          }
          RC = XAie_PerfCounterControlSet(&aieDevInst, loc, mod, i, startEvent, endEvent);
          if(RC != XAIE_OK) {
            xrt_core::message::send(severity_level::error, "XRT", "AIE Performance Counter Set Failed.");
            break;
          }

          aie::profile::configGroupEvents(&aieDevInst, loc, mod, type, metricSet, startEvent, channel0);
          if (aie::profile::isStreamSwitchPortEvent(startEvent))
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

          std::vector<uint64_t> Regs = regValues.at(type);
          // 25 is column offset and 20 is row offset for IPU
          op_profile_data.emplace_back(register_data_t{Regs[i] + (col << 25) + (row << 20)});

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

    op_size = sizeof(read_register_op_t) + sizeof(register_data_t) * (counterId - 1);
    op = (read_register_op_t*)malloc(op_size);
    op->count = counterId;
    for (size_t i = 0; i < op_profile_data.size(); i++) {
      op->data[i] = op_profile_data[i];
    }
    
    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    if (!transactionHandler->initializeKernel("XDP_KERNEL"))
      return false;

    if (!transactionHandler->submitTransaction(txn_ptr))
      return false;

    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE Profiling Transaction Buffer.");

    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);
    return runtimeCounters;
  }

  // Configure stream switch ports for monitoring purposes
  // NOTE: Used to monitor streams: trace, interfaces, and MEM tiles
  void
  AieProfile_WinImpl::configStreamSwitchPorts(const tile_type& tile, const XAie_LocType& loc,
    const module_type& type, const std::string& metricSet, const uint8_t channel, const XAie_Events startEvent)
  {
    // Hardcoded
    uint8_t rscId = 0;
    uint8_t portnum = aie::profile::getPortNumberFromEvent(startEvent);
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

  void
  AieProfile_WinImpl::poll(const uint32_t index, void* handle)
  {
    if (finishedPoll)
      return;

    if (db->infoAvailable(xdp::info::ml_timeline)) {
      db->broadcast(VPDatabase::MessageType::READ_RECORD_TIMESTAMPS, nullptr);
      xrt_core::message::send(severity_level::debug, "XRT", "Done reading recorded timestamps.");
    }

    auto context = metadata->getHwContext();
    xrt::bo resultBO;
    uint32_t* output = nullptr;
    try {
      resultBO = xrt_core::bo_int::create_bo(context, 0x20000, xrt_core::bo_int::use_type::debug);
      output = resultBO.map<uint32_t*>();
      memset(output, 0, 0x20000);
    } catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to create 128KB buffer for AIE Profile results. Cannot get AIE Profile info. " << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }

    (void)handle;
    double timestamp = xrt_core::time_ns() / 1.0e6;

    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    XAie_AddCustomTxnOp(&aieDevInst, XAIE_IO_CUSTOM_OP_READ_REGS, (void*)op, op_size);
    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    // If we haven't properly initialized the transaction handler, don't poll
    if (!transactionHandler)
      return;
    transactionHandler->setTransactionName("AIE Profile Poll");
    if (!transactionHandler->submitTransaction(txn_ptr))
      return;

    XAie_ClearTransaction(&aieDevInst);

    resultBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    for (uint32_t i = 0; i < op->count; i++) {
      std::stringstream msg;
      msg << "Counter address/values: 0x" << std::hex << op->data[i].address << ": " << std::dec << output[i];
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      std::vector<uint64_t> values = outputValues[i];
      values[5] = static_cast<uint64_t>(output[i]); //write pc value
      db->getDynamicInfo().addAIESample(index, timestamp, values);
    }

    finishedPoll=true;
    free(op);
  }

  void
  AieProfile_WinImpl::freeResources()
  {
    
  }
}  // namespace xdp
