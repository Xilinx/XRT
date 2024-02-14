/**
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

#ifndef AIE_TRACE_TIMESTAMPS_WRITER_DOT_H
#define AIE_TRACE_TIMESTAMPS_WRITER_DOT_H

#include <string>
#include "xdp/profile/writer/vp_base/vp_writer.h"

namespace xdp {

  class AIETraceTimestampsWriter : public VPWriter
  {
  public:
    AIETraceTimestampsWriter(const char* fileName, const char* deviceName, 
                             uint64_t deviceIndex);
    ~AIETraceTimestampsWriter();

    virtual bool write(bool openNewFile);
    
  private:
    std::string mDeviceName;
    uint64_t mDeviceIndex;
  };

} // end namespace xdp

#endif
