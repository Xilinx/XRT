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

#include "device.h"
#include "memory.h"
#include "program.h"
#include "compute_unit.h"

#include "xocl/xclbin/xclbin.h"
#include "xocl/api/profile.h"

#include "xrt/util/memory.h"
#include "xrt/scheduler/scheduler.h"

#include <iostream>
#include <fstream>
#include <sstream>

namespace {

static unsigned int uid_count = 0;

static
std::string
to_hex(void* addr)
{
  std::stringstream str;
  str << std::hex << addr;
  return str.str();
}

static void
unaligned_message(void* addr)
{
  xrt::message::send(xrt::message::severity_level::WARNING,
                     "unaligned host pointer '"
                     + to_hex(addr)
                     + "' detected, this leads to extra memcpy");
}

static inline unsigned
myctz(unsigned val)
{
  return __builtin_ctz(val);
}

static void
buffer_resident_or_error(const xocl::memory* buffer, const xocl::device* device)
{
  if (!buffer->is_resident(device))
    throw std::runtime_error("buffer ("
                             + std::to_string(buffer->get_uid())
                             + ") is not resident in device ("
                             + std::to_string(device->get_uid()) + ")");
}

// Copy hbuf to ubuf if necessary
static void
sync_to_ubuf(xocl::memory* buffer, size_t offset, size_t size,
             xrt::device* xdevice, const xrt::device::BufferObjectHandle& boh)
{
  if (buffer->is_aligned())
    return;

  auto ubuf = buffer->get_host_ptr();
  if (ubuf) {
    auto hbuf = xdevice->map(boh);
    xdevice->unmap(boh);
    if (ubuf!=hbuf) {
      ubuf = static_cast<char*>(ubuf) + offset;
      hbuf = static_cast<char*>(hbuf) + offset;
      std::memcpy(ubuf,hbuf,size);
    }
  }
}

// Copy ubuf to hbuf if necessary
static void
sync_to_hbuf(xocl::memory* buffer, size_t offset, size_t size,
             xrt::device* xdevice, const xrt::device::BufferObjectHandle& boh)
{
  if (buffer->is_aligned())
    return;

  auto ubuf = buffer->get_host_ptr();
  if (ubuf) {
    auto hbuf = xdevice->map(boh);
    xdevice->unmap(boh);
    if (ubuf!=hbuf) {
      ubuf = static_cast<char*>(ubuf) + offset;
      hbuf = static_cast<char*>(hbuf) + offset;
      std::memcpy(hbuf,ubuf,size);
    }
  }
}

static void
open_or_error(xrt::device* device, const std::string& log)
{
  if (!device->open(log.size() ? log.c_str() : nullptr, xrt::device::verbosity_level::quiet))
    throw xocl::error(CL_DEVICE_NOT_FOUND,"Device setup failed");
}

static bool
is_hw_emulation()
{
  // Temporary work-around used to set the mDevice based on
  // XCL_EMULATION_MODE=hw_emu.  Otherwise default is mSwEmDevice
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool hwem = xem ? std::strcmp(xem,"hw_emu")==0 : false;
  return hwem;
}

static bool
is_sw_emulation()
{
  // Temporary work-around used to set the mDevice based on
  // XCL_EMULATION_MODE=hw_emu.  Otherwise default is mSwEmDevice
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
}

static bool
is_singleprocess_cpu_em()
{
  static auto ecpuem = std::getenv("ENHANCED_CPU_EM");
  static bool single_process_cpu_em = ecpuem ? std::strcmp(ecpuem,"false")==0 : false;
  return single_process_cpu_em;
}

static void
init_scheduler(xocl::device* device)
{
  auto program = device->get_program();

  if (!program)
    throw xocl::error(CL_INVALID_PROGRAM,"Cannot initialize MBS before program is loadded");

  // cu base address offset from xclbin in current program
  auto xclbin = device->get_xclbin();
  size_t cu_base_offset = xclbin.cu_base_offset();
  size_t cu_shift = xclbin.cu_size();
  bool cu_isr = xclbin.cu_interrupt();

  auto cu2addr = xclbin.cu_base_address_map();

  size_t regmap_size = xclbin.kernel_max_regmap_size();
  XOCL_DEBUG(std::cout,"max regmap size:",regmap_size,"\n");

  xrt::scheduler::init(device->get_xrt_device()
                 ,regmap_size
                 ,cu_isr
                 ,device->get_num_cus()
                 ,cu_shift // cu_offset in lsh value
                 ,cu_base_offset
                 ,cu2addr);
}


}

