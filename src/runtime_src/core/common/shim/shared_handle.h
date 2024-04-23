// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_SHARED_HANDLE_H
#define XRT_CORE_SHARED_HANDLE_H

#include <cstdint>

namespace xrt_core {

// shared_handle - Representation of a shared object.

// A shared object is something underlying shim object that
// can be shared between a device or processes, for example
// buffer objects and fence objects can be shared.
//
// The shared object can be imported by another device or process.
// The exporting process can hold on to the shared object so as to
// reuse for multiple share requests.  The shared object itself could
// be released when the exporting object is destructed, e.g.,
// implementing what used to be specific close APIs like
// ishim::close_export_handle
class shared_handle
{
public:
#ifdef _WIN32
  using export_handle = uint64_t;
#else
  using export_handle = int;
#endif

  virtual ~shared_handle() {}

  // Get an export handle for marshaling to another process
  virtual export_handle
  get_export_handle() const = 0;
};

} // xrt_core

#endif
