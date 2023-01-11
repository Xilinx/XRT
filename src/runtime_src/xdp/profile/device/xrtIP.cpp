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
  std::string fullname,
  uint64_t baseAddress
) : xdpDevice(handle)
  , fullname(fullname)
  , baseAddress(baseAddress)
{}

int XrtIP::read(uint32_t offset, uint32_t* data) {
  return -1;
//  return xdpDevice->readXrtIP(fullname.c_str(), offset, data);
}

void XrtIP::printDeadlockDiagnosis(const std::map<uint32_t, std::vector<std::string>>& config)
{
  std::string output;
  for (const auto& e: config) {
    uint32_t reg = 0;
    auto offset = e.first;
    auto& messages = e.second;

    read(offset, &reg);
    for (unsigned int i=0; i<32; i++) {
      uint32_t bit_i = ((reg >> i) & 0x1);
      if (bit_i)
        output += messages[bit_i] + "\n";
    }
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", output);
  }
}

}   // namespace xdp
