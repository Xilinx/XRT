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

#include "device.h"
#include "memory.h"
#include "program.h"
#include "compute_unit.h"

#include "xocl/api/plugin/xdp/profile.h"
#include "xocl/api/plugin/xdp/debug.h"
#include "xocl/xclbin/xclbin.h"
#include "xrt/scheduler/scheduler.h"
#include "xrt/util/config_reader.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "core/common/xclbin_parser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#pragma warning ( disable : 4244 4245 4267 4996 4505 )
#endif


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
  xrt_xocl::message::send(xrt_xocl::message::severity_level::warning,
                     "unaligned host pointer '"
                     + to_hex(addr)
                     + "' detected, this leads to extra memcpy");
}

static void
userptr_bad_alloc_message(void* addr)
{
  xrt_xocl::message::send(xrt_xocl::message::severity_level::info,
                     "might be noncontiguous host pointer '"
                     + to_hex(addr)
                     + "' detected, check dmesg for more information."
                     + " This could lead to extra memcpy."
                     + " To avoid this, please try xclGetMemObjectFd() and xclGetMemObjectFromFd(),"
                     + " instead of use CL_MEM_USE_HOST_PTR.");
}

static void
host_copy_message(const xocl::memory* dst, const xocl::memory* src)
{
  std::stringstream str;
  str << "Reverting to host copy for src buffer(" << src->get_uid() << ") "
      << "to dst buffer(" << dst->get_uid() << ")";
  xrt_xocl::message::send(xrt_xocl::message::severity_level::warning,str.str());
}

XOCL_UNUSED static void
cmd_copy_message(const xocl::memory* dst, const xocl::memory* src)
{
  std::stringstream str;
  str << "No M2M, reverting to command based copying for src buffer(" << src->get_uid() << ") "
      << "to dst buffer(" << dst->get_uid() << ")";
  xrt_xocl::message::send(xrt_xocl::message::severity_level::warning,str.str());
}

static void
buffer_resident_or_error(const xocl::memory* buffer, const xocl::device* device)
{
  if (!buffer->is_resident(device))
    throw std::runtime_error("buffer ("
                             + std::to_string(buffer->get_uid())
                             + ") is not resident in device ("
                             + std::to_string(device->get_uid()) + ") so migration from device to host fails");
}

// Copy hbuf to ubuf if necessary
static void
sync_to_ubuf(xocl::memory* buffer, size_t offset, size_t size,
             xrt_xocl::device* xdevice, const xocl::device::buffer_object_handle& boh)
{
  if (!buffer->need_extra_sync())
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
             xrt_xocl::device* xdevice, const xocl::device::buffer_object_handle& boh)
{
  if (!buffer->need_extra_sync())
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

static std::vector<uint64_t>
get_xclbin_cus(const xocl::device* d)
{
  if (is_sw_emulation()) {
    auto xml = d->get_axlf_section(EMBEDDED_METADATA);
    return xml.first ? xrt_core::xclbin::get_cus(xml.first, xml.second) : std::vector<uint64_t>{};
  }

  auto ip_layout = d->get_axlf_section<const ::ip_layout*>(axlf_section_kind::IP_LAYOUT);
  return ip_layout ? xrt_core::xclbin::get_cus(ip_layout) : std::vector<uint64_t>{};
}

XOCL_UNUSED static bool
is_emulation_mode()
{
  static bool val = is_sw_emulation() || is_hw_emulation();
  return val;
}

static void
init_scheduler(xocl::device* device)
{
  auto program = device->get_program();

  if (!program)
    throw xocl::error(CL_INVALID_PROGRAM,"Cannot initialize MBS before program is loadded");

  xrt_xocl::scheduler::init(device->get_xdevice());
}

}

