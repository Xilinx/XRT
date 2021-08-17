/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef NATIVE_EVENTS_DOT_H
#define NATIVE_EVENTS_DOT_H

#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  class NativeAPICall : public APICall
  {
  public:
    XDP_EXPORT NativeAPICall(uint64_t s_id, double ts, uint64_t name) ;
    XDP_EXPORT ~NativeAPICall() ;

    XDP_EXPORT virtual bool isNativeHostEvent() { return true ; }
    XDP_EXPORT virtual bool isWrite() { return false ; }
    XDP_EXPORT virtual bool isRead()  { return false ; }

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket) ;

    // For printing out the event in a different bucket as a different
    //  type of event, without having to store additional events in the database
    XDP_EXPORT virtual void dumpSync(std::ofstream& fout, uint32_t bucket);
  } ;

  class NativeSyncRead : public NativeAPICall
  {
  private:
    uint64_t readStr ;
  public:
    XDP_EXPORT NativeSyncRead(uint64_t s_id, double ts, uint64_t name,
                              uint64_t r) ;
    XDP_EXPORT ~NativeSyncRead() = default ;

    XDP_EXPORT virtual bool isRead() { return true ; }
    XDP_EXPORT virtual void dumpSync(std::ofstream& fout, uint32_t bucket) ;
  } ;

  class NativeSyncWrite : public NativeAPICall
  {
  private:
    uint64_t writeStr ;
  public:
    XDP_EXPORT NativeSyncWrite(uint64_t s_id, double ts, uint64_t name,
                               uint64_t w) ;
    XDP_EXPORT ~NativeSyncWrite() = default ;

    XDP_EXPORT virtual bool isWrite() { return true ; }
    XDP_EXPORT virtual void dumpSync(std::ofstream& fout, uint32_t bucket) ;
  } ;

} // end namespace xdp

#endif
