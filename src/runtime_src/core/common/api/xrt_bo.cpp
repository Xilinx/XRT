/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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

#include "bo.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/memalign.h"
#include "core/common/unistd.h"

#include <map>
#include <set>

#ifdef _WIN32
# pragma warning( disable : 4244 )
#endif

////////////////////////////////////////////////////////////////
// Exposed for Cardano as extensions to xrt_bo.h
// Revisit post 2020.1
////////////////////////////////////////////////////////////////
/**
 * xrtBOAddress() - Get the address of device side of buffer
 *
 * @bo:      Buffer object
 * Return:   Address of device side buffer
 */
XCL_DRIVER_DLLESPEC
uint64_t
xrtBOAddress(const xrt::bo& bo);

/**
 * xrtBOAddress() - Get the address of device side of buffer
 *
 * @handle:  Buffer handle
 * Return:   Address of device side buffer
 */
XCL_DRIVER_DLLESPEC
uint64_t
xrtBOAddress(xrtBufferHandle bhdl);
///////////////////////////////////////////////////////////////

namespace {

inline size_t
get_alignment()
{
  return xrt_core::getpagesize();
}

inline  bool
is_aligned_ptr(void* p)
{
  return p && (reinterpret_cast<uintptr_t>(p) % get_alignment())==0;
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
protected:
  std::shared_ptr<xrt_core::device> device;
  xclBufferHandle handle;  // driver handle
  size_t size;             // size of buffer
  bool free_bo;            // should dtor free bo

public:
  bo_impl(xclDeviceHandle dhdl, xclBufferHandle bhdl, size_t sz)
    : device(xrt_core::get_userpf_device(dhdl)), handle(bhdl), size(sz), free_bo(true)
  {}

  bo_impl(const bo_impl* parent, size_t sz)
    : device(parent->device), handle(parent->handle), size(sz), free_bo(false)
  {}

  virtual
  ~bo_impl()
  {
    if (free_bo)
      device->free_bo(handle);
  }

  void
  sync(xclBOSyncDirection dir, size_t sz, size_t offset)
  {
    device->sync_bo(handle, dir, sz, offset);
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

  virtual uint64_t
  address() const
  {
    xclBOProperties prop;
    device->get_bo_properties(handle, &prop);
    return prop.paddr;
  }

  virtual size_t get_size()      const { return size;    }
  virtual void*  get_hbuf()      const { return nullptr; }
  virtual bool   is_sub_buffer() const { return false;   }
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
    device->unmap_bo(handle, hbuf);
  }

  virtual void*
  get_hbuf() const
  {
    return hbuf;
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
  address() const
  {
    return bo_impl::address() + offset;
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

static std::shared_ptr<xrt::bo_impl>
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

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", msg);
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
alloc(xclDeviceHandle dhdl, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  auto type = flags & ~XRT_BO_FLAGS_MEMIDX_MASK;
  switch (type) {
  case 0:
    return alloc_hbuf(dhdl, xrt_core::aligned_alloc(get_alignment(), sz), sz, flags, grp);
  case XCL_BO_FLAGS_CACHEABLE:
  case XCL_BO_FLAGS_SVM:
  case XCL_BO_FLAGS_DEV_ONLY:
  case XCL_BO_FLAGS_HOST_ONLY:
  case XCL_BO_FLAGS_P2P:
  case XCL_BO_FLAGS_EXECBUF:
    return alloc_kbuf(dhdl, sz, flags, grp);
  default:
    throw std::runtime_error("Unknown buffer type");
  }
}

static std::shared_ptr<xrt::bo_impl>
alloc(xclDeviceHandle dhdl, void* userptr, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  return alloc_ubuf(dhdl, userptr, sz, flags, grp);
}


static std::shared_ptr<xrt::bo_impl>
sub_buffer(const std::shared_ptr<xrt::bo_impl>& parent, size_t size, size_t offset)
{
  return std::make_shared<xrt::buffer_sub>(parent, size, offset);
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_bo implementation of extension APIs not exposed to end-user
////////////////////////////////////////////////////////////////
namespace xrt_core { namespace bo {

uint64_t
address(const xrt::bo& bo)
{
  auto boh = bo.get_handle();
  return boh->address();
}

uint64_t
address(xrtBufferHandle handle)
{
  auto boh = get_boh(handle);
  return boh->address();
}

}} // namespace bo, xrt_core


////////////////////////////////////////////////////////////////
// xrt_bo C++ API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////
namespace xrt {

bo::
bo(xclDeviceHandle dhdl, void* userptr, size_t sz, buffer_flags flags, memory_group grp)
  : handle(alloc(dhdl, userptr, sz, flags, grp))
{}

bo::
bo(xclDeviceHandle dhdl, size_t size, buffer_flags flags, memory_group grp)
  : handle(alloc(dhdl, size, flags, grp))
{}

bo::
bo(const bo& parent, size_t size, size_t offset)
  : handle(sub_buffer(parent.handle, size, offset))
{}

void
bo::
sync(xclBOSyncDirection dir, size_t size, size_t offset)
{
  handle->sync(dir, size, offset);
}

void*
bo::
map()
{
  return handle->get_hbuf();
}

void
bo::
write(const void* src, size_t size, size_t seek)
{
  handle->write(src, size, seek);
}

void
bo::
read(void* dst, size_t size, size_t skip)
{
  handle->read(dst, size, skip);
}

} // xrt

////////////////////////////////////////////////////////////////
// xrt_bo API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////
xrtBufferHandle
xrtBOAllocUserPtr(xclDeviceHandle dhdl, void* userptr, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  try {
    auto boh = alloc(dhdl, userptr, size, flags, grp);
    bo_cache[boh.get()] = boh;
    return boh.get();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;
}

xrtBufferHandle
xrtBOAlloc(xclDeviceHandle dhdl, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  try {
    auto boh = alloc(dhdl, size, flags, grp);
    bo_cache[boh.get()] = boh;
    return boh.get();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;
}

xrtBufferHandle
xrtBOSubAlloc(xrtBufferHandle phdl, size_t sz, size_t offset)
{
  try {
    auto parent = get_boh(phdl);
    auto boh = sub_buffer(parent, sz, offset);
    bo_cache[boh.get()] = boh;
    return boh.get();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;

}

int
xrtBOFree(xrtBufferHandle bhdl)
{
  try {
    free_bo(bhdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return errno = 0;
  }
}

int
xrtBOSync(xrtBufferHandle bhdl, xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    auto boh = get_boh(bhdl);
    boh->sync(dir, size, offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return errno = 0;
  }
}

void*
xrtBOMap(xrtBufferHandle bhdl)
{
  try {
    auto boh = get_boh(bhdl);
    return boh->get_hbuf();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;
}

int
xrtBOWrite(xrtBufferHandle bhdl, const void* src, size_t size, size_t seek)
{
  try {
    auto boh = get_boh(bhdl);
    boh->write(src, size, seek);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return errno = 0;
  }
}

int
xrtBORead(xrtBufferHandle bhdl, void* dst, size_t size, size_t skip)
{
  try {
    auto boh = get_boh(bhdl);
    boh->read(dst, size, skip);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return errno = 0;
  }
}

uint64_t
xrtBOAddress(xrtBufferHandle bhdl)
{
  try {
    return xrt_core::bo::address(bhdl);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return errno = 0;
  }
}

uint64_t
xrtBOAddress(const xrt::bo& bo)
{
  return xrt_core::bo::address(bo);
}