namespace xocl {

void
device::
track(const memory* mem)
{
#if 0
  // not yet enabled.  need memory.cpp to use and ensure calls to free
  // also proper handling of mem objects imported and gotten from
  // xclGetMemObjectFromFD (should go through xocl::device, not directly
  // to xrt::device)
  std::lock_guard<std::mutex> lk(m_mutex);
  m_memobjs.insert(mem);
#endif
}

xrt::device::BufferObjectHandle
device::
alloc(memory* mem, unsigned int memidx)
{
  auto host_ptr = mem->get_host_ptr();
  auto sz = mem->get_size();
  if (is_aligned_ptr(host_ptr)) {
    auto boh = m_xdevice->alloc(sz,xrt::device::memoryDomain::XRT_DEVICE_RAM,memidx,host_ptr);
    track(mem);
    return boh;
  }

  auto p2p_flag = (mem->get_ext_flags() >> 30) & 0x1;
  auto domain = p2p_flag
    ? xrt::device::memoryDomain::XRT_DEVICE_P2P_RAM
    : xrt::device::memoryDomain::XRT_DEVICE_RAM;

  auto boh = m_xdevice->alloc(sz,domain,memidx,nullptr);
  track(mem);

  // Handle unaligned user ptr
  if (host_ptr) {
    unaligned_message(host_ptr);
    auto bo_host_ptr = m_xdevice->map(boh);
    memcpy(bo_host_ptr, host_ptr, sz);
    m_xdevice->unmap(boh);
  }
  return boh;
}

xrt::device::BufferObjectHandle
device::
alloc(memory* mem)
{
  auto host_ptr = mem->get_host_ptr();
  auto sz = mem->get_size();

  if (is_aligned_ptr(host_ptr)) {
    auto boh = m_xdevice->alloc(sz,host_ptr);
    track(mem);
    return boh;
  }

  auto boh = m_xdevice->alloc(sz);
  // Handle unaligned user ptr
  if (host_ptr) {
    unaligned_message(host_ptr);
    auto bo_host_ptr = m_xdevice->map(boh);
    memcpy(bo_host_ptr, host_ptr, sz);
    m_xdevice->unmap(boh);
  }
  track(mem);
  return boh;
}

device::
device(platform* pltf, xrt::device* xdevice)
  : m_uid(uid_count++), m_platform(pltf), m_xdevice(xdevice)
{
  XOCL_DEBUG(std::cout,"xocl::device::device(",m_uid,")\n");
}

device::
device(platform* pltf, xrt::device* hw_device, xrt::device* swem_device, xrt::device* hwem_device)
  : m_uid(uid_count++), m_platform(pltf), m_xdevice(nullptr)
  , m_hw_device(hw_device), m_swem_device(swem_device), m_hwem_device(hwem_device)
{
  XOCL_DEBUG(std::cout,"xocl::device::device(",m_uid,")\n");

  // Open the devices.  I don't recall what it means to open a device?
  std::string hallog = xrt::config::get_hal_logging();
  if (!hallog.compare("null"))
    hallog.clear();

  if (m_swem_device) {
    std::string log = hallog;
    if (log.size())
      log.append(".sw_em");
    open_or_error(m_swem_device,log);
  }

  if (m_hwem_device) {
    std::string log = hallog;
    if (log.size())
      log.append(".hw_em");
    open_or_error(m_hwem_device,log);
  }

  // Hack to accomodate missing sw_em device info.
  if (m_hwem_device && m_swem_device && is_singleprocess_cpu_em())
    m_swem_device->copyDeviceInfo(m_hwem_device);

  if (m_hw_device)
    open_or_error(m_hw_device,hallog);

  // Default to SwEm mode
  // Later code unconditionally calls through mOperations-> with a
  // null device handle which then becomes a noop in the shim code
  // The code should check for a valid mOperations before dereferencing
  // but for now we just default mOperations as before, but importantly
  // do not set mDeviceHandle (this is deferred as before to loadBinary).
  if (m_hwem_device && is_hw_emulation())
    set_xrt_device(m_hwem_device,true); // final
  else if (m_swem_device && is_sw_emulation())
    set_xrt_device(m_swem_device,true); // final
  else if (m_swem_device)
    set_xrt_device(m_swem_device,false);
  else if (m_hwem_device)
    set_xrt_device(m_hwem_device,false);
  else
    set_xrt_device(m_hw_device,true); // final
}

device::
device(platform* pltf, xrt::device* swem_device, xrt::device* hwem_device)
  : device(pltf,nullptr,swem_device,hwem_device)
{}

device::
device(device* parent, const compute_unit_vector_type& cus)
  : m_uid(uid_count++)
  , m_active(parent->m_active)
  , m_xclbin(parent->m_xclbin)
  , m_platform(parent->m_platform)
  , m_xdevice(parent->m_xdevice)
  , m_hw_device(parent->m_hw_device)
  , m_swem_device(parent->m_swem_device)
  , m_hwem_device(parent->m_hwem_device)
  , m_parent(parent)
  , m_computeunits(std::move(cus))
{
  XOCL_DEBUG(std::cout,"xocl::device::device(",m_uid,")\n");

  // The subdevice is *not* added to platform devices.  The platform
  // tracks physical devices only.  A subdevice is deleted through
  // normal reference counting.

  // Current program tracks this subdevice on which it is implicitly loaded.
  m_active->add_device(this);
}

device::
~device()
{
  XOCL_DEBUG(std::cout,"xocl::device::~device(",m_uid,")\n");
}

void
device::
set_xrt_device(xrt::device* xd,bool final)
{
  if (!final && m_xdevice)
    throw std::runtime_error("temp device already set");
  m_xdevice = xd;

  // start the DMA threads if necessary
  if (final)
    m_xdevice->setup();
}

void
device::
set_xrt_device(const xocl::xclbin& xclbin)
{
  using target_type = xocl::xclbin::target_type;
  auto core_target = xclbin.target();

  // Check device is capable of core_target
  if(core_target==target_type::bin && !m_hw_device)
    throw xocl::error(CL_INVALID_PROGRAM,"device::load_binary binary target=Bin, no Hw HAL handle");
  if(core_target==target_type::hwem && !m_hwem_device)
    throw xocl::error(CL_INVALID_PROGRAM,"device::load_binary binary target=HwEm, no HwEm HAL handle");
  if(core_target==target_type::csim && !m_swem_device)
    throw xocl::error(CL_INVALID_PROGRAM,"device::load_binary binary target=SwEm, no SwEm HAL handle");

  //Setup device for binary core_target
  if (core_target==target_type::bin)
    set_xrt_device(m_hw_device);
  else if (core_target==target_type::hwem)
    set_xrt_device(m_hwem_device);
  else if (core_target==target_type::csim)
    set_xrt_device(m_swem_device);
  else
    throw xocl::error(CL_INVALID_PROGRAM,"Unknown core target");
}

unsigned int
device::
lock()
{
  std::lock_guard<std::mutex> lk(m_mutex);
  // If already locked, return increment lock count
  if (m_locks)
    return ++m_locks;

  // First time, but only hw devices need locking
#ifndef PMD_OCL
  if (m_hw_device) {
    auto rv = m_hw_device->lockDevice();
    if (rv.valid() && rv.get())
      throw  xocl::error(CL_DEVICE_NOT_AVAILABLE,"could not lock device");
  }
#endif

  // All good, return increment lock count
  return ++m_locks;
}

unsigned int
device::
unlock()
{
  std::lock_guard<std::mutex> lk(m_mutex);
  // Return lock count if not locked or decremented lock count > 0
  if (!m_locks || --m_locks)
    return m_locks;

  // Last locked was released, now unlock hw device if any
#ifndef PMD_OCL
  if (m_hw_device) {
    auto rv = m_hw_device->unlockDevice();
    if (rv.valid() && rv.get())
      throw  xocl::error(CL_DEVICE_NOT_AVAILABLE,"could not unlock device");
  }
#endif

  return m_locks; // 0
}

xrt::device::BufferObjectHandle
device::
allocate_buffer_object(memory* mem)
{
  if (mem->get_flags() & CL_MEM_REGISTER_MAP)
    return nullptr;

  auto xdevice = get_xrt_device();

  // sub buffer
  if (auto parent = mem->get_sub_buffer_parent()) {
    auto boh = parent->get_buffer_object(this);
    auto offset = mem->get_sub_buffer_offset();
    auto size = mem->get_size();
    return xdevice->alloc(boh,size,offset);
  }

  if ((mem->get_flags() & CL_MEM_EXT_PTR_XILINX)
	  && xdevice->hasBankAlloc())
  {
    //Extension flags were passed. Get the extension flags.
    //First 8 bits will indicate legacy/mem_topology etc.
    //Rest 24 bits directly indexes into mem topology section OR.
    //have legacy one-hot encoding.
    auto flag = mem->get_ext_flags();
    int32_t memidx = 0;
    if(flag & XCL_MEM_TOPOLOGY) {
      memidx = flag & 0xffffff;
    }else {
      flag = flag & 0xffffff;
      auto bank = myctz(flag);
      memidx = m_xclbin.banktag_to_memidx(std::string("bank")+std::to_string(bank));
      if(memidx==-1){
        memidx = bank;
      }
    }

    try {
      auto boh = alloc(mem,memidx);
      XOCL_DEBUG(std::cout,"memory(",mem->get_uid(),") allocated on device(",m_uid,") in memory index(",flag,")\n");
      return boh;
    }
    catch (const std::bad_alloc&) {
    }
  }

//  //auto flag = (mem->get_ext_flags() >> 8) & 0xff;
//  auto flag = (mem->get_ext_flags()) & 0xffffff;
//  if (flag && xdevice->hasBankAlloc()) {
//    auto bank = myctz(flag);
//    auto memidx = m_xclbin.banktag_to_memidx(std::string("bank")+std::to_string(bank));
//
//    // HBM support does not use bank tag, host code must use proper enum value
//    if(memidx==-1)
//      memidx = bank;
//
//    // Determine the bank number for the buffers
//    try {
//      auto boh = alloc(mem,memidx);
//      XOCL_DEBUG(std::cout,"memory(",mem->get_uid(),") allocated on device(",m_uid,") in memory index(",flag,")\n");
//      return boh;
//    }
//    catch (const std::bad_alloc&) {
//    }
//  }

  // If buffer could not be allocated on the requested bank,
  // or if no bank was specified, then allocate on the bank
  // (memidx) matching the CU connectivity of CUs in device.
  auto memidx = get_cu_memidx();
  if (memidx>=0) {
    try {
      auto boh = alloc(mem,memidx);
      XOCL_DEBUG(std::cout,"memory(",mem->get_uid(),") allocated on device(",m_uid,") in bank with idx(",memidx,")\n");
      return boh;
    }
    catch (const std::bad_alloc&) {
    }
  }

  // Else just allocated on any bank
  XOCL_DEBUG(std::cout,"memory(",mem->get_uid(),") allocated on device(",m_uid,") in default bank\n");
  return alloc(mem);
}

xrt::device::BufferObjectHandle
device::
allocate_buffer_object(memory* mem, uint64_t memidx)
{
  if (mem->get_flags() & CL_MEM_REGISTER_MAP)
    throw std::runtime_error("Cannot allocate register map buffer on bank");

  auto xdevice = get_xrt_device();

  // sub buffer
  if (mem->get_sub_buffer_parent()) {
    throw std::runtime_error("sub buffer bank allocation not implemented");
  }

  auto flag = (mem->get_ext_flags()) & 0xffffff;
  if (flag && xdevice->hasBankAlloc()) {
    auto bank = myctz(flag);
    auto midx = m_xclbin.banktag_to_memidx(std::string("bank")+std::to_string(bank));
    if (midx==-1)
      midx=bank;
    if (static_cast<uint64_t>(midx)!=memidx)
      throw std::runtime_error("implicitly request memidx("
                               +std::to_string(memidx)
                               +") does not match explicit memidx("
                               +std::to_string(midx)+")");
  }

  auto boh = alloc(mem,memidx);
  XOCL_DEBUG(std::cout,"memory(",mem->get_uid(),") allocated on device(",m_uid,") in memory index(",memidx,")\n");
  return boh;
}

void
device::
free(const memory* mem)
{
  std::lock_guard<std::mutex> lk(m_mutex);
  auto itr = m_memobjs.find(mem);
  if (itr==m_memobjs.end())
    throw std::runtime_error("Internal error: xocl::mem("
                             + std::to_string(mem->get_uid())
                             + ") is not allocated on device("
                             + std::to_string(get_uid()) + ")");
  m_memobjs.erase(itr);
}

xrt::device::BufferObjectHandle
device::
allocate_buffer_object(memory* mem, xrt::device::memoryDomain domain, uint64_t memoryIndex, void* user_ptr)
{
  auto xdevice = get_xrt_device();
  return xdevice->alloc(mem->get_size(),domain,memoryIndex,user_ptr);
}

uint64_t
device::
get_boh_addr(const xrt::device::BufferObjectHandle& boh) const
{
  auto xdevice = get_xrt_device();
  return xdevice->getDeviceAddr(boh);
}

device::memidx_bitmask_type
device::
get_boh_memidx(const xrt::device::BufferObjectHandle& boh) const
{
  auto addr = get_boh_addr(boh);
  return m_xclbin.mem_address_to_memidx(addr);
}

std::string
device::
get_boh_banktag(const xrt::device::BufferObjectHandle& boh) const
{
  auto addr = get_boh_addr(boh);
  auto memidx = m_xclbin.mem_address_to_first_memidx(addr);
  if (memidx == -1)
    return "Unknown";
  return m_xclbin.memidx_to_banktag(memidx);
}

int
device::
get_cu_memidx() const
{
  std::lock_guard<std::mutex> lk(m_mutex);
  if (m_cu_memidx == -2) {
    m_cu_memidx = -1;

    if (get_num_cus()) {
      // compute intersection of all CU memory masks
      memidx_bitmask_type mask;
      mask.set();
      for (auto& cu : get_cu_range())
        mask &= cu->get_memidx_intersect();

      // select first common memory bank index if any
      for (size_t idx=0; idx<mask.size(); ++idx) {
        if (mask.test(idx)) {
          m_cu_memidx = idx;
          break;
        }
      }
    }
  }
  return m_cu_memidx;
}

device::memidx_bitmask_type
device::
get_cu_memidx(kernel* kernel, int argidx) const
{
  bool set = false;
  memidx_bitmask_type memidx;
  memidx.set();
  auto sid = kernel->get_symbol_uid();

  // iterate CUs
  for (auto& cu : get_cus()) {
    if (cu->get_symbol_uid()!=sid)
      continue;
    memidx &= cu->get_memidx(argidx);
    set = true;
  }

  if (!set)
    memidx.reset();

  return memidx;
}

xrt::device::BufferObjectHandle
device::
import_buffer_object(const device* src_device, const xrt::device::BufferObjectHandle& src_boh)
{
  // Consider moving to xrt::device

  // Export from exporting device (src_device)
  auto fd = src_device->get_xrt_device()->getMemObjectFd(src_boh);

  // Import into this device
  size_t size=0;
  return get_xrt_device()->getBufferFromFd(fd,size,1);
}

void*
device::
map_buffer(memory* buffer, cl_map_flags map_flags, size_t offset, size_t size, void* assert_result)
{
  auto xdevice = get_xrt_device();
  xrt::device::BufferObjectHandle boh;

  // If buffer is resident it must be refreshed unless CL_MAP_INVALIDATE_REGION
  // is specified in which case host will discard current content
  if (!(map_flags & CL_MAP_WRITE_INVALIDATE_REGION) && buffer->is_resident(this)) {
    boh = buffer->get_buffer_object_or_error(this);
    xdevice->sync(boh,buffer->get_size(),0,xrt::hal::device::direction::DEVICE2HOST,false);
  }

  if (!boh)
    boh = buffer->get_buffer_object(this);

  auto ubuf = buffer->get_host_ptr();
  if (!ubuf || !is_aligned_ptr(ubuf)) {
    // boh was created with it's own alloced host_ptr
    auto hbuf = xdevice->map(boh);
    xdevice->unmap(boh);
    assert(ubuf!=hbuf);
    if (ubuf)
      memcpy(ubuf,hbuf,buffer->get_size());
    else
      ubuf = hbuf;
  }

  void* result = static_cast<char*>(ubuf) + offset;
  assert(!assert_result || result==assert_result);

  // If this buffer is being mapped for writing, then a following
  // unmap will have to sync the data to device, so record this.  We
  // will not enforce that map is followed by unmap, so two maps of
  // same buffer for write without corresponding unmaps is not an
  // and will simply override previous map value.
  std::lock_guard<std::mutex> lk(m_mutex);
  m_mapped[result] = map_flags;
  return result;
}

void
device::
unmap_buffer(memory* buffer, void* mapped_ptr)
{
  cl_map_flags flags = 0;
  {
    // There is no checking that map/unmap match.  Only one active
    // map of a mapped_ptr is maintained and is erased on first unmap
    std::lock_guard<std::mutex> lk(m_mutex);
    auto itr = m_mapped.find(mapped_ptr);
    if (itr!=m_mapped.end()) {
      flags = (*itr).second;
      m_mapped.erase(itr);
    }
  }

  auto xdevice = get_xrt_device();
  auto boh = buffer->get_buffer_object_or_error(this);

  // Sync data to boh if write flags, and sync to device if resident
  if (flags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION)) {
    if (auto ubuf = buffer->get_host_ptr())
      xdevice->write(boh,ubuf,buffer->get_size(),0,false);
    if (buffer->is_resident(this))
      xdevice->sync(boh,buffer->get_size(),0,xrt::hal::device::direction::HOST2DEVICE,false);
  }
}

