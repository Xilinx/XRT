/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/api/hw_context_int.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/common/client_transaction.h"
#include "xdp/profile/plugin/aie_pc/clientDev/aie_pc.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"

#include "core/common/api/bo_int.h"
#include "xrt/xrt_bo.h"
#include "core/common/api/xclbin_int.h"
#include "core/include/xclbin.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
  #include <xaiengine/xaie_events_aie.h>
}

namespace xdp {

  struct PCInfo {
    uint64_t startPC;
    uint64_t endPC;
    XAie_Events startPCEvent;
    XAie_Events endPCEvent;
    uint64_t perfCounterOffset;
    uint8_t  perfCounterId;
  };

  struct TilePCInfo {
    std::unique_ptr<PCInfo> eventsCorePC_0_1;
    std::unique_ptr<PCInfo> eventsCorePC_2_3;
  };

  AIEPCClientDevImpl::AIEPCClientDevImpl(VPDatabase*dB)
    : AIEPCImpl(dB)
  {
  }

  AIEPCClientDevImpl::~AIEPCClientDevImpl()
  {}

  void AIEPCClientDevImpl::updateDevice(void* hwCtxImpl)
  {
    (void)hwCtxImpl;

    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "In AIEPCClientDevImpl::updateDevice");

    std::unique_ptr<aie::ClientTransaction> txnHandler
        = std::make_unique<aie::ClientTransaction>(mHwContext, "AIE PC");

    if (!txnHandler->initializeKernel("XDP_KERNEL"))
      return;
  
    boost::property_tree::ptree aieMetadata;    
    try {
      auto device = xrt_core::hw_context_int::get_core_device(mHwContext);
      xrt::xclbin xrtXclbin = device.get()->get_xclbin(device.get()->get_xclbin_uuid());
      auto data = xrt_core::xclbin_int::get_axlf_section(xrtXclbin, AIE_METADATA);

      if (!data.first || !data.second) {
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Empty AIE Metadata in xclbin");
        return;
      }

      std::stringstream ss;
      ss.write(data.first,data.second);

      boost::property_tree::read_json(ss, aieMetadata);
    } catch (const std::exception& e) {
      std::string msg("AIE Metadata could not be read/processed from xclbin: ");
      msg += e.what();
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }

    std::string str = xrt_core::config::get_aie_pc_settings();
    
    std::vector<std::string> addresses;

    std::map<uint64_t /*col*/, std::map<uint64_t /*row*/, std::unique_ptr<TilePCInfo>>>::iterator itrSpec;
    std::map<uint64_t /*row*/, std::unique_ptr<TilePCInfo>>::iterator itrTileInfo;

    uint32_t nEntries = 0;

