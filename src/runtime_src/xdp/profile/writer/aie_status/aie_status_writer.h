/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef AIE_STATUS_WRITER_DOT_H
#define AIE_STATUS_WRITER_DOT_H

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <string>

#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/writer/vp_base/vp_writer.h"

namespace bpt = boost::property_tree;

namespace xdp {

  /*
   * Writer for AIE status
   */
  class AIEStatusWriter : public VPWriter
  {
  public:
    AIEStatusWriter(const char* fileName, const char* deviceName,
                    uint64_t deviceIndex, int hwGen,
                    std::shared_ptr<xrt_core::device> d);
    ~AIEStatusWriter();

    virtual bool write(bool openNewFile);
    virtual bool write(bool openNewFile, void* handle);

  private:

    bool writeDevice(bool openNewFile, xrt::device xrtDevice);

  private:
    std::string mDeviceName;
    uint64_t mDeviceIndex;
    int mHardwareGen;
    bool mWroteValidData;
    std::shared_ptr<xrt_core::device> mXrtCoreDevice;
  };

} // end namespace xdp

#endif
