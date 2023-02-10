/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/native_events.h"

namespace xdp {

  NativeAPICall::NativeAPICall(uint64_t s_id, double ts, uint64_t name) :
    APICall(s_id, ts, name, NATIVE_API_CALL)
  {
  }

  void NativeAPICall::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket);
    fout << "," << functionName << "\n";
  }

  NativeSyncRead::NativeSyncRead(uint64_t s_id, double ts, uint64_t name) :
    NativeAPICall(s_id, ts, name)
  {
    readStr = VPDatabase::Instance()->getDynamicInfo().addString("READ");
  }

  void NativeSyncRead::dumpSync(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket);
    fout << "," << readStr << "\n";
  }

  NativeSyncWrite::NativeSyncWrite(uint64_t s_id, double ts, uint64_t name) :
    NativeAPICall(s_id, ts, name)
  {
    writeStr = VPDatabase::Instance()->getDynamicInfo().addString("WRITE");
  }

  void NativeSyncWrite::dumpSync(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket);
    fout << "," << writeStr << "\n";
  }

} // end namespace xdp
