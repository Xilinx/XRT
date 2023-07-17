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

#define XDP_SOURCE

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

// XRT headers
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "core/common/shim/hwctx_handle.h"
#include <windows.h> 

constexpr std::uint64_t CONFIGURE_OPCODE = std::uint64_t{2};
constexpr std::uint64_t PROFILE_OPCODE = std::uint64_t{3};


namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;

  typedef struct {
    uint64_t perf_counters[3];
    char msg[512];
  } aie_read_op_t;

  AieProfile_WinImpl::AieProfile_WinImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata)
  {

    mCounterBases = {
      {module_type::core,     static_cast<uint16_t>(0)},
      {module_type::dma,      BASE_MEMORY_COUNTER},
      {module_type::shim,     BASE_SHIM_COUNTER},
      {module_type::mem_tile, BASE_MEM_TILE_COUNTER}
    };

    // **** Core Module Counters ****
    mCoreStartEvents = {
      {"heat_map",                {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                                   XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stalls",                  {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                                   XAIE_EVENT_LOCK_STALL_CORE,           XAIE_EVENT_CASCADE_STALL_CORE}},
      {"execution",               {XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_INSTR_LOAD_CORE,
                                   XAIE_EVENT_INSTR_STORE_CORE,          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"floating_point",          {XAIE_EVENT_FP_HUGE_CORE,              XAIE_EVENT_INT_FP_0_CORE, 
                                   XAIE_EVENT_FP_INVALID_CORE,           XAIE_EVENT_FP_INF_CORE}},
      {"stream_put_get",          {XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
                                   XAIE_EVENT_INSTR_STREAM_GET_CORE,     XAIE_EVENT_INSTR_STREAM_PUT_CORE}},
      {"write_throughputs",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_PUT_CORE,
                                   XAIE_EVENT_INSTR_CASCADE_PUT_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"read_throughputs",        {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_GET_CORE,
                                   XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"input_throughputs",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_PORT_RUNNING_0_CORE}},
      {"output_throughputs",      {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_PORT_RUNNING_0_CORE}}
    };

    mCoreEndEvents = mCoreStartEvents;

    // **** Memory Module Counters ****
    mMemoryStartEvents = {
      {"conflicts",               {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {"dma_locks",               {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}},
      {"write_throughputs",       {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
                                   XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}},
      {"read_throughputs",        {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
                                   XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}},
      {"input_throughputs",       {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,
                                   XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM,
                                   XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM,
                                   XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM}},
      {"output_throughputs",      {XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM,
                                   XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM,
                                   XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_MEM,
                                   XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_MEM}}
    };
    mMemoryEndEvents = mMemoryStartEvents;

    // **** Interface Tile Counters ****
    mShimStartEvents = {
      {"input_throughputs",       {XAIE_EVENT_GROUP_DMA_ACTIVITY_PL, XAIE_EVENT_PORT_RUNNING_0_PL}},
      {"output_throughputs",      {XAIE_EVENT_GROUP_DMA_ACTIVITY_PL, XAIE_EVENT_PORT_RUNNING_0_PL}},
      {"packets",                 {XAIE_EVENT_PORT_TLAST_0_PL,   XAIE_EVENT_PORT_TLAST_1_PL}}
    };
    mShimEndEvents = mShimStartEvents;

    // **** MEM Tile Counters ****
    mMemTileStartEvents = {
      {"input_channels",          {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE,   
                                   XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE}},
      {"input_channels_details",  {XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE, 
                                   XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE}},
      {"output_channels",         {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE,   
                                   XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE}},
      {"output_channels_details", {XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE, 
                                   XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE}},
      {"memory_stats",            {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM_TILE,
                                   XAIE_EVENT_GROUP_ERRORS_MEM_TILE,
                                   XAIE_EVENT_GROUP_LOCK_MEM_TILE,
                                   XAIE_EVENT_GROUP_WATCHPOINT_MEM_TILE}},
      {"input_throughputs",        {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}},
      {"output_throughputs",       {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}}
    };
    mMemTileEndEvents = mMemTileStartEvents;

  }

  void AieProfile_WinImpl::updateDevice()
  {
    setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());
  }



  bool AieProfile_WinImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    std::cout << "reached setmetricssettings: " << deviceId  << handle << std::endl;

    int RC = XAIE_OK;
    XAie_Config config { 
        XAIE_DEV_GEN_AIE2IPU,                                 //xaie_dev_gen_aie
        metadata->getAIEConfigMetadata("base_address").get_value<uint64_t>(),        //xaie_base_addr
        metadata->getAIEConfigMetadata("column_shift").get_value<uint8_t>(),         //xaie_col_shift
        metadata->getAIEConfigMetadata("row_shift").get_value<uint8_t>(),            //xaie_row_shift
        metadata->getAIEConfigMetadata("num_rows").get_value<uint8_t>(),             //xaie_num_rows, 
        metadata->getAIEConfigMetadata("num_columns").get_value<uint8_t>(),          //xaie_num_cols, 
        metadata->getAIEConfigMetadata("shim_row").get_value<uint8_t>(),             //xaie_shim_row,
        metadata->getAIEConfigMetadata("reserved_row_start").get_value<uint8_t>(),   //xaie_res_tile_row_start, 
        metadata->getAIEConfigMetadata("reserved_num_rows").get_value<uint8_t>(),    //xaie_res_tile_num_rows,
        metadata->getAIEConfigMetadata("aie_tile_row_start").get_value<uint8_t>(),   //xaie_aie_tile_row_start, 
        metadata->getAIEConfigMetadata("aie_tile_num_rows").get_value<uint8_t>(),    //xaie_aie_tile_num_rows
        {0}                                                   // PartProp
    };

    RC = XAie_CfgInitialize(&aieDevInst, &config);
    if(RC != XAIE_OK) {
       xrt_core::message::send(severity_level::warning, "XRT", "Driver Initialization Failed.");
       return false;
    }

    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);


    int counterId = 0;
    bool runtimeCounters = false;
    std::vector<XAie_ModuleType> falModuleTypes = {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD, XAIE_MEM_MOD};

    auto configChannel0 = metadata->getConfigChannel0();
    //auto configChannel1 = metadata->getConfigChannel1();

    for (int module = 0; module < metadata->getNumModules(); ++module) {
      // int numTileCounters[metadata->getNumCountersMod(module)+1] = {0};
      XAie_ModuleType mod = falModuleTypes[module];
      std::cout << "Module: " << module << std::endl;
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : metadata->getConfigMetrics(module)) {
    
        auto tile        = tileMetric.first;
        auto col         = tile.col;
        auto row         = tile.row;
        std::cout << "Col, Row: " << col << " " << row << std::endl;
        auto type        = getModuleType(row, mod);
        if (!isValidType(type, mod))
          continue;

        auto& metricSet  = tileMetric.second;
        auto loc         = XAie_TileLoc(static_cast<uint8_t>(col), static_cast<uint8_t>(row));
        // auto& xaieTile   = aieDevice->tile(col, row);
        // auto xaieModule  = (mod == XAIE_CORE_MOD) ? xaieTile.core()
        //                  : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
        //                  : xaieTile.pl());

        auto startEvents = (type  == module_type::core) ? mCoreStartEvents[metricSet]
                         : ((type == module_type::dma)  ? mMemoryStartEvents[metricSet]
                         : ((type == module_type::shim) ? mShimStartEvents[metricSet]
                         : mMemTileStartEvents[metricSet]));
        auto endEvents   = (type  == module_type::core) ? mCoreEndEvents[metricSet]
                         : ((type == module_type::dma)  ? mMemoryEndEvents[metricSet]
                         : ((type == module_type::shim) ? mShimEndEvents[metricSet]
                         : mMemTileEndEvents[metricSet]));

        int numCounters  = 0;
        uint8_t numFreeCtr  = (type == module_type::dma || type == module_type::shim) ? 2 : 4;
        if (startEvents.size() < numFreeCtr)
          numFreeCtr = static_cast<uint8_t>(startEvents.size());

        std::cout << "Number of free Counters: " << (int)numFreeCtr << std::endl;

        // // Specify Sel0/Sel1 for MEM tile events 21-44
        auto iter0 = configChannel0.find(tile);
        // auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        std::cout << "Channel was specified: Channel: " << (int) channel0 << std::endl;
        // uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        configEventSelections(loc, type, metricSet, channel0);

        // Request and configure all available counters for this tile
        for (uint8_t i=0; i < numFreeCtr; ++i) {
          auto startEvent    = startEvents.at(i);
          auto endEvent      = endEvents.at(i);
          uint8_t resetEvent = 0;

          //Check if we're the memory module: Then set the correct Events based on channel
          if (type == module_type::dma) {
            if (channel0 != 0 && metricSet.find("input") != std::string::npos){
              startEvent = XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM;
              endEvent = XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM;
            } else if (channel0 != 0 && metricSet.find("output") != std::string::npos) {
              startEvent = XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_MEM;
              endEvent = XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_MEM;
            }
          }

          //No resource manager - manually manage the counters:
          RC = XAie_PerfCounterReset(&aieDevInst, loc, mod, i);
          if(RC != XAIE_OK) break;
          RC = XAie_PerfCounterControlSet(&aieDevInst, loc, mod, i, startEvent, endEvent);
          if(RC != XAIE_OK) break;

          //Configure group Events for Memory/Shim Modules. 
          configGroupEvents(loc, mod, startEvent, metricSet, channel0);
          if (!isStreamSwitchPortEvent(startEvent))
            configStreamSwitchPorts(tileMetric.first, loc, type, metricSet, channel0);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(&aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = tmpStart + mCounterBases[type];
          uint16_t phyEndEvent   = tmpEnd   + mCounterBases[type];
          auto payload = getCounterPayload(tileMetric.first, type, col, row, 
                                           startEvent, metricSet, channel0);

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
                phyStartEvent, phyEndEvent, resetEvent, payload, metadata->getClockFreqMhz(), 
                metadata->getModuleName(module), counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," 
            << row << ") using metric set " << metricSet << ".";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        // numTileCounters[numCounters]++;
      }

       // // Report counters reserved per tile
      // {
      //   std::stringstream msg;
      //   msg << "AIE profile counters reserved in " << metadata->getModuleName(module) << " - ";
      //   for (int n=0; n <= metadata->getNumCountersMod(module); ++n) {
      //     if (numTileCounters[n] == 0) continue;
      //     msg << n << ": " << numTileCounters[n] << " tiles";
      //     if (n != metadata->getNumCountersMod(module)) msg << ", ";

      //     (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
      //   }
      //   xrt_core::message::send(severity_level::info, "XRT", msg.str());
      // }
      runtimeCounters = true;
  

    } // modules
    
	  uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    XAie_TxnHeader *hdr = (XAie_TxnHeader *)txn_ptr;
    std::cout << "Txn Size: " << hdr->TxnSize << " bytes" << std::endl;

    xrt::hw_context* context;
    std::cout << "Reached Context creation!" << std::endl;
    context = static_cast<xrt::hw_context*>(metadata->getHwContext());
    std::cout << "Context Ptr Value: " << context << std::endl;
    std::cout << "Creating kernel!" << std::endl;
    xrt::kernel kernel;
    try {
      kernel = xrt::kernel(*context, "DPU_1x4_NEW");  
    } catch (std::exception &e){
      std::cout << "caught exception: " << e.what() << std::endl;
      return false;
    } 
    std::cout << "Reached kernel creation!" << std::endl;

    // // Create the DPU kernel
 
    // std::cout << "About to create device:" << std::endl;
    // std::cout << "Getting Handle:" << std::endl;
    // std::cout << "handle: " << metadata->getHandle() << std::endl;
    // auto spdevice = xrt_core::get_userpf_device(metadata->getHandle());
    // auto device = xrt::device(spdevice);
    // std::cout << "failed to continue:" << std::endl;
    // auto uuid = device.get_xclbin_uuid();

    op_buf instr_buf;
    instr_buf.addOP(transaction_op(txn_ptr));
    std::cout << "Added Op" << std::endl;
    xrt::bo instr_bo;

    try {
      instr_bo = xrt::bo(context->get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
      std::cout << "Created instruction bo" << std::endl;  
    } catch (std::exception &e){
      std::cout << "caught exception: " << e.what() << std::endl;
      return false;
    } 

    instr_bo.write(instr_buf.ibuf_.data());
    instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "About to run the kernel!" << std::endl;
    // Sleep(20000);
    // auto run = kernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0, 0);
    auto run = kernel(CONFIGURE_OPCODE, 0, 0, 0, 0, instr_bo, instr_bo.size()/sizeof(int), 0);
    std::cout << "Finished Running Kernel!" << std::endl;
    // Sleep(20000);
    run.wait2();

    std::cout << "Finished running the kernel!" << std::endl;

    //Schedule PS kernel
    return runtimeCounters;
  }

  bool AieProfile_WinImpl::isStreamSwitchPortEvent(const XAie_Events event)
  {
    // AIE tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_CORE) 
        && (event < XAIE_EVENT_GROUP_BROADCAST_CORE))
      return true;
    // Interface tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_PL) 
        && (event < XAIE_EVENT_GROUP_BROADCAST_A_PL))
      return true;
    // MEM tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_MEM_TILE) 
        && (event < XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM_TILE))
      return true;

    return false;
  }

  void AieProfile_WinImpl::configGroupEvents(const XAie_LocType loc,
                                             const XAie_ModuleType mod,
                                             const XAie_Events event, 
                                             std::string metricSet, 
                                             uint8_t channel)
  {
    // Set masks for group events
    // NOTE: Group error enable register is blocked, so ignoring
    std::cout << "Got to config Group Events! " << std::endl; 
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM)
      XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_DMA_MASK);
    else if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_PL)
      // Pass channel and set correct mask 
      if (metricSet.find("input") != std::string::npos)
        if (channel == 0)
          XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_SHIM_S2MM0_STALL_MASK);
        else 
          XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_SHIM_S2MM1_STALL_MASK);
      else 
        if (channel == 0)
          XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_SHIM_MM2S0_STALL_MASK);
        else 
          XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_SHIM_MM2S1_STALL_MASK);
    else if (event == XAIE_EVENT_GROUP_LOCK_MEM)
      XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_LOCK_MASK);
    else if (event == XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM)
      XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_CONFLICT_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE)
      XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_CORE_PROGRAM_FLOW_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_STALL_CORE)
      XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_CORE_STALL_MASK);
  }

  // Configure stream switch ports for monitoring purposes
  // NOTE: Used to monitor streams: trace, interfaces, and MEM tiles
  void AieProfile_WinImpl::configStreamSwitchPorts(const tile_type& tile,
                                                    const XAie_LocType loc,
                                                    const module_type type,
                                                    const std::string metricSet,
                                                    const uint8_t channel)
  {
    // Hardcoded
    uint8_t rscId = 0;

    // AIE Tiles (e.g., trace streams)
    if (type == module_type::core) {
      auto slaveOrMaster = (metricSet.find("output") != std::string::npos) ?
        XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      XAie_EventSelectStrmPort(&aieDevInst, loc, rscId, slaveOrMaster, DMA, channel);
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
      return;
    }

    if (type == module_type::mem_tile) {
      auto slaveOrMaster = (metricSet.find("output") != std::string::npos) ?
        XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      XAie_EventSelectStrmPort(&aieDevInst, loc, rscId, slaveOrMaster, DMA, channel);
    }
  }

  void 
  AieProfile_WinImpl::configEventSelections(const XAie_LocType loc,
                                             const module_type type,
                                             const std::string metricSet,
                                             const uint8_t channel0) 
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = (metricSet.find("input") != std::string::npos) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(&aieDevInst, loc, 0, dmaDir, channel0);
    // XAie_EventSelectDmaChannel(aieDevInst, loc, 1, dmaDir, channel1);
  }


  module_type 
  AieProfile_WinImpl::getModuleType(uint16_t absRow, XAie_ModuleType mod)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < metadata->getAIETileRowOffset())
      return module_type::mem_tile;
    return ((mod == XAIE_CORE_MOD) ? module_type::core : module_type::dma);
  }

  bool AieProfile_WinImpl::isValidType(module_type type, XAie_ModuleType mod)
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



  uint32_t 
  AieProfile_WinImpl::getCounterPayload( const tile_type& tile, 
                                         const module_type type, 
                                         uint16_t column, 
                                         uint16_t row, 
                                         XAie_Events startEvent, 
                                         const std::string metricSet,
                                         const uint8_t channel)
  {
    // 1. Stream IDs for interface tiles
    if (type == module_type::shim) {
      // NOTE: value = ((master or slave) << 8) & (stream ID)
      return ((tile.itr_mem_col << 8) | tile.itr_mem_row);
    }

    // 2. Channel IDs for MEM tiles
    if (type == module_type::mem_tile) {
      // NOTE: value = ((master or slave) << 8) & (channel ID)
      uint8_t isMaster = (metricSet.find("input") != std::string::npos) ? 1 : 0;
      return ((isMaster << 8) | channel);
    }

    // 3. DMA BD sizes for AIE tiles
    if ((startEvent != XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM))
      return 0;

    uint32_t payloadValue = 0;

    constexpr int NUM_BDS = 8;
    constexpr uint32_t BYTES_PER_WORD_PROFILE = 4;
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

    auto tileOffset = _XAie_GetTileAddr(&aieDevInst, static_cast<uint8_t>(row), static_cast<uint8_t>(column));
    for (int bd = 0; bd < NUM_BDS; ++bd) {
      uint32_t regValue = 0;
      XAie_Read32(&aieDevInst, tileOffset + offsets[bd], &regValue);
      
      if (regValue & valids[bd]) {
        uint32_t bdBytes = BYTES_PER_WORD_PROFILE * (((regValue >> lsbs[bd]) & masks[bd]) + ACTUAL_OFFSET);
        payloadValue = std::max(bdBytes, payloadValue);
      }
    }

    return payloadValue;
  }
  

  void AieProfile_WinImpl::poll(uint32_t index, void* handle)
  {
    std::cout << "Polling! " << index << handle << std::endl;
    // For now, we are waiting for a way to retreive polling information from
    // the AIE.

    xrt::hw_context* context;
    std::cout << "Reached Context creation!" << std::endl;
    context = static_cast<xrt::hw_context*>(metadata->getHwContext());
    std::cout << "Context Ptr Value: " << context << std::endl;
    std::cout << "Creating kernel!" << std::endl;
    xrt::kernel kernel;
    try {
      // std::cout << "UUID: " << context->get_xclbin_uuid().to_string() << std::endl;
      std::cout << "Break!" << std::endl;
      kernel = xrt::kernel(*context, "DPU_1x4_NEW");  
    } catch (std::exception &e){
      std::cout << "caught exception: " << e.what() << std::endl;
      return;
    } 
    std::cout << "Reached kernel creation!" << std::endl;

    xrt::bo input_bo;

    try {
      input_bo = xrt::bo(context->get_device(), 4096, XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
      std::cout << "Created instruction bo" << std::endl;  
    } catch (std::exception &e){
      std::cout << "caught exception: " << e.what() << std::endl;
      return;
    }

    XAie_Config config { 
        XAIE_DEV_GEN_AIE2IPU,                                 //xaie_dev_gen_aie
        metadata->getAIEConfigMetadata("base_address").get_value<uint64_t>(),        //xaie_base_addr
        metadata->getAIEConfigMetadata("column_shift").get_value<uint8_t>(),         //xaie_col_shift
        metadata->getAIEConfigMetadata("row_shift").get_value<uint8_t>(),            //xaie_row_shift
        metadata->getAIEConfigMetadata("num_rows").get_value<uint8_t>(),             //xaie_num_rows, 
        metadata->getAIEConfigMetadata("num_columns").get_value<uint8_t>(),          //xaie_num_cols, 
        metadata->getAIEConfigMetadata("shim_row").get_value<uint8_t>(),             //xaie_shim_row,
        metadata->getAIEConfigMetadata("reserved_row_start").get_value<uint8_t>(),   //xaie_res_tile_row_start, 
        metadata->getAIEConfigMetadata("reserved_num_rows").get_value<uint8_t>(),    //xaie_res_tile_num_rows,
        metadata->getAIEConfigMetadata("aie_tile_row_start").get_value<uint8_t>(),   //xaie_aie_tile_row_start, 
        metadata->getAIEConfigMetadata("aie_tile_num_rows").get_value<uint8_t>(),    //xaie_aie_tile_num_rows
        {0}                                                   // PartProp
    };

    auto RC = XAie_CfgInitialize(&aieDevInst, &config);
    if(RC != XAIE_OK) {
       xrt_core::message::send(severity_level::warning, "XRT", "Driver Initialization Failed.");
       return;
    }

    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);
    std::string msg = "hello profile!";
    uint8_t col = 2;
    uint8_t row = 2;
        aie_read_op_t op; 
        auto read_op_code_ = XAie_RequestCustomTxnOp(&aieDevInst) + 2;
        std::cout << "got custom op code :" << read_op_code_ << std::endl;
        std::vector<uint64_t> Regs;
        Regs.push_back(0x31520);
        Regs.push_back(0x31524);
        Regs.push_back(0x31528);

        // Support only three performance counters for now.
        // 1. 1st performance counters counts the number of cycles to execute from start to end
        // 2. Num of times start PC was executed
        // 3. Num of times end PC was executed
        for (int i = 0; i < 3; i++) {
            // 25 is column offset and 20 is row offset for IPU
          op.perf_counters[i] = Regs[i] + (col << 25) + (row << 20);
        }
        if (msg.length() > 512) {
          msg = msg.substr(0, 512);
        }
        msg.copy(op.msg, msg.length());
    XAie_AddCustomTxnOp(&aieDevInst, (uint8_t)read_op_code_, (void*)&op, sizeof(op));
    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    XAie_TxnHeader *hdr = (XAie_TxnHeader *)txn_ptr;
    std::cout << "Poll Txn Size: " << hdr->TxnSize << " bytes" << std::endl;
    op_buf instr_buf;
    instr_buf.addOP(transaction_op(txn_ptr));
    input_bo.write(instr_buf.ibuf_.data());

    //auto inbo_map = input_bo.map<uint32_t*>();
    //memset(inbo_map, 0, 4096);
    input_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "About to run the kernel!" << std::endl;
    // auto run = kernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0, 0);
    auto run = kernel(CONFIGURE_OPCODE, 0, 0, 0, 0, input_bo, 4096, 0);
    std::cout << "Finished Running Kernel!" << std::endl;

    try {
      run.wait2();
    } catch (std::exception &e) {
      std::cout << "Caught exception: " << e.what() << std::endl;
    }
    
    //std::cout << "Not About to Sync" << std::endl;

    //input_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    //std::cout << "Synced Finished" << std::endl;
    //for (int i = 0; i < 32; i++) {
    //  std::cout << "Output Value: " << inbo_map[i] << std::endl;
    //}
    
    std::cout << "Finished running the kernel!" << std::endl;

    return;
  }

  void AieProfile_WinImpl::freeResources()
  {
    // TODO - if there are any resources we need to free after the application
    // is complete, it must be done here.
  }
}  // namespace xdp
