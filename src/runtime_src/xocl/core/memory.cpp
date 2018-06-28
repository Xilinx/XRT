/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "memory.h"
#include "device.h"
#include "context.h"
#include "error.h"

#include "xrt/util/memory.h"

#include "xdp/appdebug/appdebug_track.h"

#include <iostream>

namespace {

// Hack to determine if a context is associated with exactly one
// device.  Additionally, in emulation mode, the device must be
// active, e.g. loaded through a call to loadBinary.
//
// This works around a problem where clCreateBuffer is called in
// emulation mode before clCreateProgramWithBinary->loadBinary has
// been called.  The call to loadBinary can end up switching the
// device from swEm to hwEm.
//
// In non emulation mode it is sufficient to check that the context
// has only one device.
static xocl::device*
singleContextDevice(cl_context context)
{
  auto device = xocl::xocl(context)->get_device_if_one();
  if (!device)
    return nullptr;

  return (device->is_active())
    ? device
    : nullptr;
}

} // namespace

namespace xocl {

memory::
memory(context* cxt, cl_mem_flags flags)
  : m_context(cxt), m_flags(flags)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  XOCL_DEBUG(std::cout,"xocl::memory::memory(): ",m_uid,"\n");
  appdebug::add_clmem(this);
}

memory::
~memory()
{
  XOCL_DEBUG(std::cout,"xocl::memory::~memory(): ",m_uid,"\n");

  if (m_dtor_notify)
    std::for_each(m_dtor_notify->rbegin(),m_dtor_notify->rend(),
                  [](std::function<void()>& fcn) { fcn(); });
   appdebug::remove_clmem(this);
}


void
memory::
update_buffer_object_map(device* device, buffer_object_handle boh)
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  if (m_bomap.size() == 0) {
    m_bomap[device] = std::move(boh);
  } else {
    throw std::runtime_error("memory::update_buffer_object_map: bomap should be empty. This is a new cl_mem object.");
  }
}

memory::buffer_object_handle
memory::
get_buffer_object(device* device, xrt::device::memoryDomain domain, uint64_t memidx)
{
  // for progvar only
  assert(domain==xrt::device::memoryDomain::XRT_DEVICE_PREALLOCATED_BRAM);

  std::lock_guard<std::mutex> lk(m_boh_mutex);
  auto itr = m_bomap.find(device);
  return (itr==m_bomap.end())
    ? (m_bomap[device] = device->allocate_buffer_object(this,domain,memidx,nullptr))
    : (*itr).second;
}

memory::buffer_object_handle
memory::
get_buffer_object(device* device)
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  auto itr = m_bomap.find(device);

  // Maybe import from XARE device
  if (m_bomap.size() && itr==m_bomap.end() && device->is_xare_device()) {
    auto first = m_bomap.begin(); // import any existing BO
    return (m_bomap[device] = device->import_buffer_object(first->first,first->second));
  }

  // Regular none XARE device, or first BO for this mem object
  return (itr==m_bomap.end())
    ? (m_bomap[device] = device->allocate_buffer_object(this))
    : (*itr).second;
}

memory::buffer_object_handle
memory::
get_buffer_object(kernel* kernel, unsigned long argidx)
{
  // Must be single device context
  if (auto device=singleContextDevice(get_context())) {
    // Memory intersection of arg connection across all CUs in current
    // device for the kernel
    auto cu_memidx_mask = device->get_cu_memidx(kernel,argidx);

    auto memidx_mask = get_memidx(device);
    if (memidx_mask.any()) {
      // "this" buffer is already allocated on device, verify that
      // current bank match that reqired for kernel argument
      if ((cu_memidx_mask & memidx_mask).none()) {
        // revisit error code
        throw std::runtime_error("Buffer is allocated in wrong memory bank\n");
      }
      return get_buffer_object_or_error(device);
    }
    else {
      // "this" buffer is not curently allocated on device, allocate
      // in first available bank for argument
      for (size_t idx=0; idx<cu_memidx_mask.size(); ++idx) {
        if (cu_memidx_mask.test(idx)) {
          try {
            std::lock_guard<std::mutex> lk(m_boh_mutex);
            return (m_bomap[device] = device->allocate_buffer_object(this,idx));
          }
          catch (const std::bad_alloc&) {
          }
        }
      }
      throw std::bad_alloc();
    }
  }
  throw std::bad_alloc();
}

memory::buffer_object_handle
memory::
get_buffer_object_or_error(const device* device) const
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  auto itr = m_bomap.find(device);
  if (itr==m_bomap.end())
    throw std::runtime_error("Internal error. cl_mem doesn't map to buffer object");
  return (*itr).second;
}

memory::buffer_object_handle
memory::
get_buffer_object_or_null(const device* device) const
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  auto itr = m_bomap.find(device);
  return itr==m_bomap.end()
    ? nullptr
    : (*itr).second;
}


memory::buffer_object_handle
memory::
try_get_buffer_object_or_error(const device* device) const
{
  std::unique_lock<std::mutex> lk(m_boh_mutex, std::defer_lock);
  if (!lk.try_lock())
    throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on buffer object");
  auto itr = m_bomap.find(device);
  if (itr == m_bomap.end())
    throw xocl::error(DBG_EXCEPT_NOBUF_HANDLE, "Failed to find buffer handle");
  return (*itr).second;
}

memory::memidx_bitmask_type
memory::
get_memidx(const device* dev) const
{
  if (auto boh = get_buffer_object_or_null(dev))
    return dev->get_boh_memidx(boh);
  return memidx_bitmask_type(0);
}

memory::memidx_bitmask_type
memory::get_memidx() const
{
  if (auto device = singleContextDevice(get_context()))
    return get_memidx(device);
  return memidx_bitmask_type(0);
}

void
memory::
try_get_address_bank(uint64_t& addr, std::string& bank) const
{
  if (auto device = singleContextDevice(get_context())) {
    auto boh = try_get_buffer_object_or_error(device);
    addr = device->get_boh_addr(boh);
    bank = device->get_boh_banktag(boh);
    return;
  }
  throw xocl::error(DBG_EXCEPT_NO_DEVICE, "No devices found");
}

void
memory::
add_dtor_notify(std::function<void()> fcn)
{
  if (!m_dtor_notify)
    m_dtor_notify = xrt::make_unique<std::vector<std::function<void()>>>();
  m_dtor_notify->emplace_back(std::move(fcn));
}

//Functions for derived classes.
memory::buffer_object_handle
image::
get_buffer_object(device* device, xrt::device::memoryDomain domain, uint64_t memidx)
{
  if (auto boh = get_buffer_object_or_null(device))
    return boh;
  memory::buffer_object_handle boh = memory::get_buffer_object(device, domain, memidx);
  image_info info;
  populate_image_info(info);
  device->write_buffer(this, 0, get_image_data_offset(), &info);
  return boh;
}

memory::buffer_object_handle
image::
get_buffer_object(device* device)
{
  if (auto boh = get_buffer_object_or_null(device))
    return boh;
  memory::buffer_object_handle boh = memory::get_buffer_object(device);
  image_info info;
  populate_image_info(info);
  device->write_buffer(this, 0, get_image_data_offset(), &info);
  return boh;
}

} // xocl
