// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_module.h
#define XRT_API_SOURCE         // exporting xrt_module.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "experimental/xrt_module.h"
#include "experimental/xrt_elf.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_uuid.h"

#include "core/common/error.h"

#include <cstring>

namespace {

} // namespace

namespace xrt {

// class module_impl - Base class for different implementations
//
// An xrt::bo buffer object that represents the instructions and is
// passed to hw execution.  This buffer object is constructed in
// context of the kernel object that will use it.
class module_impl
{
protected:
  xrt::hw_context m_hwctx;
  xrt::uuid m_cfg_uuid;     // matching hw configuration id

public:
  module_impl(xrt::hw_context hwctx)
    : m_hwctx(std::move(hwctx))
    , m_cfg_uuid(m_hwctx.get_xclbin_uuid())
  {}

  module_impl(const module_impl* parent)
    : m_hwctx(parent->m_hwctx)
    , m_cfg_uuid(parent->m_cfg_uuid)
  {}

  virtual
  ~module_impl()
  {}

  xrt::uuid
  get_cfg_uuid() const
  {
    return m_hwctx.get_xclbin_uuid();
  }

  xrt::hw_context
  get_hw_context() const
  {
    return m_hwctx;
  }

  virtual xrt::bo
  get_instruction_buffer(const std::string& nm) const = 0;
};

// class module_elf - Elf provided by application
class module_elf : public module_impl
{
  xrt::elf m_elf;
  // TBD...
public:
  module_elf(xrt::hw_context hwctx, xrt::elf elf)
    : module_impl(hwctx)
    , m_elf(elf)
  {}

  xrt::bo
  get_instruction_buffer(const std::string&) const override
  {
    throw std::runtime_error("elf instruction buffer not implemented");
  }
};

// class module_userptr - Opaque userptr provided by application
class module_userptr : public module_impl
{
  xrt::bo m_buffer;    // instruction parent bufffer

  xrt::bo
  create_instruction_buffer(const void* userptr, size_t bytes)
  {
    xrt::bo bo{m_hwctx, bytes, xrt::bo::flags::cacheable, 0};
    std::memcpy(bo.map(), userptr, bytes);
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    return bo;
  }

public:
  module_userptr(xrt::hw_context hwctx, void* userptr, size_t sz, xrt::uuid uuid)
    : module_impl{hwctx}
    , m_buffer{create_instruction_buffer(userptr, sz)}
  {
    if (m_cfg_uuid != uuid)
      throw xrt_core::error("Instruction buffer is not compatible with configured hardware");
  }

  xrt::bo
  get_instruction_buffer(const std::string&) const override
  {
    return m_buffer;
  }
};

// class module_sub - Create a sub module from parent
class module_sub : public module_impl
{
  std::shared_ptr<module_impl> m_parent;
  xrt::bo m_buffer;
public:
  module_sub(std::shared_ptr<module_impl> parent, size_t size, size_t offset)
    : module_impl{parent.get()}
    , m_parent{parent}
    , m_buffer{m_parent->get_instruction_buffer("tbd"), size, offset}
  {}

  xrt::bo
  get_instruction_buffer(const std::string&) const override
  {
    return m_buffer;
  }
};

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal module APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::module_int {

} // xrt_core::module_int

////////////////////////////////////////////////////////////////
// xrt_module C++ API implementation (xrt_module.h)
////////////////////////////////////////////////////////////////
namespace xrt {

module::
module(const xrt::hw_context& hwctx, const xrt::elf& elf)
  //  : detail::pimpl<module_impl>(std::make_shared<module_impl>(elf))
  : detail::pimpl<module_impl>{std::make_shared<module_elf>(hwctx, elf)}
{}

module::
module(const xrt::hw_context& hwctx, void* userptr, size_t sz, const xrt::uuid& uuid)
  : detail::pimpl<module_impl>{std::make_shared<module_userptr>(hwctx, userptr, sz, uuid)}
{}

module::
module(const xrt::module& parent, size_t size, size_t offset)
  : detail::pimpl<module_impl>{std::make_shared<module_sub>(parent.handle, size, offset)}
{}

xrt::uuid
module::
get_cfg_uuid() const
{
  return handle->get_cfg_uuid();
}

xrt::bo
module::
get_instruction_buffer(const std::string& nm) const
{
  return handle->get_instruction_buffer(nm);
}

xrt::hw_context
module::
get_hw_context() const
{
  return handle->get_hw_context();
}

} // namespace xrt
