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

  void setupAIEProfiler(uint8_t col, uint8_t row, uint32_t start, uint32_t end)
  {
    std::cout << col << " " << row << " " << start << " " << end << std::endl;
    // aiegraph_handle_.clearTransactionBuffer();
    // buffers_.clear();
    // inputs_.clear();
    // outputs_.clear();
    // bufferidx_to_xrt_subbo_.clear();
    // buffer_to_xrt_bo_.clear();

    // std::vector<uint32_t>pc_list;
    // pc_list.push_back(start);
    // pc_list.push_back(end);
  }

  bool AieProfile_WinImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    std::cout << deviceId << handle << std::endl;
    return true;
  }

  void AieProfile_WinImpl::poll(uint32_t index, void* handle)
  {
    std::cout << index << handle << std::endl;
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
