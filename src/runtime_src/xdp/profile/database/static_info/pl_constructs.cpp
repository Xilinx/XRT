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

// Anonymous namespace for local static helper functions
namespace {

  // This function will check if the spTag a particular port or argument
  // is connected to belongs to a particular memory resource.  If the strings
  // are an exact match, or if the memory resource is a range that the
  // spTag is a part of, we return true.
  static bool compare(const std::string& spTag, const std::string& memory)
  {
    if (spTag == memory)
      return true;

    // If it is not an exact match, check to see if there is a range
    // specification and if the spTag falls in that range.  For example,
    // PLRAM[2] should match PLRAM[0:2].

    auto bracePosSpTag    = spTag.find("[");
    auto endBracePosSpTag = spTag.find("]");
    auto bracePosMem      = memory.find("[");
    auto endBracePosMem   = memory.find("]");

    // Both the spTag and memory resource must have braces in order
    if (bracePosSpTag    == std::string::npos ||
        bracePosMem      == std::string::npos ||
        endBracePosSpTag == std::string::npos ||
        endBracePosMem   == std::string::npos)
      return false;

    // First, make sure the memory type before the brace is the same
    std::string spResource  = spTag.substr(0, bracePosSpTag);
    std::string memResource = memory.substr(0, bracePosMem);

    if (spResource != memResource)
      return false;

    // The two memory types are the same, so we need to check the range.
    // We are assuming the spTag is a single location and the memory has
    // the range.
    std::string spRange = spTag.substr(bracePosSpTag + 1,
                                       endBracePosSpTag - bracePosSpTag - 1);
    std::string memRange = memory.substr(bracePosMem + 1,
                                         endBracePosMem - bracePosMem - 1);

    auto colonPos = memRange.find(":");
    if (colonPos == std::string::npos)
      return false;

    int spBank = 0;
    try {
      spBank = std::stoi(spRange);
    } catch (std::exception&) {
      // If the spRange isn't actually a single integer, an exception
      // will be thrown and we should just assume the spTag does not match
      // the memory range
      return false ;
    }

    std::string rangeStart = memRange.substr(0, colonPos);
    std::string rangeEnd = memRange.substr(colonPos + 1);

    int memRangeStart = 0;
    int memRangeEnd = 0;
    try {
      memRangeStart = std::stoi(rangeStart);
      memRangeEnd = std::stoi(rangeEnd);
    } catch(std::exception&) {
      // The start and/or end of the range aren't well formed integers.  Just
      // return false then as we cannot compare the range.
      return false;
    }

    // If the specified bank is in the range, return true
    return (spBank >= memRangeStart) && (spBank <= memRangeEnd);
  }

} // end anonymous namespace

namespace xdp {

  void Port::addMemoryConnection(Memory* mem)
  {
    for (auto m : memories) {
      if (m == mem)
        return;
    }
    memories.push_back(mem);
  }

  std::string Port::constructArgumentList(const std::string& memoryName)
  {
    std::string argList = "";
    bool first = true;
    for (auto& arg : args) {
      if (argToMemory[arg] && compare(argToMemory[arg]->spTag, memoryName)) {
        if (!first)
          argList += "|";
        argList += arg;
        first = false;
      }
    }
    return argList;
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

