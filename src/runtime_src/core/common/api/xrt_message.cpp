// Copyright (C) 2021 Xilinx, Inc
// SPDX-License-Identifier: Apache-2.0

// This file implements XRT error APIs as declared in
// core/include/experimental/xrt_message.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_ini.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/xrt/experimental/xrt_message.h"

#include "core/common/config_reader.h"
#include "core/common/message.h"

namespace xrt::message {

namespace detail {
  
bool
enabled(level lvl)
{
  return xrt_core::config::get_verbosity() >= static_cast<int>(lvl);
}

} // detail

void
log(level lvl, const std::string& tag, const std::string& msg)
{
  xrt_core::message::send(lvl, tag, msg);
}

} // namespace xrt::message

////////////////////////////////////////////////////////////////
// xrt_message C API implmentations (xrt_message.h)
////////////////////////////////////////////////////////////////
