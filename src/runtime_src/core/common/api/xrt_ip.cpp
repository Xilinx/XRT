// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx Inc. All rights reserved
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT IP APIs as declared in
// core/include/experimental/xrt_ip.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_ip.h
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in same dll as coreutil
#include "core/include/xrt/experimental/xrt_ip.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include "core/common/api/hw_context_int.h"
#include "core/common/api/ip_int.h"
#include "core/common/api/native_profile.h"

#include "core/common/device.h"
#include "core/common/config_reader.h"
#include "core/common/cuidx_type.h"
#include "core/common/debug.h"
#include "core/common/error.h"

#include <cstdlib>
#include <cstring>
#include <atomic>
#include <memory>
#include <string>

#ifdef _WIN32
# pragma warning( disable : 4244 4996)
#endif

namespace {

constexpr size_t operator"" _kb(unsigned long long v)  { return 1024u * v; } //NOLINT

inline bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
}

inline bool
has_reg_read_write()
{
#ifdef _WIN32
  return false;
#else
  return !is_sw_emulation();
#endif
}

// Determine the QoS value to use when constructing xrt::hw_context in
// legacy constructor.  The default is exclusive context, but if
// xrt.ini:get_rw_shared() is set then access should be shared.
static xrt::hw_context::access_mode
hwctx_access_mode()
{
  return (xrt_core::config::get_rw_shared())
    ? xrt::hw_context::access_mode::shared
    : xrt::hw_context::access_mode::exclusive;
}

} // namespace

namespace xrt {

class ip::interrupt_impl
{
  std::shared_ptr<xrt_core::device> device; // shared ownership
  xclInterruptNotifyHandle handle;
  unsigned int irqidx;

public:
  interrupt_impl(std::shared_ptr<xrt_core::device> dev, unsigned int ipidx)
    : device(std::move(dev)), irqidx(ipidx)
  {
    handle = device->open_ip_interrupt_notify(irqidx);
    enable();
  }

  ~interrupt_impl()
  {
    try {
      device->close_ip_interrupt_notify(handle);
    }
    catch (...) {
    }
  }

  interrupt_impl(const interrupt_impl&) = delete;
  interrupt_impl(interrupt_impl&&) = delete;
  interrupt_impl& operator=(interrupt_impl&) = delete;
  interrupt_impl& operator=(interrupt_impl&&) = delete;

  void
  enable()
  {
    device->enable_ip_interrupt(handle);
  }

  void
  disable()
  {
    device->disable_ip_interrupt(handle);
  }

  void
  wait()
  {
    // Waits for interrupt, upon return interrupt is disabled.
    device->wait_ip_interrupt(handle);
    enable(); // re-enable interrupts
  }

  [[nodiscard]] std::cv_status
  wait(const std::chrono::milliseconds& timeout)
  {
    // Waits for interrupt, or return on timeout
    auto status = device->wait_ip_interrupt(handle, static_cast<int32_t>(timeout.count()));
    if (status == std::cv_status::no_timeout)
      enable(); //re-enable interrupts
    return status;
  }
};

// struct ip_impl - The internals of an xrt::ip
class ip_impl
{
  // struct ip_context - Simple management IP context
  //
  // Constructing an ip object opens a exclusive context on specifed
  // IP.  Upon deletion of the xrt::ip object implementation, the
  // context is closed.
  struct ip_context
  {
    std::shared_ptr<xrt_core::device> m_device;
    xrt::hw_context m_hwctx;
    xrt_core::cuidx_type m_idx; // index of ip per driver, for open context
    xrt::xclbin::ip m_ip;
    uint64_t m_size;            // address range of ip

    std::pair<uint32_t, uint32_t> m_readrange = {0,0};  // start address, size

    ip_context(xrt::hw_context xhwctx, const std::string& nm)
      : m_device(xrt_core::hw_context_int::get_core_device(xhwctx))
      , m_hwctx(std::move(xhwctx))
      , m_idx{0} // intialized in ctor
    {
      auto xclbin = m_hwctx.get_xclbin();

      // nm can be in three forms, but must identify exactly one IP
      // 1. base name (kname) without an embedded ":"
      // 2. curly brace syntax (kname:{inst})
      // 3. fully qualified / canonical ip name (kname:inst)
      if (nm.find(":") == std::string::npos || nm.find(":{") != std::string::npos) {
        // case 1 and 2 use get_ips to do name matching
        auto ips = xclbin.get_ips(nm);

        if (ips.size() > 1)
          throw xrt_core::error(EINVAL, "More than one IP matching '" + nm + "'");

        if (ips.size() == 1)
          m_ip = ips.front();
      }
      else {
        // case 3 use get_ip
        m_ip = xclbin.get_ip(nm);
      }

      if (!m_ip)
        throw xrt_core::error(EINVAL, "No IP matching '" + nm + "'");

      // address range
      m_size = m_ip.get_size();

      // context, driver allows shared context per xrt.ini
      auto hwctx_hdl = static_cast<xrt_core::hwctx_handle*>(m_hwctx);
      m_idx = hwctx_hdl->open_cu_context(m_ip.get_name());
    }

    ~ip_context()
    {
      auto hwctx_hdl = static_cast<xrt_core::hwctx_handle*>(m_hwctx);
      hwctx_hdl->close_cu_context(m_idx);
    }

    ip_context(const ip_context&) = delete;
    ip_context(ip_context&&) = delete;
    ip_context& operator=(ip_context&) = delete;
    ip_context& operator=(ip_context&&) = delete;

    [[nodiscard]] unsigned int
    get_idx() const
    {
      return m_idx.index;
    }