void
device::
migrate_buffer(memory* buffer,cl_mem_migration_flags flags)
{
  // Support clEnqueueMigrateMemObjects device->host
  if (flags & CL_MIGRATE_MEM_OBJECT_HOST) {
    buffer_resident_or_error(buffer,this);
    auto boh = buffer->get_buffer_object_or_error(this);
    auto xdevice = get_xrt_device();
    xdevice->sync(boh,buffer->get_size(),0,xrt::hal::device::direction::DEVICE2HOST,false);
    sync_to_ubuf(buffer,0,buffer->get_size(),xdevice,boh);
    return;
  }

  // Host to device for kernel args and clEnqueueMigrateMemObjects
  // Get or create the buffer object on this device.
  auto xdevice = get_xrt_device();
  xrt::device::BufferObjectHandle boh = buffer->get_buffer_object(this);

  // Sync from host to device to make make buffer resident of this device
  sync_to_hbuf(buffer,0,buffer->get_size(),xdevice,boh);
  xdevice->sync(boh,buffer->get_size(), 0, xrt::hal::device::direction::HOST2DEVICE,false);

  // Now buffer is resident on this device and migrate is complete
  buffer->set_resident(this);
}

void
device::
write_buffer(memory* buffer, size_t offset, size_t size, const void* ptr)
{
  auto xdevice = get_xrt_device();
  auto boh = buffer->get_buffer_object(this);

  // Write data to buffer object at offset
  xdevice->write(boh,ptr,size,offset,false);

  // Update unaligned ubuf if necessary
  sync_to_ubuf(buffer,offset,size,xdevice,boh);

  if (buffer->is_resident(this))
    // Sync new written data to device at offset
    // HAL performs read/modify write if necesary
    xdevice->sync(boh,size,offset,xrt::hal::device::direction::HOST2DEVICE,false);
}

