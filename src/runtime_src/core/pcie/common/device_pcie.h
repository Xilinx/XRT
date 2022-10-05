// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef DEVICE_PCIE_H
#define DEVICE_PCIE_H

#include "core/common/device.h"

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
   * Throws if called on non userpf devices
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
