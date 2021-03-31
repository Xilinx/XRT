/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_bo.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_bo.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_bo.h"
#include "core/include/experimental/xrt_aie.h"
#include "native_profile.h"
#include "bo.h"

#include "device_int.h"
#include "kernel_int.h"
#include "core/common/device.h"
#include "core/common/memalign.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "core/common/unistd.h"
#include "core/common/xclbin_parser.h"

#include <cstdlib>
#include <map>
#include <set>

#ifdef _WIN32
# pragma warning( disable : 4244 4100 4996 4505 )
#endif

namespace {

XRT_CORE_UNUSED
static bool
is_noop_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool noop = xem ? (std::strcmp(xem,"noop")==0) : false;
  return noop;
}

XRT_CORE_UNUSED
static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swemu = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swemu;
}

static bool
is_nodma(xclDeviceHandle xhdl)
{
  auto core_device = xrt_core::get_userpf_device(xhdl);
  return core_device->is_nodma();
}

}

////////////////////////////////////////////////////////////////
// Exposed for Vitis aietools as extensions to xrt_bo.h
// Revisit post 2020.1
////////////////////////////////////////////////////////////////
// Removed as address was exposed through public API per request
///////////////////////////////////////////////////////////////

namespace {

inline size_t
get_alignment()
{
  return xrt_core::getpagesize();
}

inline  bool
is_aligned_ptr(const void* p)
{
  return p && (reinterpret_cast<uintptr_t>(p) % get_alignment())==0;
}

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
}

inline void
send_exception_message(const std::string& msg)
{
  send_exception_message(msg.c_str());
}

} // namespace

namespace xrt {

// class bo_impl - Base class for buffer objects
//
// [bo_impl]: base class
//
// Derived classes:
// [hbuf]: host side buffer allocated in virtual memory and managed by XRT
// [ubuf]: host side buffer allocated and managed by user
// [kbuf]: host side buffer allocated and managed by kernel driver
// [sub]:  sub buffer
//
// Add new sub classes as needed, for example p2p, cma, svm.
//
// Life time of buffers are managed through shared pointers.
// A buffer is freed when last references is released.
class bo_impl
{
public:
  static constexpr uint64_t no_addr = std::numeric_limits<uint64_t>::max();
  static constexpr int32_t no_group = std::numeric_limits<int32_t>::max();

private:
  void
  get_bo_properties() const
  {
    xclBOProperties prop;
    device->get_bo_properties(handle, &prop);
    addr = prop.paddr;
    grpid = prop.flags & XRT_BO_FLAGS_MEMIDX_MASK;

#ifdef _WIN32 // All shims minus windows return proper flags
    // Remove when driver returns the flags that were used to ctor the bo
    auto mem_topo = device->get_axlf_section<const ::mem_topology*>(ASK_GROUP_TOPOLOGY);
    grpid = xrt_core::xclbin::address_to_memidx(mem_topo, addr);
#endif
  }

protected:
  std::shared_ptr<xrt_core::device> device;
  xclBufferHandle handle;           // driver handle
  size_t size;                      // size of buffer
  mutable uint64_t addr = no_addr;  // bo device address
  mutable int32_t grpid = no_group; // memory group index
  bool free_bo;                     // should dtor free bo

public:
  explicit bo_impl(size_t sz)
    : handle(XRT_NULL_BO), size(sz), free_bo(false)
  {}

  bo_impl(xclDeviceHandle dhdl, xclBufferHandle bhdl, size_t sz)
    : device(xrt_core::get_userpf_device(dhdl)), handle(bhdl), size(sz), free_bo(true)
  {}

  bo_impl(xclDeviceHandle dhdl, xclBufferExportHandle ehdl)
    : device(xrt_core::get_userpf_device(dhdl)), handle(device->import_bo(ehdl)), free_bo(true)
  {
    xclBOProperties prop;
    device->get_bo_properties(handle, &prop);
    size = prop.size;
  }

