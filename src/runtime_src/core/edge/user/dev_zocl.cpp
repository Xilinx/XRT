// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights

#include <iostream>
#include <sstream>
#include <cstring>
#include <filesystem>
#include <regex>
#include "dev.h"
#include "dev_zocl.h"
#include "shim.h"
#include "device_linux.h"
#include "plugin/xdp/aie_status.h"

namespace xrt_core::edge {

dev_zocl::dev_zocl(const std::string& root) : dev(root) {}
    
dev_zocl::~dev_zocl()
{
  xdp::aie::sts::end_poll(nullptr);
}

device::handle_type
dev_zocl::
create_shim(device::id_type id) const
{
  auto handle = new ZYNQ::shim(id, std::const_pointer_cast<xrt_core::edge::dev_zocl>(shared_from_this()));
  return static_cast<device::handle_type>(handle);
}

std::shared_ptr<xrt_core::device>
dev_zocl::
create_device(device::handle_type handle, device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<xrt_core::device_linux>(new xrt_core::device_linux(handle, id, true));
}

} //namespace xrt_core::edge