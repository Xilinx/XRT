/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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
#include "kernel.h"
#include "context.h"
#include "error.h"


#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#pragma warning ( disable : 4267 4245 4996 )
#endif

namespace {

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
}

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

static xocl::memory::memory_callback_list sg_constructor_callbacks;
static xocl::memory::memory_callback_list sg_destructor_callbacks;

#ifdef __GNUC__
int
ctz(unsigned int x)
{
  return __builtin_ctz(x);
}
#endif

#ifdef _WIN32
#pragma intrinsic(_BitScanForward64)
int
ctz(uint64_t x)
{
  if (!x)
    return 64;
  unsigned long idx = 0;
  _BitScanForward64(&idx,x);
  return idx;
}
#endif


} // namespace

namespace xocl {
memory::
memory(context* cxt, cl_mem_flags flags)
  : m_context(cxt), m_flags(flags)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  XOCL_DEBUG(std::cout,"xocl::memory::memory(): ",m_uid,"\n");

  for (auto& cb: sg_constructor_callbacks)
    cb(this);

  //appdebug::add_clmem(this);
}

memory::
~memory()
{
  XOCL_DEBUG(std::cout,"xocl::memory::~memory(): ",m_uid,"\n");

  try {
    if (m_dtor_notify)
      std::for_each(m_dtor_notify->rbegin(),m_dtor_notify->rend(),
                    [](std::function<void()>& fcn) { fcn(); });

    for (auto& cb: sg_destructor_callbacks)
      cb(this);

    if(m_connidx==-1)
      return;
    //Not very clean, having to remove a const cast.
    const device* dev = get_resident_device();
    if(dev)
      const_cast<device*>(dev)->clear_connection(m_connidx);
  }
  catch (...) {}
}

bool
memory::
set_kernel_argidx(const kernel* kernel, unsigned int argidx)
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  auto itr = std::find_if(m_karg.begin(),m_karg.end(),[kernel](auto& value) {return value.first==kernel;});
  // A buffer can be connected to multiple arguments of same kernel
  if (itr==m_karg.end() || (*itr).second!=argidx) {
    m_karg.push_back(std::make_pair(kernel,argidx));
    return true;
  }
  return false;
}

void
memory::
update_buffer_object_map(const device* device, buffer_object_handle boh)
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  if (m_bomap.size() == 0) {
    update_memidx_nolock(device,boh);
    m_bomap[device] = std::move(boh);
  }
  else {
    throw std::runtime_error("memory::update_buffer_object_map: bomap should be empty. This is a new cl_mem object.");
  }
}

memory::buffer_object_handle
memory::
get_buffer_object(device* device, memory::memidx_type subidx)
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  auto itr = m_bomap.find(device);

  if (itr!=m_bomap.end())
    return (*itr).second;

  // Get memory bank index if assigned, -1 if not assigned, which will trigger
  // allocation error when default allocation is disabled
  get_memidx_nolock(device, subidx); // computes m_memidx
  auto boh = (m_bomap[device] = device->allocate_buffer_object(this,m_memidx));

  // To be deleted when strict bank rules are enforced
  if (boh && m_memidx==-1) {
    auto mset = device->get_boh_memidx(boh);
    // As connectivity section contains both group and bank index. Traverse from
    // the higher order to give priority on group index over bank index
    for (int idx=mset.size() - 1; idx >= 0; --idx) {
      if (mset.test(idx)) {
        m_memidx=idx;
        break;
      }
    }
  }

  if (m_memidx>=0) {
    // Lock kernels to compatibile CUs
    for (auto& karg : m_karg) {
      auto kernel = karg.first;
      auto argidx = karg.second;
      if (!kernel->validate_cus(device,argidx,m_memidx))
        throw xocl::error(CL_MEM_OBJECT_ALLOCATION_FAILURE,
                          "Buffer connected to memory '"
                          + std::to_string(m_memidx)
                          + "' cannot be used as argument to kernel '"
                          + kernel->get_name()
                          + "' because kernel has no compute units that support required connectivity.\n"
                          + kernel->connectivity_debug());
    }
  }

  return boh;
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
    ? xrt::bo{}
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