  bo_impl(const bo_impl* parent, size_t sz)
    : device(parent->device), handle(parent->handle), size(sz), free_bo(false)
  {}

  virtual
  ~bo_impl()
  {
    if (free_bo)
      device->free_bo(handle);
  }

  xclBufferHandle
  get_xcl_handle() const
  {
    return handle;
  }

  const xrt_core::device*
  get_device() const
  {
    return device.get();
  }

  xclBufferExportHandle
  export_buffer() const
  {
    return device->export_bo(handle);
  }

  void
  write(const void* src, size_t sz, size_t seek)
  {
    if (sz + seek > size)
      throw xrt_core::error(-EINVAL,"attempting to write past buffer size");
    auto hbuf = static_cast<char*>(get_hbuf()) + seek;
    std::memcpy(hbuf, src, sz);
  }

  void
  read(void* dst, size_t sz, size_t skip)
  {
    if (sz + skip > size)
      throw xrt_core::error(-EINVAL,"attempting to read past buffer size");
    auto hbuf = static_cast<char*>(get_hbuf()) + skip;
    std::memcpy(dst, hbuf, sz);
  }

  void
  copy(const bo_impl* src, size_t sz, size_t src_offset, size_t dst_offset)
  {
    // Check size and offset of dst and src
    if (!sz)
      throw xrt_core::system_error(EINVAL, "size must be a positive number");
    if (sz + dst_offset > size)
      throw xrt_core::system_error(EINVAL, "copying past destination buffer size");
    if (src->get_size() < sz + src_offset)
      throw xrt_core::system_error(EINVAL, "copying past source buffer size");

    if (get_device() != src->get_device()) {
      copy_with_export(src, sz, src_offset, dst_offset);
      return;
    }

    // try copying with m2m
    try {
      auto m2m = xrt_core::device_query<xrt_core::query::m2m>(get_device());
      if (xrt_core::query::m2m::to_bool(m2m)) {
        device->copy_bo(get_xcl_handle(), src->get_xcl_handle(), sz, dst_offset, src_offset);
        return;
      }
    }
    catch (const std::exception&) {
    }

    // try copying with kdma
    try {
      xrt_core::kernel_int::copy_bo_with_kdma
        (device, sz, get_xcl_handle(), dst_offset, src->get_xcl_handle(), src_offset);
      return;
    }
    catch (const std::exception& ex) {
      auto fmt = boost::format("Reverting to host copy of buffers (%s)") % ex.what();
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",  fmt.str());
    }

    // special case sw emulation on imported buffers
    if (is_sw_emulation() && (is_imported() || src->is_imported())) {
      device->copy_bo(get_xcl_handle(), src->get_xcl_handle(), sz, dst_offset, src_offset);
      return;
    }
      
    // revert to copying through host
    copy_through_host(src, sz, src_offset, dst_offset);
  }

  void
  copy_with_export(const bo_impl* src, size_t sz, size_t src_offset, size_t dst_offset)
  {
    // export bo from other device and create an import bo to copy from
    auto src_export_handle = src->export_buffer();
    auto src_import_bo = xrt::bo(device->get_user_handle(), src_export_handle);
    copy(src_import_bo.get_handle().get(), sz, src_offset, dst_offset);
  }

  void
  copy_through_host(const bo_impl* src, size_t sz, size_t src_offset, size_t dst_offset)
  {
    auto src_hbuf = static_cast<const char*>(src->get_hbuf());
    if (!src_hbuf)
      throw xrt_core::system_error(EINVAL, "No host side buffer in source buffer");

    auto dst_hbuf = static_cast<char*>(get_hbuf());
    if (!dst_hbuf)
      throw xrt_core::system_error(EINVAL, "No host side buffer in destination buffer");

    // sync to src to ensure data integrity, logically const
    const_cast<bo_impl*>(src)->sync(XCL_BO_SYNC_BO_FROM_DEVICE, sz, src_offset);

    // copy host side buffer
    std::memcpy(dst_hbuf + dst_offset, src_hbuf + src_offset, sz);

    // sync modified host buffer to device
    sync(XCL_BO_SYNC_BO_TO_DEVICE, sz, dst_offset);
  }

#ifdef XRT_ENABLE_AIE
  void
  sync(xrt::bo& bo, const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset)
  {
    device->sync_aie_bo(bo, port.c_str(), dir, sz, offset);
  }
#endif

