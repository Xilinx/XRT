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

#ifndef HAL_API_CALLS_DOT_H
#define HAL_API_CALLS_DOT_H

#include <fstream>

#include "xdp/profile/database/events/vtf_event.h"

#include "xdp/config.h"

namespace xdp {

  class HALAPICall : public APICall
  {
  private:
    HALAPICall() = delete ;
  public:
    XDP_EXPORT HALAPICall(uint64_t s_id, double ts, uint64_t name) ;
    XDP_EXPORT ~HALAPICall() ;

    virtual bool isHALAPI()       { return true ; }
    virtual bool isHALHostEvent() { return true ; }

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

  class AllocBoCall : public HALAPICall
  {
  private:
    AllocBoCall() = delete ;
  public:
    XDP_EXPORT AllocBoCall(uint64_t s_id, double ts, uint64_t name) ;
    XDP_EXPORT ~AllocBoCall() ;

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;
  } ;

} // end namespace xdp

#endif
