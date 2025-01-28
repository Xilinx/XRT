// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT BO APIs as declared in
// core/include/experimental/xrt_bo.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_bo.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#define XRT_API_SOURCE         // in same dll as api
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_aie.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/detail/xrt_mem.h"
#include "core/include/xrt/experimental/xrt_ext.h"

#include "native_profile.h"
#include "bo.h"

#include "device_int.h"
#include "handle.h"
#include "hw_context_int.h"
#include "kernel_int.h"
#include "core/common/api/bo_int.h"
#include "core/common/device.h"
#include "core/common/memalign.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "core/common/trace.h"
#include "core/common/unistd.h"
#include "core/common/xclbin_parser.h"

#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/shared_handle.h"

#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
# pragma warning( disable : 4244 4100 4996 4505 26813)
#endif

// This file uses static globals, which clang-tidy warns about.  We
// disable the warning for this file.
namespace {

[[maybe_unused]]
static bool
is_noop_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE"); // NOLINT(concurrency-mt-unsafe)
  static bool noop = xem ? (std::strcmp(xem,"noop")==0) : false;
  return noop;
}

[[maybe_unused]]
static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE"); // NOLINT(concurrency-mt-unsafe)
  static bool swemu = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swemu;
}

inline bool
is_nodma(const xrt_core::device* device)
{
  return device->is_nodma();
}

