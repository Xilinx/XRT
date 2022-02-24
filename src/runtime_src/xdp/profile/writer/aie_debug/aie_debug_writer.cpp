/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "xdp/profile/writer/aie_debug/aie_debug_writer.h"
#include "xdp/profile/database/database.h"

#include <vector>
#include <boost/optional/optional.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace xdp {

  /*
   * Writer for AIE tiles status
   */

  AIEDebugWriter::AIEDebugWriter(const char* fileName,
               const char* deviceName, uint64_t deviceIndex)
    : VPWriter(fileName)
    , mDeviceName(deviceName)
    , mDeviceIndex(deviceIndex)
  {
  }

  bool AIEDebugWriter::write(bool openNewFile)
  {
    refreshFile();

    auto xrtDevice = xrt::device((int)mDeviceIndex);
    auto aieInfoStr = xrtDevice.get_info<xrt::info::device::aie>();

    // Make sure report is not empty and no error occurred
    // NOTE: catch json parser errors (e.g., non-UTF-8 characters)
    if (aieInfoStr.empty())
      return true;
    try {
      std::stringstream ss(aieInfoStr);
      boost::property_tree::ptree pt_aie;
      boost::property_tree::read_json(ss, pt_aie);
      if (pt_aie.get_child_optional("error_msg"))
        return true;
    } catch (...) {
      return true;
    }

    // Write approved AIE report to file
    fout << aieInfoStr << std::endl;

    if (openNewFile)
      switchFiles();
    return true;
  }

  bool AIEDebugWriter::write(bool openNewFile, void* handle)
  {
    refreshFile();

    auto xrtDevice = xrt::device(handle);
    auto aieInfoStr = xrtDevice.get_info<xrt::info::device::aie>();

    // Make sure report is not empty and no error occurred
    // NOTE: catch json parser errors (e.g., non-UTF-8 characters)
    if (aieInfoStr.empty())
      return true;
    try {
      std::stringstream ss(aieInfoStr);
      boost::property_tree::ptree pt_aie;
      boost::property_tree::read_json(ss, pt_aie);
      if (pt_aie.get_child_optional("error_msg"))
        return true;
    } catch (...) {
      return true;
    }

    // Write approved AIE report to file
    fout << aieInfoStr << std::endl;

    if (openNewFile)
      switchFiles();
    return true;
  }

  /*
   * Writer for AIE shim tiles status
   */

  AIEShimDebugWriter::AIEShimDebugWriter(const char* fileName,
               const char* deviceName, uint64_t deviceIndex)
    : VPWriter(fileName)
    , mDeviceName(deviceName)
    , mDeviceIndex(deviceIndex)
  {
  }

  bool AIEShimDebugWriter::write(bool openNewFile)
  {
    refreshFile();

    auto xrtDevice = xrt::device((int)mDeviceIndex);
    auto aieShimInfoStr = xrtDevice.get_info<xrt::info::device::aie_shim>();
    
    // Make sure report is not empty and no error occurred
    // NOTE: catch json parser errors (e.g., non-UTF-8 characters)
    if (aieShimInfoStr.empty())
      return true;
    try {
      std::stringstream ss(aieShimInfoStr);
      boost::property_tree::ptree pt_aie;
      boost::property_tree::read_json(ss, pt_aie);
      if (pt_aie.get_child_optional("error_msg"))
        return true;
    } catch (...) {
      return true;
    }

    // Write approved AIE shim report to file
    fout << aieShimInfoStr << std::endl;

    if (openNewFile)
      switchFiles();
    return true;
  }

  bool AIEShimDebugWriter::write(bool openNewFile, void* handle)
  {
    refreshFile();

    auto xrtDevice = xrt::device(handle);
    auto aieShimInfoStr = xrtDevice.get_info<xrt::info::device::aie_shim>();
    
    // Make sure report is not empty and no error occurred
    // NOTE: catch json parser errors (e.g., non-UTF-8 characters)
    if (aieShimInfoStr.empty())
      return true;
    try {
      std::stringstream ss(aieShimInfoStr);
      boost::property_tree::ptree pt_aie;
      boost::property_tree::read_json(ss, pt_aie);
      if (pt_aie.get_child_optional("error_msg"))
        return true;
    } catch (...) {
      return true;
    }

    // Write approved AIE shim report to file
    fout << aieShimInfoStr << std::endl;

    if (openNewFile)
      switchFiles();
    return true;
  }

} // end namespace xdp
