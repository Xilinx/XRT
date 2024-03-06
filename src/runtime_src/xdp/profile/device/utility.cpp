/**
 * Copyright (C) 2022 Xilinx, Inc
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

#define XDP_CORE_SOURCE

#include "core/common/system.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"

#include "xdp/profile/device/utility.h"

namespace xdp { namespace util {

  uint64_t getAIMSlotId(uint64_t idx) {
    return ((idx - min_trace_id_aim)/num_trace_id_per_aim);
  }

  uint64_t getAMSlotId(uint64_t idx) {
    return ((idx - min_trace_id_am)/num_trace_id_per_am);
  }

  uint64_t getASMSlotId(uint64_t idx) {
    return ((idx - min_trace_id_asm)/num_trace_id_per_asm);
  }

  std::string getDebugIpLayoutPath(void* deviceHandle)
  {
    std::string path = "";
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::get_userpf_device(deviceHandle);
    if (!coreDevice) {
      return path;
    }
    try {
      path = xrt_core::device_query<xrt_core::query::debug_ip_layout_path>(coreDevice, sysfs_max_path_length);
    } catch (const xrt_core::query::no_such_key&) {
      //  xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Device query for Debug IP Layout not implemented");
    } catch (const std::exception &) {
      xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", "Failed to retrieve Debug IP Layout path");
    }
    return path;
  }

  std::string getDeviceName(void* deviceHandle)
  {
    std::string deviceName = "";
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::get_userpf_device(deviceHandle);
    if (!coreDevice) {
      return deviceName;
    }
    try {
      deviceName = xrt_core::device_query<xrt_core::query::rom_vbnv>(coreDevice);
    } catch (const xrt_core::query::no_such_key&) {
      //  xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Device query for Device Name not implemented");
    } catch (const std::exception &) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Failed to retrieve Device Name");
    }
    return deviceName;
  }

} // end namespace util
} // end namespace xdp

