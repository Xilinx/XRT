/*
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
 * Xilinx Runtime IP Access for debug
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

#ifndef XDP_XRT_IP_H
#define XDP_XRT_IP_H

#include <iostream>
#include <map>
#include <vector>

#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp_base_device.h"

namespace xdp {

/**
 * XRT IP
 * 
 * Description:
 * 
 * This class represents the high level access to an XRT IP on the device.
 * An XRT IP is one which is defined in IP_LAYOUT
 * We can't do exclusive access or inherit from profile IP as we don't own this IP
 * For now, the only functionality that it needs to support is read registers
 * specifically for deadlock information.
 */
class XrtIP {
public:

  XrtIP(
    Device* handle       /** < xrt or hal device handle */,
    const std::unique_ptr<ip_metadata>& ip_metadata_section,
    const std::string& fullname /** < fullname of the IP in IP_LAYOUT */
  );
  ~XrtIP() {}

  // For now, this is all we need
  std::string& getDeadlockDiagnosis(bool print=true);

private:
  Device* xdpDevice;
  std::string fullname;
  std::string deadlockDiagnosis;
  std::string kernelName;
  kernel_reginfo regInfo;
  int32_t index;

private:
  int read(uint32_t offset, uint32_t* data);

  bool initialized() { return index >= 0; }
};

} //  xdp

#endif