  virtual void
  sync(xclBOSyncDirection dir, size_t sz, size_t offset)
  {
    device->sync_bo(handle, dir, sz, offset + get_offset());
  }

  virtual uint64_t
  get_address() const
  {
    if (addr == no_addr)
      get_bo_properties();

    return addr;
  }

  virtual uint64_t
  get_group_id() const
  {
    if (grpid == no_group)
      get_bo_properties();

    return grpid;
  }

  virtual size_t get_size()      const { return size;    }
  virtual void*  get_hbuf()      const { return nullptr; }
  virtual bool   is_sub_buffer() const { return false;   }
  virtual bool   is_imported()   const { return false;   }
  virtual size_t get_offset()    const { return 0;       }
};


// class buffer_ubuf - User provide host side buffer
//
// Provided buffer must be aligned or exception is thrown
class buffer_ubuf : public bo_impl
{
  void* ubuf;

public:
  buffer_ubuf(xclDeviceHandle dhdl, xclBufferHandle bhdl, size_t sz, void* buf)
    : bo_impl(dhdl, bhdl, sz)
    , ubuf(buf)
  {}

  virtual void*
  get_hbuf() const
  {
    return ubuf;
  }
};


// class buffer_hbuf - XRT allocated host side buffer
//
// XRT allocated host side buffer.  The host side buffer
// is allocated in virtual memory on user space side.
class buffer_hbuf : public bo_impl
{
  xrt_core::aligned_ptr_type hbuf;

public:
  buffer_hbuf(xclDeviceHandle dhdl, xclBufferHandle bhdl, size_t sz, xrt_core::aligned_ptr_type&& b)
    : bo_impl(dhdl, bhdl, sz), hbuf(std::move(b))
  {}

  virtual void*
  get_hbuf() const
  {
    return hbuf.get();
  }
};

// class buffer_kbuf - Kernel driver host side buffer
//
// Kernel driver allocated host side buffer.  The host side buffer
// is allocated in physical memory by kernel driver.
class buffer_kbuf : public bo_impl
{
  void* hbuf;

public:
  buffer_kbuf(xclDeviceHandle dhdl, xclBufferHandle bhdl, size_t sz)
    : bo_impl(dhdl, bhdl, sz), hbuf(device->map_bo(handle, true))
  {}

  ~buffer_kbuf()
  {
    try {
      device->unmap_bo(handle, hbuf);
    }
    catch (...) {
    }
  }

  virtual void*
  get_hbuf() const
  {
    return hbuf;
  }
};

// class buffer_imported - Buffer imported from another device
//
// The exported buffer handle is an opaque type from a call
// to export_buffer() on a buffer to be exported.
class buffer_import : public bo_impl
{
  void* hbuf;

public:
  buffer_import(xclDeviceHandle dhdl, xclBufferExportHandle ehdl)
    : bo_impl(dhdl, ehdl), hbuf(device->map_bo(handle, true))
  {}

  ~buffer_import()
  {
    try {
      device->unmap_bo(handle, hbuf);
    }
    catch (...) {
    }
  }

  virtual bool
  is_imported() const
  {
    return true;
  }

  virtual void*
  get_hbuf() const
  {
    return hbuf;
  }
};

// class buffer_dbuf - device only buffer
//
class buffer_dbuf : public bo_impl
{
public:
  buffer_dbuf(xclDeviceHandle dhdl, xclBufferHandle bhdl, size_t sz)
    : bo_impl(dhdl, bhdl, sz)
  {}

