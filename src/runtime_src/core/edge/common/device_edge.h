/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef DEVICE_EDGE_H
#define DEVICE_EDGE_H

#include "common/device.h"

namespace xrt_core {

class device_edge : public device
{
public:
  /**
   * device_edge() - Construct and open a device
   */
  device_edge(id_type device_id, bool user);

  /**
   * device_edge() - Construct from a device_handle
   *
   * Bypasses call to xclOpen
   */
  device_edge(handle_type device_handle, id_type device_id);

  ~device_edge();

  virtual void
  get_info(boost::property_tree::ptree& pt) const;

  /**
   * get_device_handle() - Get underlying shim device handle
   *
   * Throws if called on non userof devices
   */
  xclDeviceHandle
  get_device_handle() const;

private:
  xclDeviceHandle m_handle = XRT_NULL_HANDLE;
  bool m_userpf;
  bool m_managed;
};

} // xrt_core

#endif /* DEVICE_EDGE_H */
