// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc. All rights reserved.

// This file is delivered with core library (libxrt_core), see
// core/pcie/mcdm/CMakeLists.txt.  To prevent compilation of this
// file from importing symbols from libxrt_core we define this source
// file to instead export with same macro as used in libxrt_core.
#define XCL_DRIVER_DLL_EXPORT
#include "system_mcdm.h"
#include "device_mcdm.h"
#include <memory>

#ifdef _WIN32
# pragma warning (disable : 4996)
#endif

namespace {

// Singleton registers with base class xrt_core::system
// during static global initialization.  If statically
// linking with libxrt_core, then explicit initialiation
// is required
static xrt_core::system_mcdm*
singleton_instance()
{
  static xrt_core::system_mcdm singleton;
  return &singleton;
}

// Dynamic linking automatically constructs the singleton
struct X
{
  X() { singleton_instance(); }
} x;

}

namespace xrt_core {

std::pair<device::id_type, device::id_type>
system_mcdm::
get_total_devices(bool) const
{
  unsigned int count = xclProbe(); //is_user ? xclProbe() : mgmtpf::probe();
  return {count, count};
}

std::shared_ptr<device>
system_mcdm::
get_userpf_device(device::id_type id) const
{
  return xrt_core::get_userpf_device(xclOpen(id, nullptr, XCL_QUIET));
}

std::shared_ptr<device>
system_mcdm::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_mcdm>(new device_mcdm(handle, id, true));
}

std::shared_ptr<device>
system_mcdm::
get_mgmtpf_device(device::id_type) const
{
  throw std::runtime_error("Not implemented");
}

namespace pcie_mcdm {

std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id)
{
  singleton_instance(); // force loading if necessary
  return xrt_core::get_userpf_device(device_handle, id);
}

}

} // xrt_core
