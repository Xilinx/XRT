/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef AIE_WRITER_DOT_H
#define AIE_WRITER_DOT_H

#include <string>
#include "xdp/profile/writer/vp_base/vp_writer.h"

namespace xdp {

  class AIEProfilingWriter : public VPWriter
  {
  public:
    AIEProfilingWriter(const char* fileName, const char* deviceName, 
                       uint64_t deviceID);
    ~AIEProfilingWriter() = default;

    virtual void writerDataColumnHeader();
    void writeHeader();
    void writeMetricSettings();
    virtual bool write(bool openNewFile = true);
    
  private:
    std::string mDeviceName;
    uint64_t mDeviceID;
    bool mHeaderWritten;
  };

} // end namespace xdp

#endif
