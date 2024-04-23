// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>

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

  constexpr uint32_t size_4K   = 0x1000;
  constexpr uint32_t offset_3K = 0x0C00;

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
    auto context = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
    auto device = xrt_core::hw_context_int::get_core_device(context);
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

    transactionHandler = std::make_unique<aie::ClientTransaction>(context, "AIE Debug");
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
    std::vector<profile_data_t> op_profile_data;

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
          op_profile_data.emplace_back(profile_data_t{Regs[i] + (tile.col << 25) + (tile.row << 20)});
          counterId++;
        }
      }
    }

    auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);
    if (RC != XAIE_OK) {
      xrt_core::message::send(severity_level::warning, "XRT", "AIE Driver Initialization Failed.");
      return;
    }

    op_size = sizeof(aie_profile_op_t) + sizeof(profile_data_t) * (counterId - 1);
    op = (aie_profile_op_t*)malloc(op_size);
    op->count = counterId;
    for (int i = 0; i < op_profile_data.size(); i++)
      op->profile_data[i] = op_profile_data[i];

    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);
    // Profiling is 3rd custom OP
    XAie_RequestCustomTxnOp(&aieDevInst);
    XAie_RequestCustomTxnOp(&aieDevInst);
    auto read_op_code_ = XAie_RequestCustomTxnOp(&aieDevInst);

    // try {
    //   mKernel = xrt::kernel(context, "XDP_KERNEL");  
    // } catch (std::exception &e){
    //   std::stringstream msg;
    //   msg << "Unable to find XDP_KERNEL from hardware context. Not configuring AIE Debug. " << e.what() ;
    //   xrt_core::message::send(severity_level::warning, "XRT", msg.str());
    //   return;
    // }

    if (!transactionHandler->initializeKernel("XDP_KERNEL"))
      return;

    XAie_AddCustomTxnOp(&aieDevInst, (uint8_t)read_op_code_, (void*)op, op_size);
    txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);


    // op_buf instr_buf;
    // instr_buf.addOP(transaction_op(txn_ptr));

    // Initialize instructions
    // try {
    //   instr_bo = xrt::bo(context.get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, mKernel.group_id(1));
    // } catch (std::exception &e){
    //   std::stringstream msg;
    //   msg << "Unable to create the instruction buffer for polling during AIE Debug. " << e.what() << std::endl;
    //   xrt_core::message::send(severity_level::warning, "XRT", msg.str());
    //   return;
    // }
    // instr_bo.write(instr_buf.ibuf_.data());
    // instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // // results BO syncs AIE Debug result from device
    // try {
    //   result_bo = xrt::bo(context.get_device(), size_4K, XCL_BO_FLAGS_CACHEABLE, mKernel.group_id(1));
    // } catch (std::exception &e) {
    //   std::stringstream msg;
    //   msg << "Unable to create result buffer for AIE Debug. Cannot get AIE Debug Info." << e.what() << std::endl;
    //   xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
    //   return;
    // }

    XAie_ClearTransaction(&aieDevInst);

    // Poll once at beginning
    poll();
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

    if (!transactionHandler->submitTransaction(txn_ptr))
      return;
    auto result_bo = transactionHandler->syncResults();
    if (!result_bo)
      return;

    // auto run = mKernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0);
    // try {
    //   run.wait2();
    // } catch (std::exception &e) {
    //   std::stringstream msg;
    //   msg << "Unable to successfully execute AIE Debug polling kernel. " << e.what() << std::endl;
    //   xrt_core::message::send(severity_level::warning, "XRT", msg.str());
    //   return;
    // }

    auto result_bo_map = result_bo.map<uint8_t*>();

    uint32_t* output = reinterpret_cast<uint32_t*>(result_bo_map+offset_3K);

    for (uint32_t i = 0; i < op->count; i++) {
      std::stringstream msg;
      int col = (op->profile_data[i].perf_address >> 25) & 0x1F;
      int row = (op->profile_data[i].perf_address >> 20) & 0x1F;
      int reg = (op->profile_data[i].perf_address) & 0xFFFFF;
      
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