namespace xocl {

std::string
device::
get_bdf() const 
{
  if (!m_xdevice)
    throw xocl::error(CL_INVALID_DEVICE, "No BDF");

  // logically const
  auto lk = const_cast<device*>(this)->lock_guard();

  auto core_device = m_xdevice->get_core_device();
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(core_device);
  return xrt_core::query::pcie_bdf::to_string(bdf);
}

bool
device::
is_nodma() const 
{
  if (!m_xdevice)
    throw xocl::error(CL_INVALID_DEVICE, "Can't check for nodma");

  // logically const
  auto lk = const_cast<device*>(this)->lock_guard();

  auto core_device = m_xdevice->get_core_device();
  return core_device->is_nodma();
}

void*
device::
get_handle() const
{
  if (m_xdevice)
    return m_xdevice->get_xcl_handle();
  throw xocl::error(CL_INVALID_DEVICE, "No device handle");
}

xrt_xocl::device::memoryDomain
get_mem_domain(const memory* mem)
{
  if (mem->is_device_memory_only())
    return xrt_xocl::device::memoryDomain::XRT_DEVICE_ONLY_MEM;
  else if (mem->is_device_memory_only_p2p())
    return xrt_xocl::device::memoryDomain::XRT_DEVICE_ONLY_MEM_P2P;
  else if (mem->is_host_only())
    return xrt_xocl::device::memoryDomain::XRT_HOST_ONLY_MEM;

  return xrt_xocl::device::memoryDomain::XRT_DEVICE_RAM;
}

void
device::
clear_connection(connidx_type conn)
{
  assert(conn!=-1);
  m_metadata.clear_connection(conn);
}

device::buffer_object_handle
device::
alloc(memory* mem, memidx_type memidx)
{
  auto host_ptr = mem->get_host_ptr();
  auto sz = mem->get_size();
  bool aligned_flag = false;

  if (is_aligned_ptr(host_ptr)) {
    aligned_flag = true;
    try {
      auto boh = m_xdevice->alloc(sz,xrt_xocl::device::memoryDomain::XRT_DEVICE_RAM,memidx,host_ptr);
      return boh;
    }
    catch (const std::bad_alloc&) {
      userptr_bad_alloc_message(host_ptr);
    }
  }

  auto domain = get_mem_domain(mem);

  auto boh = m_xdevice->alloc(sz,domain,memidx,nullptr);

  // Handle unaligned user ptr or bad alloc host_ptr
  if (host_ptr) {
    if (!aligned_flag)
      unaligned_message(host_ptr);
    mem->set_extra_sync();
    auto bo_host_ptr = m_xdevice->map(boh);
    // No need to copy data to a CL_MEM_WRITE_ONLY buffer
    if (!(mem->get_flags() & CL_MEM_WRITE_ONLY))
        memcpy(bo_host_ptr, host_ptr, sz);

    m_xdevice->unmap(boh);
  }
  return boh;
}

int
device::
get_stream(xrt_xocl::device::stream_flags flags, xrt_xocl::device::stream_attrs attrs,
           const cl_mem_ext_ptr_t* ext, xrt_xocl::device::stream_handle* stream, int32_t& conn)
{
  uint64_t route = std::numeric_limits<uint64_t>::max();
  uint64_t flow = std::numeric_limits<uint64_t>::max();

  if(ext && ext->param) {
    auto kernel = xocl::xocl(ext->kernel);

    auto& kernel_name = kernel->get_name_from_constructor();
    auto memidx = m_metadata.get_memidx_from_arg(kernel_name,ext->flags,conn);
    auto mems = m_metadata.get_mem_topology();

    if (!mems)
      throw xocl::error(CL_INVALID_OPERATION,"Mem topology section does not exist");

    if(memidx<0 || (memidx+1)>mems->m_count)
      throw xocl::error(CL_INVALID_OPERATION,"Mem topology section count is less than memidex");

    auto& mem = mems->m_mem_data[memidx];
    route = mem.route_id;
    flow = mem.flow_id;

    auto read = strstr((const char*)mem.m_tag, "_r");
    auto write = strstr((const char*)mem.m_tag, "_w");

    //TODO: Put an assert/throw if both read and write are not set, but currently that check will break as full m_tag not yet available

    if(read && !(flags & XCL_STREAM_WRITE_ONLY))
      throw xocl::error(CL_INVALID_OPERATION,
           "Connecting a kernel write only stream to non-user-read stream, argument " + ext->flags);

    if(write &&  !(flags & XCL_STREAM_READ_ONLY))
      throw xocl::error(CL_INVALID_OPERATION,
           "Connecting a kernel read stream to non-user-write stream, argument " + ext->flags);

    if(mem.m_type != MEM_STREAMING)
      throw xocl::error(CL_INVALID_OPERATION,
           "Connecting a streaming argument to non-streaming bank");

    xocl(kernel)->set_argument(ext->flags,sizeof(cl_mem),nullptr);
  }

  int rc = 0;
  if (flags & XCL_STREAM_WRITE_ONLY)  // kernel writes, user reads
    rc = m_xdevice->createReadStream(flags, attrs, route, flow, stream);
  else if (flags & XCL_STREAM_READ_ONLY) // kernel reads, user writes
    rc = m_xdevice->createWriteStream(flags, attrs, route, flow, stream);
  else
    throw xocl::error(CL_INVALID_OPERATION,"Unknown stream type specified");

  if(rc)
    throw xocl::error(CL_INVALID_OPERATION,"Create stream failed");
  return rc;
}

int
device::
close_stream(xrt_xocl::device::stream_handle stream, int connidx)
{
  assert(connidx!=-1);
  clear_connection(connidx);
  return m_xdevice->closeStream(stream);
}

ssize_t
device::
write_stream(xrt_xocl::device::stream_handle stream, const void* ptr, size_t size, xrt_xocl::device::stream_xfer_req* req)
{
  return m_xdevice->writeStream(stream, ptr, size, req);
}

ssize_t
device::
read_stream(xrt_xocl::device::stream_handle stream, void* ptr, size_t size, xrt_xocl::device::stream_xfer_req* req)
{
  return m_xdevice->readStream(stream, ptr, size, req);
}

xrt_xocl::device::stream_buf
device::
alloc_stream_buf(size_t size, xrt_xocl::device::stream_buf_handle* handle)
{
  return m_xdevice->allocStreamBuf(size,handle);
}

int
device::
free_stream_buf(xrt_xocl::device::stream_buf_handle handle)
{
  return m_xdevice->freeStreamBuf(handle);
}

int
device::
poll_streams(xrt_xocl::device::stream_xfer_completions* comps, int min, int max, int* actual, int timeout)
{
  return m_xdevice->pollStreams(comps, min,max,actual,timeout);
}

int
device::
poll_stream(xrt_xocl::device::stream_handle stream, xrt_xocl::device::stream_xfer_completions* comps, int min, int max, int* actual, int timeout)
{
  return m_xdevice->pollStream(stream, comps, min,max,actual,timeout);
}

int
device::
set_stream_opt(xrt_xocl::device::stream_handle stream, int type, uint32_t val)
{
  return m_xdevice->setStreamOpt(stream, type, val);
}

device::
device(platform* pltf, xrt_xocl::device* xdevice)
  : m_uid(uid_count++), m_platform(pltf), m_xdevice(xdevice)
{
  XOCL_DEBUG(std::cout,"xocl::device::device(",m_uid,")\n");
}

device::
device(device* parent, const compute_unit_vector_type& cus)
  : m_uid(uid_count++)
  , m_active(parent->m_active)
  , m_metadata(parent->m_metadata)
  , m_platform(parent->m_platform)
  , m_xdevice(parent->m_xdevice)
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
clear_cus()
{
  // Release CU context only on parent device
  if (!is_sub_device())
    for (auto& cu : get_cus())
      release_context(cu.get());
  m_computeunits.clear();
}

unsigned int
device::
lock()
{
  std::lock_guard<std::mutex> lk(m_mutex);
  // If already locked, return increment lock count
  if (m_locks)
    return ++m_locks;

  // sub-device should lock parent as well
  if (m_parent.get())
    m_parent->lock();

  // Open the underlying device if not sub device
  if (!m_parent.get())
    m_xdevice->open();
  
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

  // sub-device should unlock the parent
  if (m_parent.get())
    m_parent->unlock();

  // Close the underlying device
  if (!m_parent.get())
    m_xdevice->close();

  return m_locks; // 0
}

device::buffer_object_handle
device::
allocate_buffer_object(memory* mem, memidx_type memidx)
{
  if (memidx==-1)
    throw std::runtime_error("Unexpected error memidx == -1");

  if (mem->get_flags() & CL_MEM_REGISTER_MAP)
    throw std::runtime_error("Cannot allocate register map buffer on bank");

  // sub buffer
  if (auto parent = mem->get_sub_buffer_parent()) {
    // parent buffer should be allocated in bank selected by sub-buffer
    auto boh = parent->get_buffer_object(this, memidx);
    auto pmemidx = get_boh_memidx(boh);
    if (pmemidx.test(memidx)) {
      auto offset = mem->get_sub_buffer_offset();
      auto size = mem->get_size();
      return m_xdevice->alloc(boh,size,offset);
    }
    throw std::runtime_error("parent sub-buffer memory bank mismatch");
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

bool
device::
is_imported(const memory* mem) const
{
  auto boh = mem->get_buffer_object_or_null(this);
  return boh ? m_xdevice->is_imported(boh) : false;
}

device::buffer_object_handle
device::
allocate_buffer_object(memory* mem, xrt_xocl::device::memoryDomain domain, uint64_t memidx, void* user_ptr)
{
  return m_xdevice->alloc(mem->get_size(),domain,memidx,user_ptr);
}

uint64_t
device::
get_boh_addr(const buffer_object_handle& boh) const
{
  return m_xdevice->getDeviceAddr(boh);
}

device::memidx_bitmask_type
device::
get_boh_memidx(const buffer_object_handle& boh) const
{
  auto addr = get_boh_addr(boh);
  auto bset = m_metadata.mem_address_to_memidx(addr);
  if (bset.none() && is_sw_emulation())
    bset.set(0); // default bank in sw_emu

  return bset;
}

std::string
device::
get_boh_banktag(const buffer_object_handle& boh) const
{
  auto addr = get_boh_addr(boh);
  auto memidx = m_metadata.mem_address_to_first_memidx(addr);
  if (memidx == -1)
    return "Unknown";
  return m_metadata.memidx_to_banktag(memidx);
}

device::memidx_type
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

      // Select first common Group index if present prior to bank index.
      // Traverse from the higher order of the mask as groups comes in higher order
      for (int idx=mask.size() - 1; idx >= 0; --idx) {
        if (mask.test(idx)) {
          m_cu_memidx = idx;
          break;
        }
      }
    }
  }
  return m_cu_memidx;
}