inline bool
is_nodma(const xrt::device& device)
{
  return is_nodma(device.get_handle().get());
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

namespace {

// The xrt::bo is extended to capture the hardware context (if any) in
// which the bo is allocated.  In order to minimize changes to various
// constructors, the device_type class captures both a core device
// object and a hardware context, where the hardware context is allowed
// to be empty.
class device_type
{
  xrt::hw_context m_hwctx;
  std::shared_ptr<xrt_core::device> m_device;
public:
  device_type() = default;

  device_type(std::shared_ptr<xrt_core::device> device) // NOLINT converting ctor
    : m_device(std::move(device))
  {}

  device_type(xrt::hw_context hwctx)  // NOLINT converting ctor
    : m_hwctx(std::move(hwctx))
    , m_device(xrt_core::hw_context_int::get_core_device(m_hwctx))
  {}

  [[nodiscard]] bool
  is_valid_hwctx() const
  {
    return static_cast<xrt_core::hwctx_handle*>(m_hwctx) != nullptr;
  }

  [[nodiscard]] const xrt_core::device*
  get_core_device() const
  {
    return m_device.get();
  }

  [[nodiscard]] const std::shared_ptr<xrt_core::device>&
  get_device() const
  {
    return m_device;
  }

  [[nodiscard]] xrt_core::hwctx_handle*
  get_hwctx_handle() const
  {
    return (m_hwctx)
      ? static_cast<xrt_core::hwctx_handle*>(m_hwctx)
      : nullptr;
  }

  xrt_core::device*
  operator->() const
  {
    return m_device.get();
  }
};

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
  using export_handle = xrt_core::shared_handle::export_handle;

  static constexpr uint64_t no_addr = std::numeric_limits<uint64_t>::max();
  static constexpr uint32_t no_group = std::numeric_limits<uint32_t>::max();
  static constexpr bo::flags no_flags = static_cast<bo::flags>(std::numeric_limits<uint32_t>::max());

private:
  void
  get_bo_properties() const
  {
    auto prop = handle->get_properties();
    addr = prop.paddr;

    // Flags are what was used by shim::alloc_bo when the BO was
    // created. What is stored in bo_impl, are the flags that were used
    // to indicate the type of the BO (per xrt::bo::flags). Currrently
    // bo_impl doesn't track or provide access to the extension flags
    // in xcl_bo_flags.
    xcl_bo_flags xflags {prop.flags};
    grpid = xflags.bank;
    flags = static_cast<bo::flags>(xflags.flags & ~XRT_BO_FLAGS_MEMIDX_MASK);
  }

  // Usage logger for logging buffer stats
  std::shared_ptr<xrt_core::usage_metrics::base_logger> m_usage_logger =
      xrt_core::usage_metrics::get_usage_metrics_logger();

protected:
  // deliberately made protected, this is a file-scoped controlled API
  device_type device;                              // NOLINT device where bo is allocated
  std::vector<std::shared_ptr<bo_impl>> clones;    // NOLINT local m2m clones if any
  std::shared_ptr<xrt_core::buffer_handle> handle; // NOLINT shim handle
  size_t size = 0;                                 // NOLINT size of buffer
  mutable uint64_t addr = no_addr;                 // NOLINT bo device address
  mutable uint32_t grpid = no_group;               // NOLINT memory group index
  mutable bo::flags flags = no_flags;              // NOLINT flags per bo properties
  mutable std::unique_ptr<xrt_core::shared_handle> shared_handle; // NOLINT

public:
  // No handle
  explicit bo_impl(size_t sz)
    : size(sz)
  {}

  // Managed handle shared with another bo_impl
  bo_impl(device_type dev, std::shared_ptr<xrt_core::buffer_handle> bhdl, size_t sz)
    : device(std::move(dev))
    , handle(std::move(bhdl))
    , size(sz)
  {}

  // Managed handle
  bo_impl(device_type dev, std::unique_ptr<xrt_core::buffer_handle> bhdl, size_t sz)
    : device(std::move(dev))
    , handle(std::move(bhdl))
    , size(sz)
  {}

  // Managed imported handle
  bo_impl(device_type dev, pid_type pid, xrt_core::shared_handle::export_handle ehdl)
    : device(std::move(dev))
  {
    auto hwctx = device.get_hwctx_handle();
    handle = hwctx
      ? hwctx->import_bo(pid.pid, ehdl)
      : device->import_bo(pid.pid, ehdl);

    auto prop = handle->get_properties();
    size = prop.size;
  }

  // Managed imported handle
  bo_impl(const device_type& dev, xrt_core::shared_handle::export_handle ehdl)
    : bo_impl(dev, pid_type{0}, ehdl)
  {}

  // Not supported
  bo_impl(device_type dev, xcl_buffer_handle xhdl)
  {} // throw xrt_core::error(std::errc::not_supported, "xcl type objects are no longer supported");

  // Share handle with parent
  bo_impl(const bo_impl* parent, size_t sz)
    : device(parent->device)
    , handle(parent->handle)
    , size(sz)
  {}

  virtual ~bo_impl() = default;

  bo_impl(const bo_impl&) = delete;
  bo_impl(bo_impl&&) = delete;
  bo_impl& operator=(bo_impl&) = delete;
  bo_impl& operator=(bo_impl&&) = delete;

  xrt_core::buffer_handle*
  get_handle() const
  {
    return handle.get();
  }

  xrt_core::usage_metrics::base_logger*
  get_usage_logger() const
  {
    return m_usage_logger.get();
  }

  // BOs can be cloned internally by XRT to statisfy kernel
  // connectivity, the lifetime of a cloned BO is tied to the
  // lifetime of the BO from which is was cloned.
  void
  add_clone(std::shared_ptr<bo_impl> clone)
  {
    clones.push_back(std::move(clone));
  }

  const xrt_core::device*
  get_core_device() const
  {
    return device.get_core_device();
  }

  const std::shared_ptr<xrt_core::device>&
  get_device() const
  {
    return device.get_device();
  }

  xrt_core::hwctx_handle*
  get_hwctx_handle() const
  {
    return device.get_hwctx_handle();
  }

  void*
  get_hbuf_or_error() const
  {
    if (auto hbuf = get_hbuf())
      return hbuf;

    throw xrt_core::error("buffer is not mapped");
  }

  export_handle
  export_buffer() const
  {
    if (!shared_handle)
      shared_handle = handle->share();

    return shared_handle->get_export_handle();
  }

  virtual void
  write(const void* src, size_t sz, size_t seek)
  {
    if (sz + seek > size)
      throw xrt_core::error(-EINVAL,"attempting to write past buffer size");
    auto hbuf = static_cast<char*>(get_hbuf_or_error()) + seek;
    std::memcpy(hbuf, src, sz);
  }

  virtual void
  read(void* dst, size_t sz, size_t skip)
  {
    if (sz + skip > size)
      throw xrt_core::error(-EINVAL,"attempting to read past buffer size");
    auto hbuf = static_cast<char*>(get_hbuf_or_error()) + skip;
    std::memcpy(dst, hbuf, sz);
  }

  virtual void
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
        handle->copy(src->handle.get(), sz, dst_offset, src_offset);
        return;
      }
    }
    catch (const std::exception&) {
      // try next option
    }

    // try copying with kdma
    try {
      if (xrt_core::config::get_cdma()) {
        xrt_core::kernel_int::copy_bo_with_kdma
          (get_device(), sz, handle.get(), dst_offset, src->handle.get(), src_offset);
        return;
      }
    }
    catch (const std::exception& ex) {
      auto fmt = boost::format("Reverting to host copy of buffers (%s)") % ex.what();
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",  fmt.str());
    }

    // special case sw emulation on imported buffers
    if (is_sw_emulation() && (is_imported() || src->is_imported())) {
      handle->copy(src->handle.get(), sz, dst_offset, src_offset);
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
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) // special case
    const_cast<bo_impl*>(src)->sync(XCL_BO_SYNC_BO_FROM_DEVICE, sz, src_offset);

    // copy host side buffer
    std::memcpy(dst_hbuf + dst_offset, src_hbuf + src_offset, sz);

    // sync modified host buffer to device
    sync(XCL_BO_SYNC_BO_TO_DEVICE, sz, dst_offset);
  }

  void
  sync(xrt::bo& bo, const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset)
  {
    //call sync & async functions with "Buffer handle" instead of with device, so that it could be handled with both device & hwctx
    handle->sync_aie_bo(bo, port.c_str(), dir, sz, offset);
  }

  xrt::bo::async_handle
  async(xrt::bo& bo, const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset);

  xrt::bo::async_handle
  async(xrt::bo& bo, xclBOSyncDirection dir, size_t sz, size_t offset);

  virtual void
  sync(xclBOSyncDirection dir, size_t sz, size_t offset)
  {
    // One may think that host_only BOs should not be synced, but here
    // is the deal: The sync does not really do DMA, but just flush
    // the CPU cache (to_device) so that device will get the most
    // up-to-date data from physical memory or invalid CPU cache
    // (from_device) so that host CPU can read the most up-to-date
    // data device has put into the physical memory. As of today, all
    // Xilinx's Alveo devices will automatically trigger cache
    // coherence actions when it reads from or write to physical
    // memory, but we still recommend user to perform explicit BO sync
    // operation just in case the HW changes in the future.
    // if (get_flags() != bo::flags::host_only)
    handle->sync(static_cast<xrt_core::buffer_handle::direction>(dir), sz, offset);
    m_usage_logger->log_buffer_sync(device->get_device_id(), device.get_hwctx_handle(), sz, dir);
  }

  virtual uint64_t
  get_address() const
  {
    if (addr == no_addr)
      get_bo_properties();

    return addr;
  }

  virtual uint32_t
  get_group_id() const
  {
    if (grpid == no_group)
      get_bo_properties();

    return grpid;
  }

  virtual bo::flags
  get_flags() const
  {
    if (flags == no_flags)
      get_bo_properties();

    return flags;
  }

  virtual size_t get_size()      const { return size;    }
  virtual size_t get_offset()    const { return 0;       }
  virtual void*  get_hbuf()      const { return nullptr; }
  virtual bool   is_sub()        const { return false;   }
  virtual bool   is_imported()   const { return false;   }
};

// class bo::async_handle_impl - Base class for asynchronous buffer DMA handle
//
// Derived classes:
// [aie::b ::async_handle_impl]: For AIE BOs
//
// Impl Class associated with async bo which allows to wait for completion
class bo::async_handle_impl
{
public:
  xrt::bo m_bo;

public:
  // async_bo_impl() - Construct async_bo_obj
  explicit async_handle_impl(xrt::bo bo)
    : m_bo(std::move(bo))
  {}

  virtual ~async_handle_impl() = default;

  async_handle_impl() = delete;
  async_handle_impl(const async_handle_impl&) = delete;
  async_handle_impl(async_handle_impl&&) = delete;
  async_handle_impl& operator=(const async_handle_impl&) = delete;
  async_handle_impl& operator=(async_handle_impl&&) = delete;

