// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

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
  namespace pt = boost::property_tree;

  bool AieDebugPlugin::live = false;

  AieDebugPlugin::
  AieDebugPlugin() : XDPPlugin()
  {
    AieDebugPlugin::live = true;

    db->registerPlugin(this);
    db->getStaticInfo().setAieApplication();
  }

  AieDebugPlugin::
  ~AieDebugPlugin()
  {
    // Stop the polling thread
    endPoll();
    free(op);

    if (VPDatabase::alive())
      db->unregisterPlugin(this);

    AieDebugPlugin::live = false;
  }

  bool
  AieDebugPlugin::
  alive()
  {
    return AieDebugPlugin::live;
  }

  void
  AieDebugPlugin::
  updateAIEDevice(void* handle) {
    if (!xrt_core::config::get_aie_debug())
      return;

    // AIE Debug plugin is built only for client 
    mHwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
    auto device = xrt_core::hw_context_int::get_core_device(mHwContext);
    auto deviceID = getDeviceIDFromHandle(handle);
    
    (db->getStaticInfo()).updateDeviceClient(deviceID, device);
    (db->getStaticInfo()).setDeviceName(deviceID, "win_device");

    // Delete old data for this handle
    if (handleToAIEData.find(handle) != handleToAIEData.end())
      handleToAIEData.erase(handle);

    //Setting up struct 
    auto& aieData = handleToAIEData[handle];
    aieData.deviceID = deviceID;
    
    metadataReader = (db->getStaticInfo()).getAIEmetadataReader();
    if (!metadataReader)
      return;

    transactionHandler = std::make_unique<aie::ClientTransaction>(mHwContext, "AIE Debug");
    xdp::aie::driver_config meta_config = getAIEConfigMetadata();

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

    auto regValues = parseMetrics();
    std::vector<register_data_t> op_profile_data;

    int counterId = 0;
    for (auto const& kv : moduleTypes) {
      auto type = kv.first;
      auto name = kv.second;

      // Registers for current module
      auto& Regs = regValues[type];
      if (Regs.empty())
        continue;
    
      std::vector<tile_type> tiles;
      if (type == module_type::shim) {
        tiles = metadataReader->getInterfaceTiles("all", "all", "input_output");
      } else {
        tiles = metadataReader->getTiles("all", type, "all");
      }

      if (tiles.empty()) {
        std::stringstream msg;
        msg << "AIE Debug found no tiles for module: " << name << ".";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        continue;
      }

      std::sort(tiles.begin(), tiles.end());
      tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());
      std::stringstream msg;
      msg << "AIE Debug monitoring tiles of type " << name << ":\n";
      for (const auto& t: tiles)
        msg << t.col << "," << t.row << " ";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());

      for (auto &tile : tiles) {
        for (int i = 0; i < Regs.size(); i++) {
          op_profile_data.emplace_back(register_data_t{Regs[i] + (tile.col << 25) + (tile.row << 20)});
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
    for (int i = 0; i < op_profile_data.size(); i++)
      op->data[i] = op_profile_data[i];

  #if 0
    /* For larger debug buffer support, only one Debug BO can be alive at a time.
     * Need ML Timeline Buffer to be created at update device to capture all calls.
     * So, skip this poll
     */
    poll();
  #endif
  }


  void
  AieDebugPlugin::
  endAIEDebugRead(void* /*handle*/)
  {
    endPoll();
  }

  std::map<module_type, std::vector<uint64_t>>
  AieDebugPlugin::
  parseMetrics()
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

  std::vector<std::string>
  AieDebugPlugin::
  getSettingsVector(std::string settingsString)
  {
    if (settingsString.empty())
      return {};

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;
    boost::replace_all(settingsString, " ", "");
    boost::split(settingsVector, settingsString, boost::is_any_of(","));
    return settingsVector;
  }

  void
  AieDebugPlugin::
  endPoll()
  {
    static bool finished = false;
    if (!finished) {
      finished = true;
      poll();
    }
  }

  void
  AieDebugPlugin::
  poll()
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

  aie::driver_config
  AieDebugPlugin::
  getAIEConfigMetadata()
  {
    return metadataReader->getDriverConfig();
  }

  uint64_t
  AieDebugPlugin::
  getDeviceIDFromHandle(void* handle)
  { 
    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

    return db->addDevice("win_device");
  }

}  // end namespace xdp

