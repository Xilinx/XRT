/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/writer/aie_status/aie_status_writer.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "core/common/message.h"

namespace xdp {

  /*
   * Writer for AIE status
   */

  AIEStatusWriter::AIEStatusWriter(const char* fileName,
                                   const char* deviceName,
                                   uint64_t deviceIndex,
                                   int hwGen,
                                   std::shared_ptr<xrt_core::device> d)
    : VPWriter(fileName)
    , mDeviceName(deviceName)
    , mDeviceIndex(deviceIndex)
    , mHardwareGen(hwGen)
    , mWroteValidData(false)
    , mXrtCoreDevice(d)
  {
  }

  bool AIEStatusWriter::write(bool openNewFile)
  {
    return writeDevice(openNewFile, xrt::device(mXrtCoreDevice));
  }

  bool AIEStatusWriter::write(bool openNewFile, void* handle)
  {
    return writeDevice(openNewFile, xrt::device(mXrtCoreDevice));
  }

  bool AIEStatusWriter::writeDevice(bool openNewFile, xrt::device xrtDevice)
  {
    // TBD on support of 'all'
    //auto aieInfoStr = xrtDevice.get_info<xrt::info::device::all>();
    auto aieInfoStr = xrtDevice.get_info<xrt::info::device::aie>();
    bpt::ptree pt_aie;

    // Catch if not valid or errors (e.g., json parsing of non-UTF-8 characters)
    bool aieValid = false;
    if (!aieInfoStr.empty()) {
      aieValid = true;
      try {
        std::stringstream ss(aieInfoStr);
        bpt::read_json(ss, pt_aie);
        if (!pt_aie.get_child_optional("graphs")) 
          aieValid = false;
      } catch (...) {
        aieValid = false;
      }
    }

    // AIE tiles section must be valid to write
    if (!aieValid)
      return true;

    // Now that we're valid, let's read the report the rest
    std::string memoryInfoStr;
    if (mHardwareGen > 1)
      memoryInfoStr = xrtDevice.get_info<xrt::info::device::aie_mem>();
    auto interfaceInfoStr = xrtDevice.get_info<xrt::info::device::aie_shim>();
    
    bpt::ptree pt_memory;
    bpt::ptree pt_interface;
    
    // Catch if not valid or errors
    bool memoryValid = false;
    if (!memoryInfoStr.empty()) {
      memoryValid = true;
      try {
        std::stringstream ss(memoryInfoStr);
        bpt::read_json(ss, pt_memory);
      } catch (...) {
        memoryValid = false;
      }
    }

    bool interfaceValid = false;
    if (!interfaceInfoStr.empty()) {
      interfaceValid = true;
      try {
        std::stringstream ss(interfaceInfoStr);
        bpt::read_json(ss, pt_interface);
      } catch (...) {
        interfaceValid = false;
      }
    }
    
    // Refresh and write to file
    refreshFile();

    bpt::ptree pt_top;
    bpt::ptree pt_schema;

    pt_schema.put("schema", "JSON");
    pt_schema.put("creation_date", getCurrentDateTime());
    pt_top.add_child("schema_version", pt_schema);

    // NOTE: only one device supported
    bpt::ptree pt_device;
    pt_device.add_child("aie_metadata", pt_aie);
    if (memoryValid)
      pt_device.add_child("aie_mem_status", pt_memory);
    if (interfaceValid)
      pt_device.add_child("aie_shim_status", pt_interface);
    
    bpt::ptree pt_devices;
    pt_devices.push_back(std::make_pair("", pt_device));
    pt_top.add_child("devices", pt_devices);

    bpt::write_json(fout, pt_top);
    mWroteValidData = true;

    if (openNewFile)
      switchFiles();
    return true;
  }

  // Warn if application exits without writing valid data
  AIEStatusWriter::~AIEStatusWriter()
  {
    if (!mWroteValidData) {
      std::string msg("No valid data found for AIE status. Please run xbutil.");
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
    }
  }

} // end namespace xdp