  // wait() - Wait for async to complete
  virtual void
  wait()
  {
    throw std::runtime_error("Unsupported feature");
  }
};

class aie::bo::async_handle_impl : public xrt::bo::async_handle_impl
{
  // class for holding AIE BO Async DMA transfer information
  class handle_map
  {
  public:
    // Insert async_handle for every new DMA transfer
    void
    insert(const std::string& gmio_name, const xrt::aie::bo::async_handle_impl* hdl)
    {
      std::lock_guard lk(async_bo_hdls_mutex);
      async_bo_hdls[gmio_name].emplace_back(hdl);
    }

    // Clear all handles for a given gmio_name as DMA has finished
    void
    clear(const std::string& gmio_name)
    {
      std::lock_guard lk(async_bo_hdls_mutex);
      async_bo_hdls[gmio_name].clear();
    }

    // Check if async_hdl is present for a particular gmio_name
    // If present then DMA has not finished yet
    bool
    found(const std::string& gmio_name, const xrt::aie::bo::async_handle_impl* hdl) const
    {
      std::lock_guard lk(async_bo_hdls_mutex);
      auto itr = async_bo_hdls.find(gmio_name);
      if (itr == async_bo_hdls.end())
        throw std::runtime_error("Unexpected error");

      // Check gmio in the map to see if DMA has finished
      return (std::find(itr->second.begin(), itr->second.end(), hdl) != itr->second.end());
    }

  private:
    // Map of gmio -> list of async handles
    // Handle entry means there DMA is in progress for that gmio name
    // All handle entries for a gmio name are removed when wait() is called
    using handles = std::vector<const xrt::aie::bo::async_handle_impl*>;
    std::unordered_map<std::string, handles> async_bo_hdls;
    mutable std::mutex async_bo_hdls_mutex;// Mutex for use with above map
  };

public:
  size_t m_bd_num; // For future use
  std::string m_gmio_name;
  static handle_map async_info;

public:
  // async_bo_impl() - Construct async_bo_obj
  async_handle_impl(xrt::bo bo, size_t bd_num, std::string gmio_name)
    : xrt::bo::async_handle_impl(std::move(bo))
    , m_bd_num(bd_num)
    , m_gmio_name(std::move(gmio_name))
  {
    async_info.insert(m_gmio_name, this);
  }

  // wait() - Wait for async to complete
  void
  wait() override
  {
    // DMA has already finished if not found
    if(!async_info.found(m_gmio_name, this))
      return;

    auto device = m_bo.get_handle()->get_device();
    // DMA has not finished; Wait for it;
    // In future wait only for specific m_bd_num
    device->wait_gmio(m_gmio_name.c_str());
    // All outstanding DMAs for this gmio_name have finishedd; for all bd numbers
    async_info.clear(m_gmio_name);
  }
}; // class aie::bo::async_handle_impl

// Initialize static data member for async info
aie::bo::async_handle_impl::handle_map aie::bo::async_handle_impl::async_info;

xrt::bo::async_handle
bo_impl::
async(xrt::bo& bo, const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset)
{
  //call sync & async functions with "Buffer handle" instead of with device, so that it could be handled with both device & hwctx
  handle->sync_aie_bo_nb(bo, port.c_str(), dir, sz, offset);

  auto a_bo_impl = std::make_shared<xrt::aie::bo::async_handle_impl>(bo, 0, port);

  return xrt::bo::async_handle{a_bo_impl};
}

xrt::bo::async_handle
bo_impl::
async(xrt::bo& bo, xclBOSyncDirection dir, size_t sz, size_t offset)
{
  throw std::runtime_error("Unsupported feature");

#if 0
  //TODO for Alveo; base xrt::bo class
  auto a_bo_impl = std::make_shared<xrt::bo::async_handle_impl>(bo);
  return xrt::bo::async_handle{a_bo_impl};
#endif
}

// class buffer_ubuf - User provide host side buffer
//
// Provided buffer must be aligned or exception is thrown
class buffer_ubuf : public bo_impl
{
  void* ubuf;

public:
  buffer_ubuf(const device_type& dev, std::unique_ptr<xrt_core::buffer_handle> bhdl, size_t sz, void* buf)
    : bo_impl(dev, std::move(bhdl), sz)
    , ubuf(buf)
  {}

  void*
  get_hbuf() const override
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
  buffer_hbuf(const device_type& dev, std::unique_ptr<xrt_core::buffer_handle> bhdl, size_t sz, xrt_core::aligned_ptr_type&& b)
    : bo_impl(dev, std::move(bhdl), sz)
    , hbuf(std::move(b))
  {}

  void*
  get_hbuf() const override
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
  buffer_kbuf(const device_type& dev, std::unique_ptr<xrt_core::buffer_handle> bhdl, size_t sz)
    : bo_impl(dev, std::move(bhdl), sz)
    , hbuf(handle->map(xrt_core::buffer_handle::map_type::write))
  {}

  ~buffer_kbuf() override
  {
    // Imported BO can fail in xclUnmapBO if the exported BO has
    // already been unmapped or vice versa.
    try {
      handle->unmap(hbuf);
    }
    catch (...) {
    }
  }

  buffer_kbuf(const buffer_kbuf&) = delete;
  buffer_kbuf(buffer_kbuf&&) = delete;
  buffer_kbuf& operator=(buffer_kbuf&) = delete;
  buffer_kbuf& operator=(buffer_kbuf&&) = delete;

  void*
  get_hbuf() const override
  {
    return hbuf;
  }
};

// class buffer_imported - Buffer imported from another device
//
// The exported buffer handle is an opaque type from a call
// to export_buffer() on a buffer to be exported.  The exported
// buffer can be imported within same process or from another
// process (linux pidfd support required)
class buffer_import : public bo_impl
{
  void* hbuf;

public:
  // buffer_import() - Import the buffer
  //
  // @device:  device to import to
  // @ehdl:    export handle obtained by calling export_buffer
  buffer_import(const device_type& dev, export_handle ehdl)
    : bo_impl(dev, ehdl)
  {
    try {
      hbuf = handle->map(xrt_core::buffer_handle::map_type::write);
    }
    catch (const std::exception&) {
      hbuf = nullptr;
    }
  }

