/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 */

#include "context_mgr.h"
#include "core/common/cuidx_type.h"
#include "core/common/debug.h"
#include "core/common/device.h"

#include <bitset>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>

using namespace std::chrono_literals;

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
  // in the range [0..max_cus[.  This sllows for a compact
  // bitset representation.
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
  // for the CU identified by argument slot and cuname.
  // The returned index is only valid if the context bitset is not null.
  std::pair<size_t, std::bitset<max_cus>*>
  get_idx_ctx(slot_id slot, const std::string& cuname)
  {
    try {
      auto idx = m_device->get_cuidx(slot, cuname);
      return {ctxidx(idx), get_ctx(idx)};
    }
    catch (const std::exception&) {
      return {0, nullptr};
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

  // Open context on cu identified by slot, uuid, and name.
  // Open the cu context when it is safe to do so.  Note, that usage
  // of the context manager does not support multiple threads calling
  // this open() function on the same ip. The intended use-case
  // (xrt::kernel) prevents this situation.
  void
  open(slot_id slot, const xrt::uuid& uuid, const std::string& ipname, bool shared)
  {
    std::unique_lock<std::mutex> ul(m_mutex);
    auto [idx, ctx] = get_idx_ctx(slot, ipname);
    while (ctx && ctx->test(idx)) {
      if (m_cv.wait_for(ul, 100ms) == std::cv_status::timeout)
        throw std::runtime_error("aquiring cu context timed out");
    }
    m_device->open_context(slot, uuid.get(), ipname.c_str(), shared);

    // Successful context creation means CU idx is now known
    if (!ctx)
      std::tie(idx, ctx) = get_idx_ctx(slot, ipname);

    if (!ctx)
      throw std::runtime_error("Unexpected ctx error");

    ctx->set(idx);
  }

  // Open the cu context when it is safe to do so.  Note, that usage
  // of the context manager does not support multiple threads calling
  // this open() function on the same ip. The intended use-case
  // (xrt::kernel) prevents this situation.
  void
  open(const xrt::uuid& uuid, cuidx_type ipidx, bool shared)
  {
    auto idx = ctxidx(ipidx);
    std::unique_lock<std::mutex> ul(m_mutex);
    auto ctx = get_ctx(ipidx);
    while (ctx->test(idx)) {
      if (m_cv.wait_for(ul, 100ms) == std::cv_status::timeout)
        throw std::runtime_error("aquiring cu context timed out");
    }
    m_device->open_context(uuid.get(), ipidx.index, shared);
    ctx->set(idx);
  }

  // Close the cu context and notify threads that might be waiting
  // to open this cu
  void
  close(const xrt::uuid& uuid, cuidx_type ipidx)
  {
    auto idx = ctxidx(ipidx);
    std::lock_guard<std::mutex> lk(m_mutex);
    auto ctx = get_ctx(ipidx);
    if (!ctx->test(idx))
      throw std::runtime_error("ctx " + std::to_string(ipidx.index) + " not open");
    m_device->close_context(uuid.get(), ipidx.index);
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
void
open_context(xrt_core::device* device, slot_id slot, const xrt::uuid& uuid, const std::string& cuname, bool shared)
{
  if (auto ctxmgr = get_device_context_mgr(device)) {
    ctxmgr->open(slot, uuid, cuname, shared);
    return;
  }

  throw std::runtime_error("No context manager for device");
}

// Virtual CU
void
open_context(xrt_core::device* device, const xrt::uuid& uuid, cuidx_type cuidx, bool shared)
{
  if (auto ctxmgr = get_device_context_mgr(device)) {
    ctxmgr->open(uuid, cuidx, shared);
    return;
  }

  throw std::runtime_error("No context manager for device");
}

void
close_context(xrt_core::device* device, const xrt::uuid& uuid, cuidx_type cuidx)
{
  if (auto ctxmgr = get_device_context_mgr(device)) {
    ctxmgr->close(uuid, cuidx);
    return;
  }

  throw std::runtime_error("No context manager for device");
}

}} // context_mgr, xrt_core