    boost::split(addresses, str, boost::is_any_of(";"));
    // Format : col, row:start_pc:end_pc ; col,row:start_pc:end_pc
    for (uint32_t i = 0; i < addresses.size(); i++) {
      std::vector<std::string> address;
      boost::split(address, addresses[i], boost::is_any_of(":"));
      if (3 != address.size()) {
        continue;
      }
      std::vector<std::string> loc;
      boost::split(loc, address[0], boost::is_any_of(","));
      if (2 != loc.size()) {
        continue;
      }
      uint64_t col = 0, row = 0;
      col = std::stoul(loc[0], nullptr, 10);
      row = std::stoul(loc[1], nullptr, 10);

      itrSpec = spec.find(col);
      // No entries added for current column
      if (itrSpec == spec.end()) {
        // Populate info
        std::unique_ptr<PCInfo> info = std::make_unique<PCInfo>();
        info->startPC = std::stoul(address[1], nullptr, 10);
        info->endPC   = std::stoul(address[2], nullptr, 10);
        info->startPCEvent = (XAie_Events)XAIE_EVENT_PC_0_CORE;
        info->endPCEvent = (XAie_Events)XAIE_EVENT_PC_1_CORE;
        info->perfCounterId = 0;
        info->perfCounterOffset = 0x0031520;
        
        std::stringstream msg;
        msg << "Configure PC event for Core "
          << col << ", " << row << " Start PC " << info->startPC << " End PC " << info->endPC 
          << " using perf counter id " << std::to_string(info->perfCounterId)
          << " perf counter address " << std::hex << info->perfCounterOffset << std::dec;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

        // Add new map for current column
        spec[col] = std::map<uint64_t /*row*/, std::unique_ptr<TilePCInfo>>();

        // Add TilePCInfo entry for current column, row
        spec[col][row] = std::make_unique<TilePCInfo>();
        spec[col][row]->eventsCorePC_0_1 = std::move(info);

        nEntries++;
        continue;                  
      }

      // Entry found for current column
      if (itrSpec != spec.end()) {
        itrTileInfo = itrSpec->second.find(row);

        // No entry found for current column,row core tile
        if (itrTileInfo == itrSpec->second.end()) {
          // Populate info
          std::unique_ptr<PCInfo> info = std::make_unique<PCInfo>();
          info->startPC = std::stoul(address[1], nullptr, 10);
          info->endPC   = std::stoul(address[2], nullptr, 10);
          info->startPCEvent = (XAie_Events)XAIE_EVENT_PC_0_CORE;
          info->endPCEvent = (XAie_Events)XAIE_EVENT_PC_1_CORE;
          info->perfCounterId = 0;
          info->perfCounterOffset = 0x0031520;
          
          std::stringstream msg;
          msg << "Configure PC event for Core "
            << col << ", " << row << " Start PC " << info->startPC << " End PC " << info->endPC 
            << " using perf counter id " << std::to_string(info->perfCounterId)
            << " perf counter address " << std::hex << info->perfCounterOffset << std::dec;
          xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

          // Add TilePCInfo entry for current column, row
          itrSpec->second.emplace(row, std::make_unique<TilePCInfo>());
          spec[col][row]->eventsCorePC_0_1 = std::move(info);

          nEntries++;
          continue;  
        }

        // Entry found for current column,row core tile
        if (itrTileInfo != itrSpec->second.end()) {

          // Check whether XAIE_EVENT_PC_2_CORE, XAIE_EVENT_PC_3_CORE configured for current column,row core tile
          if (nullptr == itrTileInfo->second->eventsCorePC_2_3) {

            // Populate info
            std::unique_ptr<PCInfo> info = std::make_unique<PCInfo>();
            info->startPC = std::stoul(address[1], nullptr, 10);
            info->endPC   = std::stoul(address[2], nullptr, 10);
            info->startPCEvent = (XAie_Events)XAIE_EVENT_PC_2_CORE; 
            info->endPCEvent = (XAie_Events)XAIE_EVENT_PC_3_CORE;
            info->perfCounterId = 1;
            info->perfCounterOffset = 0x0031524;

            std::stringstream msg;
            msg << "Configure PC event for Core "
              << col << ", " << row << " Start PC " << info->startPC << " End PC " << info->endPC 
              << " using perf counter id " << std::to_string(info->perfCounterId)
              << " perf counter address " << std::hex << info->perfCounterOffset << std::dec;
            xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

            itrTileInfo->second->eventsCorePC_2_3 = std::move(info);
            nEntries++;
            continue;
          } else {
            std::string msg;
            msg += "Core PC Events for tile in settings " + addresses[i] 
                   + " are already used up. So, it is ignored. Please use a different core for this Start/End PC addresses.\n";
            xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
            continue;
          }
        }
      }
    } 
    // end parsing settings and populating desired configurations