void
device::
read_buffer(memory* buffer, size_t offset, size_t size, void* ptr)
{
  auto xdevice = get_xrt_device();
  auto boh = buffer->get_buffer_object(this);

  if (buffer->is_resident(this))
    // Sync back from device at offset to buffer object
    // HAL performs skip/copy read if necesary
    xdevice->sync(boh,size,offset,xrt::hal::device::direction::DEVICE2HOST,false);

  // Read data from buffer object at offset
  xdevice->read(boh,ptr,size,offset,false);

  // Update unaligned ubuf if necessary
  sync_to_ubuf(buffer,offset,size,xdevice,boh);
}

void
device::
copy_buffer(memory* src_buffer, memory* dst_buffer, size_t src_offset, size_t dst_offset, size_t size)
{
  char* hbuf_src = static_cast<char*>(map_buffer(src_buffer,CL_MAP_READ,src_offset,size,nullptr));
  char* hbuf_dst = static_cast<char*>(map_buffer(dst_buffer,CL_MAP_WRITE_INVALIDATE_REGION,dst_offset,size,nullptr));
  std::memcpy(hbuf_dst,hbuf_src,size);
  unmap_buffer(src_buffer,hbuf_src);
  unmap_buffer(dst_buffer,hbuf_dst);
}

void
device::
copy_p2p_buffer(memory* src_buffer, memory* dst_buffer, size_t src_offset, size_t dst_offset, size_t size)
{
  auto xdevice = get_xrt_device();
  auto src_boh = src_buffer->get_buffer_object(this);
  auto dst_boh = dst_buffer->get_buffer_object(this);
  xdevice->copy(dst_boh, src_boh, size, dst_offset, src_offset);
}

