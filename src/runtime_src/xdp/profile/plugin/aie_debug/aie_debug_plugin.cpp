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

#include "xdp/profile/device/utility.h"
#include "xdp/profile/device/xdp_base_device.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_debug/aie_debug_writer.h"

#ifdef XDP_CLIENT_BUILD
#include "client/aie_debug.h"
#else
#include "core/edge/user/shim.h"
#include "edge/aie_debug.h"
#endif

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;

  bool AieDebugPlugin::live = false;

  /****************************************************************************
   * Constructor
   ***************************************************************************/
  AieDebugPlugin::AieDebugPlugin() : XDPPlugin()
  {
    AieDebugPlugin::live = true;

    db->registerPlugin(this);
    db->getStaticInfo().setAieApplication();
  }

  /****************************************************************************
   * Destructor
   ***************************************************************************/
  AieDebugPlugin::~AieDebugPlugin()
  {
    xrt_core::message::send(severity_level::info, "XRT", "!!!! Calling ~AieDebugPlugin destructor.");

    for (const auto& kv : handleToAIEData)
      endPollforDevice(kv.first);

    XDPPlugin::endWrite();
    handleToAIEData.clear();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }
      db->unregisterPlugin(this);}

    AieDebugPlugin::live = false;
  }

  /****************************************************************************
   * Check if plugin is alive
   ***************************************************************************/
  bool AieDebugPlugin::alive()
  {
    return AieDebugPlugin::live;
  }

  /****************************************************************************
   * Get device ID from handle
   ***************************************************************************/
  uint64_t AieDebugPlugin::getDeviceIDFromHandle(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "!!!! Calling AIE DEBUG AieDebugPlugin::getDeviceIDFromHandle.");
    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

#ifdef XDP_CLIENT_BUILD
    return db->addDevice("win_device");
#else
    //return db->addDevice(util::getDebugIpLayoutPath(handle));
    return db->addDevice("temp_edge_device");
#endif
  }

  /****************************************************************************
   * Update AIE device
   ***************************************************************************/
  void AieDebugPlugin::updateAIEDevice(void* handle)
  {
    if (!xrt_core::config::get_aie_debug() || !handle)
      return;
    xrt_core::message::send(severity_level::info, "XRT", "!!!! Calling AIE DEBUG update AIE device.");

    // Handle relates to HW context in case of Client XRT
#ifdef XDP_CLIENT_BUILD
    xrt::hw_context context = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
    auto device = xrt_core::hw_context_int::get_core_device(context);
#else

#endif
    auto deviceID = getDeviceIDFromHandle(handle);
    std::stringstream msg;
    msg<<"AieDebugPlugin::updateAIEDevice. Device Id =. "<<deviceID;
    xrt_core::message::send(severity_level::info, "XRT", msg.str());
    // Update the static database with information from xclbin
    {
#ifdef XDP_CLIENT_BUILD
      (db->getStaticInfo()).updateDeviceClient(deviceID, device);
      (db->getStaticInfo()).setDeviceName(deviceID, "win_device");
#else
      (db->getStaticInfo()).updateDevice(deviceID, nullptr, handle);
      std::string deviceName = util::getDeviceName(handle);
      if (deviceName != "")
        (db->getStaticInfo()).setDeviceName(deviceID, deviceName);
#endif
    }


    // Delete old data
    if (handleToAIEData.find(handle) != handleToAIEData.end())
#ifdef XDP_CLIENT_BUILD
      return;
#else
      handleToAIEData.erase(handle);
#endif
    auto& AIEData = handleToAIEData[handle];

    AIEData.deviceID = deviceID;
    AIEData.metadata = std::make_unique<AieDebugMetadata>(deviceID, handle);
    if (AIEData.metadata->aieMetadataEmpty())
    {
      AIEData.valid = false;
      xrt_core::message::send(severity_level::debug, "XRT", "AIE DEBUG : no AIE metadata available for this xclbin update, skipping updateAIEDevice()");
      return;
    }
    AIEData.valid = true;

   //TODO insert hw gen specific code and parse metrics pushed up from edge impl
    auto hwGen = AIEData.metadata->getHardwareGen();
    //UsedRegisters* usedRegisters;
    if (hwGen==1){
        usedRegisters=std::make_unique<AIE1UsedRegisters>();
    }
    else if (hwGen==3){
        usedRegisters=std::make_unique<AIE2UsedRegisters>();
    }
    else if (hwGen==4 || hwGen==8 || hwGen==9){
        usedRegisters=std::make_unique<AIE2pUsedRegisters>();
    }
    else if (hwGen==5){
        usedRegisters=std::make_unique<AIE2psUsedRegisters>();
    }
    else if (hwGen>= 40){
        usedRegisters=std::make_unique<AIE4UsedRegisters>();
    }
    usedRegisters->populateRegNameToValueMap();
    usedRegisters->populateRegValueToNameMap();

    //const module_type* moduleTypes = AIEData.metadata->getmoduleTypes();
    auto parsedMetrics = parseMetrics();
    AIEData.metadata->setParsedRegValues(parsedMetrics);
   //************************************************** */

#ifdef XDP_CLIENT_BUILD
    AIEData.metadata->setHwContext(context);
    AIEData.implementation = std::make_unique<AieDebug_WinImpl>(db, AIEData.metadata);
#else
    AIEData.implementation = std::make_unique<AieDebug_EdgeImpl>(db, AIEData.metadata);
#endif

    auto& implementation = AIEData.implementation;
    implementation->updateAIEDevice(handle);

// Open the writer for this device
//#if 0
    auto time = std::time(nullptr);
#ifdef _WIN32
    std::tm tm{};
    localtime_s(&tm, &time);
    std::string deviceName = "win_device";
#else
    auto tm = *std::localtime(&time);
    std::string deviceName = util::getDeviceName(handle);
#endif

    std::ostringstream timeOss;
    timeOss << std::put_time(&tm, "_%Y_%m_%d_%H%M%S");
    std::string timestamp = timeOss.str();

    std::string outputFile = "aie_debug_" + deviceName + timestamp + ".csv";
//#if 0
    VPWriter* writer = new AIEDebugWriter(outputFile.c_str(), deviceName.c_str(), mIndex,this);
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_DEBUG");
//#endif

    mIndex++;
  }

  /****************************************************************************
   * Finish debugging
   ***************************************************************************/
  void AieDebugPlugin::endAIEDebugRead(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "!!! AieDebugPlugin::endAIEDebugRead.");
    auto deviceID = getDeviceIDFromHandle(handle);
    std::stringstream msg;
    msg << "!!!! AieDebugPlugin::endAIEDebugRead deviceID is= "<<deviceID;
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    handleToAIEData[handle].implementation->poll(deviceID, handle);
  }

  /****************************************************************************
   * End polling
   ***************************************************************************/
  void AieDebugPlugin::endPollforDevice(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "!!! Calling AIE Debug AieDebugPlugin::endPollforDevice.");
    if (handleToAIEData.empty())
      return;

    auto& AIEData = handleToAIEData[handle];
    if(!AIEData.valid)
      return;

