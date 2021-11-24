/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */
#ifndef DEVICE_WINDOWS_ALVEO_H
#define DEVICE_WINDOWS_ALVEO_H

#include "core/common/ishim.h"
#include "core/pcie/common/device_pcie.h"

namespace xrt_core {

// concrete class derives from device_pcie, but mixes in
// shim layer functions for access through base class
class device_windows : public shim<device_pcie>
{
public:
  // Open an unmanged device.  This ctor is called by xclOpen
  device_windows(handle_type device_handle, id_type device_id, bool user);

  ~device_windows();

  xclDeviceHandle
  get_mgmt_handle() const
  {
    return m_mgmthdl;
  }

  xclDeviceHandle
  get_user_handle() const
  {
    return get_device_handle();
  }

  // query functions
  virtual void read_dma_stats(boost::property_tree::ptree &_pt) const;

  virtual void read(uint64_t addr, void* buf, uint64_t len) const;
  virtual void write(uint64_t addr, const void* buf, uint64_t len) const;
  virtual int  open(const std::string& subdev, int flag) const;
  virtual void close(int dev_handle) const;
  virtual void reset(const char*, const char*, const char*) const;
  virtual void xclmgmt_load_xclbin(const char* buffer) const;

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const;

  xclDeviceHandle m_mgmthdl = XRT_NULL_HANDLE;
};

} // xrt_core

#endif
