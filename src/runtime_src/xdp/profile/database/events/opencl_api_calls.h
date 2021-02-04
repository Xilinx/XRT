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

#ifndef OPENCL_API_CALLS_DOT_H
#define OPENCL_API_CALLS_DOT_H

#include "xdp/profile/database/events/vtf_event.h"

#include "xdp/config.h"

namespace xdp {

  class OpenCLAPICall : public APICall
  {
  private:
    uint64_t queueAddress ;

    OpenCLAPICall() = delete ;
  public:
    XDP_EXPORT OpenCLAPICall(uint64_t s_id, double ts, uint64_t f_id, uint64_t name, uint64_t q);
    XDP_EXPORT ~OpenCLAPICall() ;

    inline uint64_t getQueueAddress() { return queueAddress ; } 

    virtual bool isOpenCLAPI() { return true ; }
    virtual bool isOpenCLHostEvent() { return true ; }
    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

} // end namespace xdp

#endif
