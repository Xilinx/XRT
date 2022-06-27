/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <string>

#define XDP_SOURCE

#include "xdp/profile/database/static_info/pl_constructs.h"

namespace xdp {

  void Port::addMemoryConnection(Memory* mem)
  {
    for (auto m : memories) {
      if (m == mem)
        return;
    }
    memories.push_back(mem);
  }

  ComputeUnitInstance::ComputeUnitInstance(int32_t i, const std::string& n)
    : index(i)
  {
    std::string fullName(n) ;
    size_t pos = fullName.find(':') ;
    kernelName = fullName.substr(0, pos) ;
    name = fullName.substr(pos + 1) ;
  }

  std::string ComputeUnitInstance::getDim()
  {
    std::string combined ;
    combined =  std::to_string(dim[0]) ;
    combined += ":" ;
    combined += std::to_string(dim[1]) ;
    combined += ":" ;
    combined += std::to_string(dim[2]) ;

    return combined ;
  }

  void ComputeUnitInstance::addConnection(int32_t argIdx, int32_t memIdx)
  {
    if (connections.find(argIdx) == connections.end()) {
      std::vector<int32_t> mems(1, memIdx) ;
      connections[argIdx] = mems ;
      return ;
    }
    connections[argIdx].push_back(memIdx) ;
  }

  void ComputeUnitInstance::addPort(const std::string& n, int32_t w)
  {
    masterPorts.push_back(std::move(Port(n, w)));
  }

  void ComputeUnitInstance::addArgToPort(const std::string& arg,
                                         const std::string& portName)
  {
    for (auto& port : masterPorts) {
      if (port.name == portName)
        port.args.push_back(arg);
    }
  }

  void ComputeUnitInstance::addMemoryToPort(Memory* mem,
                                            const std::string& portName)
  {
    if (mem == nullptr)
      return;

    for (auto& port : masterPorts) {
      if (port.name == portName)
        port.addMemoryConnection(mem);
    }
  }

  void ComputeUnitInstance::connectArgToMemory(const std::string& portName,
                                               const std::string& arg,
                                               Memory* mem)
  {
    for (auto& port : masterPorts) {
      if (port.name == portName)
        port.argToMemory[arg] = mem;
    }
  }

  Port* ComputeUnitInstance::getPort(const std::string& portName)
  {
    for (auto& port : masterPorts) {
      if (port.name == portName)
        return &port;
    }
    return nullptr;
  }

  void Memory::convertBankToDDR()
  {
    auto loc = tag.find("bank");
    if (loc == std::string::npos) {
      spTag = tag;
      return;
    }

    std::string ddr = "DDR[";
    ddr += tag.substr(loc + 4);
    ddr += "]";
    spTag = ddr;
  }

} // end namespace xdp