void
device::
fill_buffer(memory* buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size)
{
  auto boh = xocl::xocl(buffer)->get_buffer_object(this);
  char* hbuf = static_cast<char*>(map_buffer(buffer,CL_MAP_WRITE_INVALIDATE_REGION,offset,size,nullptr));
  char* dst = hbuf;
  for (; pattern_size <= size; size-=pattern_size, dst+=pattern_size)
    std::memcpy(dst,pattern,pattern_size);
  if (size)
    std::memcpy(dst,pattern,size);
  unmap_buffer(buffer,hbuf);
}

static void
rw_image(device* device,
         memory* image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch
         ,char* read_to,const char* write_from)
{
  auto boh = image->get_buffer_object(device);
  auto xdevice = device->get_xrt_device();

  size_t image_offset = image->get_image_data_offset()
    + image->get_image_bytes_per_pixel()*origin[0]
    + image->get_image_row_pitch()*origin[1]
    + image->get_image_slice_pitch()*origin[2];

  if (!origin[0] && region[0]==image->get_image_width() && row_pitch==image->get_image_row_pitch()
      && (region[2]==1
          || (!origin[1] && region[1]==image->get_image_height() && slice_pitch==image->get_image_slice_pitch())
          )
      ) {
    size_t sz = region[2]==1 ? row_pitch*region[1] : slice_pitch*region[2];
    if (read_to)
      xdevice->read(boh,read_to,sz,image_offset,false);
    else
      xdevice->write(boh,write_from,sz,image_offset,false);
  }
  else {
    size_t image_offset_tmp = image_offset;
    for (unsigned int j=0; j<region[2]; ++j) {
      size_t offset = image_offset_tmp;
      for (unsigned int i=0; i<region[1]; ++i) {
        if (read_to) {
          xdevice->read(boh,read_to,image->get_image_bytes_per_pixel()*region[0],offset,false);
          read_to += row_pitch;
        }
        else {
          xdevice->write(boh,write_from,image->get_image_bytes_per_pixel()*region[0],offset, false);
          write_from += row_pitch;
        }
        offset += image->get_image_row_pitch();
      }
      image_offset_tmp += image->get_image_slice_pitch();
      if (read_to)
        read_to += slice_pitch;
      else
        write_from += slice_pitch;
    }
  }
}

