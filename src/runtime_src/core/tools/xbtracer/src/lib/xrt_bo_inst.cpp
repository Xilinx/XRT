// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#define XCL_DRIVER_DLL_EXPORT
#define XRT_API_SOURCE         // in same dll as api
#include "capture.h"
#include "logger.h"

#include <iostream>

namespace xtx = xrt::tools::xbtracer;

// NOLINTNEXTLINE(cert-err58-cpp)
static const xtx::xrt_ftbl& dtbl = xtx::xrt_ftbl::get_instance();

/*
 * BO Class instrumented methods
 * */
namespace xrt {

bo::bo(const xrt::device& device, void* userptr, size_t sz, bo::flags flags,
       memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::device&, void*, size_t, xrt::bo::flags, xrt::memory_group)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_dev_up_s_f_g, this, device, userptr, sz,
    flags, grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func,
    device.get_handle().get(), userptr, sz, (int)flags, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const xrt::device& device, void* userptr, size_t sz, memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::device&, void*, size_t, xrt::memory_group)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_dev_up_s_g, this, device, userptr, sz,
    grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, device.get_handle().get(),
    userptr, sz, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const xrt::device& device, size_t sz, bo::flags flags, memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::device&, size_t, xrt::bo::flags, xrt::memory_group)";

  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_dev_s_f_g, this, device, sz, flags, grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, device.get_handle().get(),
    sz, (int)flags, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const xrt::device& device, size_t sz, xrt::memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::device&, size_t, xrt::memory_group)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_dev_s_g, this, device, sz, grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, device.get_handle().get(),
    sz, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const xrt::hw_context& hwctx, void* userptr, size_t sz, bo::flags flags,
       memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::hw_context&, void*, size_t, bo::flags, memory_group)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_cxt_up_s_f_g, this, hwctx, userptr, sz,
    flags, grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, hwctx.get_handle().get(),
    userptr, sz, (int)flags, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const xrt::hw_context& hwctx, void* userptr, size_t sz, memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::hw_context&, void*, size_t, xrt::memory_group)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_cxt_up_s_g, this, hwctx, userptr, sz,
    grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, hwctx.get_handle().get(),
    userptr, sz, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const xrt::hw_context& hwctx, size_t sz, bo::flags flags,
       memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::hw_context&, size_t, xrt::bo::flags, xrt::memory_group)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_cxt_s_f_g, this, hwctx, sz, flags, grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, hwctx.get_handle().get(),
    sz, (int)flags, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const xrt::hw_context& hwctx, size_t sz, memory_group grp)
{
  auto func = "xrt::bo::bo(const xrt::hw_context& hwctx, size_t sz, memory_group grp)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_cxt_s_g, this, hwctx, sz, grp);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, hwctx.get_handle().get(),
    sz, grp);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(xclDeviceHandle dhdl, xclBufferExportHandle ehdl)
{
  auto func = "xrt::bo::bo(xclDeviceHandle, xclBufferExportHandle)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_exp_bo, this, dhdl, ehdl);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &dhdl, &ehdl);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(xclDeviceHandle dhdl, pid_type pid, xclBufferExportHandle ehdl)
{
  auto func = "xrt::bo::bo(xclDeviceHandle, pid_type, xclBufferExportHandle)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_exp_bo_pid, this, dhdl, pid, ehdl);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &dhdl, pid.pid, &ehdl);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(const bo& parent, size_t size, size_t offset)
{
  auto func = "xrt::bo::bo(const xrt::bo&, size_t, size_t)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_bo_s_o, this, parent, size, offset);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, parent.get_handle().get(),
    size, offset);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(xclDeviceHandle dhdl, xcl_buffer_handle xhdl)
{
  auto func = "xrt::bo::bo(xclDeviceHandle, xcl_buffer_handle)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_xcl_bh, this, dhdl, xhdl);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &dhdl, &xhdl);
  XRT_TOOLS_XBT_FUNC_EXIT(dtbl.bo.ctor_xcl_bh);
}

size_t bo::size() const
{
  auto func = "xrt::bo::size()";
  size_t msz = 0;
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.bo.size, msz);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, msz);
  return msz;
}

uint64_t bo::address() const
{
  auto func = "xrt::bo::address()";
  uint64_t madd = 0;
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.bo.address, madd);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, madd);
  return madd;
}