device::buffer_object_handle
device::
import_buffer_object(const device* src_device, const buffer_object_handle& src_boh)
{
  // Consider moving to xrt_xocl::device

  // Export from exporting device (src_device)
  auto fd = src_device->get_xdevice()->getMemObjectFd(src_boh);

  // Import into this device
  size_t size=0;
  return get_xdevice()->getBufferFromFd(fd,size,1);
}

void*
device::
map_buffer(memory* buffer, cl_map_flags map_flags, size_t offset, size_t size, void* assert_result, bool nosync)
{
  buffer_object_handle boh;

  // If buffer is resident it must be refreshed unless CL_MAP_INVALIDATE_REGION
  // is specified in which case host will discard current content
  if (!nosync && !(map_flags & CL_MAP_WRITE_INVALIDATE_REGION) && buffer->is_resident(this) && !buffer->no_host_memory()) {
    boh = buffer->get_buffer_object_or_error(this);
    m_xdevice->sync(boh,size,offset,xrt_xocl::hal::device::direction::DEVICE2HOST,false);
  }

  if (!boh)
    boh = buffer->get_buffer_object(this);

  auto ubuf = buffer->get_host_ptr();
  if (!ubuf || !is_aligned_ptr(ubuf)) {
    // boh was created with it's own alloced host_ptr
    auto hbuf = m_xdevice->map(boh);
    m_xdevice->unmap(boh);
    assert(ubuf!=hbuf);
    if (ubuf && !nosync) {
      auto dst = static_cast<char*>(ubuf) + offset;
      auto src = static_cast<char*>(hbuf) + offset;
      memcpy(dst,src,size);
    }
    else if (!ubuf)
      ubuf = hbuf;
  }

  void* result = static_cast<char*>(ubuf) + offset;
  assert(!assert_result || result==assert_result);

  // If this buffer is being mapped for writing, then a following
  // unmap will have to sync the data to device, so record this.  We
  // will not enforce that map is followed by unmap, so two maps of
  // same buffer for write without corresponding unmaps will simply
  // override previous map value.  We do however keep track of the
  // map with largest size to subsume small maps into the largest.
  // That way largest chunk is synced to device if necessary.
  std::lock_guard<std::mutex> lk(m_mutex);
  auto& mapinfo = m_mapped[result];
  mapinfo.flags = map_flags;
  mapinfo.offset = offset;
  mapinfo.size = std::max(mapinfo.size,size);
  return result;
}

