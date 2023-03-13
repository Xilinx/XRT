// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_SHARED_HANDLE_H
#define XRT_CORE_SHARED_HANDLE_H

#include <memory>

namespace xrt_core {

// Representation of a shared buffer_handle. The shared buffer can
// be imported by another device or process.  The exporting process
// can hold on to the shared buffer so as to reuse for multiple
// share requests.  The shared_buffer itself could be released when
// the exporting buffer is destructed, e.g., implementing what used
// to be ishim::close_export_handle
class shared_handle
{
public:
#ifdef _WIN32
  using export_handle = void*;
#else
  using export_handle = int;
#endif

  virtual ~shared_handle() {}

  // The export handle is for marshaling to another process
  virtual export_handle
  get_export_handle() const = 0;
};

} // xrt_core

#endif