  // buffer_import() - Import the buffer from another process
  //
  // @device:  device to import to
  // @pid:     process id of exporting process
  // @ehdl:    export handle obtained from exporting process
  //
  // This consrructor works on linux only and require pidfd support in
  // linux kernel.
  buffer_import(const device_type& dev, pid_type pid, export_handle ehdl)
    : bo_impl(dev, pid, ehdl)
  {
    try {
      hbuf = handle->map(xrt_core::buffer_handle::map_type::write);
    }
    catch (const std::exception&) {
      hbuf = nullptr;
    }
  }

  ~buffer_import() override
  {
    // Imported BO can fail in xclUnmapBO if the exported BO has
    // already been unmapped or vice versa.
    try {
      handle->unmap(hbuf);
    }
    catch (...) {
    }
  }

  buffer_import(const buffer_import&) = delete;
  buffer_import(buffer_import&&) = delete;
  buffer_import& operator=(buffer_import&) = delete;
  buffer_import& operator=(buffer_import&&) = delete;

  bool
  is_imported() const override
  {
    return true;
  }

  void*
  get_hbuf() const override
  {
    if (!hbuf)
      throw xrt_core::system_error(std::errc::bad_address, "No host memory for imported buffer");
    return hbuf;
  }
};

// class buffer_dbuf - device only buffer
//
class buffer_dbuf : public bo_impl
{
public:
  buffer_dbuf(const device_type& dev, std::unique_ptr<xrt_core::buffer_handle> bhdl, size_t sz)
    : bo_impl(dev, std::move(bhdl), sz)
  {}

  buffer_dbuf(const device_type& dev, std::shared_ptr<xrt_core::buffer_handle> bhdl, size_t sz)
    : bo_impl(dev, std::move(bhdl), sz)
  {}

  void*
  get_hbuf() const override
  {
    throw xrt_core::error(-EINVAL, "device only buffer has no host buffer");
  }

  void
  read(void* dst, size_t sz, size_t skip) override
  {
    if (sz + skip > size)
      throw xrt_core::error(-EINVAL,"attempting to read past buffer size");
    device->unmgd_pread(dst, sz, get_address() + skip);
  }

  void
  write(const void* src, size_t sz, size_t seek) override
  {
    if (sz + seek > size)
      throw xrt_core::error(-EINVAL,"attempting to write past buffer size");
    device->unmgd_pwrite(src, sz, get_address() + seek);
  }
};

class buffer_nodma : public bo_impl
{
  buffer_kbuf m_host_only;
  buffer_dbuf m_device_only;

  void
  valid_or_error(size_t sz, size_t offset)
  {
    if (!sz)
      throw xrt_core::system_error(EINVAL, "size must be a positive number");
    if (sz + offset > size)
      throw xrt_core::system_error(EINVAL, "offset exceeds buffer size");
  }

public:
  buffer_nodma(const device_type& dev, std::unique_ptr<xrt_core::buffer_handle> hbuf, std::unique_ptr<xrt_core::buffer_handle> dbuf, size_t sz)
    : bo_impl(dev, std::move(dbuf), sz)
    , m_host_only(dev, std::move(hbuf), sz)
    , m_device_only(dev, handle, sz) // share handle of this bo_impl
  {}

  void*
  get_hbuf() const override
  {
    return m_host_only.get_hbuf();
  }

  // sync is M2M copy between host and device bo
  // nodma is guaranteed to have M2M
  void
  sync(xclBOSyncDirection dir, size_t sz, size_t offset) override
  {
    if (dir == XCL_BO_SYNC_BO_TO_DEVICE) {
      // dst, src, size, dst_offset, src_offset
      auto hdl = m_device_only.get_handle();
      hdl->copy(m_host_only.get_handle(), size, offset, offset);
      //device->copy_bo(m_device_only.get_xcl_handle(), m_host_only.get_xcl_handle(), sz, offset, offset);
    }
    else {
      // dst, src, size, dst_offset, src_offset
      auto hdl = m_host_only.get_handle();
      hdl->copy(m_device_only.get_handle(), sz, offset, offset);
    }
  }

  void
  copy(const bo_impl* src, size_t sz, size_t src_offset, size_t dst_offset) override
  {
    // Copy src device bo to dst (this) device bo
    bo_impl::copy(src, sz, src_offset, dst_offset);

    // Copy dst (this) dbuf to dst (this) hbuf
    auto hdl = m_host_only.get_handle();
    hdl->copy(m_device_only.get_handle(), sz, dst_offset, dst_offset);
  }
};

// class buffer_sub - Sub buffer
//
// Sub-buffer created from parent buffer
class buffer_sub : public bo_impl
{
  std::shared_ptr<bo_impl> m_parent;  // participate in ownership of parent
  size_t m_offset;
  void* m_hbuf;

public:
  buffer_sub(std::shared_ptr<bo_impl> par, size_t size, size_t off)
    : bo_impl(par.get(), size)
    , m_parent(std::move(par))
    , m_offset(off)
    , m_hbuf(static_cast<char*>(m_parent->get_hbuf()) + m_offset)
  {
    if (size + m_offset > m_parent->get_size())
      throw xrt_core::error(-EINVAL, "sub buffer size and offset");
  }

  void*
  get_hbuf() const override
  {
    return m_hbuf;
  }

  bool
  is_sub() const override
  {
    return true;
  }

  size_t
  get_offset() const override
  {
    return m_offset;
  }

  uint64_t
  get_address() const override
  {
    return bo_impl::get_address() + m_offset;
  }

  void
  sync(xclBOSyncDirection dir, size_t sz, size_t offset) override
  {
    size_t off = offset + m_offset;
    if (off + sz > m_parent->get_size())
      throw xrt_core::error(-EINVAL, "Invalid offset and size when syncing sub buffer");

    // sync through parent buffer, which handles nodma case also
    m_parent->sync(dir, sz, off);
  }
};