  virtual void*
  get_hbuf() const
  {
    throw xrt_core::error(-EINVAL, "device only buffer has no host buffer");
  }
};

class buffer_nodma : public bo_impl
{
  buffer_kbuf m_host_only;
  buffer_dbuf m_device_only;

public:
  buffer_nodma(xclDeviceHandle dhdl, xclBufferHandle hbuf, xclBufferHandle dbuf, size_t sz)
    : bo_impl(sz), m_host_only(dhdl, hbuf, sz), m_device_only(dhdl, dbuf, sz)
  {

    device = xrt_core::get_userpf_device(dhdl);
    handle = dbuf;
  }

  virtual void*
  get_hbuf() const
  {
    return m_host_only.get_hbuf();
  }

  // sync is M2M copy between host and device bo
  // nodma is guaranteed to have M2M
  void
  sync(xclBOSyncDirection dir, size_t sz, size_t offset)
  {
    if (dir == XCL_BO_SYNC_BO_TO_DEVICE)
      // dst, src, size, dst_offset, src_offset
      device->copy_bo(m_device_only.get_xcl_handle(), m_host_only.get_xcl_handle(), sz, offset, offset);
    else
      // dst, src, size, dst_offset, src_offset
      device->copy_bo(m_host_only.get_xcl_handle(), m_device_only.get_xcl_handle(), sz, offset, offset);
  }
};

// class buffer_sub - Sub buffer
//
// Sub-buffer created from parent buffer
class buffer_sub : public bo_impl
{
  std::shared_ptr<bo_impl> parent;  // participate in ownership of parent
  size_t offset;
  void* hbuf;

public:
  buffer_sub(std::shared_ptr<bo_impl> par, size_t size, size_t off)
    : bo_impl(par.get(), size)
    , parent(std::move(par))
    , offset(off)
    , hbuf(static_cast<char*>(parent->get_hbuf()) + offset)
  {
    if (size + offset > parent->get_size())
      throw xrt_core::error(-EINVAL, "sub buffer size and offset");
  }

  virtual size_t
  get_offset() const
  {
    return offset;
  }

  virtual void*
  get_hbuf() const
  {
    return hbuf;
  }

  virtual bool
  is_sub_buffer() const
  {
    return true;
  }

  virtual uint64_t
  get_address() const
  {
    return bo_impl::get_address() + offset;
  }
};

} // namespace xrt

