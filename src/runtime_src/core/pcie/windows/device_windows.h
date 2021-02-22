/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

#ifndef DEVICE_WINDOWS_H
#define DEVICE_WINDOWS_H

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
  virtual void load_xclbin(const MemoryBuffer &buffer) const;

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const;

  xclDeviceHandle m_mgmthdl = XRT_NULL_HANDLE;
};

} // xrt_core

#endif
