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
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  AieProfile_WinImpl::AieProfile_WinImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata)
  {
    // auto spdevice = xrt_core::get_userpf_device(metadata->getHandle());
    // device = xrt::device(spdevice);

    // auto uuid = device.get_xclbin_uuid();

    // if (metadata->getHardwareGen() == 1)
    //   aie_profile_kernel = xrt::kernel(device, uuid.get(),
    //   "aie_profile_config");
    // else
    //   aie_profile_kernel = xrt::kernel(device, uuid.get(),
    //   "aie2_profile_config");
  }

  void AieProfile_WinImpl::updateDevice()
  {
    setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());
  }



  bool AieProfile_WinImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    std::cout << "reached setmetricssettings: " << deviceId  << handle << std::endl;

    // uint16_t num_rows = metadata->getAIEConfigMetadata("num_rows").get_value<uint16_t>();
    // uint16_t num_cols = metadata->getAIEConfigMetadata("num_columns").get_value<uint16_t>();
    // std::cout << "NumRows: " << num_rows << std::endl;
    // std::cout << "NumCols: " << num_cols << std::endl;
    // return true;

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


    // int counterId = 0;
    bool runtimeCounters = false;
    // std::vector<XAie_ModuleType> falModuleTypes = {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD, XAIE_MEM_MOD};

    auto configChannel0 = metadata->getConfigChannel0();
    auto configChannel1 = metadata->getConfigChannel1();

    for (int module = 0; module < metadata->getNumModules(); ++module) {
      // int numTileCounters[metadata->getNumCountersMod(module)+1] = {0};
      // XAie_ModuleType mod = falModuleTypes[module];
      std::cout << "Module: " << module << std::endl;
      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : metadata->getConfigMetrics(module)) {
    
        auto tile        = tileMetric.first;
        auto col         = tile.col;
        auto row         = tile.row;
        std::cout << "Col, Row: " << col << " " << row << std::endl;
        // auto type        = getModuleType(row, mod);
        // if (!isValidType(type, mod))
        //   continue;

        // auto& metricSet  = tileMetric.second;
        // auto loc         = XAie_TileLoc(col, row);
        // auto& xaieTile   = aieDevice->tile(col, row);
        // auto xaieModule  = (mod == XAIE_CORE_MOD) ? xaieTile.core()
        //                  : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
        //                  : xaieTile.pl());

        // auto startEvents = (type  == module_type::core) ? mCoreStartEvents[metricSet]
        //                  : ((type == module_type::dma)  ? mMemoryStartEvents[metricSet]
        //                  : ((type == module_type::shim) ? mShimStartEvents[metricSet]
        //                  : mMemTileStartEvents[metricSet]));
        // auto endEvents   = (type  == module_type::core) ? mCoreEndEvents[metricSet]
        //                  : ((type == module_type::dma)  ? mMemoryEndEvents[metricSet]
        //                  : ((type == module_type::shim) ? mShimEndEvents[metricSet]
        //                  : mMemTileEndEvents[metricSet]));

        // int numCounters  = 0;
        // auto numFreeCtr  = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);

        // // Specify Sel0/Sel1 for MEM tile events 21-44
        // auto iter0 = configChannel0.find(tile);
        // auto iter1 = configChannel1.find(tile);
        // uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        // uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        // configEventSelections(aieDevInst, loc, XAIE_MEM_MOD, type, metricSet, channel0, channel1);

        // // Request and configure all available counters for this tile
        // for (int i=0; i < numFreeCtr; ++i) {
        //   auto startEvent    = startEvents.at(i);
        //   auto endEvent      = endEvents.at(i);
        //   uint8_t resetEvent = 0;

        //   // Request counter from resource manager
        //   auto perfCounter = xaieModule.perfCounter();
        //   auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
        //   if (ret != XAIE_OK) break;
        //   ret = perfCounter->reserve();
        //   if (ret != XAIE_OK) break;
        
        //   // Channel number is based on monitoring port 0 or 1
        //   auto channel = (startEvent <= XAIE_EVENT_PORT_TLAST_0_MEM_TILE) ? channel0 : channel1;

        //   configGroupEvents(aieDevInst, loc, mod, startEvent, metricSet);
        //   configStreamSwitchPorts(aieDevInst, tileMetric.first, xaieTile, loc, type,
        //                           startEvent, i, metricSet, channel);
        
        //   // Start the counters after group events have been configured
        //   ret = perfCounter->start();
        //   if (ret != XAIE_OK) break;
        //   mPerfCounters.push_back(perfCounter);

        //   // Convert enums to physical event IDs for reporting purposes
        //   uint8_t tmpStart;
        //   uint8_t tmpEnd;
        //   XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, startEvent, &tmpStart);
        //   XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod,   endEvent, &tmpEnd);
        //   uint16_t phyStartEvent = tmpStart + mCounterBases[type];
        //   uint16_t phyEndEvent   = tmpEnd   + mCounterBases[type];
        //   auto payload = getCounterPayload(aieDevInst, tileMetric.first, type, col, row, 
        //                                    startEvent, metricSet, channel);

        //   // Store counter info in database
        //   std::string counterName = "AIE Counter " + std::to_string(counterId);
        //   (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
        //         phyStartEvent, phyEndEvent, resetEvent, payload, metadata->getClockFreqMhz(), 
        //         metadata->getModuleName(module), counterName);
        //   counterId++;
        //   numCounters++;
        // }

        // std::stringstream msg;
        // msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," 
        //     << row << ") using metric set " << metricSet << ".";
        // xrt_core::message::send(severity_level::debug, "XRT", msg.str());
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
    
	  // uint8_t *Ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
  
    //Schedule PS kernel
    return runtimeCounters;
  }
  

  void AieProfile_WinImpl::poll(uint32_t index, void* handle)
  {
    std::cout << "Polling! " << index << handle << std::endl;
    // For now, we are waiting for a way to retreive polling information from
    // the AIE.
    return;
  }

  void AieProfile_WinImpl::freeResources()
  {
    // TODO - if there are any resources we need to free after the application
    // is complete, it must be done here.
  }
}  // namespace xdp