// class buffer_xbuf - Wrapper for extern managed xclBufferHandle
//
// This class is added to support xrt::bo object for host
// managed xclBufferHandles.  This allows the xclBufferHandle
// to be used as argument for kernel execution.  All other
// operations must be managed explicity by host via xcl APIs.
class buffer_xbuf : public bo_impl
{
public:
  buffer_xbuf(const device_type& dev, xcl_buffer_handle xhdl)
    : bo_impl(dev, xhdl)
  {
    throw xrt_core::error(std::errc::not_supported, "xcl type objects are no longer supported");
  }

  void*
  get_hbuf() const override
  {
    throw xrt_core::error(std::errc::not_supported, "no host buffer access for xcl managed BOs");
  }

  void
  copy(const bo_impl*, size_t, size_t, size_t) override
  {
    throw xrt_core::error(std::errc::not_supported, "no copy of xcl managed BOs");
  }

  void
  sync(xclBOSyncDirection, size_t, size_t) override
  {
    throw xrt_core::error(std::errc::not_supported, "no sync of xcl managed BOs");
  }

  bool
  is_sub() const override
  {
    throw xrt_core::error(std::errc::not_supported, "no sub buffer property for xcl managed BOs");
  }

  bool
  is_imported() const override
  {
    throw xrt_core::error(std::errc::not_supported, "no import property for xcl managed BOs");
  }
};

// class buffer_clone - cloned buffer in different memory bank
//
// A cloned buffer is identical to src buffer except for its physical
// device location (memory group). The clone is valid only as long as
// the src buffer is valid, lifetime of clone is tied to lifetime of
// src per alloc_clone() implementation.
class buffer_clone : public bo_impl
{
public:
  buffer_clone(const device_type& dev, const std::shared_ptr<bo_impl>& src, std::unique_ptr<xrt_core::buffer_handle> clone, size_t sz)
    : bo_impl(dev, std::move(clone), sz)
  {
    // copy src to clone
    copy(src.get(), src->get_size(), 0, 0);
  }
};

} // namespace xrt

// Implementation details
namespace {

// C-API handles that must be explicitly closed. Corresponding managed
// handles are inserted in this map.  When the unmanaged handle is
// closed, it is removed from this map and underlying buffer is
// deleted if no other shared ptrs exists for this buffer
static xrt_core::handle_map<xrtBufferHandle, std::shared_ptr<xrt::bo_impl>> bo_cache;

static const std::shared_ptr<xrt::bo_impl>&
get_boh(xrtBufferHandle bhdl)
{
  return bo_cache.get_or_error(bhdl);
}

static std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(const device_type& device, void* userptr, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  // Embed grp in flags
  xcl_bo_flags xflags{flags};
  xcl_bo_flags xgrp{grp};
  xflags.bank = xgrp.bank;
  xflags.slot = xgrp.slot;

  auto hwctx  = device.get_hwctx_handle();
  return hwctx
    ? hwctx->alloc_bo(userptr, sz, xflags.all)
    : device->alloc_bo(userptr, sz, xflags.all);
}

static std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(const device_type& device, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  // Embed grp in flags
  xcl_bo_flags xflags{flags};
  xcl_bo_flags xgrp{grp};
  xflags.bank = xgrp.bank;
  xflags.slot = xgrp.slot;

  try {
    auto hwctx  = device.get_hwctx_handle();
    return hwctx
      ? hwctx->alloc_bo(sz, xflags.all)
      : device->alloc_bo(sz, xflags.all);
  }
  catch (const std::exception& ex) {
    if (flags == XRT_BO_FLAGS_HOST_ONLY) {
      auto fmt = boost::format("Failed to allocate host memory buffer (%s), make sure host bank is enabled "
                               "(see xrt-smi configure --host-mem)") % ex.what();
      send_exception_message(fmt.str());
    }
    throw;
  }
}

// driver allocates host buffer
static std::shared_ptr<xrt::bo_impl>
alloc_kbuf(const device_type& device, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_kbuf);
  auto handle = alloc_bo(device, sz, flags, grp);
  auto boh = std::make_shared<xrt::buffer_kbuf>(device, std::move(handle), sz);
  boh->get_usage_logger()->log_buffer_info_construct(device->get_device_id(), sz, device.get_hwctx_handle());
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_ubuf(const device_type& device, void* userptr, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_ubuf);
  // On NoDMA platforms a userptr would require userspace management
  // of specified userptr with extra memcpy on sync and copy.  If
  // supported then it would hide inefficient application code, so
  // just say no.
  if (is_nodma(device.get_core_device()))
    throw xrt_core::error(EINVAL, "userptr is not supported for NoDMA platforms");

  // error if userptr is not aligned properly
  if (!is_aligned_ptr(userptr))
    throw xrt_core::error(EINVAL, "userptr is not aligned");

