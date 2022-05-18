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

#ifndef PCIE_HWEMU_DEVICE_LINUX_H
#define PCIE_HWEMU_DEVICE_LINUX_H

#include "core/common/ishim.h"
#include "core/pcie/common/device_pcie.h"

#include "shim_int.h"

namespace xrt_core { namespace hwemu {

// concrete class derives from device_edge, but mixes in
// shim layer functions for access through base class
class device : public shim<device_pcie>
{
public:
  device(handle_type device_handle, id_type device_id, bool user);

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const;

  uint32_t // ctx handle aka slotidx
  create_hw_context(const xrt::uuid& xclbin_uuid, uint32_t qos) const override
  {
    return xrt::shim_int::create_hw_context(get_device_handle(), xclbin_uuid, qos);
  }

  void
  destroy_hw_context(uint32_t ctxhdl) const override
  {
    xrt::shim_int::destroy_hw_context(get_device_handle(), ctxhdl);
  }

  void
  register_xclbin(const xrt::xclbin& xclbin) const override
  {
    xrt::shim_int::register_xclbin(get_device_handle(), xclbin);
  }
};

}} // hwemu, xrt_core

#endif
