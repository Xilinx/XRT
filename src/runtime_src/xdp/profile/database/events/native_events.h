/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef NATIVE_EVENTS_DOT_H
#define NATIVE_EVENTS_DOT_H

#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  class NativeAPICall : public APICall
  {
  public:
    XDP_CORE_EXPORT NativeAPICall(uint64_t s_id, double ts, uint64_t name);
    XDP_CORE_EXPORT ~NativeAPICall() = default;

    XDP_CORE_EXPORT virtual bool isNativeHostEvent() { return true; }

    XDP_CORE_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket);
  };

  class NativeSyncRead : public NativeAPICall
  {
  private:
    uint64_t readStr;
  public:
    XDP_CORE_EXPORT NativeSyncRead(uint64_t s_id, double ts, uint64_t name);
    XDP_CORE_EXPORT ~NativeSyncRead() = default;

    XDP_CORE_EXPORT virtual bool isNativeRead() override { return true; }

    // For printing out the event in a different bucket as a different
    //  type of event, without having to store additional events in the database
    XDP_CORE_EXPORT virtual void dumpSync(std::ofstream& fout, uint32_t bucket) override;
  };

  class NativeSyncWrite : public NativeAPICall
  {
  private:
    uint64_t writeStr;
  public:
    XDP_CORE_EXPORT NativeSyncWrite(uint64_t s_id, double ts, uint64_t name);
    XDP_CORE_EXPORT ~NativeSyncWrite() = default;

    XDP_CORE_EXPORT virtual bool isNativeWrite() override { return true; }

    // For printing out the event in a different bucket as a different
    //  type of event, without having to store additional events in the databaes
    XDP_CORE_EXPORT virtual void dumpSync(std::ofstream& fout, uint32_t bucket) override;
  };

} // end namespace xdp

#endif