memory_group bo::get_memory_group() const
{
  auto func = "xrt::bo::get_memory_group()";
  memory_group mgrp = 0;
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.bo.get_memory_group, mgrp);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, (int)mgrp);
  return mgrp;
}

bo::flags bo::get_flags() const
{
  auto func = "xrt::bo::get_flags()";
  bo::flags mflags = bo::flags::normal;
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.bo.get_flags, mflags);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, (int)mflags);
  return mflags;
}

xclBufferExportHandle bo::export_buffer()
{
  auto func = "xrt::bo::export_buffer()";
  xclBufferExportHandle hbufexp = {};
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.bo.export_buffer, hbufexp);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, &hbufexp);
  return hbufexp;
}

bo::async_handle bo::async(xclBOSyncDirection dir, size_t sz, size_t offset)
{
  auto func = "xrt::bo::async(xclBOSyncDirection, size_t, size_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, dir, sz, offset);
  if (dtbl.bo.async) {
    auto mhasync = (this->*dtbl.bo.async)(dir, sz, offset);
    XRT_TOOLS_XBT_FUNC_EXIT_RET(func, &mhasync);
    return mhasync;
  } else {
    std::cerr << "dtbl.bo.async is NULL @ " << __FILE__ << __LINE__ << "\n";
    return (bo::async_handle) nullptr;
  }
}

void bo::sync(xclBOSyncDirection dir, size_t size, size_t offset)
{
  auto func = "xrt::bo::sync(xclBOSyncDirection, size_t, size_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, dir, size, offset);
  XRT_TOOLS_XBT_CALL_METD(dtbl.bo.sync, dir, size, offset);
  std::vector<unsigned char> buffer(size);
  this->read(buffer.data(), size, 0);
  xtx::membuf bo_buf(buffer.data(), (unsigned int)size);
  XRT_TOOLS_XBT_FUNC_EXIT(func, "xrt::bo_buf", bo_buf);
}

void* bo::map()
{
  auto func = "xrt::bo::map()";
  void* mptr = nullptr;
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.bo.map, mptr);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, mptr);
  return mptr;
}

void bo::write(const void* src, size_t size, size_t seek)
{
  auto func = "xrt::bo::write(const void*, size_t, size_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, src, size, seek);
  XRT_TOOLS_XBT_CALL_METD(dtbl.bo.write, src, size, seek);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

#ifndef _WIN32
/*
  On windows system, original class methods are taken from IDT, if
  application is not using this API, reference of this is not present in the
  IDT. As it's not present in IDT, original fptr corresponding to this method
  remains to initialized value ie NULL. We are using this API from the sync to
  capture the bo buffer in trace. As a workarround we are disbling the
  interception of xrt::bo::read. As xrt::bo::read definition is not present
  in current class linker would link this to one comming from xrt_coreutils.

  This work-arround was accepted during the team discussion. If there is any
  compelling reason to include this in the trace, we would revist.

  TODO: Should we remove this for Linux as well for keeping uniformity.
*/
void bo::read(void* dst, size_t size, size_t skip)
{
  auto func = "xrt::bo::read(void*, size_t, size_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, dst, size, skip);
  XRT_TOOLS_XBT_CALL_METD(dtbl.bo.read, dst, size, skip);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}
#endif

void bo::copy(const bo& src, size_t sz, size_t src_offset, size_t dst_offset)
{
  auto func = "xrt::bo::copy(const bo&, size_t, size_t, size_t)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &src, sz, src_offset, dst_offset);
  XRT_TOOLS_XBT_CALL_METD(dtbl.bo.copy, src, sz, src_offset, dst_offset);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

bo::bo(xrtBufferHandle hbuf)
{
  auto func = "xrt::bo::bo(xrtBufferHandle)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.bo.ctor_capi, this, hbuf);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &hbuf);
  handle = this->get_handle();
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}
}  // namespace xrt


////////////////////////////////////////////////////////////////
// xrt_ext::bo C++ API implmentations (xrt_ext.h)
////////////////////////////////////////////////////////////////
namespace xrt::ext {
bo::
bo(const xrt::hw_context& hwctx, size_t sz, access_mode access)
{
  auto func = "ext::bo::bo(constxrt::hw_context&, size_t, xrt::ext::bo::access_mode)";
  XRT_TOOLS_XBT_CALL_EXT_CTOR(dtbl.ext.bo_ctor_cxt_s_a, this, hwctx, sz, access);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, hwctx.get_handle().get(),
    sz, (int)access);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}
} // namespace xrt::ext
