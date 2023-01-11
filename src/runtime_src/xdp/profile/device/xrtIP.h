/*
 * Copyright (C) 2019 Xilinx Inc - All rights reserved
 * Xilinx Debug & Profile (XDP) APIs
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

#ifndef XDP_PROFILE_COMPUTE_UNIT_IP_H
#define XDP_PROFILE_COMPUTE_UNIT_IP_H

#include <iostream>
#include <map>
#include <vector>

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
 * 
 */
class XrtIP {
public:

  /**
   * The constructor takes a device handle and a ip index
   * means that the instance of this class has a one-to-one
   * association with one specific IP on one specific device.
   * During the construction, the exclusive access to this
   * IP will be requested, otherwise exception will be thrown.
   */
  XrtIP(
    Device* handle       /** < xrt or hal device handle */,
    std::string fullname /** < fullname of the IP in IP_LAYOUT */,
    uint64_t baseAddress /** < base Address of the IP in IP_LAYOUT */
  );
  ~XrtIP() {}

  // For now, this is all we need
  void printDeadlockDiagnosis(const std::map<uint32_t, std::vector<std::string>>& config);

private:
  Device* xdpDevice;
  std::string fullname;
  uint64_t baseAddress;

private:
  int read(uint32_t offset, uint32_t* data);
};

} //  xdp

#endif

