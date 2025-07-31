// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in smae dll as coreutil
#include "context_mgr.h"
#include "hw_context_int.h"
#include "core/common/cuidx_type.h"
#include "core/common/device.h"
#include "core/common/shim/hwctx_handle.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>

using namespace std::chrono_literals;

namespace xrt_core::context_mgr {

// class device_context_mgr - synchroize open and close context for IPs
//
// If multiple threads share the same device object and acquire /
// release context on the same CUs, then careful synchronization of
// low level xclOpen/CloseContext is required.
//
// The synchronization ensures that when a thread is in the process of
// releasing a context, another thread wont call xclOpenContext before
// the former has closed its context.
class device_context_mgr : public xrt_core::device::context_mgr
{
  // CU indeces are managed per hwctx
  // This struct manages CUs are that are opened by the
  // context mananger.  It supports mapping
  // - {ctx, nm} -> ip_info   // for opening
  // - {ctx, idx} -> ip_info  // for closing
  // where the ip_info data is shared by both maps
  struct ctx
  {
    struct ip {
      std::string ipname;
      cuidx_type ipidx;

      ip(std::string nm, cuidx_type idx)
        : ipname(std::move(nm)), ipidx(idx)
      {}
    };

    std::map<std::string, std::shared_ptr<ip>> m_nm2ip;
    std::map<decltype(cuidx_type::index), std::shared_ptr<ip>> m_idx2ip;

    const ip*
    get(const std::string& ipname)
    {
      return m_nm2ip[ipname].get();
    }

    const ip*
    get(cuidx_type ipidx)
    {
      return m_idx2ip[ipidx.index].get();
    }

    void
    add(const std::string& ipname, cuidx_type ipidx)
    {
      m_idx2ip[ipidx.index] = m_nm2ip[ipname] = std::make_shared<ip>(ipname, ipidx);
    }

    void
    erase(cuidx_type ipidx)
    {
      const auto& cu = m_idx2ip.at(ipidx.index);
      m_nm2ip.erase(cu->ipname);
      m_idx2ip.erase(cu->ipidx.index);  // cu local variable dies
    }
  };

  std::mutex m_mutex;
  std::map<const hwctx_handle*, ctx> m_ctx;
  std::condition_variable m_cv;

public:
  // Open context on IP in specified hardware context.
  // Open the IP context when it is safe to do so.  Note, that usage
  // of the context manager does not support multiple threads calling
  // this open() function on the same ip. The intended use-case
  // (xrt::kernel) prevents this situation.
  cuidx_type
  open(const xrt::hw_context& hwctx, const std::string& ipname)
  {
    std::unique_lock<std::mutex> ul(m_mutex);
    auto hwctx_hdl = static_cast<hwctx_handle*>(hwctx);
    auto& ctx = m_ctx[hwctx_hdl];
    while (ctx.get(ipname)) {
      if (m_cv.wait_for(ul, 100ms) == std::cv_status::timeout) // NOLINT
        throw std::runtime_error("aquiring cu context timed out");
    }

    auto ipidx = hwctx_hdl->open_cu_context(ipname);
    ctx.add(ipname, ipidx);
    return ipidx;
  }

  // Close the cu context and notify threads that might be waiting
  // to open this cu
  void
  close(const xrt::hw_context& hwctx, cuidx_type ipidx)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto hwctx_hdl = static_cast<hwctx_handle*>(hwctx);
    auto& ctx = m_ctx[hwctx_hdl];
    if (!ctx.get(ipidx))
      throw std::runtime_error("ctx " + std::to_string(ipidx.index) + " not open");

    hwctx_hdl->close_cu_context(ipidx);
    ctx.erase(ipidx);
    m_cv.notify_all();
  }
};

////////////////////////////////////////////////////////////////
// Exposed API
////////////////////////////////////////////////////////////////

static std::shared_ptr<device_context_mgr>
get_device_context_mgr(xrt_core::device* device)
{
  if (!device)
    throw std::runtime_error("Empty device, failed to get device context manager.");

  return std::static_pointer_cast<device_context_mgr>(device->get_context_mgr());
}

std::shared_ptr<device_context_mgr>
create(xrt_core::device* device)
{
  return get_device_context_mgr(device);
}

// Regular CU
cuidx_type
open_context(const xrt::hw_context& hwctx, const std::string& cuname)
{
  auto device = xrt_core::hw_context_int::get_core_device_raw(hwctx);
  auto ctxmgr = get_device_context_mgr(device);
  return ctxmgr->open(hwctx, cuname);
}

void
close_context(const xrt::hw_context& hwctx, cuidx_type cuidx)
{
  auto device = xrt_core::hw_context_int::get_core_device_raw(hwctx);
  auto ctxmgr = get_device_context_mgr(device);
  ctxmgr->close(hwctx, cuidx);
}

} // xrt_core::context_mgr

////////////////////////////////////////////////////////////////
// xrt_core::device context manager APIs definition
////////////////////////////////////////////////////////////////
std::shared_ptr<xrt_core::device::context_mgr>
xrt_core::device::
get_context_mgr()
{
   std::lock_guard<std::mutex> lk(m_mutex);
   if (!m_ctx_mgr)
     m_ctx_mgr = std::make_shared<xrt_core::context_mgr::device_context_mgr>();

   return m_ctx_mgr;
}
