/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_SOURCE

#include "xdp/profile/database/events/hal_api_calls.h"

namespace xdp {

  HALAPICall::HALAPICall(uint64_t s_id, double ts, uint64_t name)
            : APICall(s_id, ts, name, HAL_API_CALL)
  {
  }

  HALAPICall::~HALAPICall()
  {
  }

  void HALAPICall::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << "," << functionName << std::endl ;
  }

  AllocBoCall::AllocBoCall(uint64_t s_id, double ts, uint64_t name) 
             : HALAPICall(s_id, ts, name)
  {
  }

  AllocBoCall::~AllocBoCall()
  {
  }

  void AllocBoCall::dump(std::ofstream& fout, uint32_t bucket)
  {
    HALAPICall::dump(fout, bucket) ;
  }

} // end namespace xdp
