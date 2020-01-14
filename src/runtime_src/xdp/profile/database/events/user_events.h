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

#ifndef USER_EVENT_DOT_H
#define USER_EVENT_DOT_H

#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  // **********************
  // User level events
  // **********************

  class UserMarker : public VTFEvent
  {
  private:
    UserMarker() = delete ;
  public:
    virtual bool isUserEvent() { return true ; } 

    UserMarker(uint64_t s_id, double ts) ;
    ~UserMarker() ;
  } ;

  class UserRange : public VTFEvent
  {
  private:
    UserRange() = delete ;
  public:
    virtual bool isUserEvent() { return true ; } 

    UserRange(uint64_t s_id, double ts) ;
    ~UserRange() ;
  } ;

} // end namespace xdp

#endif
