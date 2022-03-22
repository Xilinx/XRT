/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include <string>
#include "xdp/profile/writer/vp_base/vp_writer.h"

namespace xdp {

  /*
   * Writer for AIE tiles status
   */
  class AIEDebugWriter : public VPWriter
  {
  public:
    AIEDebugWriter(const char* fileName, const char* deviceName,
                   uint64_t deviceIndex);
    ~AIEDebugWriter();

    virtual bool write(bool openNewFile);
    virtual bool write(bool openNewFile, void* handle);

  private:
    std::string mDeviceName;
    uint64_t mDeviceIndex;
    bool mWroteValidData;
  };

  /*
   * Writer for AIE shim tiles status
   */
  class AIEShimDebugWriter : public VPWriter
  {
  public:
    AIEShimDebugWriter(const char* fileName, const char* deviceName,
                       uint64_t deviceIndex);
    ~AIEShimDebugWriter();

    virtual bool write(bool openNewFile);
    virtual bool write(bool openNewFile, void* handle);

  private:
    std::string mDeviceName;
    uint64_t mDeviceIndex;
    bool mWroteValidData;
  };

} // end namespace xdp

#endif