void
device::
unmap_buffer(memory* buffer, void* mapped_ptr)
{
  cl_map_flags flags = 0; // flags of mapped_ptr
  size_t offset = 0;      // offset of mapped_ptr wrt BO
  size_t size = 0;        // size of mapped_ptr
  {
    // There is no checking that map/unmap match.  Only one active
    // map of a mapped_ptr is maintained and is erased on first unmap
    std::lock_guard<std::mutex> lk(m_mutex);
    auto itr = m_mapped.find(mapped_ptr);
    if (itr!=m_mapped.end()) {
      flags = (*itr).second.flags;
      offset = (*itr).second.offset;
      size = (*itr).second.size;
      m_mapped.erase(itr);
    }
  }

  auto boh = buffer->get_buffer_object_or_error(this);

  // Sync data to boh if write flags, and sync to device if resident
  if (flags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION)) {
    if (auto ubuf = static_cast<char*>(buffer->get_host_ptr()))
      m_xdevice->write(boh,ubuf+offset,size,offset,false);
    if (buffer->is_resident(this) && !buffer->no_host_memory())
      m_xdevice->sync(boh,size,offset,xrt_xocl::hal::device::direction::HOST2DEVICE,false);
  }
}

void
device::
migrate_buffer(memory* buffer,cl_mem_migration_flags flags)
{
  if (buffer->no_host_memory())
    // shouldn't happen
    throw xocl::error(CL_INVALID_OPERATION,"buffer flags do not allow migrate_buffer");

  // Support clEnqueueMigrateMemObjects device->host
  if (flags & CL_MIGRATE_MEM_OBJECT_HOST) {
    buffer_resident_or_error(buffer,this);
    auto boh = buffer->get_buffer_object_or_error(this);
    m_xdevice->sync(boh,buffer->get_size(),0,xrt_xocl::hal::device::direction::DEVICE2HOST,false);
    sync_to_ubuf(buffer,0,buffer->get_size(),m_xdevice,boh);
    return;
  }

  // Host to device for kernel args and clEnqueueMigrateMemObjects
  // Get or create the buffer object on this device.
  buffer_object_handle boh = buffer->get_buffer_object(this);

  // Sync from host to device to make make buffer resident of this device
  sync_to_hbuf(buffer,0,buffer->get_size(),m_xdevice,boh);
  m_xdevice->sync(boh,buffer->get_size(), 0, xrt_xocl::hal::device::direction::HOST2DEVICE,false);
  // Now buffer is resident on this device and migrate is complete
  buffer->set_resident(this);
}

