// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_HWCTX_H_
#define XRT_HWCTX_H_

#include <cstdint>

#ifdef __cplusplus

#ifdef _WIN32
# pragma warning( push )
# pragma warning( disable : 4201 )
#endif

// Shim level representation of a hardware context handle is an opaque
// pointer, e.g. a void*, but when the handle is exchanged between
// shim and XRT common layer it is wrapped in a typed
// xrt_hwctx_handle.
//
// The struct allows the handle to be both a true pointer to some
// opaque structure managed by shim, or a slot index for legacy
// reasons.
struct xrt_hwctx_handle
{
  union {
    void* handle;
    uint64_t slot;
  };

  xrt_hwctx_handle() : handle(nullptr) {}
  xrt_hwctx_handle(void* hdl) : handle(hdl) {}
  xrt_hwctx_handle(uint32_t slotidx) : slot(slotidx) {}
  operator void* () const { return handle; }
  operator uint32_t () const { return static_cast<uint32_t>(slot); }
  bool operator<  (const xrt_hwctx_handle& rhs) const { return handle < rhs.handle; }
  bool operator== (const xrt_hwctx_handle& rhs) const { return handle == rhs.handle; }
};

const xrt_hwctx_handle XRT_NULL_HWCTX {0xffffffff};

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif // __cplusplus

#endif
