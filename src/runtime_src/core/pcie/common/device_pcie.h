/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef DEVICE_PCIE_H
#define DEVICE_PCIE_H

#include "common/device.h"

namespace xrt_core {

class device_pcie : public device
{
public:
  /**
   * device_pcie() - Construct from a device_handle
   */
  device_pcie(handle_type device_handle, id_type device_id, bool user);

  virtual void
  get_info(boost::property_tree::ptree& pt) const;

  /**
   * get_device_handle() - Get underlying shim device handle
   *
   * Throws if called on non userof devices
   */
  xclDeviceHandle
  get_device_handle() const;

  /**
   * is_userpf_device() - Is this device a userpf
   */
  bool
  is_userpf() const
  {
    return m_userpf;
  }

private:
  xclDeviceHandle m_handle = XRT_NULL_HANDLE;
  bool m_userpf;
};

} // xrt_core

#endif
