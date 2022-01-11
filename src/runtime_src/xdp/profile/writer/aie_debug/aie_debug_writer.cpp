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

#include <vector>

#include "xdp/profile/database/database.h"

namespace xdp {

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
    fout << aieInfoStr << std::endl;

    if (openNewFile)
      switchFiles();
    return true;
  }

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
    fout << aieShimInfoStr << std::endl;

    if (openNewFile)
      switchFiles();
    return true;
  }

} // end namespace xdp
