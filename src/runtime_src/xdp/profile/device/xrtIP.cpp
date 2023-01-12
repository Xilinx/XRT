/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "core/common/message.h"

#include "xrtIP.h"

namespace xdp {

XrtIP::XrtIP(
  Device* handle,
  std::shared_ptr<ip_metadata> ip_metadata_section,
  const std::string& fullname,
  uint64_t baseAddress
) : xdpDevice(handle)
  , ip_metadata_section(ip_metadata_section)
  , fullname(fullname)
  , baseAddress(baseAddress)
  , deadlockDiagnosis(std::string())
{}

int XrtIP::read(uint32_t offset, uint32_t* data) {
  return xdpDevice->readXrtIP(fullname.c_str(), offset, baseAddress, data);
}

std::string& XrtIP::getDeadlockDiagnosis(bool print)
{
  // Find register info for our kernel
  kernel_reginfo info;
  for (auto& pair : ip_metadata_section->kernel_infos) {
    auto& kname = pair.first;
    if (fullname.find(kname) != std::string ::npos)
      info = pair.second;
  }

  if (!info.empty())
    return deadlockDiagnosis;

  // Query this IP
  for (const auto& e: info) {
    uint32_t reg = 0;
    auto offset = e.first;
    auto& messages = e.second;

    read(offset, &reg);
    for (unsigned int i=0; i < num_bits_deadlock_diagnosis; i++) {
      uint32_t bit_i = ((reg >> i) & 0x1);
      if (bit_i)
        deadlockDiagnosis += messages[bit_i] + "\n";
    }

    if (print)
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", deadlockDiagnosis);
  }
  return deadlockDiagnosis;
}

}   // namespace xdp