void
device::
write_image(memory* image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch,const void *ptr)
{
  // Write from ptr into image
  rw_image(this,image,origin,region,row_pitch,slice_pitch,nullptr,static_cast<const char*>(ptr));

  // Sync newly writte data to device if image is resident
  if (image->is_resident(this)) {
    auto boh = image->get_buffer_object_or_error(this);
    get_xrt_device()->sync(boh, image->get_size(), 0,xrt::hal::device::direction::HOST2DEVICE,false);
  }
}

void
device::
read_image(memory* image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch,void *ptr)
{
  // Sync back from device if image is resident
  if (image->is_resident(this)) {
    auto boh = image->get_buffer_object_or_error(this);
    get_xrt_device()->sync(boh,image->get_size(),0,xrt::hal::device::direction::DEVICE2HOST,false);
  }

  // Now read from image into ptr
  rw_image(this,image,origin,region,row_pitch,slice_pitch,static_cast<char*>(ptr),nullptr);
}

void
device::
read_register(memory* mem, size_t offset,void* ptr, size_t size)
{
  if (!(mem->get_flags() & CL_MEM_REGISTER_MAP))
    throw xocl::error(CL_INVALID_OPERATION,"read_register requures mem object with CL_MEM_REGISTER_MAP");
  get_xrt_device()->read_register(offset,ptr,size);
}

