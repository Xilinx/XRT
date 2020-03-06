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

#include "xdp/profile/database/events/opencl_api_calls.h"

namespace xdp {

  OpenCLAPICall::OpenCLAPICall(uint64_t s_id, double ts, unsigned int /*f_id*/,
                               uint64_t name, uint64_t q)
      : APICall(s_id, ts, name, OPENCL_API_CALL),
        queueAddress(q)
  {
  }

  OpenCLAPICall::~OpenCLAPICall()
  {
  }

  void OpenCLAPICall::dump(std::ofstream& fout, int bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << "," << functionName << std::endl ;
  }

} // end namespace xdp
