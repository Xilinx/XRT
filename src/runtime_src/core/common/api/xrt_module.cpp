// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_module.h
#define XRT_API_SOURCE         // exporting xrt_module.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "experimental/xrt_module.h"
#include "experimental/xrt_elf.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_uuid.h"

#include "module_int.h"
#include "core/common/debug.h"
#include "core/common/error.h"

#include <cstring>
#include <string>

namespace {

} // namespace

namespace xrt {

// class module_impl - Base class for different implementations
class module_impl
{
protected:
  xrt::uuid m_cfg_uuid;     // matching hw configuration id

public:
  module_impl(xrt::uuid cfg_uuid)
    : m_cfg_uuid(std::move(cfg_uuid))
  {}

  module_impl(const module_impl* parent)
    : m_cfg_uuid(parent->m_cfg_uuid)
  {}

  virtual
  ~module_impl()
  {}

  xrt::uuid
  get_cfg_uuid() const
  {
    return m_cfg_uuid;
  }

  virtual std::pair<const char*, size_t>
  get_data() const
  {
    return {nullptr, 0};
  }

  virtual xrt::hw_context
  get_hw_context() const
  {
    return {};
  }

  virtual xrt::bo
  get_instruction_buffer(const std::string&) const
  {
    return {};
  }
};

// class module_elf - Elf provided by application
class module_elf : public module_impl
{
  xrt::elf m_elf;

public:
  module_elf(xrt::elf elf)
    : module_impl{elf.get_cfg_uuid()}
    , m_elf(std::move(elf))
  {}

  virtual std::pair<const char*, size_t>
  get_data() const override
  {
    return {nullptr, 0}; // TBD
  }
};

// class module_userptr - Opaque userptr provided by application
class module_userptr : public module_impl
{
  std::vector<char> m_buffer; // userptr copy

public:
  module_userptr(char* userptr, size_t sz, const xrt::uuid& uuid)
    : module_impl{uuid}
    , m_buffer{userptr, userptr + sz}
  {}

  module_userptr(void* userptr, size_t sz, const xrt::uuid& uuid)
    : module_userptr(static_cast<char*>(userptr), sz, uuid)
  {}

  virtual std::pair<const char*, size_t>
  get_data() const override
  {
    return {m_buffer.data(), m_buffer.size()};
  }
};

// class module_sram - Create an sram module from parent
class module_sram : public module_impl
{
  std::shared_ptr<module_impl> m_parent;
  xrt::hw_context m_hwctx;
  xrt::bo m_buffer;

  xrt::bo
  create_instruction_buffer(const module_impl* parent)
  {
    XRT_PRINTF("-> module_sram::create_instruction_buffer()\n");
    auto data = parent->get_data();
    xrt::bo bo{m_hwctx, data.second, xrt::bo::flags::cacheable, 1 /* fix me */};
    std::memcpy(bo.map(), data.first, data.second);
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    XRT_PRINTF("<- module_sram::create_instruction_buffer()\n");
    return bo;
  }

public:
  module_sram(std::shared_ptr<module_impl> parent, xrt::hw_context hwctx)
    : module_impl{parent->get_cfg_uuid()}
    , m_parent{std::move(parent)}
    , m_hwctx{std::move(hwctx)}  
    , m_buffer{create_instruction_buffer(m_parent.get())}
  {
  }

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

xrt::bo
get_instruction_buffer(const xrt::module& module, const std::string& nm)
{
  return module.get_handle()->get_instruction_buffer(nm);
}

} // xrt_core::module_int

////////////////////////////////////////////////////////////////
// xrt_module C++ API implementation (xrt_module.h)
////////////////////////////////////////////////////////////////
namespace xrt {

module::
module(const xrt::elf& elf)
  : detail::pimpl<module_impl>{std::make_shared<module_elf>(elf)}
{}

module::
module(void* userptr, size_t sz, const xrt::uuid& uuid)
  : detail::pimpl<module_impl>{std::make_shared<module_userptr>(userptr, sz, uuid)}
{}

module::
module(const xrt::module& parent, const xrt::hw_context& hwctx)
  : detail::pimpl<module_impl>{std::make_shared<module_sram>(parent.handle, hwctx)}
{}

xrt::uuid
module::
get_cfg_uuid() const
{
  return handle->get_cfg_uuid();
}

xrt::hw_context
module::
get_hw_context() const
{
  return handle->get_hw_context();
}

} // namespace xrt
