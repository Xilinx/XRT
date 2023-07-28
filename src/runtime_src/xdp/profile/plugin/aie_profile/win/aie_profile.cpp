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
    uint64_t perf_address[256];
    uint32_t perf_value[256];
  } aie_profile_op_t;

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
      {"s2mm_throughputs",       {XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_PORT_RUNNING_0_CORE, XAIE_EVENT_PORT_IDLE_0_CORE, XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"mm2s_throughputs",      {XAIE_EVENT_ACTIVE_CORE,  XAIE_EVENT_PORT_RUNNING_0_CORE, XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_PORT_RUNNING_0_CORE}}
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
      {"s2mm_throughputs",       {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,
                                   XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM,
                                   XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM,
                                   XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM}},
      {"mm2s_throughputs",      {XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM,
                                   XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM,
                                   XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_MEM,
                                   XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_MEM}}
    };
    mMemoryEndEvents = mMemoryStartEvents;

    // **** Interface Tile Counters ****
    mShimStartEvents = {
      {"s2mm_throughputs",       {XAIE_EVENT_TRUE_PL, XAIE_EVENT_PORT_RUNNING_0_PL}},
      //{"s2mm_throughputs",       {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL, XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL}},
      {"mm2s_throughputs",      {XAIE_EVENT_GROUP_DMA_ACTIVITY_PL, XAIE_EVENT_PORT_RUNNING_0_PL}},
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
      // {"s2mm_throughputs",        {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE,
      //                              XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE,
      //                              XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE,
      //                              XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}},
      {"s2mm_throughputs",        {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_START_TASK_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}},
      //{"mm2s_throughputs",       {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
      //                             XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE,
      //                             XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE,
      //                             XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE}}
      {"mm2s_throughputs",       {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_DMA_MM2S_SEL0_START_TASK_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE,
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

     std::map<module_type, std::vector<uint64_t>> regValues {
      {module_type::core, {0x31520,0x31524,0x31528,0x3152C}}, 
      {module_type::dma, {0x11020,0x11024}}, 
      {module_type::shim, {0x31020, 0x31024}}, 
      {module_type::mem_tile, {0x91020,0x91024,0x91028,0x9102C}}, 
    };

      std::size_t totalSize = sizeof(aie_profile_op_t);
      aie_profile_op_t* op = (aie_profile_op_t*) malloc(totalSize);

      for (int i =0; i < 256; i++) {
        op->perf_address[i] = 0;
        op->perf_value[i] = 0;
      }

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


    std::cout << "PRINTING CHANNEL INFO" << std::endl;
    for (auto& t: configChannel0) { 
      std::cout << "Col, Row:  " << t.first.col << " " << t.first.row << std::endl;
      std::cout << "Channel: " << (int)t.second << std::endl;
    }
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

        if (type == module_type::mem_tile) {
          col = 0;
        }

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
            if (channel0 != 0 && (metricSet.find("input") != std::string::npos || metricSet.find("s2mm") != std::string::npos)){
              startEvent = XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM;
              endEvent = XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM;
            } else if (channel0 != 0 && (metricSet.find("output") != std::string::npos || metricSet.find("mm2s") != std::string::npos)) {
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


          std::vector<uint64_t> Regs = regValues[type];
          // 25 is column offset and 20 is row offset for IPU
          op->perf_address[counterId] = Regs[i] + (col << 25) + (row << 20);
          std::cout << "Perf address: " << std::hex << op->perf_address[counterId] << std::endl;
        
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

    std::cout << "Reached Context creation!" << std::endl;
    auto context = metadata->getHwContext();
    std::cout << "Creating kernel!" << std::endl;
    try {
      mKernel = xrt::kernel(context, "DPU_1x4_PROFILE");  
    } catch (std::exception &e){
      std::cout << "caught exception: " << e.what() << std::endl;
      return false;
    } 
    std::cout << "Reached kernel creation!" << std::endl;

    op_buf instr_buf;
    instr_buf.addOP(transaction_op(txn_ptr));
    std::cout << "Added Op" << std::endl;
    xrt::bo instr_bo;

    // Configuration bo
    try {
      instr_bo = xrt::bo(context.get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, mKernel.group_id(1));
      std::cout << "Created instruction bo" << std::endl;  
    } catch (std::exception &e){
      std::cout << "caught exception: " << e.what() << std::endl;
      return false;
    }

    // Create polling bo
    try {
      input_bo = xrt::bo(context.get_device(), 8192, XCL_BO_FLAGS_NONE, mKernel.group_id(3));
      std::cout << "Created input bo" << std::endl;  
    } catch (std::exception &e){
      std::cout << "caught exception: " << e.what() << std::endl;
      return false;
    }

    uint8_t* input = reinterpret_cast<uint8_t*>(op);
    auto input_bo_map = input_bo.map<uint8_t*>();
    // std::fill(input_bo_map, inbo_map + 8192, 0);

    std::memcpy(input_bo_map, input, 8192);


    instr_bo.write(instr_buf.ibuf_.data());
    instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    input_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, 8192, 0);

    std::cout << "About to run the kernel!" << std::endl;

    auto run = mKernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), input_bo, totalSize, 0, 0, 0, 0);
    std::cout << "Finished Running Kernel!" << std::endl;
    run.wait2();

    std::cout << "Finished running the kernel!" << std::endl;

    //Schedule PS kernel
    free(op);
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
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_PL)
      std::cout << "reached EVENT GROUP_DMA_ACTIVITY_PL" << std::endl;

    std::cout << "Got to config Group Events! " << std::endl; 
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM)
      XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_DMA_MASK);
    else if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_PL)
      // Pass channel and set correct mask 
      if (metricSet.find("input") != std::string::npos  || metricSet.find("s2mm") != std::string::npos)
        if (channel == 0)
          XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_SHIM_S2MM0_STALL_MASK);
        else 
          XAie_EventGroupControl(&aieDevInst, loc, mod, event, GROUP_SHIM_S2MM1_STALL_MASK);
      else 
        if (channel == 2)
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
    std::cout << "CONFIGUREING STREAM SWITCH PORTS" << std::endl;
    // AIE Tiles (e.g., trace streams)
    if (type == module_type::core) {
      auto slaveOrMaster = (metricSet.find("mm2s") != std::string::npos) ?
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
      std::cout << "Configuring Stream Port ID: " << (int)streamPortId << std::endl; 
      // Define stream switch port to monitor interface 
      XAie_EventSelectStrmPort(&aieDevInst, loc, rscId, slaveOrMaster, SOUTH, streamPortId);
      return;
    }

    if (type == module_type::mem_tile) {
      auto slaveOrMaster = (metricSet.find("mm2s") != std::string::npos) ?
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

    XAie_DmaDirection dmaDir = (metricSet.find("s2mm") != std::string::npos) ? DMA_S2MM : DMA_MM2S;
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
    std::cout << "row, col" << row << " " << column << std::endl; 
    // 1. Stream IDs for interface tiles
    if (type == module_type::shim) {
      // NOTE: value = ((master or slave) << 8) & (stream ID)
      return ((tile.itr_mem_col << 8) | tile.itr_mem_row);
    }

    // 2. Channel IDs for MEM tiles
    if (type == module_type::mem_tile) {
      // NOTE: value = ((master or slave) << 8) & (channel ID)
      uint8_t isMaster = (metricSet.find("s2mm") != std::string::npos) ? 1 : 0;
      return ((isMaster << 8) | channel);
    }

    // 3. DMA BD sizes for AIE tiles
    if ((startEvent != XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM))
      return 0;

    uint32_t payloadValue = 0;

    // constexpr int NUM_BDS = 8;
    // constexpr uint32_t BYTES_PER_WORD_PROFILE = 4;
    // constexpr uint32_t ACTUAL_OFFSET = 1;
    // uint64_t offsets[NUM_BDS] = {XAIEGBL_MEM_DMABD0CTRL,            XAIEGBL_MEM_DMABD1CTRL,
    //                              XAIEGBL_MEM_DMABD2CTRL,            XAIEGBL_MEM_DMABD3CTRL,
    //                              XAIEGBL_MEM_DMABD4CTRL,            XAIEGBL_MEM_DMABD5CTRL,
    //                              XAIEGBL_MEM_DMABD6CTRL,            XAIEGBL_MEM_DMABD7CTRL};
    // uint32_t lsbs[NUM_BDS]    = {XAIEGBL_MEM_DMABD0CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD1CTRL_LEN_LSB,
    //                              XAIEGBL_MEM_DMABD2CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD3CTRL_LEN_LSB,
    //                              XAIEGBL_MEM_DMABD4CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD5CTRL_LEN_LSB,
    //                              XAIEGBL_MEM_DMABD6CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD7CTRL_LEN_LSB};
    // uint32_t masks[NUM_BDS]   = {XAIEGBL_MEM_DMABD0CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD1CTRL_LEN_MASK,
    //                              XAIEGBL_MEM_DMABD2CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD3CTRL_LEN_MASK,
    //                              XAIEGBL_MEM_DMABD4CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD5CTRL_LEN_MASK,
    //                              XAIEGBL_MEM_DMABD6CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD7CTRL_LEN_MASK};
    // uint32_t valids[NUM_BDS]  = {XAIEGBL_MEM_DMABD0CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD1CTRL_VALBD_MASK,
    //                              XAIEGBL_MEM_DMABD2CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD3CTRL_VALBD_MASK,
    //                              XAIEGBL_MEM_DMABD4CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD5CTRL_VALBD_MASK,
    //                              XAIEGBL_MEM_DMABD6CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD7CTRL_VALBD_MASK};

    // auto tileOffset = _XAie_GetTileAddr(&aieDevInst, static_cast<uint8_t>(row), static_cast<uint8_t>(column));
    // for (int bd = 0; bd < NUM_BDS; ++bd) {
    //   uint32_t regValue = 0;
    //   XAie_Read32(&aieDevInst, tileOffset + offsets[bd], &regValue);
      
    //   if (regValue & valids[bd]) {
    //     uint32_t bdBytes = BYTES_PER_WORD_PROFILE * (((regValue >> lsbs[bd]) & masks[bd]) + ACTUAL_OFFSET);
    //     payloadValue = std::max(bdBytes, payloadValue);
    //   }
    // }

    return payloadValue;
  }
  

  void AieProfile_WinImpl::poll(uint32_t index, void* handle)
  {
    std::cout << "Polling! " << index << handle << std::endl;
    // For now, we are waiting for a way to retreive polling information from
    // the AIE.

    // XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);
    // // Profiling is 3rd custom OP
    // XAie_RequestCustomTxnOp(&aieDevInst);
    // XAie_RequestCustomTxnOp(&aieDevInst);
    // auto read_op_code_ = XAie_RequestCustomTxnOp(&aieDevInst);

    // std::vector<XAie_ModuleType> falModuleTypes = {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD, XAIE_MEM_MOD};

    // std::map<module_type, std::vector<uint64_t>> regValues {
    //   {module_type::core, {0x31520,0x31524,0x31528,0x3152C}}, 
    //   {module_type::dma, {0x11020,0x11024}}, 
    //   {module_type::shim, {0x31020, 0x31024}}, 
    //   {module_type::mem_tile, {0x91020,0x91024,0x91028,0x9102C}}, 
    // };
    
    // //auto configChannel1 = metadata->getConfigChannel1();

    // aie_profile_op_t op = {}; 
    // bool debug = true;
    // int numCounters = 0;

    // std::vector<int> shimDebug {0x31000, 0x31008, 0x31020, 0x31024,
    //                   0x3FF00, 0x34504 ,0x34200,0x34204, 
    //                   0x34208,0x3420C, 0x1D220, 0x1D224, 
    //                   0x1D228, 0x1D22C};
    // // std::vector<int> memDebug {0x91020,0x91024,0x91028,0x9102C,0x91000,0x91004,0x91008,0xB0F00,0x91024,0x91024,0x91024};
    // // std::vector<int> coreDebug {0x31500, 0x31504, 0x31508, 0x31520, 0x31524, 0x31528, 0x3152C, 0x32004, 0x3FF00, 0x34200, 0x34204, 0x34208, 0x3420C, 0x11000, 0x11008, 0x11020,0x11024};

    // if (debug) {
    //     XAie_ModuleType mod = falModuleTypes[2];
    //    for (auto& tileMetric : metadata->getConfigMetrics(2)) {
    //     auto tile = tileMetric.first;
    //     uint8_t col = static_cast<uint8_t>(tile.col);
    //     uint8_t row = static_cast<uint8_t>(tile.row);
    //     auto type   = getModuleType(row, mod);
      
    //    if (type == module_type::mem_tile)
    //       col = 0;

    //     std::vector<uint64_t> Regs = regValues[type];
    //     // 25 is column offset and 20 is row offset for IPU
    //     for (auto& r : shimDebug)
    //       op.perf_address[numCounters++] = r + (col << 25) + (row << 20);
    //   }
    // } else {
    //   for (int module = 0; module < metadata->getNumModules(); ++module) {
    //     // int numTileCounters[metadata->getNumCountersMod(module)+1] = {0};
    //     XAie_ModuleType mod = falModuleTypes[module];
    //     // Iterate over tiles and metrics to configure all desired counters
    //     for (auto& tileMetric : metadata->getConfigMetrics(module)) {
    //       auto tile = tileMetric.first;
    //       uint8_t col         = static_cast<uint8_t>(tile.col);
    //       uint8_t row         = static_cast<uint8_t>(tile.row);
    //       auto type        = getModuleType(row, mod);

    //       if (type == module_type::mem_tile)
    //         col = 0;
    //       uint8_t numFreeCtr  = (type == module_type::dma || type == module_type::shim) ? 2 : 4;
    //       std::vector<uint64_t> Regs = regValues[type];
    //       // 25 is column offset and 20 is row offset for IPU
    //       for (int i = 0; i < numFreeCtr; i++)
    //         op.perf_address[numCounters++] = Regs[i] + (col << 25) + (row << 20);
    //     }
    //   }
    // }
    // XAie_AddCustomTxnOp(&aieDevInst, (uint8_t)read_op_code_, (void*)&op, sizeof(op));
    // uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    // XAie_TxnHeader *hdr = (XAie_TxnHeader *)txn_ptr;
    // std::cout << "Poll Txn Size: " << hdr->TxnSize << " bytes" << std::endl;
    // op_buf instr_buf;
    // instr_buf.addOP(transaction_op(txn_ptr));
    // input_bo.write(instr_buf.ibuf_.data());

    auto inbo_map = input_bo.map<uint8_t*>();
    // //memset(inbo_map, 0, 4096);
    // input_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // std::cout << "About to run the kernel!" << std::endl;
    // auto run = mKernel(CONFIGURE_OPCODE, input_bo, instr_buf.ibuf_.size()/sizeof(int), 0, 0, 0, 0);

    // try {
    //   run.wait2();
    // } catch (std::exception &e) {
    //   std::cout << "Caught exception: " << e.what() << std::endl;
    // }

    std::cout << "Finished running the kernel!" << std::endl;

    std::cout << "About to Sync2" << std::endl;

    input_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE,8192,0);
    std::cout << "Synced Finished" << std::endl;


    for (int i = 0; i < 32; i++) {
     std::cout << "Output Value: " << inbo_map[i] << std::endl;
    }

    aie_profile_op_t* output = reinterpret_cast<aie_profile_op_t*>(inbo_map);

    for (int i = 0; i < 20; i++) {
      std::cout << "Address: " << std::hex << output->perf_address[i] << std::endl;
      std::cout << "Values: " << std::hex << output->perf_value[i] << std::endl;
    }
 

    return;
  }

  void AieProfile_WinImpl::freeResources()
  {
    // TODO - if there are any resources we need to free after the application
    // is complete, it must be done here.
  }
}  // namespace xdp
