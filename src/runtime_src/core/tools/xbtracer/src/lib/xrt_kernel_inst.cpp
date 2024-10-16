// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include <iostream>

#define XCL_DRIVER_DLL_EXPORT
#define XRT_API_SOURCE
#include "capture.h"
#include "logger.h"

namespace xtx = xrt::tools::xbtracer;

// NOLINTNEXTLINE(cert-err58-cpp)
const xtx::xrt_ftbl& dtbl = xtx::xrt_ftbl::get_instance();

/*
 *  kernel/run class instrumented methods
 * */
namespace xrt {
XCL_DRIVER_DLLESPEC
run::run(const kernel& krnl)
{
  auto func = "xrt::run::run(const xrt::kernel&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.run.ctor, this, krnl);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, krnl.get_handle().get());
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

XCL_DRIVER_DLLESPEC
void run::start()
{
  auto func = "xrt::run::start()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.start);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

void run::start(const autostart& iterations)
{
  auto func = "xrt::run::start(const autostart&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, iterations.iterations);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.start_itr, iterations);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

XCL_DRIVER_DLLESPEC
void run::stop()
{
  auto func = "xrt::run::stop()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.stop);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

XCL_DRIVER_DLLESPEC
ert_cmd_state run::abort()
{
  auto func = "xrt::run::abort()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  ert_cmd_state status = ERT_CMD_STATE_NEW;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.run.abort, status);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, status);
  return status;
}

ert_cmd_state run::wait(const std::chrono::milliseconds& timeout_ms) const
{
  auto func = "xrt::run::wait(const std::chrono::milliseconds&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, timeout_ms.count());
  ert_cmd_state status = ERT_CMD_STATE_NEW;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.run.wait, status, timeout_ms);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, status);
  return status;
}

std::cv_status run::wait2(const std::chrono::milliseconds& timeout) const
{
  auto func = "xrt::run::wait2(const std::chrono::milliseconds&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, timeout.count());
  std::cv_status status = std::cv_status::no_timeout;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.run.wait2, status, timeout);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, (int)status);
  return status;
}

ert_cmd_state run::state() const
{
  auto func = "xrt::run::state()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  ert_cmd_state status = ERT_CMD_STATE_NEW;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.run.state, status);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, status);
  return status;
}

uint32_t run::return_code() const
{
  auto func = "xrt::run::return_code()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  uint32_t ret_code = 0;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.run.return_code, ret_code);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, ret_code);
  return ret_code;
}

void run::add_callback(ert_cmd_state state,
           std::function<void(const void*, ert_cmd_state, void*)> callback,
           void* data)
{
  auto func = "xrt::run::add_callback(ert_cmd_state, std::function<void(const void*, ert_cmd_state, void*), void*)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, state, &callback, data);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.add_callback, state, callback, data);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

void run::submit_wait(const xrt::fence& fence)
{
  auto func = "xrt::run::submit_wait(const xrt::fence&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &fence);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.submit_wait, fence);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

void run::submit_signal(const xrt::fence& fence)
{
  auto func = "xrt::run::submit_signal(const xrt::fence&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &fence);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.submit_signal, fence);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

ert_packet* run::get_ert_packet() const
{
  auto func = "xrt::run::get_ert_packet()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  ert_packet* pert_pkt = nullptr;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.run.get_ert_packet, pert_pkt);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, pert_pkt);
  return pert_pkt;
}

void run::set_arg_at_index(int index, const void* value, size_t bytes)
{
  auto func = "xrt::run::set_arg_at_index(int, const void*, size_t)";
  const int* intvalue = static_cast<const int*>(value);
  XRT_TOOLS_XBT_FUNC_ENTRY(func, index, *intvalue, bytes);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.set_arg3, index, value, bytes);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

void run::set_arg_at_index(int index, const xrt::bo& mbo)
{
  auto func = "xrt::run::set_arg_at_index(int, const xrt::bo&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, index, mbo.get_handle().get());
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.set_arg2, index, mbo);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

void run::update_arg_at_index(int index, const void* value, size_t bytes)
{
  auto func = "xrt::run::update_arg_at_index(int, const void*, size_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, index, value, bytes);
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.update_arg3, index, value, bytes);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

void run::update_arg_at_index(int index, const xrt::bo& mbo)
{
  auto func = "xrt::run::update_arg_at_index(int, const xrt::bo&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, index, mbo.get_handle().get());
  XRT_TOOLS_XBT_CALL_METD(dtbl.run.update_arg2, index, mbo);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

kernel::kernel(const xrt::device& xdev, const xrt::uuid& xclbin_id,
               const std::string& name, cu_access_mode mode)
{
  auto func = "xrt::kernel::kernel(const xrt::device&, const xrt::uuid&, const std::string&, xrt::kernel::cu_access_mode)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.kernel.ctor, this, xdev, xclbin_id, name, mode);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, xdev.get_handle().get(), &xclbin_id, name,
    (int)mode);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

kernel::kernel(const xrt::hw_context& ctx, const std::string& name)
{
  auto func = "xrt::kernel::kernel(const xrt::hw_context&, const std::string&)";
  /* If you see a crash from here on windows platform, please check the build
    * mode. The build mode of XRT lib and application must match. This is a
    * known problem on windows platform */
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.kernel.ctor2, this, ctx, name);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, ctx.get_handle().get(), name);
  handle = this->get_handle();
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

int kernel::group_id(int argno) const
{
  auto func = "xrt::kernel::group_id(int)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, argno);
  int gid = 0;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.kernel.group_id, gid, argno);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, gid);
  return gid;
}

uint32_t kernel::offset(int argno) const
{
  auto func = "xrt::kernel::offset(int)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, argno);
  uint32_t moffset = 0;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.kernel.offset, moffset, argno);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, moffset);
  return moffset;
}

void kernel::write_register(uint32_t offset, uint32_t data)
{
  auto func = "xrt::kernel::write_register(uint32_t, uint32_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, offset, data);
  XRT_TOOLS_XBT_CALL_METD(dtbl.kernel.write_register, offset, data);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

uint32_t kernel::read_register(uint32_t offset) const
{
  auto func = "xrt::kernel::read_register(uint32_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, offset);
  uint32_t data = 0;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.kernel.read_register, data, offset);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, data);
  return data;
}

std::string kernel::get_name() const
{
  auto func = "xrt::kernel::get_name()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  std::string name;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.kernel.get_name, name);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, name);
  return name;
}

xrt::xclbin kernel::get_xclbin() const
{
  auto func = "xrt::kernel::get_xclbin()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  xrt::xclbin xclbin;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.kernel.get_xclbin, xclbin);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, &xclbin);
  return xclbin;
}
}  // namespace xrt

////////////////////////////////////////////////////////////////
// xrt_ext::kernel C++ API implmentations (xrt_ext.h)
////////////////////////////////////////////////////////////////
namespace xrt::ext
{

kernel::kernel(const xrt::hw_context& ctx, const xrt::module& mod, const std::string& name)
{
  auto func = "ext::kernel::kernel(const xrt::hw_context&, const xrt::module&, const std::string&)";
  XRT_TOOLS_XBT_CALL_EXT_CTOR(dtbl.ext.kernel_ctor_ctx_m_s, this, ctx, mod, name);
  XRT_TOOLS_XBT_FUNC_ENTRY(func, ctx.get_handle().get(), mod.get_handle().get(), name);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

}