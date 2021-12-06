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

#include <string>

#include "xdp/profile/database/static_info/pl_constructs.h"

namespace xdp {

  ComputeUnitInstance::ComputeUnitInstance(int32_t i, const std::string& n)
    : index(i)
  {
    std::string fullName(n) ;
    size_t pos = fullName.find(':') ;
    kernelName = fullName.substr(0, pos) ;
    name = fullName.substr(pos + 1) ;
  }

  ComputeUnitInstance::~ComputeUnitInstance()
  {
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

} // end namespace xdp

