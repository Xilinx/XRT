/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#ifndef AIE_DEBUG_WRITER_DOT_H
#define AIE_DEBUG_WRITER_DOT_H

#include "xdp/profile/writer/vp_base/vp_writer.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"
#include <string>

namespace xdp {
class AIEDebugWriter : public VPWriter
  {
public:
    AIEDebugWriter(const char* fileName, const char* deviceName,
                   uint64_t deviceIndex, AieDebugPlugin* AieDebugPluginPtr);
    ~AIEDebugWriter()=default;

    void writeHeader();
    void writerDataColumnHeader();
    virtual bool write(bool openNewFile = true);

  private:
    std::string mDeviceName;
    uint64_t mDeviceIndex;
    //int mHardwareGen;
    bool mHeaderWritten;
    //bool mWroteValidData;
    AieDebugPlugin* ptrtoAieDebugPlugin;
  };
} // end namespace xdp

#endif