// private
memory::memidx_type
memory::
get_ext_memidx_nolock(const xclbin& xclbin) const
{
  if (m_memidx>=0)
    return m_memidx;

  if ((m_flags & CL_MEM_EXT_PTR_XILINX) && !m_ext_kernel) {
    auto memid = m_ext_flags & 0xffff;
    if (m_ext_flags & XCL_MEM_TOPOLOGY) {
      m_memidx = memid;
    } else if (memid != 0) {
      auto bank = ctz(memid);
      m_memidx = xclbin.banktag_to_memidx(std::string("bank")+std::to_string(bank));
      if (m_memidx==-1)
        m_memidx = bank;
    } else {
        m_memidx = -1;
    }
  }

  // In software emulation all connections must default to memory
  // index 0 to reflect the connectiviy in the internally created
  // CONNECTIVITY section (core/common/xclbin_swemu.cpp)
  if (m_memidx > 0 && is_sw_emulation())
    m_memidx = 0;

  return m_memidx;
}

memory::memidx_type
memory::
get_ext_memidx(const xclbin& xclbin) const
{
  std::lock_guard<std::mutex> lk(m_boh_mutex);
  return get_ext_memidx_nolock(xclbin);
}

memory::memidx_type
memory::
update_memidx_nolock(const device* device, const buffer_object_handle& boh)
{
  auto mset = device->get_boh_memidx(boh);
  // As connectivity section contains both group and bank index. Traverse from
  // the higher order to give priority on group index over bank index
  for (int idx=mset.size() - 1; idx >= 0; --idx) {
    if (mset.test(idx)) {
      m_memidx=idx;
      break;
    }
  }
  return m_memidx;
}

// private
memory::memidx_type
memory::
get_memidx_nolock(const device* dev, memory::memidx_type subidx) const
{
  // already initialized
  if (m_memidx>=0)
    return m_memidx;

  if (m_flags & CL_MEM_REGISTER_MAP)
    return -1;

  // subbuffer case must be tested thoroughly
  if (auto parent = get_sub_buffer_parent()) {
    m_memidx = parent->get_memidx();
    if (m_memidx>=0)
      return m_memidx;
  }

  // ext assigned
  m_memidx = get_ext_memidx_nolock(dev->get_xclbin());

  if (m_memidx>=0)
    return m_memidx;

  // unique CU connectivity
  m_memidx = dev->get_cu_memidx();

  if (m_memidx>=0)
    return m_memidx;

  if (m_karg.empty())
    // memory index could be from sub-buffer
    return (m_memidx = subidx);

  // kernel,argidx deduced
  memidx_bitmask_type mset;
  mset.set();
  for (auto& karg : m_karg) {
    auto kernel = karg.first;
    auto argidx = karg.second;
    mset &= kernel->get_memidx(dev,argidx);
  }

  if (mset.none())
    throw std::runtime_error("No matching memory index");

  // As connectivity section contains both group and bank index. Traverse from
  // the higher order to give priority on group index over bank index
  for (int idx=mset.size() - 1; idx >= 0; --idx) {
    if (mset.test(idx)) {
      m_memidx = idx;
      break;
    }
  }

  return m_memidx;
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
    m_dtor_notify = std::make_unique<std::vector<std::function<void()>>>();
  m_dtor_notify->emplace_back(std::move(fcn));
}

void
memory::
register_constructor_callbacks (memory::memory_callback_type&& cb)
{
  sg_constructor_callbacks.emplace_back(std::move(cb));
}

void
memory::
register_destructor_callbacks (memory::memory_callback_type&& cb)
{
  sg_destructor_callbacks.emplace_back(std::move(cb));
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