  // driver pins and manages userptr
  auto handle = alloc_bo(device, userptr, sz, flags, grp);
  auto boh = std::make_shared<xrt::buffer_ubuf>(device, std::move(handle), sz, userptr);
  boh->get_usage_logger()->log_buffer_info_construct(device->get_device_id(), sz, device.get_hwctx_handle());
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_hbuf(const device_type& device, xrt_core::aligned_ptr_type&& hbuf, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_hbuf);
  auto handle =  alloc_bo(device, hbuf.get(), sz, flags, grp);
  auto boh = std::make_shared<xrt::buffer_hbuf>(device, std::move(handle), sz, std::move(hbuf));
  boh->get_usage_logger()->log_buffer_info_construct(device->get_device_id(), sz, device.get_hwctx_handle());
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_dbuf(const device_type& device, size_t sz, xrtBufferFlags, xrtMemoryGroup grp)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_dbuf);
  auto handle = alloc_bo(device, sz, XCL_BO_FLAGS_DEV_ONLY, grp);
  auto boh = std::make_shared<xrt::buffer_dbuf>(device, std::move(handle), sz);
  boh->get_usage_logger()->log_buffer_info_construct(device->get_device_id(), sz, device.get_hwctx_handle());
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_nodma(const device_type& device, size_t sz, xrtBufferFlags, xrtMemoryGroup grp)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_nodma);
  constexpr size_t align = 64;
  if (sz % align)
    throw xrt_core::error(EINVAL, "Invalid buffer size '" + std::to_string(sz) +
                          "', must be multiple of 64 bytes for NoDMA platforms");

  auto hbuf_handle = alloc_bo(device, sz, XCL_BO_FLAGS_HOST_ONLY, grp);
  auto dbuf_handle = alloc_bo(device, sz, XCL_BO_FLAGS_DEV_ONLY, grp);
  auto boh = std::make_shared<xrt::buffer_nodma>(device, std::move(hbuf_handle), std::move(dbuf_handle), sz);
  boh->get_usage_logger()->log_buffer_info_construct(device->get_device_id(), sz, device.get_hwctx_handle());
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc(const device_type& device, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  xcl_bo_flags xflags{flags};
  auto type = xflags.flags & ~XRT_BO_FLAGS_MEMIDX_MASK;
  switch (type) {
  case 0:
#ifndef XRT_EDGE
    if (is_nodma(device.get_core_device()))
      return alloc_nodma(device, sz, flags, grp);
    else if (is_sw_emulation()) // NOLINT hicpp-braces-around-statements
      // In DC scenario, for sw_emu, use the xclAllocBO and xclMapBO instead of xclAllocUserPtrBO,
      // which helps to remove the extra copy in sw_emu.
      return alloc_kbuf(device, sz, flags, grp);
    else  // NOLINT hicpp-braces-around-statements
      return alloc_hbuf(device, xrt_core::aligned_alloc(get_alignment(), sz), sz, flags, grp);
#endif
  case XCL_BO_FLAGS_CACHEABLE:
  case XCL_BO_FLAGS_SVM:
  case XCL_BO_FLAGS_HOST_ONLY:
  case XCL_BO_FLAGS_P2P:
  case XCL_BO_FLAGS_EXECBUF:
    return alloc_kbuf(device, sz, flags, grp);
  case XCL_BO_FLAGS_DEV_ONLY:
    return alloc_dbuf(device, sz, flags, grp);
  default:
    throw std::runtime_error("Unknown buffer type");
  }
}

static std::shared_ptr<xrt::bo_impl>
alloc_xbuf(const device_type& device, xcl_buffer_handle xhdl)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_xbuf);
  return std::make_shared<xrt::buffer_xbuf>(device, xhdl);
}

static std::shared_ptr<xrt::bo_impl>
alloc_userptr(const device_type& device, void* userptr, size_t sz, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_userptr);
  return alloc_ubuf(device, userptr, sz, flags, grp);
}

static std::shared_ptr<xrt::bo_impl>
alloc_import(const device_type& device, xrt::bo_impl::export_handle ehdl)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_import);
  auto boh = std::make_shared<xrt::buffer_import>(device, ehdl);
  boh->get_usage_logger()->log_buffer_info_construct(device->get_device_id(), boh->get_size(), device.get_hwctx_handle());
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_import_from_pid(const device_type& device, xrt::pid_type pid, xrt::bo_impl::export_handle ehdl)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_import_from_pid);
  auto boh = std::make_shared<xrt::buffer_import>(device, pid, ehdl);
  boh->get_usage_logger()->log_buffer_info_construct(device->get_device_id(),
                                                     boh->get_size(),
                                                     device.get_hwctx_handle());
  return boh;
}

static std::shared_ptr<xrt::bo_impl>
alloc_sub(const std::shared_ptr<xrt::bo_impl>& parent, size_t size, size_t offset)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_sub);
  auto boh = std::make_shared<xrt::buffer_sub>(parent, size, offset);
  boh->get_usage_logger()->log_buffer_info_construct(boh->get_core_device()->get_device_id(),
                                                     boh->get_size(),
                                                     boh->get_hwctx_handle());
  return boh;
}

// alloc_clone() - Create a clone of src BO in specified memory bank
static std::shared_ptr<xrt::bo_impl>
alloc_clone(const std::shared_ptr<xrt::bo_impl>& src, xrt::memory_group grp)
{
  XRT_TRACE_POINT_SCOPE(xrt_bo_alloc_clone);
  // Same device and flags as src bo
  auto device = src->get_device();
  auto xflags = static_cast<xrtBufferFlags>(src->get_flags());

  auto clone_handle = alloc_bo(device, src->get_size(), xflags, grp);
  auto clone = std::make_shared<xrt::buffer_clone>(device, src, std::move(clone_handle), src->get_size());

  // the clone implmentation lifetime is tied to src
  src->add_clone(clone);
  clone->get_usage_logger()->log_buffer_info_construct(device->get_device_id(),
                                                       clone->get_size(),
                                                       clone->get_hwctx_handle());
  return clone;
}

static device_type
xcl_to_core_device(xclDeviceHandle xhdl)
{
  return {xrt_core::get_userpf_device(xhdl)};
}


static device_type
xrt_to_core_device(xrtDeviceHandle dhdl)
{
  return {xrt_core::device_int::get_core_device(dhdl)};
}

// When no flags are specified, automatically infer host_only for
// NoDMA platforms when memory bank is host memory only.
static xrtBufferFlags
adjust_buffer_flags(const xrt::device& device, xrt::bo::flags flags, xrt::memory_group grp)
{
  if (flags != xrt::bo::flags::normal)
    return static_cast<xrtBufferFlags>(flags);

  if (!is_nodma(device))
    return static_cast<xrtBufferFlags>(flags);

  if (device.get_handle()->get_memory_type(grp) == xrt_core::device::memory_type::host)
    return static_cast<xrtBufferFlags>(xrt::bo::flags::host_only);

  return static_cast<xrtBufferFlags>(flags);
}