    [[nodiscard]] uint64_t
    get_address() const
    {
      return m_ip.get_base_address();
    }

    [[nodiscard]] uint64_t
    get_size() const
    {
      return m_size;
    }

    void
    set_read_range(uint32_t start, uint32_t size)
    {
      m_device->set_cu_read_range(m_idx, start, size);
      m_readrange = {start, size};
    }
  };

  [[nodiscard]] unsigned int
  get_cuidx_or_error(size_t offset) const
  {
    if ((offset + sizeof(uint32_t)) > m_ipctx.get_size())
        throw std::out_of_range("Cannot read or write outside ip register space");

    return m_ipctx.get_idx();
  }

  static uint32_t
  create_uid()
  {
    static std::atomic<uint32_t> count {0};
    return count++;
  }

private:
  std::shared_ptr<xrt_core::device> m_device;      // shared ownership
  std::weak_ptr<ip::interrupt_impl> m_interrupt;   // interrupt if active
  ip_context m_ipctx;
  uint32_t m_uid;                                  // internal unique id for debug

public:
  // ip_impl - constructor
  //
  // @dev:     device associated with this kernel object
  // @xid:     uuid of xclbin to mine for kernel meta data
  // @nm:      name identifying an ip in IP_LAYOUT of xclbin
  ip_impl(std::shared_ptr<xrt_core::device> dev, const xrt::uuid& xid, const std::string& nm)
    : m_device(std::move(dev))                                   // share ownership
    , m_ipctx(xrt::hw_context{xrt::device{m_device}, xid, hwctx_access_mode()}, nm)
    , m_uid(create_uid())
  {
    XRT_DEBUGF("ip_impl::ip_impl(%d)\n" , m_uid);
  }

  ip_impl(const xrt::hw_context& hwctx, const std::string& nm)
    : m_device(xrt_core::hw_context_int::get_core_device(hwctx)) // share ownership
    , m_ipctx(hwctx, nm)
    , m_uid(create_uid())
  {
    XRT_DEBUGF("ip_impl::ip_impl(%d)\n" , m_uid);
  }

  ~ip_impl()
  {
    XRT_DEBUGF("ip_impl::~ip_impl(%d)\n" , m_uid);
  }

  ip_impl(const ip_impl&) = delete;
  ip_impl(ip_impl&&) = delete;
  ip_impl& operator=(ip_impl&) = delete;
  ip_impl& operator=(ip_impl&&) = delete;

  [[nodiscard]] uint32_t
  read_register(uint32_t offset) const
  {
    auto idx = get_cuidx_or_error(offset);
    uint32_t value = 0;
    if (has_reg_read_write())
      m_device->reg_read(idx, offset, &value);
    else
      m_device->xread(XCL_ADDR_KERNEL_CTRL, m_ipctx.get_address() + offset, &value, 4);
    return value;
  }

  void
  write_register(uint32_t offset, uint32_t data)
  {
    auto idx = get_cuidx_or_error(offset);
    if (has_reg_read_write())
      m_device->reg_write(idx, offset, data);
    else
      m_device->xwrite(XCL_ADDR_KERNEL_CTRL, m_ipctx.get_address() + offset, &data, 4);
  }

  std::shared_ptr<ip::interrupt_impl>
  get_interrupt()
  {
    auto intr = m_interrupt.lock();
    if (!intr)
      // NOLINTNEXTLINE(modernize-make-shared) used in weak_ptr
      m_interrupt = intr = std::shared_ptr<ip::interrupt_impl>(new ip::interrupt_impl(m_device, m_ipctx.get_idx()));

    return intr;
  }

  void
  set_read_range(uint32_t start, uint32_t size)
  {
    m_ipctx.set_read_range(start, size);
  }
  
}; // ip_impl

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal IP APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::ip_int {

void
set_read_range(const xrt::ip& ip, uint32_t start, uint32_t size)
{
  auto handle = ip.get_handle();
  handle->set_read_range(start, size);
}

} // xrt_core::ip_int


////////////////////////////////////////////////////////////////
// xrt_kernel C++ API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
namespace xrt {

ip::
ip(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name)
  : detail::pimpl<ip_impl>(std::make_shared<ip_impl>(device.get_handle(), xclbin_id, name))
{}

ip::
ip(const xrt::hw_context& ctx, const std::string& name)
  : detail::pimpl<ip_impl>(std::make_shared<ip_impl>(ctx, name))
{}

void
ip::
write_register(uint32_t offset, uint32_t data)
{
  xdp::native::profiling_wrapper("xrt::ip::write_register",[this, offset, data]{
    handle->write_register(offset, data);
  }) ;
}

uint32_t
ip::
read_register(uint32_t offset) const
{
  return xdp::native::profiling_wrapper("xrt::ip::read_register", [this, offset] {
    return handle->read_register(offset);
  }) ;
}

xrt::ip::interrupt
ip::
create_interrupt_notify()
{
  return xrt::ip::interrupt{handle->get_interrupt()};
}

////////////////////////////////////////////////////////////////
// xrt::ip::interrupt
////////////////////////////////////////////////////////////////
void
ip::interrupt::
enable()
{
  if (handle)
    handle->enable();
}

void
ip::interrupt::
disable()
{
  if (handle)
    handle->disable();
}

void
ip::interrupt::
wait()
{
  if (handle)
    handle->wait();
}

std::cv_status
ip::interrupt::
wait(const std::chrono::milliseconds& timeout) const
{
  if (handle)
    return handle->wait(timeout);

  return std::cv_status::no_timeout;
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// xrt_kernel API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
