// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
#define XRT_CORE_COMMON_SOURCE
#include "core/common/xdp/profile.h"
#include "core/common/xdp/aie_profile.h"

#include "core/common/config_reader.h"

// This file makes the connections between all xrt_coreutil level hooks
// to the corresponding xdp plugins.  It is responsible for loading all of
// modules.

namespace xrt_core::xdp {

void 
update_device(void* handle)
{
  if (xrt_core::config::get_aie_profile()) {
    try {
      xrt_core::xdp::aie::profile::load();
    } 
    catch (...) {
      return;
    }
    xrt_core::xdp::aie::profile::update_device(handle);
  }
}

void 
finish_flush_device(void* handle)
{
  if (xrt_core::config::get_aie_profile())
    xrt_core::xdp::aie::profile::end_poll(handle);
}

} // end namespace xrt_core::xdp