void
device::
write_buffer(memory* buffer, size_t offset, size_t size, const void* ptr)
{
  auto boh = buffer->get_buffer_object(this);

  // Write data to buffer object at offset
  m_xdevice->write(boh,ptr,size,offset,false);

  // Update ubuf if necessary
  sync_to_ubuf(buffer,offset,size,m_xdevice,boh);

  if (buffer->is_resident(this) && !buffer->no_host_memory())
    // Sync new written data to device at offset
    // HAL performs read/modify write if necesary
    m_xdevice->sync(boh,size,offset,xrt_xocl::hal::device::direction::HOST2DEVICE,false);
}

void
device::
read_buffer(memory* buffer, size_t offset, size_t size, void* ptr)
{
  auto boh = buffer->get_buffer_object(this);

  if (buffer->is_resident(this) && !buffer->no_host_memory())
    // Sync back from device at offset to buffer object
    // HAL performs skip/copy read if necesary
    m_xdevice->sync(boh,size,offset,xrt_xocl::hal::device::direction::DEVICE2HOST,false);

  // Read data from buffer object at offset
  m_xdevice->read(boh,ptr,size,offset,false);

  // Update ubuf if necessary
  sync_to_ubuf(buffer,offset,size,m_xdevice,boh);
}

void
device::
copy_buffer(memory* src_buffer, memory* dst_buffer, size_t src_offset, size_t dst_offset, size_t size, const cmd_type& cmd)
{
  // if m2m present then use xclCopyBO
  try {
    auto core_device = m_xdevice->get_core_device();
    auto m2m = xrt_core::device_query<xrt_core::query::m2m>(core_device);
    if (xrt_core::query::m2m::to_bool(m2m)) {
      auto cb = [this](memory* sbuf, memory* dbuf, size_t soff, size_t doff, size_t sz, const cmd_type& c) {
        c->start();
        auto sboh = sbuf->get_buffer_object(this);
        auto dboh = dbuf->get_buffer_object(this);
        m_xdevice->copy(dboh, sboh, sz, doff, soff);
        c->done();
      };
      m_xdevice->schedule(cb,xrt_xocl::device::queue_type::misc,src_buffer,dst_buffer,src_offset,dst_offset,size,cmd);
      // Driver fills dst buffer same as migrate_buffer does, hence dst buffer
      // is resident after KDMA is done even if host does explicitly migrate.
      dst_buffer->set_resident(this);
      return;
    }
  }
  catch (...) {
    // enable when m2m is the norm
    // cmd_copy_message(src_buffer, dst_buffer);
  }
  
  // Check if any of the buffers are imported
  bool imported = is_imported(src_buffer) || is_imported(dst_buffer);

  // Copy via driver if p2p or device has kdma
  if (!is_sw_emulation() && (imported || get_num_cdmas())) {
    auto cppkt = xrt_xocl::command_cast<ert_start_copybo_cmd*>(cmd);
    auto src_boh = src_buffer->get_buffer_object(this);
    auto dst_boh = dst_buffer->get_buffer_object(this);
    try {
      m_xdevice->fill_copy_pkt(dst_boh,src_boh,size,dst_offset,src_offset,cppkt);
      cmd->start();    // done() called by scheduler on success
      cmd->execute();  // throws on error
      XOCL_DEBUG(std::cout,"xocl::device::copy_buffer scheduled kdma copy\n");
      // Driver fills dst buffer same as migrate_buffer does, hence dst buffer
      // is resident after KDMA is done even if host does explicitly migrate.
      dst_buffer->set_resident(this);
      return;
    }
    catch (...) {
      host_copy_message(dst_buffer,src_buffer);
    }
  }

  // Copy via host of local buffers and no kdma and neither buffer is p2p (no shadow buffer in host)
  if (!imported && !src_buffer->no_host_memory() && !dst_buffer->no_host_memory()) {
    // non p2p BOs then copy through host
    auto cb = [this](memory* sbuf, memory* dbuf, size_t soff, size_t doff, size_t sz,const cmd_type& c) {
      try {
        c->start();
        char* hbuf_src = static_cast<char*>(map_buffer(sbuf,CL_MAP_READ,soff,sz,nullptr));
        char* hbuf_dst = static_cast<char*>(map_buffer(dbuf,CL_MAP_WRITE_INVALIDATE_REGION,doff,sz,nullptr));
        std::memcpy(hbuf_dst,hbuf_src,sz);
        unmap_buffer(sbuf,hbuf_src);
        unmap_buffer(dbuf,hbuf_dst);
        c->done();
      }
      catch (const std::exception& ex) {
        c->error(ex);
      }
    };
    XOCL_DEBUG(std::cout,"xocl::device::copy_buffer schedules host copy\n");
    m_xdevice->schedule(cb,xrt_xocl::device::queue_type::misc,src_buffer,dst_buffer,src_offset,dst_offset,size,cmd);
    return;
  }

  // Ideally all cases should be handled above regardless of flow
  // target and buffer type.  Need to enhance emulation drivers to
  // ensure this being the case.
  if (is_sw_emulation()) {
    // Old code path for p2p buffer xclEnqueueP2PCopy
    if (imported) {
      // old code path for p2p buffer
      cmd->start();
      copy_p2p_buffer(src_buffer,dst_buffer,src_offset,dst_offset,size);
      cmd->done();
      return;
    }
  }

  // Could not copy
  std::stringstream err;
  err << "Copying of buffers failed.\n";
  if (is_imported(src_buffer))
    err << "The src buffer is imported from another device\n";
  if (is_imported(dst_buffer))
    err << "The dst buffer is imported from another device\n";
  if (src_buffer->no_host_memory())
    err << "The src buffer is a device memory only buffer\n";
  if (dst_buffer->no_host_memory())
    err << "The dst buffer is a device memory only buffer\n";
  err << "The targeted device has " << get_num_cdmas() << " KDMA kernels\n";
  throw std::runtime_error(err.str());
}