// Implementation details
namespace {

// C-API handles that must be explicitly closed. Corresponding managed
// handles are inserted in this map.  When the unmanaged handle is
// closed, it is removed from this map and underlying buffer is
// deleted if no other shared ptrs exists for this buffer
static std::map<xrtBufferHandle, std::shared_ptr<xrt::bo_impl>> bo_cache;

static const std::shared_ptr<xrt::bo_impl>&
get_boh(xrtBufferHandle bhdl)
{
  auto itr = bo_cache.find(bhdl);
  if (itr == bo_cache.end())
    throw xrt_core::error(-EINVAL, "No such buffer handle");
  return (*itr).second;
}

static xclBufferHandle
alloc_bo(xclDeviceHandle dhdl, void* userptr, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  auto device = xrt_core::get_userpf_device(dhdl);
  flags = (flags & ~XRT_BO_FLAGS_MEMIDX_MASK) | grp;
  return device->alloc_bo(userptr, sz, flags);
}

static xclBufferHandle
alloc_bo(xclDeviceHandle dhdl, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  auto device = xrt_core::get_userpf_device(dhdl);
  flags = (flags & ~XRT_BO_FLAGS_MEMIDX_MASK) | grp;
  return device->alloc_bo(sz, flags);
}

static void
free_bo(xrtBufferHandle bhdl)
{
  if (bo_cache.erase(bhdl) == 0)
    throw std::runtime_error("Unexpected internal error");
}

// driver allocates host buffer
static std::shared_ptr<xrt::bo_impl>
alloc_kbuf(xclDeviceHandle dhdl, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  auto handle = alloc_bo(dhdl, sz, flags, grp);
  auto boh = std::make_shared<xrt::buffer_kbuf>(dhdl, handle, sz);
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_ubuf(xclDeviceHandle dhdl, void* userptr, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  // error if userptr is not aligned properly
  if (!is_aligned_ptr(userptr))
    throw xrt_core::error(-EINVAL, "userptr is not aligned");

  auto handle = alloc_bo(dhdl, userptr, sz, flags, grp);
  auto boh = std::make_shared<xrt::buffer_ubuf>(dhdl, handle, sz, userptr);
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_hbuf(xclDeviceHandle dhdl, xrt_core::aligned_ptr_type&& hbuf, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  auto handle =  alloc_bo(dhdl, hbuf.get(), sz, flags, grp);
  auto boh = std::make_shared<xrt::buffer_hbuf>(dhdl, handle, sz, std::move(hbuf));
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_dbuf(xclDeviceHandle dhdl, size_t sz, xrtBufferFlags, xrtMemoryGroup grp)
{
  auto handle = alloc_bo(dhdl, sz, XCL_BO_FLAGS_DEV_ONLY, grp);
  auto boh = std::make_shared<xrt::buffer_dbuf>(dhdl, handle, sz);
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_nodma(xclDeviceHandle dhdl, size_t sz, xrtBufferFlags, xrtMemoryGroup grp)
{
  try {
    auto hbuf_handle = alloc_bo(dhdl, sz, XCL_BO_FLAGS_HOST_ONLY, grp);
    auto dbuf_handle = alloc_bo(dhdl, sz, XCL_BO_FLAGS_DEV_ONLY, grp);
    auto boh = std::make_shared<xrt::buffer_nodma>(dhdl, hbuf_handle, dbuf_handle, sz);
    return boh;
  }
  catch (const std::exception& ex) {
    auto fmt = boost::format("Failed to allocate host memory buffer (%s), make sure host bank is enabled "
                             "(see xbutil host_mem --enable ...)") % ex.what();
    send_exception_message(fmt.str());
    throw;
  }
}

static std::shared_ptr<xrt::bo_impl>
alloc(xclDeviceHandle dhdl, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  auto type = flags & ~XRT_BO_FLAGS_MEMIDX_MASK;
  switch (type) {
  case 0:
#ifndef XRT_EDGE
    if (is_nodma(dhdl))
      return alloc_nodma(dhdl, sz, flags, grp);
    else
      return alloc_hbuf(dhdl, xrt_core::aligned_alloc(get_alignment(), sz), sz, flags, grp);
#endif
  case XCL_BO_FLAGS_CACHEABLE:
  case XCL_BO_FLAGS_SVM:
  case XCL_BO_FLAGS_HOST_ONLY:
  case XCL_BO_FLAGS_P2P:
  case XCL_BO_FLAGS_EXECBUF:
    return alloc_kbuf(dhdl, sz, flags, grp);
  case XCL_BO_FLAGS_DEV_ONLY:
    return alloc_dbuf(dhdl, sz, flags, grp);
  default:
    throw std::runtime_error("Unknown buffer type");
  }
}

static std::shared_ptr<xrt::bo_impl>
alloc_userptr(xclDeviceHandle dhdl, void* userptr, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  return alloc_ubuf(dhdl, userptr, sz, flags, grp);
}

static std::shared_ptr<xrt::bo_impl>
alloc_import(xclDeviceHandle dhdl, xclBufferExportHandle ehdl)
{
  return std::make_shared<xrt::buffer_import>(dhdl, ehdl);
}

static std::shared_ptr<xrt::bo_impl>
sub_buffer(const std::shared_ptr<xrt::bo_impl>& parent, size_t size, size_t offset)
{
  return std::make_shared<xrt::buffer_sub>(parent, size, offset);
}

static xclDeviceHandle
get_xcl_device_handle(xrtDeviceHandle dhdl)
{
  return xrt_core::device_int::get_xcl_device_handle(dhdl);
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_bo implementation of extension APIs not exposed to end-user
////////////////////////////////////////////////////////////////
namespace xrt_core { namespace bo {

uint64_t
address(const xrt::bo& bo)
{
  return bo.get_handle()->get_address();
}

uint64_t
address(xrtBufferHandle handle)
{
  return get_boh(handle)->get_address();
}

int32_t
group_id(const xrt::bo& bo)
{
  return bo.get_handle()->get_group_id();
}

void
fill_copy_pkt(const xrt::bo& dst, const xrt::bo& src, size_t sz,
              size_t dst_offset, size_t src_offset, ert_start_copybo_cmd* pkt)
{
#ifndef _WIN32
  const auto& dst_boh = dst.get_handle();
  const auto& src_boh = src.get_handle();
  ert_fill_copybo_cmd(pkt, src_boh->get_xcl_handle(), dst_boh->get_xcl_handle(), src_offset, dst_offset, sz);
#else
  throw std::runtime_error("ert_fill_copybo_cmd not implemented on windows");
#endif
}

bool
is_imported(const xrt::bo& bo)
{
  const auto& boh = bo.get_handle();
  return boh->is_imported();
}

bool
is_aligned_ptr(const void* ptr)
{
  return ::is_aligned_ptr(ptr);
}

size_t
alignment()
{
  return ::get_alignment();
}

}} // namespace bo, xrt_core


////////////////////////////////////////////////////////////////
// xrt_bo C++ API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////
namespace xrt {

bo::
bo(xclDeviceHandle dhdl, void* userptr, size_t sz, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
           alloc_userptr, dhdl, userptr, sz, static_cast<xrtBufferFlags>(flags), grp))
{}

bo::
bo(xclDeviceHandle dhdl, size_t size, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
           alloc, dhdl, size, static_cast<xrtBufferFlags>(flags), grp))
{}

bo::
bo(xclDeviceHandle dhdl, xclBufferExportHandle ehdl)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
	   alloc_import, dhdl, ehdl))
{}

bo::
bo(const bo& parent, size_t size, size_t offset)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
	   sub_buffer, parent.handle, size, offset))
{}

bo::
bo(xrtBufferHandle xhdl)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
	   get_boh, xhdl))
{}