void
device::
write_register(memory* mem, size_t offset,const void* ptr, size_t size)
{
  if (!(mem->get_flags() & CL_MEM_REGISTER_MAP))
    throw xocl::error(CL_INVALID_OPERATION,"read_register requures mem object with CL_MEM_REGISTER_MAP");

  auto cmd = std::make_shared<xrt::command>(get_xrt_device(),ERT_WRITE);
  auto packet = cmd->get_packet();
  auto idx = packet.size() + 1; // past header is start of payload
  auto addr = offset;
  auto value = reinterpret_cast<const uint32_t*>(ptr);

  while (size>0) {
    packet[idx++] = addr;
    packet[idx++] = *value;
    ++value;
    addr+=4;
    size-=4;
  }

  auto ecmd = xrt::command_cast<ert_packet*>(cmd);
  ecmd->type = ERT_KDS_LOCAL;
  ecmd->count = packet.size() - 1; // substract header

  xrt::scheduler::schedule(cmd);
  cmd->wait();
}

void
device::
load_program(program* program)
{
  if (m_parent.get())
    throw xocl::error(CL_OUT_OF_RESOURCES,"cannot load program on sub device");

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_active && !std::getenv("XCL_CONFORMANCE"))
    throw xocl::error(CL_OUT_OF_RESOURCES,"program already loaded on device");

  m_xclbin = program->get_xclbin(this);
  auto binary = m_xclbin.binary(); // ::xclbin::binary
  auto binary_data = binary.binary_data();
  auto binary_size = binary_data.second - binary_data.first;

  // validatate target binary for target device and set the xrt device
  // according to target binary this is likely temp code that is
  // needed only as long as the concrete device cannot be determined
  // up front
  set_xrt_device(m_xclbin);

  if (binary_size == 0)
    return;

  // get the xrt device.  guaranteed to be the final device after
  // above call to setXrtDevice
  auto xdevice = get_xrt_device();

  // reclocking - old
  // This is obsolete and will be removed soon (pending verify.xclbin updates)
  if (xrt::config::get_frequency_scaling()) {
    const clock_freq_topology* freqs = m_xclbin.get_clk_freq_topology();
    if(!freqs) {
      if (!is_sw_emulation())
        std::cout << "WARNING: Please update xclbin. Legacy clocking section support will be removed soon" << std::endl;
      unsigned short idx = 0;
      unsigned short target_freqs[4] = {0};
      auto kclocks = m_xclbin.kernel_clocks();
      if (kclocks.size()>2)
	throw xocl::error(CL_INVALID_PROGRAM,"Too many kernel clocks");
      for (auto& clock : kclocks) {
	if (idx == 0) {
	  std::string device_name = get_unique_name();
	  profile::set_kernel_clock_freq(device_name, clock.frequency);
	}
	target_freqs[idx++] = clock.frequency;
      }

      // System clocks
      idx=2; // system clocks start at idx==2
      auto sclocks = m_xclbin.system_clocks();
      if (sclocks.size()>2)
	throw xocl::error(CL_INVALID_PROGRAM,"Too many system clocks");
      for (auto& clock : sclocks)
	target_freqs[idx++] = clock.frequency;

//      for(int i = 0; i < 4; ++i) {
//	std::cout << "Original frequency calc: " << i  << "\t" << target_freqs[i] << std::endl;
//      }

      auto rv = xdevice->reClock2(0,target_freqs);

      if (rv.valid() && rv.get())
	  throw xocl::error(CL_INVALID_PROGRAM,"Reclocking failed");
    }
  }


