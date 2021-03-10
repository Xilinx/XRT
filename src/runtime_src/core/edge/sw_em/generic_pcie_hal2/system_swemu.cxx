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

#include "system_swemu.h"
#include "device_swemu.h"
#include "xrt.h"

#include <memory>

namespace {

// Singleton registers with base class xrt_core::system
// during static global initialization.  If statically
// linking with libxrt_core, then explicit initialiation
// is required
static xrt_core::swemu::system*
singleton_instance()
{
  static xrt_core::swemu::system singleton;
  return &singleton;
}

// Dynamic linking automatically constructs the singleton
struct X
{
  X() { singleton_instance(); }
} x;

}

namespace xrt_core { namespace swemu {

system::
system()
{
  // xclProbe must be called to set up data structures
  xclProbe();
}

std::pair<device::id_type, device::id_type>
system::
get_total_devices(bool is_user) const
{
  return {0,0};
}

std::shared_ptr<xrt_core::device>
system::
get_userpf_device(device::id_type id) const
{
  return xrt_core::get_userpf_device(xclOpen(id, nullptr, XCL_QUIET));
}

std::shared_ptr<xrt_core::device>
system::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<xrt_core::swemu::device>(new xrt_core::swemu::device(handle, id, true));
}

std::shared_ptr<xrt_core::device>
system::
get_mgmtpf_device(device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<xrt_core::swemu::device>(new xrt_core::swemu::device(nullptr, id, false));
}

std::shared_ptr<xrt_core::device>
get_userpf_device(device::handle_type device_handle, device::id_type id)
{
  singleton_instance(); // force loading if necessary
  return xrt_core::get_userpf_device(device_handle, id);
}

void
system::
program_plp(const xrt_core::device* dev, const std::vector<char> &buffer) const
{
  throw std::runtime_error("plp program is not supported");
}

}} // swemu, xrt_core
