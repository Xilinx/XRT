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

#include <string>

#define XDP_CORE_SOURCE

#include "core/common/message.h"

#include "xdp/profile/database/static_info/pl_constructs.h"

// Anonymous namespace for local static helper functions
namespace {

  static std::string convertBankToDDR(const std::string& memory)
  {
    auto loc = memory.find("bank");
    if (loc == std::string::npos)
      return memory;

    std::string ddr = "DDR[";
    ddr += memory.substr(loc + 4);
    ddr += "]";
    return ddr;
  }

  // This function will check if the spTag a particular port or argument
  // is connected to belongs to a particular memory resource.  If the strings
  // are an exact match, or if the memory resource is a range that the
  // spTag is a part of, we return true.
  static bool compare(const std::string& spTag, const std::string& memory)
  {
    if (spTag == memory)
      return true;

    // On platforms that have HOST bridge enabled, the spTag and memory
    // are hardcoded to specific values that don't match the rest of the
    // algorithm.
    if (spTag == "HOST[0]" && memory == "HOST")
      return true;

    // On some platforms, the memory name is still formatted as "bank0"
    // and needs to be changed to DDR[0].
    std::string mem = convertBankToDDR(memory);

    // On Versal, MC_NOC0 and equivalent actually represent DDR connections.
    // We do that hard coded check here
    if (spTag.find("MC_NOC") != std::string::npos &&
        mem.find("DDR")      != std::string::npos)
      return true;

    // If it is not an exact match, check to see if there is a range
    // specification and if the spTag falls in that range.  For example,
    // PLRAM[2] should match PLRAM[0:2].

    auto bracePosSpTag    = spTag.find("[");
    auto endBracePosSpTag = spTag.find("]");
    auto bracePosMem      = mem.find("[");
    auto endBracePosMem   = mem.find("]");

    // Both the spTag and memory resource must have braces in order
    if (bracePosSpTag    == std::string::npos ||
        bracePosMem      == std::string::npos ||
        endBracePosSpTag == std::string::npos ||
        endBracePosMem   == std::string::npos)
      return false;

    // First, make sure the memory type before the brace is the same
    std::string spResource  = spTag.substr(0, bracePosSpTag);
    std::string memResource = mem.substr(0, bracePosMem);

    if (spResource != memResource)
      return false;

    // The two memory types are the same, so we need to check the range.
    // We are assuming the spTag is a single location and the memory has
    // the range.
    std::string spRange = spTag.substr(bracePosSpTag + 1,
                                       endBracePosSpTag - bracePosSpTag - 1);
    std::string memRange = mem.substr(bracePosMem + 1,
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
    , fullname(n)
  {
    size_t pos = fullname.find(':') ;
    kernelName = fullname.substr(0, pos) ;
    name = fullname.substr(pos + 1) ;
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

  IpMetadata::
  IpMetadata(const boost::property_tree::ptree& pt)
  : s_major(0)
  , s_minor(0)
  {
    auto& v = pt.get_child("version");
    s_major = v.get<uint32_t>("major");
    s_minor = v.get<uint32_t>("minor");
    auto& kernels = pt.get_child("kernels");
    for (const auto& k : kernels) {
      auto kname = k.second.get<std::string>("name");
      auto reglist = k.second.get_child("deadlock_register_list");
      KernelRegisterInfo kinfo;
      for (const auto& reg : reglist) {
        std::array<std::string, NUM_DEADLOCK_DIAGNOSIS_BITS> reginfo;
        auto offset_str = reg.second.get<std::string>("register_word_offset");
        uint32_t reg_offset = get_offset_from_string(offset_str);
        auto bitinfo = reg.second.get_child("register_bit_info");
        for (const auto& bits : bitinfo) {
          auto bit_offset = bits.second.get<uint32_t>("bit");
          auto bit_msg = bits.second.get<std::string>("message");
          reginfo[bit_offset] = bit_msg;
        }
        kinfo[reg_offset] = reginfo;
      }
      kernel_infos.push_back(std::make_pair(kname, kinfo));
    }
  }

  /*
   * Useful for debug
   */
  void IpMetadata::
  print()
  {
    std::stringstream ss;
    ss << "Major : " << s_major << std::endl;
    ss << "Minor : " << s_minor << std::endl;
    for (const auto& kernel_info : kernel_infos) {
      ss << kernel_info.first << " : \n";
      for (const auto& reginfo : kernel_info.second) {
        ss <<std::hex << "0x" << reginfo.first << " :\n" << std::dec;
        for (const auto& bitstring : reginfo.second)
          if (!bitstring.empty())
            ss << " " << bitstring << "\n";
      }
      ss << std::endl;
    }
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", ss.str());
  }

} // end namespace xdp