void
device::
copy_p2p_buffer(memory* src_buffer, memory* dst_buffer, size_t src_offset, size_t dst_offset, size_t size)
{
  auto src_boh = src_buffer->get_buffer_object(this);
  auto dst_boh = dst_buffer->get_buffer_object(this);
  auto rv = m_xdevice->copy(dst_boh, src_boh, size, dst_offset, src_offset);
  if (rv.get<int>() == 0)
    return;

  // Could not copy
  std::stringstream err;
  err << "copy_p2p_buffer failed "
      << "src_buffer " << src_buffer->get_uid() << ") "
      << "dst_buffer(" << dst_buffer->get_uid() << ")";
  throw std::runtime_error(err.str());
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
  auto xdevice = device->get_xdevice();

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
  if (image->is_resident(this) && !image->no_host_memory()) {
    auto boh = image->get_buffer_object_or_error(this);
    get_xdevice()->sync(boh, image->get_size(), 0,xrt_xocl::hal::device::direction::HOST2DEVICE,false);
  }
}

void
device::
read_image(memory* image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch,void *ptr)
{
  // Sync back from device if image is resident
  if (image->is_resident(this) && !image->no_host_memory()) {
    auto boh = image->get_buffer_object_or_error(this);
    get_xdevice()->sync(boh,image->get_size(),0,xrt_xocl::hal::device::direction::DEVICE2HOST,false);
  }

  // Now read from image into ptr
  rw_image(this,image,origin,region,row_pitch,slice_pitch,static_cast<char*>(ptr),nullptr);
}

