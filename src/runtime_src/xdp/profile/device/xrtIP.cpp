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

constexpr uint32_t REGSIZE_BYTES = 0x4;

XrtIP::
XrtIP
  ( Device* handle
  , const std::unique_ptr<ip_metadata>& ip_metadata_section
  , const std::string& fullname
  )
  : xdpDevice(handle)
  , fullname(fullname)
  , deadlockDiagnosis(std::string())
  , index(-1)
{
  size_t pos = fullname.find(':');
  kernelName = fullname.substr(0, pos);

  // Find register info for our kernel
  for (auto& pair : ip_metadata_section->kernel_infos) {
    auto& kname = pair.first;
    if (kname.find(kernelName) != std::string ::npos) {
      regInfo = pair.second;
      break;
    }
  }

  if (regInfo.empty())
    return;

  // Try to enable register access
  uint32_t low  = regInfo.begin()->first;
  uint32_t high = regInfo.rbegin()->first + REGSIZE_BYTES;
  index = xdpDevice->initXrtIP(fullname.c_str(), low, (high - low));
}

int XrtIP::
read(uint32_t offset, uint32_t* data)
{
  return xdpDevice->readXrtIP(index, offset, data);
}

std::string& XrtIP::
getDeadlockDiagnosis(bool print)
{
  if (regInfo.empty() || !initialized())
    return deadlockDiagnosis;

  // Query this IP
  for (const auto& e: regInfo) {
    uint32_t regdata = 0;
    auto offset = e.first;
    auto& messages = e.second;
    read(offset, &regdata);
    for (unsigned int i=0; i < num_bits_deadlock_diagnosis; i++) {
      if ((regdata >> i) & 0x1)
        deadlockDiagnosis += messages[i] + "\n";
    }
  }

  if (print)
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", deadlockDiagnosis);
  return deadlockDiagnosis;
}

}   // namespace xdp
