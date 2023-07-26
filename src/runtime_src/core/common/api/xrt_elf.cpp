// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_elf.h
#define XRT_API_SOURCE         // exporting xrt_elf.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "experimental/xrt_elf.h"

#include "xrt/xrt_uuid.h"

#include "core/common/error.h"

namespace xrt {

// class elf_impl - Implementation
class elf_impl
{
public:
  elf_impl(const std::string&)
  {}

  xrt::uuid
  get_cfg_uuid() const
  {
    return {}; // tbd
  }
};

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal elf APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::elf_int {

} // xrt_core::elf_int

////////////////////////////////////////////////////////////////
// xrt_elf C++ API implementation (xrt_elf.h)
////////////////////////////////////////////////////////////////
namespace xrt {

elf::
elf(const std::string& fnm)
  : detail::pimpl<elf_impl>{std::make_shared<elf_impl>(fnm)}
{}

xrt::uuid
elf::
get_cfg_uuid() const
{
  return handle->get_cfg_uuid();
}

} // namespace xrt