void
device::
read_register(memory* mem, size_t offset,void* ptr, size_t size)
{
  if (!(mem->get_flags() & CL_MEM_REGISTER_MAP))
    throw xocl::error(CL_INVALID_OPERATION,"read_register requires mem object with CL_MEM_REGISTER_MAP");
  get_xdevice()->read_register(offset,ptr,size);
}

void
device::
write_register(memory* mem, size_t offset,const void* ptr, size_t size)
{
  if (!(mem->get_flags() & CL_MEM_REGISTER_MAP))
    throw xocl::error(CL_INVALID_OPERATION,"write_register requires mem object with CL_MEM_REGISTER_MAP");
  get_xdevice()->write_register(offset,ptr,size);
}

void
device::
load_program(program* program)
{
  if (m_parent.get())
    throw xocl::error(CL_OUT_OF_RESOURCES,"cannot load program on sub device");

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_active)
    throw xocl::error(CL_OUT_OF_RESOURCES,"program already loaded on device");

  auto binary_data = program->get_xclbin_binary(this);
  auto binary_size = binary_data.second - binary_data.first;
  if (binary_size == 0)
    return;

  auto top = reinterpret_cast<const axlf*>(binary_data.first);

  // Kernel debug is enabled based on if there is debug_data in the
  // binary it does not have an xrt.ini attribute. If there is
  // debug_data then make sure xdp kernel debug is loaded
  if (::xclbin::get_axlf_section(top, axlf_section_kind::DEBUG_DATA))
  {
#ifdef _WIN32
    // Kernel debug not supported on Windows
#else
    xocl::debug::load_xdp_kernel_debug() ;
#endif
  }

  xocl::debug::reset(top);
  xocl::profile::reset(top);

  // programmming
  if (xrt_xocl::config::get_xclbin_programing()) {
    auto xbrv = m_xdevice->loadXclBin(top);
    if (xbrv.valid() && xbrv.get()){
      if(xbrv.get() == -EACCES)
        throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin. Invalid DNA");
      else if (xbrv.get() == -EBUSY)
        throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin. Device Busy, see dmesg for details");
      else if (xbrv.get() == -ETIMEDOUT)
        throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin. Timeout, see dmesg for details");
      else if (xbrv.get() == -ENOMEM)
        throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin. Out of Memory, see dmesg for details");
      else
        throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin.");
    }

    if (!xbrv.valid()) {
      throw xocl::error(CL_INVALID_PROGRAM,"Failed to load xclbin.");
    }
  }

  // Initialize meta data based on sections cached in core device
  // These sections were cached when the xclbin was loaded onto the device
  auto core_device = xrt_core::get_userpf_device(get_handle());
  m_metadata = xclbin(core_device.get(), program->get_xclbin_uuid(this));

  // Add compute units for each kernel in the program.
  // Note, that conformance mode renames the kernels in the xclbin
  // so iterating kernel names and looking up symbols from kernels
  // isn't possible, we *must* iterate symbols explicitly
  clear_cus();
  m_cu_memidx = -2;
  auto cu2addr = get_xclbin_cus(this);
  for (auto symbol : m_metadata.kernel_symbols()) {
    for (auto& inst : symbol->instances) {
      if (auto cu = compute_unit::create(symbol,inst,this,cu2addr))
        add_cu(std::move(cu));
    }
  }

  m_active = program;
  profile::add_to_active_devices(get_unique_name());

  // In order to use virtual CUs (KDMA) we must open a virtual context
  m_xdevice->acquire_cu_context(-1,true);

  init_scheduler(this);
}