#ifdef XDP_CLIENT_BUILD
    AIEData.implementation->poll(0, handle);
#endif

    handleToAIEData.erase(handle);
  }

 /****************************************************************************
   * Convert each xrt.ini entry to actual list of registers
  ***************************************************************************/
  std::vector<uint64_t>
  AieDebugPlugin::stringToRegList(std::string stringEntry, module_type t)
  { //core=trace_config,0x3400
    xrt_core::message::send(severity_level::debug, "XRT", "!! Calling AIE Debug AieDebug_EdgeImpl::stringToRegList");
    std::vector<uint64_t> listofRegisters;
    if (stringEntry.rfind("0x", 0) == 0) {
      // if it starts with "0x" that means a particular register address is specified
      uint64_t val = stoul(stringEntry,nullptr,16);
      listofRegisters.push_back(val);
      return listofRegisters;
    }
    else if(stringEntry=="trace_config")
      {
        usedRegisters->populateTraceRegisters();
      }
    else if (stringEntry=="profile_config")
      {
        usedRegisters->populateProfileRegisters();
      }
    else if (stringEntry=="all")
      {
        usedRegisters->populateAllRegisters();
      }
    else {
      //first dealing with specific register names

      //if(usedRegisters->regNametovalues.find(stringEntry) != usedRegisters->regNametovalues.end()) {
      //  uint64_t tmpRedAddr = usedRegisters->regNametovalues[stringEntry];
      //  listofRegisters.push_back(tmpRedAddr);
      //}
      uint64_t tmpRedAddr=lookupRegisterAddr(stringEntry);
      if(tmpRedAddr!=-1){
        listofRegisters.push_back(tmpRedAddr);
      }
      else {
        std::stringstream msg;
        msg << "Error Parsing Debug plugin Metric String. Please enter exact register address, register name, or either of trace_config/profile_config/all. ";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      }
      return listofRegisters;
    }

    if (t==module_type::core){
      auto coreAddressList=usedRegisters->getCoreAddresses();
      listofRegisters.insert( listofRegisters.end(), coreAddressList.begin(),
                             coreAddressList.end() );
    }
    else if (t==module_type::dma){
      auto memoryAddressList = usedRegisters->getMemoryAddresses();
      listofRegisters.insert( listofRegisters.end(), memoryAddressList.begin(),
                             memoryAddressList.end() );
    }
    else if (t==module_type::shim){
      auto interfaceAddressList = usedRegisters->getInterfaceAddresses();
      listofRegisters.insert( listofRegisters.end(), interfaceAddressList.begin(),
                             interfaceAddressList.end() );
    }
    else if (t==module_type::mem_tile){
      auto memoryTileAddressList= usedRegisters->getMemoryTileAddresses();
      listofRegisters.insert( listofRegisters.end(), memoryTileAddressList.begin(),
                             memoryTileAddressList.end() );
    }
    return listofRegisters;
  }

  /****************************************************************************
   * Convert xrt.ini setting to vector
   ***************************************************************************/
  std::vector<std::string>
  AieDebugPlugin::getSettingsVector(std::string settingsString)
  {
    xrt_core::message::send(severity_level::debug, "XRT", "!! Calling AIE Debug AieDebug_EdgeImpl::getSettingsVector");
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
  AieDebugPlugin::parseMetrics()
  {
    xrt_core::message::send(severity_level::debug, "XRT", "!! Calling AIE Debug AieDebug_EdgeImpl::parseMetrics");
    //TODO: change regValues to set to prevent duplication
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
          //uint64_t val = stoul(s,nullptr,16); //old code
          std::vector<uint64_t> regValList = stringToRegList(s,type);
          for(auto val:regValList)
            regValues[type].push_back(val);
        } catch (...) {
          xrt_core::message::send(severity_level::warning, "XRT", "Error Parsing Metric String.");
        }
      }
    }

    return regValues;
  }

}  // end namespace xdp

