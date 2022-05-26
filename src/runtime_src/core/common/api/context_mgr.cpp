// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in smae dll as coreutil
#include "context_mgr.h"
#include "core/common/cuidx_type.h"
#include "core/common/debug.h"
#include "core/common/device.h"
#include "core/common/api/hw_context_int.h"

#include <bitset>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>

using namespace std::chrono_literals;

namespace {

// Transition only, to be removed
static bool
is_shared(xrt::hw_context::qos qos)
{
  switch (qos) {
  case xrt::hw_context::qos::exclusive:
    return false;
  case xrt::hw_context::qos::shared:
    return true;
  default:
    throw std::runtime_error("unexpected access mode for kernel");
  }
}

} // namespace

namespace xrt_core { namespace context_mgr {

constexpr size_t max_cus = 129;  // +1 for virtual CU
constexpr auto virtual_cu_idx = std::numeric_limits<unsigned int>::max();

// class device_context_mgr - synchroize open and close context for IPs
//
// If multiple threads share the same device object and acquire /
// release context on the same CUs, then careful synchronization of
// low level xclOpen/CloseContext is required.
//
// The synchronization ensures that when a thread is in the process of
// releasing a context, another thread wont call xclOpenContext before
// the former has closed its context.
class device_context_mgr
{
  std::mutex m_mutex;
  std::condition_variable m_cv;
  xrt_core::device* m_device;

  // CU contexts are managed per domain where indicies are
  // in the range [0..max_cus[.  This allows for a compact
  // bitset representation.  CU indices are unique within
  // a domain and global for all hardware contexts.
  using domain_type = cuidx_type::domain_type;
  std::map<domain_type, std::bitset<max_cus>> m_d2ctx; // domain -> cxt

  // Get the domain context bitset that contains this ipidx
  // Return by address since used conditionally in get_idx_ctx
  std::bitset<max_cus>*
  get_ctx(cuidx_type ipidx)
  {
    return &(m_d2ctx[ipidx.domain]);
  }

  // Get the CU index within a domain and the domain context bitset
  // for the CU identified by arguments.
  // The returned index is only valid if the context bitset is not null.
  std::pair<cuidx_type, std::bitset<max_cus>*>
  get_ipidx_ctx(xcl_hwctx_handle ctxhdl, const std::string& cuname)
  {
    try {
      auto ipidx = m_device->get_cuidx(ctxhdl, cuname);
      return {ipidx, get_ctx(ipidx)};
    }
    catch (const std::exception&) {
      return {cuidx_type{0}, nullptr};
    }
  }

  // Convert CU index into a domain index with special attention to
  // the virtual context index which is represented as the last entry
  // in the bitset.
  size_t
  ctxidx(cuidx_type ipidx)
  {
    // translate ipidx to idx used in bitset.
    // virtual cu is last entry in bitset.
    // virtual cu is always in default domain 0.
    return ipidx.index == virtual_cu_idx ? max_cus - 1 : ipidx.domain_index;
  }

public:
  device_context_mgr(xrt_core::device* device)
    : m_device(device)
  {}

  // Open context on IP in specified hardware context.
  // Open the IP context when it is safe to do so.  Note, that usage
  // of the context manager does not support multiple threads calling
  // this open() function on the same ip. The intended use-case
  // (xrt::kernel) prevents this situation.
  cuidx_type
  open(const xrt::hw_context& hwctx, const std::string& ipname)
  {
    std::unique_lock<std::mutex> ul(m_mutex);
    auto ctxhdl = xrt_core::hw_context_int::get_xcl_handle(hwctx);
    auto [ipidx, ctx] = get_ipidx_ctx(ctxhdl, ipname);
    while (ctx && ctx->test(ctxidx(ipidx))) {
      if (m_cv.wait_for(ul, 100ms) == std::cv_status::timeout)
        throw std::runtime_error("aquiring cu context timed out");
    }
    m_device->open_context(ctxhdl, hwctx.get_xclbin_uuid(), ipname, is_shared(hwctx.get_qos()));

    // Successful context creation means CU idx is now known
    if (!ctx)
      std::tie(ipidx, ctx) = get_ipidx_ctx(ctxhdl, ipname);

    if (!ctx)
      throw std::runtime_error("Unexpected ctx error");

    ctx->set(ctxidx(ipidx));

    return ipidx;
  }

  // Close the cu context and notify threads that might be waiting
  // to open this cu
  void
  close(const xrt::hw_context& hwctx, cuidx_type ipidx)
  {
    auto idx = ctxidx(ipidx);
    std::lock_guard<std::mutex> lk(m_mutex);
    auto ctx = get_ctx(ipidx);
    if (!ctx->test(idx))
      throw std::runtime_error("ctx " + std::to_string(ipidx.index) + " not open");
    m_device->close_context(hwctx.get_xclbin_uuid(), ipidx.index);
    ctx->reset(idx);
    m_cv.notify_all();
  }
};

// Get (and create) context manager for device.
// Cache the created manager so other threads can share.
static std::shared_ptr<device_context_mgr>
get_device_context_mgr(xrt_core::device* device, bool create = false)
{
  static std::map<const xrt_core::device*, std::weak_ptr<device_context_mgr>> d2cmgr;
  static std::mutex ctx_mutex;
  std::lock_guard<std::mutex> lk(ctx_mutex);
  auto cmgr = d2cmgr[device].lock();
  if (!cmgr && create)
    d2cmgr[device] = cmgr = std::shared_ptr<device_context_mgr>(new device_context_mgr(device));
  return cmgr;
}


////////////////////////////////////////////////////////////////
// Exposed API
////////////////////////////////////////////////////////////////
std::shared_ptr<device_context_mgr>
create(const xrt_core::device* device)
{
  // creating a context manager doesn't change device, but aqcuiring a
  // context is a device operation and cannot use const device
  return get_device_context_mgr(const_cast<xrt_core::device*>(device), true);
}

// Regular CU
cuidx_type
open_context(const xrt::hw_context& hwctx, const std::string& cuname)
{
  auto device = xrt_core::hw_context_int::get_core_device_raw(hwctx);
  if (auto ctxmgr = get_device_context_mgr(device))
    return ctxmgr->open(hwctx, cuname);

  throw std::runtime_error("No context manager for device");
}

void
close_context(const xrt::hw_context& hwctx, cuidx_type cuidx)
{
  auto device = xrt_core::hw_context_int::get_core_device_raw(hwctx);
  if (auto ctxmgr = get_device_context_mgr(device)) {
    ctxmgr->close(hwctx, cuidx);
    return;
  }

  throw std::runtime_error("No context manager for device");
}

}} // context_mgr, xrt_core
