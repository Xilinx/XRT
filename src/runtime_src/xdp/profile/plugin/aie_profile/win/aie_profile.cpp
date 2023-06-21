/**
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

#define XDP_SOURCE

#include "aie_profile.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cmath>
#include <cstring>
#include <memory>

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"

namespace xdp {

  AieProfile_WinImpl::AieProfile_WinImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata)
  {
    // auto spdevice = xrt_core::get_userpf_device(metadata->getHandle());
    // device = xrt::device(spdevice);

    // auto uuid = device.get_xclbin_uuid();

    // if (metadata->getHardwareGen() == 1)
    //   aie_profile_kernel = xrt::kernel(device, uuid.get(),
    //   "aie_profile_config");
    // else
    //   aie_profile_kernel = xrt::kernel(device, uuid.get(),
    //   "aie2_profile_config");
  }

  void AieProfile_WinImpl::updateDevice()
  {
    setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());
  }



  bool AieProfile_WinImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    std::cout << "reached setmetricssettings" << std::endl;
    std::cout << deviceId << handle << std::endl;

    uint16_t num_rows = metadata->getAIEConfigMetadata("num_rows").get_value<uint16_t>();
    uint16_t num_cols = metadata->getAIEConfigMetadata("num_columns").get_value<uint16_t>();
    std::cout << "NumRows: " << num_rows << std::endl;
    std::cout << "NumCols: " << num_cols << std::endl;
    return true;
  }

  void AieProfile_WinImpl::poll(uint32_t index, void* handle)
  {
    std::cout << "Polling! " << index << handle << std::endl;
    // For now, we are waiting for a way to retreive polling information from
    // the AIE.
    return;
  }

  void AieProfile_WinImpl::freeResources()
  {
    // TODO - if there are any resources we need to free after the application
    // is complete, it must be done here.
  }
}  // namespace xdp