size_t
bo::
size() const
{
  return xdp::native::profiling_wrapper("xrt::bo::size", [this]{
    return handle->get_size();
  }) ;
}

uint64_t
bo::
address() const
{
  return xdp::native::profiling_wrapper("xrt::bo::address", [this]{
    return handle->get_address();
  });
}

xclBufferExportHandle
bo::
export_buffer()
{
  return xdp::native::profiling_wrapper("xrt::bo::export_buffer", [this]{
    return handle->export_buffer();
  });
}

void
bo::
sync(xclBOSyncDirection dir, size_t size, size_t offset)
{
  return xdp::native::profiling_wrapper("xrt::bo::sync",
    [this, dir, size, offset]{
      handle->sync(dir, size, offset);
    });
}

void*
bo::
map()
{
  return xdp::native::profiling_wrapper("xrt::bo::map", [this]{
    return handle->get_hbuf();
  });
}

void
bo::
write(const void* src, size_t size, size_t seek)
{
  xdp::native::profiling_wrapper("xrt::bo::write", [this, src, size, seek]{
    handle->write(src, size, seek);
  });
}

void
bo::
read(void* dst, size_t size, size_t skip)
{
  xdp::native::profiling_wrapper("xrt::bo::read", [this, dst, size, skip]{
    handle->read(dst, size, skip);
  });
}

