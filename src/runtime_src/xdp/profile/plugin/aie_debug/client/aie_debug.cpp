// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

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
  void AieDebug_WinImpl::poll(const uint32_t /*index*/, void* /*handle*/)
  {
    xrt_core::message::send(severity_level::debug, "XRT", "Calling AIE Poll.");

    if (db->infoAvailable(xdp::info::ml_timeline)) {
      db->broadcast(VPDatabase::MessageType::READ_RECORD_TIMESTAMPS, nullptr);
      xrt_core::message::send(severity_level::debug, "XRT", "Done reading recorded timestamps.");
    }

    xrt::bo resultBO;
    uint32_t* output = nullptr;
    try {
      resultBO = xrt_core::bo_int::create_debug_bo(mHwContext, 0x20000);
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
      std::stringstream msg;
      int col = (op->data[i].address >> 25) & 0x1F;
      int row = (op->data[i].address >> 20) & 0x1F;
      int reg = (op->data[i].address) & 0xFFFFF;
      
      msg << "Debug tile (" << col << ", " << row << ") "
          << "hex address/values: " << std::hex << reg << " : "
          << output[i] << std::dec;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
    }
  }

  /****************************************************************************
   * Convert xrt.ini setting to vector
   ***************************************************************************/
  std::vector<std::string>
  AieDebug_WinImpl::getSettingsVector(std::string settingsString)
  {
    if (settingsString.empty())
      return {};

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;
    boost::replace_all(settingsString, " ", "");
    boost::split(settingsVector, settingsString, boost::is_any_of(","));
    return settingsVector;
  }

  /****************************************************************************
   * Parse AIE metrics
   ***************************************************************************/
  std::map<module_type, std::vector<uint64_t>>
  AieDebug_WinImpl::parseMetrics()
  {
    std::map<module_type, std::vector<uint64_t>> regValues {
      {module_type::core, {}},
      {module_type::dma, {}},
      {module_type::shim, {}},
      {module_type::mem_tile, {}}
    };
    std::vector<std::string> metricsConfig;

    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_core_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_memory_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_interface_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_memory_tile_registers());

    unsigned int module = 0;
    for (auto const& kv : moduleTypes) {
      auto type = kv.first;
      std::vector<std::string> metricsSettings = getSettingsVector(metricsConfig[module++]);

      for (auto& s : metricsSettings) {
        try {
          uint64_t val = stoul(s,nullptr,16);
          regValues[type].push_back(val);
        } catch (...) {
          xrt_core::message::send(severity_level::warning, "XRT", "Error Parsing Metric String.");
        }
      }
    }

    return regValues;
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
  void AieDebug_WinImpl::updateAIEDevice(void* handle) 
  {
    if (!xrt_core::config::get_aie_debug())
      return;

    auto regValues = parseMetrics();
    std::vector<register_data_t> op_profile_data;

    // Traverse all module types
    int counterId = 0;
    for (int module = 0; module < metadata->getNumModules(); ++module) {
      auto configMetrics = metadata->getConfigMetricsVec(module);
      if (configMetrics.empty())
        continue;
      
      XAie_ModuleType mod = getFalModuleType(module);
      auto name = moduleTypes[mod];

      // List of registers to read for current module
      auto& Regs = regValues[type];
      if (Regs.empty())
        continue;

      std::stringstream msg;
      msg << "AIE Debug monitoring tiles of type " << name << ":\n";
      for (auto& tileMetric : configMetrics)
        msg << tileMetric.first.col << "," << tileMetric.first.row << " ";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());

      // Traverse all active tiles for this module type
      for (auto& tileMetric : configMetrics) {
        auto& metricSet  = tileMetric.second;
        auto tile        = tileMetric.first;
        auto col         = tile.col;
        auto row         = tile.row;
        auto subtype     = tile.subtype;
        auto type        = aie::getModuleType(row, metadata->getAIETileRowOffset());
  
        for (int i = 0; i < Regs.size(); i++) {
          op_debug_data.emplace_back(register_data_t{Regs[i] + (tile.col << 25) + (tile.row << 20)});
          counterId++;
        }
      }
    }

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