// When no flags are specified, automatically infer host_only for
// NoDMA platforms when memory bank is host memory only.
// Optimized short cut to avoid converting xclDeviceHandle to core device
static xrtBufferFlags
adjust_buffer_flags(const device_type& dev, xrt::bo::flags flags, xrt::memory_group grp)
{
  if (flags == xrt::bo::flags::normal)
    return adjust_buffer_flags(xrt::device{dev.get_device()}, flags, grp);
  return static_cast<xrtBufferFlags>(flags);
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_bo implementation of extension APIs not exposed to end-user
////////////////////////////////////////////////////////////////
namespace xrt_core::bo {

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

uint32_t
group_id(const xrt::bo& bo)
{
  return bo.get_handle()->get_group_id();
}

xclDeviceHandle
device_handle(const xrt::bo& bo)
{
    return bo.get_handle()->get_device()->get_device_handle();
}

xrt::bo::flags
get_flags(const xrt::bo& bo)
{
    return bo.get_handle()->get_flags();
}

xrt::bo
clone(const xrt::bo& src, xrt::memory_group target_grp)
{
  return alloc_clone(src.get_handle(), target_grp);
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

} // xrt_core::bo


////////////////////////////////////////////////////////////////
// xrt_bo C++ API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////
namespace xrt {

void
bo::async_handle::
wait()
{
  handle->wait();
}

bo::
bo(const xrt::device& device, void* userptr, size_t sz, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_userptr, device_type{device.get_handle()}, userptr, sz
    , adjust_buffer_flags(device_type{device.get_handle()}, flags, grp), grp))
{}

bo::
bo(const xrt::device& device, void* userptr, size_t sz, memory_group grp)
  : bo(device, userptr, sz, bo::flags::normal, grp)
{}

bo::
bo(const xrt::device& device, size_t sz, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc, device_type{device.get_handle()}, sz
    , adjust_buffer_flags(device_type{device.get_handle()}, flags, grp), grp))
{}

bo::
bo(const xrt::device& device, size_t sz, memory_group grp)
  : bo(device, sz, bo::flags::normal, grp)
{}

bo::
bo(const xrt::device& device, xrt::bo::export_handle ehdl)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_import, device_type{device.get_handle()}, ehdl))
{}

bo::
bo(const xrt::device& device, pid_type pid, xrt::bo::export_handle ehdl)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_import_from_pid, device_type{device.get_handle()}, pid , ehdl))
{}

bo::
bo(const xrt::hw_context& hwctx, void* userptr, size_t sz, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_userptr, device_type{hwctx}, userptr, sz
    , adjust_buffer_flags(device_type{hwctx}, flags, grp), grp))
{}

bo::
bo(const xrt::hw_context& hwctx, void* userptr, size_t sz, memory_group grp)
  : bo(hwctx, userptr, sz, bo::flags::normal, grp)
{}

bo::
bo(const xrt::hw_context& hwctx, size_t sz, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc, device_type{hwctx}, sz
    , adjust_buffer_flags(device_type{hwctx}, flags, grp), grp))
{}

bo::
bo(const xrt::hw_context& hwctx, size_t sz, memory_group grp)
  : bo(hwctx, sz, bo::flags::normal, grp)
{}

// Deprecated
bo::
bo(xclDeviceHandle dhdl, void* userptr, size_t sz, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_userptr, xcl_to_core_device(dhdl), userptr, sz
    , adjust_buffer_flags(xcl_to_core_device(dhdl), flags, grp), grp))
{}

// Deprecated
bo::
bo(xclDeviceHandle dhdl, size_t size, bo::flags flags, memory_group grp)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc, xcl_to_core_device(dhdl), size
    , adjust_buffer_flags(xcl_to_core_device(dhdl), flags, grp), grp))
{}

// Deprecated
bo::
bo(xclDeviceHandle dhdl, xrt::bo_impl::export_handle ehdl)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_import, xcl_to_core_device(dhdl), ehdl))
{}

// Deprecated
bo::
bo(xclDeviceHandle dhdl, pid_type pid, xrt::bo_impl::export_handle ehdl)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_import_from_pid, xcl_to_core_device(dhdl), pid , ehdl))
{}

bo::
bo(const bo& parent, size_t size, size_t offset)
  : handle(xdp::native::profiling_wrapper("xrt::bo::bo",
      alloc_sub, parent.handle, size, offset))
{}

bo::
bo(xclDeviceHandle dhdl, xcl_buffer_handle xhdl)
  : handle(alloc_xbuf(xcl_to_core_device(dhdl), xhdl))
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
    return handle ? handle->get_size() : 0;
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


memory_group
bo::
get_memory_group() const
{
  return xdp::native::profiling_wrapper("xrt::bo::memory_group", [this]{
    return handle->get_group_id();
  });
}

bo::flags
bo::
get_flags() const
{
  return xdp::native::profiling_wrapper("xrt::bo::get_flags", [this]{
    return handle->get_flags();
  });
}

bo::export_handle
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
  return xdp::native::profiling_wrapper_sync("xrt::bo::sync", dir, size,
    [this, dir, size, offset]{
      handle->sync(dir, size, offset);
    });
}

bo::async_handle
bo::
async(xclBOSyncDirection dir, size_t sz, size_t offset)
{
  return handle->async(*this, dir, sz, offset);
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

bo::
~bo() = default;

} // xrt