//  // reclocking - new
//  if (xrt::config::get_frequency_scaling())
//  {
//    unsigned short idx = 0;
//    unsigned short target_freqs[4] = {0};
//    const clock_freq_topology* freqs = m_xclbin.get_clk_freq_topology();
//    if (freqs)
//    {
//      int16_t count = 0;
//      std::vector<const clock_freq*> data_clks;
//      std::vector<const clock_freq*> kernel_clks;
//      std::vector<const clock_freq*> system_clks;
//
//      while (count < freqs->m_count)
//      {
//	  const clock_freq* freq = &(freqs->m_clock_freq[count++]);
//	  if(freq->m_type == CT_DATA)
//	    data_clks.emplace_back(freq);
//	  else if(freq->m_type == CT_KERNEL)
//	    kernel_clks.emplace_back(freq);
//	  else if(freq->m_type == CT_SYSTEM)
//	    system_clks.emplace_back(freq);
//	  else
//	    throw xocl::error(CL_INVALID_PROGRAM,"Unknown clock type in xclbin");
//      }
//
//      if(data_clks.size() !=1)
//        throw xocl::error(CL_INVALID_PROGRAM,"Data clocks not found in xclbin");
//      if(kernel_clks.size() !=1)
//        throw xocl::error(CL_INVALID_PROGRAM,"Kernel clocks not found in xclbin");
//      if(system_clks.size() > 2)
//        throw xocl::error(CL_INVALID_PROGRAM,"Too many system clocks");
//
//
//      target_freqs[0] = data_clks.at(0)->m_freq_Mhz;
//      target_freqs[1] = kernel_clks.at(0)->m_freq_Mhz;
//      idx = 2;
//      for(auto & sys_clks: system_clks) {
//	target_freqs[idx] = sys_clks->m_freq_Mhz;
//	idx++;
//      }
//
//      std::string device_name = get_unique_name();
//      profile::set_kernel_clock_freq(device_name, target_freqs[0]);
//
////      for(int i = 0; i < 4; ++i) {
////	  std::cout << "New section based frequency: " << i << "\t" << target_freqs[i] << std::endl;
////      }
//
//      auto rv = xdevice->reClock2(0,target_freqs);
//
//      if (rv.valid() && rv.get())
//	  throw xocl::error(CL_INVALID_PROGRAM,"Reclocking failed");
//    }
//  }


  // programmming
  if (xrt::config::get_xclbin_programing()) {
    auto header = reinterpret_cast<const xclBin *>(binary_data.first);
    auto xbrv = xdevice->loadXclBin(header);
    if (xbrv.valid() && xbrv.get())
      throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin");

    if (!xbrv.valid()) {
      throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin");
    }
  }

  // Add compute units for each kernel in the program.
  // Note, that conformance mode renames the kernels in the xclbin
  // so iterating kernel names and looking up symbols from kernels
  // isn't possible, we *must* iterator symbols explicitly
  m_computeunits.clear();
  m_cu_memidx = -2;
  for (auto symbol : m_xclbin.kernel_symbols()) {
    for (auto& inst : symbol->instances) {
      add_cu(xrt::make_unique<compute_unit>(symbol,inst.name,this));
    }
  }

  m_active = program;
  profile::add_to_active_devices(get_unique_name());

  init_scheduler(this);
}

void
device::
unload_program(const program* program)
{
  if (m_active == program) {
    get_xrt_device()->resetKernel();
    m_computeunits.clear();
    m_active = nullptr;
  }
}

xclbin
device::
get_xclbin() const
{
  assert(!m_active || m_active->get_xclbin(this)==m_xclbin);
  return m_xclbin;
}


unsigned short
device::
get_max_clock_frequency() const
{
  if (!m_xdevice)
    return 0;

  auto freqs = m_xdevice->getClockFrequencies();
  return *std::max_element(freqs.begin(),freqs.end());
}

} // xocl