    xdp::aie::driver_config meta_config = xdp::aie::getDriverConfig(aieMetadata, "aie_metadata.driver_config");

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
      {0}
    };

    auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);
    if (RC != XAIE_OK) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "AIE Driver Initialization Failed.");
      return;
    }

    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    for (auto const &specEntry : spec) {
      for (auto const &rowEntry : specEntry.second) {
        auto coreTile = XAie_TileLoc(static_cast<uint8_t>(specEntry.first), static_cast<uint8_t>(rowEntry.first));

        if (rowEntry.second->eventsCorePC_0_1) {
          XAie_EventPCEnable(&aieDevInst, coreTile, 0, static_cast<uint16_t>(rowEntry.second->eventsCorePC_0_1->startPC));
          XAie_EventPCEnable(&aieDevInst, coreTile, 1, static_cast<uint16_t>(rowEntry.second->eventsCorePC_0_1->endPC));

          // Reset Perf Counter
          RC = XAie_PerfCounterReset(&aieDevInst, coreTile, XAIE_CORE_MOD, rowEntry.second->eventsCorePC_0_1->perfCounterId);
          if(RC != XAIE_OK) {
            xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", "AIE Performance Counter Reset Failed.");
            return;
          }
         
          RC = XAie_PerfCounterControlSet(&aieDevInst, coreTile, XAIE_CORE_MOD, rowEntry.second->eventsCorePC_0_1->perfCounterId,
                        rowEntry.second->eventsCorePC_0_1->startPCEvent, rowEntry.second->eventsCorePC_0_1->endPCEvent);

          if(RC != XAIE_OK) {
            xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", "AIE Performance Counter Set with Function Call and Return Failed.");
            return;          
          }
        } 
        if (rowEntry.second->eventsCorePC_2_3) {
          XAie_EventPCEnable(&aieDevInst, coreTile, 2, static_cast<uint16_t>(rowEntry.second->eventsCorePC_2_3->startPC));
          XAie_EventPCEnable(&aieDevInst, coreTile, 3, static_cast<uint16_t>(rowEntry.second->eventsCorePC_2_3->endPC));

          // Reset Perf Counter
          RC = XAie_PerfCounterReset(&aieDevInst, coreTile, XAIE_CORE_MOD, rowEntry.second->eventsCorePC_2_3->perfCounterId);
          if(RC != XAIE_OK) {
            xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", "AIE Performance Counter Reset Failed.");
            return;
          }
          RC = XAie_PerfCounterControlSet(&aieDevInst, coreTile, XAIE_CORE_MOD, rowEntry.second->eventsCorePC_2_3->perfCounterId,
                        rowEntry.second->eventsCorePC_2_3->startPCEvent, rowEntry.second->eventsCorePC_2_3->endPCEvent);
          if(RC != XAIE_OK) {
            xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", "AIE Performance Counter Set with Function Call and Return Failed.");
            return;          
          }
        }
      }
    }

    std::stringstream msg1;
    msg1 << "Configuration completed for " << nEntries << " entries. " << std::endl;
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg1.str());

    uint8_t* txnBin = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    if (!txnHandler->submitTransaction(txnBin))
      return;
    XAie_ClearTransaction(&aieDevInst);

    sz = sizeof(read_register_op_t) + sizeof(register_data_t) * (nEntries - 1);
    op = (read_register_op_t*)malloc(sz);
    op->count = nEntries;

    uint32_t idx = 0;
    for (auto const &specEntry : spec) {
      for (auto const &rowEntry : specEntry.second) {
        if (rowEntry.second->eventsCorePC_0_1) {
          op->data[idx].address = ((specEntry.first) << 25) /*col*/ + ((rowEntry.first) << 20) /*row*/ + rowEntry.second->eventsCorePC_0_1->perfCounterOffset;
          idx++;
        }
        if (rowEntry.second->eventsCorePC_2_3) {
          op->data[idx].address = ((specEntry.first) << 25) /*col*/ + ((rowEntry.first) << 20) /*row*/ + rowEntry.second->eventsCorePC_2_3->perfCounterOffset;
          idx++;
        }
      }
    }

    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "AIE PC txn to configure counter completed");
  }

  void AIEPCClientDevImpl::finishflushDevice(void* hwCtxImpl)
  {
    (void)hwCtxImpl;

    if (db->infoAvailable(xdp::info::ml_timeline)) {
      db->broadcast(VPDatabase::MessageType::READ_RECORD_TIMESTAMPS, nullptr);
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Done reading recorded timestamps.");
    }

    xrt::bo resultBO;
    try {
      resultBO = xrt_core::bo_int::create_bo(mHwContext, 0x20000, xrt_core::bo_int::use_type::debug);
    } catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to create 128KB buffer for AIE PC Profile results. Cannot get AIE PC Profile info. " << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }

    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "AIE PC Finish Flush ");
    std::unique_ptr<aie::ClientTransaction> txnHandler
        = std::make_unique<aie::ClientTransaction>(mHwContext, "AIE PC Handler");
    
    if (!txnHandler->initializeKernel("XDP_KERNEL"))
      return;
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    XAie_AddCustomTxnOp(&aieDevInst, XAIE_IO_CUSTOM_OP_READ_REGS, (void*)(op), sz);
    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    if (!txnHandler)
      return;
    txnHandler->setTransactionName("AIE PC Profile Read");
    if (!txnHandler->submitTransaction(txn_ptr))
      return;

    XAie_ClearTransaction(&aieDevInst);
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "AIE PC txn to read perf counter completed");

    resultBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    auto resultBOMap = resultBO.map<uint8_t*>();
    uint32_t* output = reinterpret_cast<uint32_t*>(resultBOMap);

    // Process output
    uint32_t idx = 0;
    for (auto const &specEntry : spec) {
      for (auto const &rowEntry : specEntry.second) {

        if (rowEntry.second->eventsCorePC_0_1) {
          std::stringstream msg;
          msg << "Core " << specEntry.first << ", " << rowEntry.first 
              << " PC " << rowEntry.second->eventsCorePC_0_1->startPC << ":" << rowEntry.second->eventsCorePC_0_1->endPC 
              << " Counter address/values: 0x" << std::hex << op->data[idx].address << ": " << std::dec << output[idx];
          xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg.str());
          idx++;
        }
        if (rowEntry.second->eventsCorePC_2_3) {
          std::stringstream msg;
          msg << "Core " << specEntry.first << ", " << rowEntry.first 
              << " PC " << rowEntry.second->eventsCorePC_2_3->startPC << ":" << rowEntry.second->eventsCorePC_2_3->endPC 
              << " Counter address/values: 0x" << std::hex << op->data[idx].address << ": " << std::dec << output[idx];
          xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg.str());
          idx++;
        }
      }
    }

    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "AIE PC Finish Flush Done");
  }
}