void
device::
unload_program(const program* program)
{
  if (m_active == program) {
    clear_cus();
    m_active = nullptr;
    if (!m_parent.get())
      m_xdevice->release_cu_context(-1); // release virtual CU context
  }
}

bool
device::
acquire_context(const compute_unit* cu) const
{
  static bool shared = xrt_xocl::config::get_exclusive_cu_context() ? false : true;

  std::lock_guard<std::mutex> lk(m_mutex);
  if (cu->m_context_type != compute_unit::context_type::none)
    return true;

  if (!m_metadata)
    return false;

  m_xdevice->acquire_cu_context(m_metadata.uuid(),cu->get_index(),shared);
  XOCL_DEBUG(std::cout,"acquired ",shared?"shared":"exclusive"," context for cu(",cu->get_uid(),")\n");
  cu->set_context_type(shared);
  return true;
}

bool
device::
release_context(const compute_unit* cu) const
{
  if (cu->get_context_type() == compute_unit::context_type::none)
    return true;

  if (!m_metadata)
    return false;

  m_xdevice->release_cu_context(m_metadata.uuid(),cu->get_index());
  XOCL_DEBUG(std::cout,"released context for cu(",cu->get_uid(),")\n");
  cu->reset_context_type();
  return true;
}

size_t
device::
get_num_cdmas() const
{
  return xrt_xocl::config::get_cdma() ? m_xdevice->get_cdma_count() : 0;
}

xclbin
device::
get_xclbin() const
{
  return m_metadata;
}

const axlf*
device::
get_axlf() const
{
  if (!m_active)
    return nullptr;

  auto binary = m_active->get_xclbin_binary(this);
  return reinterpret_cast<const axlf*>(binary.first);
}

std::pair<const char*, size_t>
device::
get_axlf_section(axlf_section_kind kind) const
{
  if (auto core_device = xrt_core::get_userpf_device(get_handle()))
    return core_device->get_axlf_section(kind);
  return {nullptr, 0};
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