////////////////////////////////////////////////////////////////
// xrt_ext::bo C++ API implmentations (xrt_ext.h)
////////////////////////////////////////////////////////////////
namespace xrt::ext {

static xrt::ext::bo::access_mode
operator~(xrt::ext::bo::access_mode am)
{
  return xrt::detail::operator~(am);
}

static uint32_t
mode_to_access(xrt::ext::bo::access_mode am)
{
  switch (am & ~(xrt::ext::bo::access_mode::read_write)) {
  case xrt::ext::bo::access_mode::local:
    return XRT_BO_ACCESS_LOCAL;
  case xrt::ext::bo::access_mode::shared:
    return XRT_BO_ACCESS_SHARED;
  case xrt::ext::bo::access_mode::process:
    return XRT_BO_ACCESS_PROCESS;
  case xrt::ext::bo::access_mode::hybrid:
    return XRT_BO_ACCESS_HYBRID;
  default:
    throw xrt_core::error("xrt::ext::bo: invalid access mode");
  }
}

static uint32_t
mode_to_dir(xrt::ext::bo::access_mode am)
{
  switch (am & (xrt::ext::bo::access_mode::read_write)) {
  case xrt::ext::bo::access_mode::none:
  case xrt::ext::bo::access_mode::read_write:
    return XRT_BO_ACCESS_READ_WRITE;
  case xrt::ext::bo::access_mode::read:
    return XRT_BO_ACCESS_READ;
  case xrt::ext::bo::access_mode::write:
    return XRT_BO_ACCESS_WRITE;
  default:
    throw xrt_core::error("xrt::ext::bo: invalid access mode");
  }
}
  
static xrtBufferFlags
adjust_buffer_flags(xrt::ext::bo::access_mode access)
{
  // xrt::ext::bo is always a host only BO
  // instruction buffers are allocated as regular xrt::bo objects
  // or to-be new first-class instruction buffer
  xcl_bo_flags flags {0};
  flags.flags = XRT_BO_FLAGS_HOST_ONLY;
  flags.access = mode_to_access(access);
  flags.dir = mode_to_dir(access);
  return flags.all;
}

static std::shared_ptr<xrt::bo_impl>
alloc_kbuf(const device_type& device, void* userptr, size_t sz, xrtBufferFlags flags)
{
  auto handle = userptr ? alloc_bo(device, userptr, sz, flags, 0) : alloc_bo(device, sz, flags, 0);
  auto boh = std::make_shared<xrt::buffer_kbuf>(device, std::move(handle), sz);
  return boh;
}

bo::
bo(const xrt::device& device, void* userptr, size_t sz, access_mode access)
  : xrt::bo::bo{alloc_kbuf(device_type{device.get_handle()}, userptr, sz, adjust_buffer_flags(access))}
{}

bo::
bo(const xrt::device& device, void* userptr, size_t sz)
  : bo{device, userptr, sz, xrt::ext::bo::access_mode::local}
{}

bo::
bo(const xrt::device& device, pid_type pid, xrt::bo::export_handle ehdl)
  : xrt::bo::bo{alloc_import_from_pid(device_type{device.get_handle()}, pid, ehdl)}
{}

bo::
bo(const xrt::device& device, size_t sz, access_mode access)
  : bo{device, nullptr, sz, access}
{}

bo::
bo(const xrt::device& device, size_t sz)
  : bo{device, sz, xrt::ext::bo::access_mode::local}
{}

bo::
bo(const xrt::hw_context& hwctx, size_t sz, access_mode access)
  : xrt::bo::bo{alloc_kbuf(device_type{hwctx}, nullptr, sz, adjust_buffer_flags(access))}
{}

bo::
bo(const xrt::hw_context& hwctx, size_t sz)
  : bo{hwctx, sz, xrt::ext::bo::access_mode::local}
{}

bo::
bo(const xrt::hw_context& hwctx, pid_type pid, xrt::bo::export_handle ehdl)
  : xrt::bo::bo{alloc_import_from_pid(device_type{hwctx}, pid, ehdl)}
{}

} // xrt::ext

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal BO APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::bo_int {

xrt_core::buffer_handle*
get_buffer_handle(const xrt::bo& bo)
{
  auto handle = bo.get_handle();
  return handle->get_handle();
}

size_t
get_offset(const xrt::bo& bo)
{
  auto handle = bo.get_handle();
  return handle->get_offset();
}

static xrt::bo
create_bo_helper(const xrt::hw_context& hwctx, size_t sz, uint32_t use_flag)
{
  xcl_bo_flags flags {0};  // see xrt_mem.h
  flags.flags = XRT_BO_FLAGS_CACHEABLE;
  flags.access = XRT_BO_ACCESS_LOCAL;
  flags.dir = XRT_BO_ACCESS_READ_WRITE;
  flags.use = use_flag;

  // While the memory group should be ignored (inferred) for
  // debug / trace buffers, it is still passed in as a default
  // group 1 with no implied correlation to xclbin connectivity
  // or memory group.
  return xrt::bo{alloc(device_type{hwctx}, sz, flags.all, 1)};
}

xrt::bo
create_debug_bo(const xrt::hw_context& hwctx, size_t sz)
{
  return create_bo_helper(hwctx, sz, XRT_BO_USE_DEBUG);
}

xrt::bo
create_dtrace_bo(const xrt::hw_context& hwctx, size_t sz)
{
  return create_bo_helper(hwctx, sz, XRT_BO_USE_DTRACE);
}

} // xrt_core::bo_int

////////////////////////////////////////////////////////////////
// xrt_aie_bo C++ API implmentations (xrt_aie.h)
////////////////////////////////////////////////////////////////
namespace xrt::aie {

xrt::bo::async_handle
bo::
async(const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset)
{
  return get_handle()->async(*this, port, dir, sz, offset);
}

void
bo::
sync(const std::string& port, xclBOSyncDirection dir, size_t sz, size_t offset)
{
  get_handle()->sync(*this, port, dir, sz, offset);
}

} // namespace xrt::aie

////////////////////////////////////////////////////////////////
// xrt_bo API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////
xrtBufferHandle
xrtBOAllocUserPtr(xrtDeviceHandle dhdl, void* userptr, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, userptr, size, flags, grp]{
      auto boh = alloc_userptr(xrt_to_core_device(dhdl), userptr, size, flags, grp);
      auto hdl = boh.get();
      bo_cache.add(hdl, std::move(boh));
      return hdl;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
      auto boh = alloc(xrt_to_core_device(dhdl), size, flags, grp);
      auto hdl = boh.get();
      bo_cache.add(hdl, std::move(boh));
      return hdl;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
      auto boh = alloc_sub(parent, sz, offset);
      auto hdl = boh.get();
      bo_cache.add(hdl, std::move(boh));
      return hdl;

    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
      auto boh = alloc_import(xrt_to_core_device(dhdl), ehdl);
      auto hdl = boh.get();
      bo_cache.add(hdl, std::move(boh));
      return hdl;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return static_cast<xclBufferExportHandle>(XRT_NULL_BO_EXPORT);
}

xrtBufferHandle
xrtBOAllocFromXcl(xrtDeviceHandle dhdl, xclBufferHandle xhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, xhdl] {
      auto boh = alloc_xbuf(xrt_to_core_device(dhdl), xcl_buffer_handle{xhdl});
      auto hdl = boh.get();
      bo_cache.add(hdl, std::move(boh));
      return hdl;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

int
xrtBOFree(xrtBufferHandle bhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [bhdl]{
      bo_cache.remove_or_error(bhdl);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return std::numeric_limits<size_t>::max();
}


int
xrtBOSync(xrtBufferHandle bhdl, xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    return xdp::native::profiling_wrapper_sync(__func__, dir, size,
    [bhdl, dir, size, offset]{
      get_boh(bhdl)->sync(dir, size, offset);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
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
    errno = ex.get_code();
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
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
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return std::numeric_limits<uint64_t>::max();
}
