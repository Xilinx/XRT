/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/device/device_intf.h"

namespace xdp {

  PLInfo::~PLInfo()
  {
    for (auto& i : cus) {
      delete i.second ;
    }
    for (auto& i : memoryInfo) {
      delete i.second ;
    }
    for (auto i : ams) {
      delete i ;
    }
    for (auto i : aims) {
      delete i ;
    }
    for (auto i : asms) {
      delete i ;
    }
  }

  std::vector<ComputeUnitInstance*>
  PLInfo::collectCUs(const std::string& kernelName)
  {
    std::vector<ComputeUnitInstance*> collected;

    for (auto& iter : cus) {
      auto instance = iter.second;
      if (instance->getKernelName() == kernelName)
        collected.push_back(instance);
    }
    return collected;
  }

  void PLInfo::addComputeUnitPorts(const std::string& kernelName,
                                   const std::string& portName,
                                   int32_t portWidth)
  {
    for (const auto& iter : cus) {
      auto cu = iter.second;
      if (cu->getKernelName() == kernelName)
        cu->addPort(portName, portWidth);
    }
  }

  void PLInfo::addArgToPort(const std::string& kernelName,
                            const std::string& argName,
                            const std::string& portName)
  {
    for (const auto& iter : cus) {
      auto cu = iter.second;
      if (cu->getKernelName() == kernelName)
        cu->addArgToPort(argName, portName);
    }
  }

  void PLInfo::connectArgToMemory(const std::string& cuName,
                                  const std::string& portName,
                                  const std::string& argName,
                                  int32_t memId)
  {
    if (memoryInfo.find(memId) == memoryInfo.end())
      return;

    Memory* mem = memoryInfo[memId];
    for (const auto& iter : cus) {
      auto cu = iter.second;
      if (cu->getName() == cuName)
        cu->connectArgToMemory(portName, argName, mem);
    }
  }

  AIEInfo::~AIEInfo()
  {
    for (auto i : nocList) {
      delete i ;
    }
  }

  XclbinInfo::~XclbinInfo()
  {
    if (deviceIntf)
      delete deviceIntf ;
  }

} // end namespace xdp