void
bo::
copy(const bo& src, size_t sz, size_t src_offset, size_t dst_offset)
{
  xdp::native::profiling_wrapper("xrt::bo::copy",
    [this, &src, sz, src_offset, dst_offset]{
      handle->copy(src.handle.get(), sz, src_offset, dst_offset);
    });
}

} // xrt

#ifdef XRT_ENABLE_AIE
////////////////////////////////////////////////////////////////
// xrt_aie_bo C++ API implmentations (xrt_aie.h)
////////////////////////////////////////////////////////////////
namespace xrt { namespace aie {

void
bo::
sync(const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset)
{
  const auto& handle = get_handle();
  handle->sync(*this, port, dir, sz, offset);
}

}} // namespace aie, xrt
#endif

////////////////////////////////////////////////////////////////
// xrt_bo API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////
xrtBufferHandle
xrtBOAllocUserPtr(xrtDeviceHandle dhdl, void* userptr, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, userptr, size, flags, grp]{
      auto boh = alloc_userptr(get_xcl_device_handle(dhdl), userptr, size, flags, grp);
      bo_cache[boh.get()] = boh;
      return boh.get();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;

}

xrtBufferHandle
xrtBOAlloc(xrtDeviceHandle dhdl, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, size, flags, grp]{
      auto boh = alloc(get_xcl_device_handle(dhdl), size, flags, grp);
      bo_cache[boh.get()] = boh;
      return boh.get();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

xrtBufferHandle
xrtBOSubAlloc(xrtBufferHandle phdl, size_t sz, size_t offset)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [phdl, sz, offset]{
      const auto& parent = get_boh(phdl);
      auto boh = sub_buffer(parent, sz, offset);
      bo_cache[boh.get()] = boh;
      return boh.get();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

xrtBufferHandle
xrtBOImport(xrtDeviceHandle dhdl, xclBufferExportHandle ehdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, ehdl]{
      auto boh = alloc_import(get_xcl_device_handle(dhdl), ehdl);
      bo_cache[boh.get()] = boh;
      return boh.get();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

xclBufferExportHandle
xrtBOExport(xrtBufferHandle bhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [bhdl]{
      return get_boh(bhdl)->export_buffer();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return XRT_NULL_BO_EXPORT;
}

int
xrtBOFree(xrtBufferHandle bhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [bhdl]{
      free_bo(bhdl);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

size_t
xrtBOSize(xrtBufferHandle bhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [bhdl]{
      return get_boh(bhdl)->get_size();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return 0;
}


int
xrtBOSync(xrtBufferHandle bhdl, xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [bhdl, dir, size, offset]{
      get_boh(bhdl)->sync(dir, size, offset);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

void*
xrtBOMap(xrtBufferHandle bhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [bhdl]{
      return get_boh(bhdl)->get_hbuf();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

int
xrtBOWrite(xrtBufferHandle bhdl, const void* src, size_t size, size_t seek)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [bhdl, src, size, seek]{
      get_boh(bhdl)->write(src, size, seek);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtBORead(xrtBufferHandle bhdl, void* dst, size_t size, size_t skip)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [bhdl, dst, size, skip]{
      get_boh(bhdl)->read(dst, size, skip);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtBOCopy(xrtBufferHandle dhdl, xrtBufferHandle shdl, size_t sz, size_t dst_offset, size_t src_offset)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, shdl, sz, dst_offset, src_offset]{
      const auto& dst = get_boh(dhdl);
      const auto& src = get_boh(shdl);
      dst->copy(src.get(), sz, src_offset, dst_offset);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}


uint64_t
xrtBOAddress(xrtBufferHandle bhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [bhdl]{
      return get_boh(bhdl)->get_address();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return std::numeric_limits<uint64_t>::max();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return std::numeric_limits<uint64_t>::max();
  }
}
