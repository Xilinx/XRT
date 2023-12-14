/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xrt/xrt_kernel.h"

#include "op_types.h"
#include "op_buf.hpp"
#include "op_init.hpp"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_config.h"

// XRT headers
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "core/common/shim/hwctx_handle.h"
#include <windows.h> 

constexpr std::uint64_t CONFIGURE_OPCODE = std::uint64_t{2};

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;

  AieProfile_WinImpl::
  AieProfile_WinImpl(VPDatabase* database,
    std::shared_ptr<AieProfileMetadata> metadata
  )
    : AieProfileImpl(database, metadata)
  {
    auto hwGen = metadata->getHardwareGen();

    mCoreStartEvents = aie::profile::getCoreEventSets(hwGen);
    mCoreEndEvents = mCoreStartEvents;

    mMemoryStartEvents = aie::profile::getMemoryEventSets(hwGen);
    mMemoryEndEvents = mMemoryStartEvents;

    mShimStartEvents = aie::profile::getInterfaceTileEventSets(hwGen);
    mShimEndEvents = mShimStartEvents;

    mMemTileStartEvents = aie::profile::getMemoryTileEventSets();
    mMemTileEndEvents = mMemTileStartEvents;
  }

  void
  AieProfile_WinImpl::
  updateDevice()
  {
    setMetricsSettings(metadata->getDeviceID());
  }

  bool
  AieProfile_WinImpl::
  setMetricsSettings(uint64_t deviceId)
  {
    xrt_core::message::send(severity_level::info, "XRT", "Setting AIE Profile Metrics Settings.");

    int counterId = 0;
    bool runtimeCounters = false;
    // inputs to the DPU kernel
    std::vector<profile_data_t> op_profile_data;

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


    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    auto configChannel0 = metadata->getConfigChannel0();
    for (int module = 0; module < metadata->getNumModules(); ++module) {

      XAie_ModuleType mod = aie::profile::getFalModuleType(module);
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : metadata->getConfigMetrics(module)) {
        int numCounters  = 0;

        auto tile = tileMetric.first;
        auto row  = tile.row;
        auto col  = tile.col;
        auto subtype = tile.subtype;
        auto type = aie::getModuleType(row, metadata->getAIETileRowOffset());

        if (!isValidType(type, mod))
          continue;

        auto& metricSet  = tileMetric.second;
        auto loc         = XAie_TileLoc(static_cast<uint8_t>(col), static_cast<uint8_t>(row));
        auto startEvents = (type  == module_type::core) ? mCoreStartEvents[metricSet]
                         : ((type == module_type::dma)  ? mMemoryStartEvents[metricSet]
                         : ((type == module_type::shim) ? mShimStartEvents[metricSet]
                         : mMemTileStartEvents[metricSet]));
        auto endEvents   = (type  == module_type::core) ? mCoreEndEvents[metricSet]
                         : ((type == module_type::dma)  ? mMemoryEndEvents[metricSet]
                         : ((type == module_type::shim) ? mShimEndEvents[metricSet]
                         : mMemTileEndEvents[metricSet]));

        uint8_t numFreeCtr = (type == module_type::dma) ? 2 : static_cast<uint8_t>(startEvents.size());

        auto iter0 = configChannel0.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;

        // Modify events as needed
        aie::profile::modifyEvents(type, subtype, channel0, startEvents, metadata->getHardwareGen());
        endEvents = startEvents;

        configEventSelections(loc, type, metricSet, channel0);

        // Request and configure all available counters for this tile
        for (uint8_t i = 0; i < numFreeCtr; i++) {
          auto startEvent    = startEvents.at(i);
          auto endEvent      = endEvents.at(i);
          uint8_t resetEvent = 0;

          // No resource manager - manually manage the counters:
          RC = XAie_PerfCounterReset(&aieDevInst, loc, mod, i);
          if(RC != XAIE_OK) break;
          RC = XAie_PerfCounterControlSet(&aieDevInst, loc, mod, i, startEvent, endEvent);
          if(RC != XAIE_OK) break;

          configGroupEvents(loc, mod, startEvent, metricSet, channel0);
          if (aie::profile::isStreamSwitchPortEvent(startEvent))
            configStreamSwitchPorts(tileMetric.first, loc, type, metricSet, channel0);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
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

          std::vector<uint64_t> Regs = regValues[type];
          // 25 is column offset and 20 is row offset for IPU
          op_profile_data.emplace_back(profile_data_t{Regs[i] + (col << 25) + (row << 20)});

          std::vector<uint64_t> values;
          values.insert(values.end(), {col, row, phyStartEvent, phyEndEvent, resetEvent, 0, 0, payload});
          outputValues.push_back(values);

          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," 
            << row << ") using metric set " << metricSet << " and channel " << (int)channel0 << ".";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        // numTileCounters[numCounters]++;
      }
      runtimeCounters = true;

    } // modules

    op_size = sizeof(aie_profile_op_t) + sizeof(profile_data_t) * (counterId - 1);
    op = (aie_profile_op_t*)malloc(op_size);
    op->count = counterId;
    for (int i = 0; i < op_profile_data.size(); i++) {
      op->profile_data[i] = op_profile_data[i];
    }
    
    auto context = metadata->getHwContext();

    try {
      mKernel = xrt::kernel(context, "XDP_KERNEL");  
    } catch (std::exception &e){
      std::stringstream msg;
      msg << "Unable to find XDP_KERNEL kernel from hardware context. Failed to configure AIE Profile." << e.what() ;
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return false;
    }

    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    op_buf instr_buf;
    instr_buf.addOP(transaction_op(txn_ptr));
    xrt::bo instr_bo;

    // Configuration bo
    try {
      instr_bo = xrt::bo(context.get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, mKernel.group_id(1));
    } catch (std::exception &e){
      std::stringstream msg;
      msg << "Unable to create instruction buffer for AIE Profile transaction. Not configuring AIE Profile. " << e.what() << std::endl;
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return false;
    }

    instr_bo.write(instr_buf.ibuf_.data());
    instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    auto run = mKernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0);
    run.wait2();

    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE Profiling Transaction Buffer.");

    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);
    return runtimeCounters;
  }

  void
  AieProfile_WinImpl::
  configGroupEvents(
    const XAie_LocType loc, const XAie_ModuleType mod,
    const XAie_Events event, const std::string& metricSet, uint8_t channel)
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
    else if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_PL) {
      uint32_t bitMask = aie::isInputSet(type, metricSet) 
          ? ((channel == 0) ? GROUP_SHIM_S2MM0_STALL_MASK : GROUP_SHIM_S2MM1_STALL_MASK)
          : ((channel == 0) ? GROUP_SHIM_MM2S0_STALL_MASK : GROUP_SHIM_MM2S1_STALL_MASK);
      XAie_EventGroupControl(aieDevInst, loc, mod, event, bitMask);
  }

  // Configure stream switch ports for monitoring purposes
  // NOTE: Used to monitor streams: trace, interfaces, and MEM tiles
  void
  AieProfile_WinImpl::
  configStreamSwitchPorts( 
    const tile_type& tile, const XAie_LocType& loc,
    const module_type& type, const std::string& metricSet, uint8_t channel)
  {
    // Hardcoded
    uint8_t rscId = 0;
    // AIE Tiles (e.g., trace streams)
    if (type == module_type::core) {
      auto slaveOrMaster = (metricSet.find("mm2s") != std::string::npos) ?
        XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      XAie_EventSelectStrmPort(&aieDevInst, loc, rscId, slaveOrMaster, DMA, channel);
      std::stringstream msg;
      msg << "Configured core tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") << " stream switch ports for metricset " << metricSet << " and channel " << (int)channel << ".";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      return;
    }

    // Interface tiles (e.g., PLIO, GMIO)
    if (type == module_type::shim) {
      // Grab slave/master and stream ID
      // NOTE: stored in getTilesForProfiling() above
      auto slaveOrMaster = (tile.itr_mem_col == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      auto streamPortId  = static_cast<uint8_t>(tile.itr_mem_row);
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
  AieProfile_WinImpl::
  configEventSelections(
    const XAie_LocType loc, const module_type type,
    const std::string metricSet, const uint8_t channel0) 
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = aie::isInputSet(type,metricSet) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(&aieDevInst, loc, 0, dmaDir, channel0);

    std::stringstream msg;
    msg << "Configured mem tile " << (aie::isInputSet(type,metricSet) ? "S2MM" : "MM2S") << "DMA  for metricset " << metricSet << " and channel " << (int)channel0 << ".";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
  }


  bool
  AieProfile_WinImpl::
  isValidType(module_type type, XAie_ModuleType mod)
  {
    if ((mod == XAIE_CORE_MOD) && ((type == module_type::core) 
        || (type == module_type::dma)))
      return true;
    if ((mod == XAIE_MEM_MOD) && ((type == module_type::dma) 
        || (type == module_type::mem_tile)))
      return true;
    if ((mod == XAIE_PL_MOD) && (type == module_type::shim)) 
      return true;
    return false;
  }

  void
  AieProfile_WinImpl::
  poll(uint32_t index, void* handle)
  {
    if (finishedPoll)
      return;

    (void)handle;
    double timestamp = xrt_core::time_ns() / 1.0e6;
    auto context = metadata->getHwContext();

    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);
    // Profiling is 3rd custom OP
    XAie_RequestCustomTxnOp(&aieDevInst);
    XAie_RequestCustomTxnOp(&aieDevInst);
    
    auto read_op_code_ = XAie_RequestCustomTxnOp(&aieDevInst);

    XAie_AddCustomTxnOp(&aieDevInst, (uint8_t)read_op_code_, (void*)op, op_size);
    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    op_buf instr_buf;
    instr_buf.addOP(transaction_op(txn_ptr));

    // this BO stores polling data and custom instructions
    xrt::bo instr_bo;
    try {
      instr_bo = xrt::bo(context.get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, mKernel.group_id(1));
    } catch (std::exception &e){
      std::stringstream msg;
      msg << "Unable to create the instruction buffer for polling during AIE Profile. " << e.what() << std::endl;
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return;
    }

    instr_bo.write(instr_buf.ibuf_.data());
    instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    auto run = mKernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0);
    try {
      run.wait2();
    } catch (std::exception &e) {
      std::stringstream msg;
      msg << "Unable to successfully execute AIE Profile polling kernel. " << e.what() << std::endl;
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return;
    }

    XAie_ClearTransaction(&aieDevInst);

    static constexpr uint32_t size_4K   = 0x1000;
    static constexpr uint32_t offset_3K = 0x0C00;

    // results BO syncs profile result from device
    xrt::bo result_bo;
    try {
      result_bo = xrt::bo(context.get_device(), size_4K, XCL_BO_FLAGS_CACHEABLE, mKernel.group_id(1));
    } catch (std::exception &e) {
      std::stringstream msg;
      msg << "Unable to create result buffer for AIE Profle. Cannot get AIE Profile Info." << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }

    auto result_bo_map = result_bo.map<uint8_t*>();
    result_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    uint32_t* output = reinterpret_cast<uint32_t*>(result_bo_map+offset_3K);

    for (uint32_t i = 0; i < op->count; i++) {
      std::stringstream msg;
      msg << "Counter address/values: 0x" << std::hex << op->profile_data[i].perf_address << ": " << std::dec << output[i];
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      std::vector<uint64_t> values = outputValues[i];
      values[5] = static_cast<uint64_t>(output[i]); //write pc value
      db->getDynamicInfo().addAIESample(index, timestamp, values);
    }

    finishedPoll=true;
    free(op);
  }

  void
  AieProfile_WinImpl::
  freeResources()
  {
    
  }
}  // namespace xdp
