/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"

#include "xdp/profile/util/util.h"

std::string getDebugIpLayoutPath(std::shared_ptr<xrt_core::device> coreDevice)
{
  std::string path = "";
  uint32_t size = 512;
  try {
    path = xrt_core::device_query<xrt_core::query::debug_ip_layout_path>(coreDevice, size);
  } catch (const xrt_core::query::no_such_key&) {
//    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Device query for Debug IP Layout not implemented");
  } catch (const std::exception &) {
    xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", "Failed to retrieve Debug IP Layout path");
  }
  return path;
}
