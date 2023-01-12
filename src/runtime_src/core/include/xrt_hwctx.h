// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_HWCTX_H_
#define XRT_HWCTX_H_

#include <cstdint>
#include <stdexcept>
#include <variant>

#ifdef _WIN32
# pragma warning( push )
# pragma warning( disable : 4201 )
#endif

// Shim level representation of a hardware context handle is an opaque
// pointer, but when the handle is exchanged between shim and XRT
// common layer it is wrapped in a typed xrt_hwctx_handle.
//
// The struct allows the handle to be both a pointer to
// xrt_hwctx_handle for classes derived from xrt_hwctx_handle, and it
// allows a completely opaque void*, or a just a slot index.  All
// these variants are too obscure, but this is for supporting existing
// use while we clean up the ishim layer.  A hwctx_handle should be
// simply a pointer to an object of this class, but for now it is
// passed around by value.
class xrt_hwctx_handle
{
  using self = xrt_hwctx_handle;
  std::variant<self*, void*, uint32_t> m_handle;

  // Get the slotidx from a handle.  This is for supporting
  // legacy xrt::kernel::group_id() which encodes the slotidx
  // in xrt::bo flags for driver.   This again is obscure and
  // xrt::kernel::group_id should not do any encoding, instead
  // the hwctx specific xrt::bo constructors should be used.
  virtual uint32_t
  get_slotidx() const
  {
    switch (m_handle.index()) {
    case 0:
      // For classes derived from xrt_hwctx_handle
      return std::get<0>(m_handle)->get_slotidx();
    case 1:
      // Slot index is not accessible for an opaque void* handle.
      // For shims with void* opaque handle it is not safe to throw
      // as long as xrt::kernel::group_id() uses this function.
      return 0;
    case 2:
      // For legacy slotidx handles
      return std::get<2>(m_handle);
    }

    throw std::runtime_error("Bad index");
  }

public:
  xrt_hwctx_handle() {}
  xrt_hwctx_handle(self* hdl) : m_handle{hdl} {}
  xrt_hwctx_handle(void* hdl) : m_handle{hdl} {}
  xrt_hwctx_handle(uint32_t slotidx) : m_handle{slotidx} {}
  virtual ~xrt_hwctx_handle() {}
  operator self* () const { return std::get<0>(m_handle); }
  operator void* () const { return std::get<1>(m_handle); }
  operator uint32_t () const { return get_slotidx(); }
  bool operator<  (const xrt_hwctx_handle& rhs) const { return m_handle < rhs.m_handle; }
  bool operator== (const xrt_hwctx_handle& rhs) const { return m_handle == rhs.m_handle; }

};

const xrt_hwctx_handle XRT_NULL_HWCTX {0xffffffff};

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif
