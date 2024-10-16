// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#define XCL_DRIVER_DLL_EXPORT
#define XRT_API_SOURCE
#include "capture.h"
#include "logger.h"

#include <filesystem>
#include <iostream>

namespace xtx = xrt::tools::xbtracer;
namespace fs = std::filesystem;

// NOLINTNEXTLINE(cert-err58-cpp)
static const xtx::xrt_ftbl& dtbl = xtx::xrt_ftbl::get_instance();

/*
 *  Device class instrumented methods
 * */
namespace xrt {

module::module(const xrt::elf& elf)
{
  auto func = "xrt::module::module(const xrt::elf&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.module.ctor_elf, this, elf);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, elf.get_handle().get());
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

module::module(void* userptr, size_t sz, const xrt::uuid& uuid)
{
  auto func = "xrt::module::module(void*, size_t, const xrt::uuid&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.module.ctor_usr_sz_uuid, this, userptr, sz, uuid);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, userptr, sz, uuid.to_string().c_str());
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

module::module(const xrt::module& parent, const xrt::hw_context& hwctx)
{
  auto func = "xrt::module::module(const xrt::module&, const xrt::hw_context&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.module.ctor_mod_ctx, this, parent, hwctx);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, parent.get_handle().get(), hwctx.get_handle().get());
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

xrt::uuid xrt::module::get_cfg_uuid() const
{
  auto func = "xrt::module::get_cfg_uuid()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  uuid muuid;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.module.get_cfg_uuid, muuid);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, muuid.to_string().c_str());
  return muuid;
}

xrt::hw_context xrt::module::get_hw_context() const
{
  auto func = "xrt::module::get_hw_context()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  xrt::hw_context hwctx;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.module.get_hw_context, hwctx);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, hwctx.get_handle().get());
  return hwctx;
}

}  // namespace xrt
