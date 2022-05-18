/*
 * Copyright (C) 2021-2022, Xilinx Inc - All rights reserved
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

// This file implements XRT IP APIs as declared in
// core/include/experimental/xrt_ip.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_ip.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_ip.h"
#include "core/include/experimental/xrt_xclbin.h"
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

  std::cv_status
  wait(const std::chrono::milliseconds& timeout) const
  {
    // Waits for interrupt, or return on timeout
    return device->wait_ip_interrupt(handle, static_cast<int32_t>(timeout.count()));
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
    std::shared_ptr<xrt_core::device> device;
    xrt::uuid xclbin_uuid;    //
    xrt_core::cuidx_type idx; // index of ip per driver, for open context
    xrt::xclbin::ip ip;
    uint64_t size;            // address range of ip

    ip_context(std::shared_ptr<xrt_core::device> dev, xrt::uuid xid, const std::string& nm)
      : device(std::move(dev))
      , xclbin_uuid(std::move(xid))
    {
      std::string ipnm = nm;
      auto pos1 = ipnm.find(":{");
      if (pos1 != std::string::npos && ipnm.rfind('}') == nm.size() - 1)
        ipnm.erase(pos1 + 1, 1).pop_back();

      auto xclbin = device->get_xclbin(xclbin_uuid);
      ip = xclbin.get_ip(ipnm);

      if (!ip)
        throw xrt_core::error(EINVAL, "No IP matching '" + nm + "'");

      // default to first matching slot
      auto slot = device->get_slots(xclbin_uuid).front();

      // address range
      size = ip.get_size();

      // context, driver allows shared context per xrt.ini
      device->open_context(slot, xclbin_uuid, ipnm, xrt_core::config::get_rw_shared());

      // idx is guaranteed valid only after context creation
      idx = device->get_cuidx(slot, ipnm);
    }

    ~ip_context()
    {
      device->close_context(xclbin_uuid.get(), idx.index);
    }

    ip_context(const ip_context&) = delete;
    ip_context(ip_context&&) = delete;
    ip_context& operator=(ip_context&) = delete;
    ip_context& operator=(ip_context&&) = delete;

    unsigned int
    get_idx() const
    {
      return idx.index;
    }

    uint64_t
    get_address() const
    {
      return ip.get_base_address();
    }

    uint64_t
    get_size() const
    {
      return size;
    }
  };

  unsigned int
  get_cuidx_or_error(size_t offset) const
  {
    if ((offset + sizeof(uint32_t)) > ipctx.get_size())
        throw std::out_of_range("Cannot read or write outside kernel register space");

    return ipctx.get_idx();
  }

  static uint32_t
  create_uid()
  {
    static std::atomic<uint32_t> count {0};
    return count++;
  }

private:
  std::shared_ptr<xrt_core::device> device;      // shared ownership
  std::weak_ptr<ip::interrupt_impl> interrupt;   // interrupt if active
  ip_context ipctx;
  uint32_t uid;                                  // internal unique id for debug

public:
  // ip_impl - constructor
  //
  // @dev:     device associated with this kernel object
  // @xid:     uuid of xclbin to mine for kernel meta data
  // @nm:      name identifying an ip in IP_LAYOUT of xclbin
  ip_impl(std::shared_ptr<xrt_core::device> dev, const xrt::uuid& xid, const std::string& nm)
    : device(std::move(dev))                                   // share ownership
    , ipctx(device, xid, nm)
    , uid(create_uid())
  {
    XRT_DEBUGF("ip_impl::ip_impl(%d)\n" , uid);
  }

  ~ip_impl()
  {
    XRT_DEBUGF("ip_impl::~ip_impl(%d)\n" , uid);
  }

  ip_impl(const ip_impl&) = delete;
  ip_impl(ip_impl&&) = delete;
  ip_impl& operator=(ip_impl&) = delete;
  ip_impl& operator=(ip_impl&&) = delete;

  uint32_t
  read_register(uint32_t offset) const
  {
    auto idx = get_cuidx_or_error(offset);
    uint32_t value = 0;
    if (has_reg_read_write())
      device->reg_read(idx, offset, &value);
    else
      device->xread(XCL_ADDR_KERNEL_CTRL, ipctx.get_address() + offset, &value, 4);
    return value;
  }

  void
  write_register(uint32_t offset, uint32_t data)
  {
    auto idx = get_cuidx_or_error(offset);
    if (has_reg_read_write())
      device->reg_write(idx, offset, data);
    else
      device->xwrite(XCL_ADDR_KERNEL_CTRL, ipctx.get_address() + offset, &data, 4);
  }

  std::shared_ptr<ip::interrupt_impl>
  get_interrupt()
  {
    auto intr = interrupt.lock();
    if (!intr)
      // NOLINTNEXTLINE(modernize-make-shared) used in weak_ptr
      interrupt = intr = std::shared_ptr<ip::interrupt_impl>(new ip::interrupt_impl(device, ipctx.get_idx()));

    return intr;
  }

};

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal IP APIs
////////////////////////////////////////////////////////////////
namespace xrt_core { namespace ip_int {

}} // ip_int, xrt_core


////////////////////////////////////////////////////////////////
// xrt_kernel C++ API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
namespace xrt {

ip::
ip(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name)
  : detail::pimpl<ip_impl>(std::make_shared<ip_impl>(device.get_handle(), xclbin_id, name))
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
