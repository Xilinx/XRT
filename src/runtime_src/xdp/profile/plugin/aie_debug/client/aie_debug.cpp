/**
 * Copyright (C) 2023-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/plugin/aie_debug/client/aie_debug.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/include/experimental/xrt-next.h"

#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/vp_base/info.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;

  /****************************************************************************
   * Client constructor
   ***************************************************************************/
  AieDebug_WinImpl::AieDebug_WinImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata)
    : AieDebugImpl(database, metadata)
  {
    hwContext = metadata->getHwContext();
    transactionHandler = std::make_unique<aie::ClientTransaction>(hwContext, "AIE Debug Setup");
  }

  /****************************************************************************
   * Poll all registers
   ***************************************************************************/
  void AieDebug_WinImpl::poll(const uint64_t deviceID, void* /*handle*/)
  {
    xrt_core::message::send(severity_level::debug, "XRT", "Calling AIE Poll.");

    if (db->infoAvailable(xdp::info::ml_timeline)) {
      db->broadcast(VPDatabase::MessageType::READ_RECORD_TIMESTAMPS, nullptr);
      xrt_core::message::send(severity_level::debug, "XRT", "Done reading recorded timestamps.");
    }

    xrt::bo resultBO;
    uint32_t* output = nullptr;
    try {
      resultBO = xrt_core::bo_int::create_bo(hwContext, 0x20000, xrt_core::bo_int::use_type::debug);
      output = resultBO.map<uint32_t*>();
      memset(output, 0, 0x20000);
    } catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to create 128KB buffer for AIE Debug results. Cannot get AIE Debug info. " << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }

    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    if (!transactionHandler->initializeKernel("XDP_KERNEL"))
      return;

    XAie_AddCustomTxnOp(&aieDevInst, XAIE_IO_CUSTOM_OP_READ_REGS, (void*)op, op_size);
    txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    if (!transactionHandler->submitTransaction(txn_ptr))
      return;

    XAie_ClearTransaction(&aieDevInst);

    resultBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    for (uint32_t i = 0; i < op->count; i++) {
      uint8_t col  = (op->data[i].address >> 25) & 0x1F;
      uint8_t row  = (op->data[i].address >> 20) & 0x1F;
      uint64_t reg = (op->data[i].address) & 0xFFFFF;

      if (aie::isDebugVerbosity()) {
        std::stringstream msg;
        msg << "Debug tile (" << +col << ", " << +row << ") " << "hex address/values: " 
            << std::hex << reg << " : " << output[i] << std::dec;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      }

      tile_type tile;
      tile.col = col;
      tile.row = row;
      
      if (debugTileMap.find(tile) == debugTileMap.end())
        debugTileMap[tile] = std::make_unique<ClientReadableTile>(col, row, reg);
        
      auto regName = metadata->lookupRegisterName(reg);
      debugTileMap[tile]->addOffsetName(reg, regName);
      debugTileMap[tile]->addValue(output[i]);
    }

    // Add values to database
    for (auto& tileAddr : debugTileMap)
      tileAddr.second->printValues(static_cast<uint32_t>(deviceID), db);
  }

  /****************************************************************************
   * Update device
   ***************************************************************************/
  void AieDebug_WinImpl::updateDevice()
  {
    // Do nothing for now
  }

  /****************************************************************************
   * Update AIE device
   ***************************************************************************/
  void AieDebug_WinImpl::updateAIEDevice(void* /*handle*/)
  {
    if (!xrt_core::config::get_aie_debug())
      return;

    auto regValues = metadata->getRegisterValues();
    std::vector<register_data_t> op_debug_data;

    // Traverse all module types
    int counterId = 0;
    for (int module = 0; module < metadata->getNumModules(); ++module) {
      auto configMetrics = metadata->getConfigMetricsVec(module);
      if (configMetrics.empty())
        continue;

      auto type = metadata->getModuleType(module);
      auto name = moduleTypes.at(type);

      // List of registers to read for current module
      auto Regs = regValues[type];
      if (Regs.empty())
        continue;

      if (aie::isDebugVerbosity()) {
        std::stringstream msg;
        msg << "AIE Debug monitoring tiles of type " << name << ":\n";
        for (auto& tileMetric : configMetrics)
          msg << tileMetric.first.col << "," << tileMetric.first.row << " ";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
      }

      // Traverse all active tiles for this module type
      for (auto& tileMetric : configMetrics) {
        auto tile        = tileMetric.first;
        auto tileOffset = (tile.col << 25) + (tile.row << 20);

        for (int i = 0; i < Regs.size(); i++) {
          op_debug_data.emplace_back(register_data_t{Regs[i] + tileOffset});
          counterId++;
        }
      }
    }

    auto meta_config = metadata->getAIEConfigMetadata();
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
      return;
    }

    op_size = sizeof(read_register_op_t) + sizeof(register_data_t) * (counterId - 1);
    op = (read_register_op_t*)malloc(op_size);
    op->count = counterId;
    for (int i = 0; i < op_debug_data.size(); i++)
      op->data[i] = op_debug_data[i];
  }

}  // end namespace xdp
