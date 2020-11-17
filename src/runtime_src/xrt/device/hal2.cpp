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
#include "hal2.h"

#include "ert.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "core/common/scope_guard.h"
#include "core/common/system.h"
#include "core/common/thread.h"

#include <boost/format.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring> // for std::memcpy
#include <iostream>
#include <regex>
#include <string>

#ifdef _WIN32
# pragma warning( disable : 4267 4996 4244 4245 )
#endif

namespace {

static bool
is_emulation()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
}

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", msg);
}

inline void
send_exception_message(const std::string& msg)
{
  send_exception_message(msg.c_str());
}

}

namespace xrt_xocl { namespace hal2 {

device::
device(std::shared_ptr<operations> ops, unsigned int idx)
  : m_ops(std::move(ops)), m_idx(idx), m_handle(nullptr), m_devinfo{}
{
}

device::
~device()
{
  if (is_emulation())
    // xsim will not shutdown unless there is a guaranteed call to xclClose
    close();  
  
  for (auto& q : m_queue)
    q.stop();
  for (auto& t : m_workers)
    t.join();
}

bool
device::
open_nolock()
{
  if (m_handle)
    return false;

  m_handle=m_ops->mOpen(m_idx, nullptr, XCL_QUIET);

  if (!m_handle)
    throw std::runtime_error("Could not open device");

  return true;
}

bool
device::
open()
{
  std::lock_guard<std::mutex> lk(m_mutex);
  return open_nolock();
}

void
device::
close_nolock()
{
  if (m_handle) {
    m_ops->mClose(m_handle);
    m_handle=nullptr;
  }
}

void
device::
close()
{
  std::lock_guard<std::mutex> lk(m_mutex);
  close_nolock();
}

std::ostream&
device::
printDeviceInfo(std::ostream& ostr) const
{
  if (!m_handle)
    throw std::runtime_error("Can't print device info, device is not open");
  auto dinfo = get_device_info();
  ostr << "Name: " << dinfo->mName << "\n";
  ostr << "HAL v" << dinfo->mHALMajorVersion << "." << dinfo->mHALMinorVersion << "\n";
  ostr << "HAL vendor id: " << std::hex << dinfo->mVendorId << std::dec << "\n";
  ostr << "HAL device id: " << std::hex << dinfo->mDeviceId << std::dec << "\n";
  ostr << "HAL device v" << dinfo->mDeviceVersion << "\n";
  ostr << "HAL subsystem id: " << std::hex << dinfo->mSubsystemId << std::dec << "\n";
  ostr << "HAL subsystem vendor id: " << std::hex << dinfo->mSubsystemVendorId << std::dec << "\n";
  ostr << "HAL DDR size: " << std::hex << dinfo->mDDRSize << std::dec << "\n";
  ostr << "HAL Data alignment: " << dinfo->mDataAlignment << "\n";
  ostr << "HAL DDR free size: " << std::hex << dinfo->mDDRFreeSize << std::dec << "\n";
  ostr << "HAL Min transfer size: " << dinfo->mMinTransferSize << "\n";
  ostr << "HAL OnChip Temp: " << dinfo->mOnChipTemp << "\n";
  ostr << "HAL Fan Temp: " << dinfo->mFanTemp << "\n";
  ostr << "HAL Voltage: " << dinfo->mVInt << "\n";
  ostr << "HAL Current: " << dinfo->mCurrent << "\n";
  ostr << "HAL DDR count: " << dinfo->mDDRBankCount << "\n";
  ostr << "HAL OCL freq: " << dinfo->mOCLFrequency[0] << "\n";
  ostr << "HAL PCIe width: " << dinfo->mPCIeLinkWidth << "\n";
  ostr << "HAL PCIe speed: " << dinfo->mPCIeLinkSpeed << "\n";
  ostr << "HAL DMA threads: " << dinfo->mDMAThreads << "\n";
  return ostr;
}

void
device::
setup()
{
  std::lock_guard<std::mutex> lk(m_mutex);
  if (!m_workers.empty())
    return;

  open_nolock();

  auto dinfo = get_device_info_nolock();

  auto threads = config::get_dma_threads(); // number of bidirectional channels
  if (!threads)
    threads = dinfo->mDMAThreads;
  else
    threads = std::min(static_cast<unsigned short>(threads),dinfo->mDMAThreads);
  if (!threads) // Guard against drivers who do not set m_devinfo.mDMAThreads
    threads = 2;

  XRT_DEBUG(std::cout,"Creating ",2*threads," DMA worker threads\n");
  for (unsigned int i=0; i<threads; ++i) {
    // read and write queue workers
    m_workers.emplace_back(xrt_core::thread(task::worker2,std::ref(m_queue[static_cast<qtype>(hal::queue_type::read)]),"read"));
    m_workers.emplace_back(xrt_core::thread(task::worker2,std::ref(m_queue[static_cast<qtype>(hal::queue_type::write)]),"write"));
  }
  // single misc queue worker
  m_workers.emplace_back(xrt_core::thread(task::worker2,std::ref(m_queue[static_cast<qtype>(hal::queue_type::misc)]),"misc"));
}

device::BufferObject*
device::
getBufferObject(const BufferObjectHandle& boh) const
{
  BufferObject* bo = static_cast<BufferObject*>(boh.get());
  if (bo->owner != m_handle)
    throw std::runtime_error("bad buffer object");
  return bo;
}

device::ExecBufferObject*
device::
getExecBufferObject(const ExecBufferObjectHandle& boh) const
{
  ExecBufferObject* bo = static_cast<ExecBufferObject*>(boh.get());
  if (bo->owner != m_handle)
    throw std::runtime_error("bad exec buffer object");
  return bo;
}

hal2::device_info*
device::
get_device_info_nolock() const
{
  hal2::device_info* dinfo = m_devinfo.get_ptr();
  if (dinfo)
    return dinfo;

  // Gather device info, open device if necessary
  // This is a logically const operation
  auto dev = const_cast<device*>(this);
  dinfo = (m_devinfo = hal2::device_info()).get_ptr();

  // Scope guard for closing device again if opened
  auto at_exit = [] (device* d, bool close) { if (close) d->close_nolock(); };
  xrt_core::scope_guard<std::function<void()>> g(std::bind(at_exit, dev, dev->open_nolock()));

  std::memset(dinfo,0,sizeof(hal2::device_info));
  if (m_ops->mGetDeviceInfo(m_handle,dinfo))
    throw std::runtime_error("device info not available");

  return dinfo;
}

hal2::device_info*
device::
get_device_info() const
{
  std::lock_guard<std::mutex> lk(m_mutex);
  return get_device_info_nolock();
}

std::shared_ptr<xrt_core::device>
device::
get_core_device() const
{
  return xrt_core::get_userpf_device(m_handle);
}

bool
device::
is_nodma() const
{
  auto device = get_core_device();
  return device->is_nodma();
}

void
device::
acquire_cu_context(const uuid& uuid,size_t cuidx,bool shared)
{
  if (m_handle && m_ops->mOpenContext) {
    if (m_ops->mOpenContext(m_handle,uuid.get(),cuidx,shared))
      throw std::runtime_error(std::string("failed to acquire CU(")
                               + std::to_string(cuidx)
                               + ") context '"
                               + std::strerror(errno)
                               + "'");
  }
}

void
device::
release_cu_context(const uuid& uuid,size_t cuidx)
{
  if (m_handle && m_ops->mCloseContext) {
    if (m_ops->mCloseContext(m_handle,uuid.get(),cuidx))
      throw std::runtime_error(std::string("failed to release CU(")
                               + std::to_string(cuidx)
                               + ") context '"
                               + std::strerror(errno)
                               + "'");
  }
}

hal::operations_result<int>
device::
loadXclBin(const xclBin* xclbin)
{
  if (!m_ops->mLoadXclBin)
    return hal::operations_result<int>();

  hal::operations_result<int> ret = m_ops->mLoadXclBin(m_handle,xclbin);

  // refresh device info on successful load
  if (!ret.get()) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_devinfo = boost::none;
  }

  return ret;
}

ExecBufferObjectHandle
device::
allocExecBuffer(size_t sz)
{
  auto delBufferObject = [this](ExecBufferObjectHandle::element_type* ebo) {
    ExecBufferObject* bo = static_cast<ExecBufferObject*>(ebo);
    XRT_DEBUG(std::cout,"deleted exec buffer object\n");
    m_ops->mUnmapBO(m_handle, bo->handle, bo->data);
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  auto ubo = std::make_unique<ExecBufferObject>();
  ubo->handle = m_ops->mAllocBO(m_handle,sz, 0, XCL_BO_FLAGS_EXECBUF);  // xrt_mem.h
  if (ubo->handle == NULLBO)
    throw std::bad_alloc();

  ubo->size = sz;
  ubo->owner = m_handle;
  ubo->data = m_ops->mMapBO(m_handle,ubo->handle, true /* write */);
  if (ubo->data == (void*)(-1))
    throw std::runtime_error(std::string("map failed: ") + std::strerror(errno));
  return ExecBufferObjectHandle(ubo.release(),delBufferObject);
}

BufferObjectHandle
device::
alloc(size_t sz)
{
  auto delBufferObject = [this](BufferObjectHandle::element_type* vbo) {
    BufferObject* bo = static_cast<BufferObject*>(vbo);
    XRT_DEBUGF("deleted buffer object device address(%p,%d)\n",bo->deviceAddr,bo->size);
    m_ops->mUnmapBO(m_handle, bo->handle, bo->hostAddr);
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  uint64_t flags = 0xFFFFFF; //TODO: check default, any bank.
  auto ubo = std::make_unique<BufferObject>();
  ubo->handle = m_ops->mAllocBO(m_handle, sz, 0, flags);
  if (ubo->handle == NULLBO)
    throw std::bad_alloc();

  ubo->size = sz;
  ubo->owner = m_handle;
  ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  ubo->hostAddr = m_ops->mMapBO(m_handle, ubo->handle, true /*write*/);

  XRT_DEBUGF("allocated buffer object device address(%p,%d)\n",ubo->deviceAddr,ubo->size);
  return BufferObjectHandle(ubo.release(), delBufferObject);
}

BufferObjectHandle
device::
alloc(size_t sz,void* userptr)
{
  auto delBufferObject = [this](BufferObjectHandle::element_type* vbo) {
    BufferObject* bo = static_cast<BufferObject*>(vbo);
    XRT_DEBUGF("deleted buffer object device address(%p,%d)\n",bo->deviceAddr,bo->size);
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  uint64_t flags = 0xFFFFFF; //TODO:check default
  auto ubo = std::make_unique<BufferObject>();
  ubo->handle = m_ops->mAllocUserPtrBO(m_handle, userptr, sz, flags);
  if (ubo->handle == NULLBO)
    throw std::bad_alloc();

  ubo->hostAddr = userptr;
  ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  ubo->size = sz;
  ubo->owner = m_handle;

  XRT_DEBUGF("allocated buffer object device address(%p,%d)\n",ubo->deviceAddr,ubo->size);
  return BufferObjectHandle(ubo.release(), delBufferObject);
}

BufferObjectHandle
device::
alloc(size_t sz, Domain domain, uint64_t memory_index, void* userptr)
{
  if (domain == Domain::XRT_DEVICE_RAM && is_nodma())
    return alloc_nodma(sz, domain, memory_index, userptr);
  
  const bool mmapRequired = (userptr == nullptr);
  auto delBufferObject = [mmapRequired, this](BufferObjectHandle::element_type* vbo) {
    BufferObject* bo = static_cast<BufferObject*>(vbo);
    XRT_DEBUGF("deleted buffer object device address(%p,%d)\n",bo->deviceAddr,bo->size);
    if (mmapRequired)
      m_ops->mUnmapBO(m_handle, bo->handle, bo->hostAddr);
    m_ops->mFreeBO(m_handle, bo->handle);
    if (bo->nodma)
      m_ops->mFreeBO(m_handle, bo->nodma_host_handle);
    delete bo;
  };

  auto ubo = std::make_unique<BufferObject>();

  if (domain==Domain::XRT_DEVICE_PREALLOCATED_BRAM) {
    ubo->deviceAddr = memory_index;
    ubo->hostAddr = nullptr;
  }
  else {
    uint64_t flags = memory_index;
    if(domain==Domain::XRT_DEVICE_ONLY_MEM_P2P)
      flags |= XCL_BO_FLAGS_P2P;
    else if (domain == Domain::XRT_DEVICE_ONLY_MEM)
      flags |= XCL_BO_FLAGS_DEV_ONLY;
    else if (domain == Domain::XRT_HOST_ONLY_MEM)
      flags |= XCL_BO_FLAGS_HOST_ONLY;
    else
      flags |= XCL_BO_FLAGS_CACHEABLE;

    if (userptr)
      ubo->handle = m_ops->mAllocUserPtrBO(m_handle, userptr, sz, flags);
    else
      ubo->handle = m_ops->mAllocBO(m_handle, sz, 0, flags);

    if (ubo->handle == NULLBO)
      throw std::bad_alloc();

    if (userptr)
      ubo->hostAddr = userptr;
    else
      ubo->hostAddr = m_ops->mMapBO(m_handle, ubo->handle, true /*write*/);

    ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  }
  ubo->size = sz;
  ubo->owner = m_handle;

  XRT_DEBUGF("allocated buffer object device address(%p,%d)\n",ubo->deviceAddr,ubo->size);
  return BufferObjectHandle(ubo.release(), delBufferObject);
}

BufferObjectHandle
device::
alloc_nodma(size_t sz, Domain domain, uint64_t memory_index, void* userptr)
{
  if (userptr)
      throw std::bad_alloc();

  if (domain == Domain::XRT_DEVICE_RAM) {
    // Allocate a buffer object with separate host side BO and device side BO

    // Device only
    auto boh = alloc(sz, Domain::XRT_DEVICE_ONLY_MEM, memory_index, nullptr);

    // Host only
    auto bo = getBufferObject(boh);
    bo->nodma_host_handle = m_ops->mAllocBO(m_handle, sz, 0, XCL_BO_FLAGS_HOST_ONLY);
    if (bo->nodma_host_handle == NULLBO) {
      auto fmt = boost::format("Failed to allocate host memory buffer, make sure host bank is enabled "
                               "(see xbutil host_mem --enable ...)");
      ::send_exception_message(fmt.str());
      throw std::bad_alloc();
    }

    // Map the host only BO and use this as host addr.
    bo->hostAddr = m_ops->mMapBO(m_handle, bo->nodma_host_handle, true /*write*/);
    bo->nodma = true;

    XRT_DEBUGF("allocated nodma buffer object\n");
    return boh;
  }

  // Else just regular BO 
  return alloc(sz, domain, memory_index, nullptr);
  
}

BufferObjectHandle
device::
alloc(const BufferObjectHandle& boh, size_t sz, size_t offset)
{
  auto delBufferObject = [this](BufferObjectHandle::element_type* vbo) {
    BufferObject* bo = static_cast<BufferObject*>(vbo);
    XRT_DEBUG(std::cout,"deleted offset buffer object device address(",bo->deviceAddr,",",bo->size,")\n");
    delete bo;
  };

  BufferObject* bo = getBufferObject(boh);

  auto ubo = std::make_unique<BufferObject>();
  ubo->handle = bo->handle;
  ubo->deviceAddr = bo->deviceAddr+offset;
  ubo->hostAddr = static_cast<char*>(bo->hostAddr)+offset;
  ubo->size = sz;
  ubo->offset = offset;
  ubo->flags = bo->flags;
  ubo->owner = bo->owner;
  ubo->parent = boh;  // keep parent boh reference
  ubo->nodma = bo->nodma;
  ubo->nodma_host_handle = bo->nodma_host_handle;

  // Verify alignment based on hardware requirement
  auto alignment = getAlignment();
  if (reinterpret_cast<uintptr_t>(bo->hostAddr) % alignment || bo->deviceAddr % alignment)
    throw std::bad_alloc();

  XRT_DEBUGF("allocated buffer object device address(%p,%d)\n",ubo->deviceAddr,ubo->size);
  return BufferObjectHandle(ubo.release(), delBufferObject);
}

void*
device::
alloc_svm(size_t sz)
{
  auto boh = alloc(sz);
  auto bo = getBufferObject(boh);
  emplaceSVMBufferObjectMap(boh, bo->hostAddr);
  return bo->hostAddr;
}

void
device::
free(const BufferObjectHandle& boh)
{
  BufferObject* bo = getBufferObject(boh);
  m_ops->mFreeBO(m_handle, bo->handle);
}

void
device::
free_svm(void* svm_ptr)
{
  auto boh = svm_bo_lookup(svm_ptr);
  auto bo = getBufferObject(boh);
  eraseSVMBufferObjectMap(bo->hostAddr);
  m_ops->mFreeBO(m_handle, bo->handle);
}

event
device::
write(const BufferObjectHandle& boh, const void* src, size_t sz, size_t offset, bool async)
{
  BufferObject* bo = getBufferObject(boh);

  char *hostAddr = static_cast<char*>(bo->hostAddr) + offset;
  return async
    ? event(addTaskF(std::memcpy,hal::queue_type::misc,hostAddr, src, sz))
    : event(typed_event<void *>(std::memcpy(hostAddr, src, sz)));
}

event
device::
read(const BufferObjectHandle& boh, void* dst, size_t sz, size_t offset, bool async)
{
  BufferObject* bo = getBufferObject(boh);
  char *hostAddr = static_cast<char*>(bo->hostAddr) + offset;
  return async
    ? event(addTaskF(std::memcpy,hal::queue_type::misc,dst,hostAddr,sz))
    : event(typed_event<void *>(std::memcpy(dst, hostAddr, sz)));
}

event
device::
sync(const BufferObjectHandle& boh, size_t sz, size_t offset, direction dir1, bool async)
{
  xclBOSyncDirection dir = XCL_BO_SYNC_BO_TO_DEVICE;
  if(dir1 == direction::DEVICE2HOST)
    dir = XCL_BO_SYNC_BO_FROM_DEVICE;

  BufferObject* bo = getBufferObject(boh);

  // A sub buffer shares the BO handle with the parent buffer.  When
  // syncing a sub buffer, the offset of the sub buffer relative to
  // the parent buffer must be included.
  auto offs = offset + bo->offset; 

  if (bo->nodma) {
    // sync is M2M copy between host and device handle
    if (dir == XCL_BO_SYNC_BO_TO_DEVICE) {
      if (m_ops->mCopyBO(m_handle, bo->handle, bo->nodma_host_handle, sz, offs, offs))
	throw std::runtime_error("failed to sync nodma buffer to device, "
				 "is the buffer 64byte aligned?  Check dmesg for errors.");
      return typed_event<int>(0);
    }
    else {
      if (m_ops->mCopyBO(m_handle, bo->nodma_host_handle, bo->handle, sz, offs, offs))
	throw std::runtime_error("failed to sync nodma buffer to host, "
				 "is the buffer 64byte aligned? Check dmesg for errors.");
      return typed_event<int>(0);
    }
  }

  if (async) {
    auto qt = (dir==XCL_BO_SYNC_BO_FROM_DEVICE) ? hal::queue_type::read : hal::queue_type::write;
    return event(addTaskF(m_ops->mSyncBO,qt,m_handle,bo->handle,dir,sz,offs));
  }
  return event(typed_event<int>(m_ops->mSyncBO(m_handle, bo->handle, dir, sz, offs)));
}

event
device::
copy(const BufferObjectHandle& dst_boh, const BufferObjectHandle& src_boh, size_t sz, size_t dst_offset, size_t src_offset)
{
  BufferObject* dst_bo = getBufferObject(dst_boh);
  BufferObject* src_bo = getBufferObject(src_boh);
  return event(typed_event<int>(m_ops->mCopyBO(m_handle, dst_bo->handle, src_bo->handle, sz, dst_offset, src_offset)));
}

void
device::
fill_copy_pkt(const BufferObjectHandle& dst_boh, const BufferObjectHandle& src_boh
              ,size_t sz, size_t dst_offset, size_t src_offset, ert_start_copybo_cmd* pkt)
{
#ifndef _WIN32
  BufferObject* dst_bo = getBufferObject(dst_boh);
  BufferObject* src_bo = getBufferObject(src_boh);
  ert_fill_copybo_cmd(pkt,src_bo->handle,dst_bo->handle,src_offset,dst_offset,sz);
#else
  throw std::runtime_error("ert_fill_copybo_cmd not implemented on windows");
#endif

  return;
}

size_t
device::
read_register(size_t offset, void* buffer, size_t size)
{
  return m_ops->mRead(m_handle, XCL_ADDR_KERNEL_CTRL, offset, buffer, size);
}

size_t
device::
write_register(size_t offset, const void* buffer, size_t size)
{
  return m_ops->mWrite(m_handle, XCL_ADDR_KERNEL_CTRL, offset, buffer, size);
}

void*
device::
map(const BufferObjectHandle& boh)
{
  auto bo = getBufferObject(boh);
  return bo->hostAddr;
}

void
device::
unmap(const BufferObjectHandle& boh)
{
/*
 * Any BO allocated through xrt_xocl::hal2 is mapped by default and cannot be munmap'ed.
 * The unmapping happens as part of the buffer object handle going out of scope.
 * xrt_xocl::device::map() simply returns the already nmap'ed host pointer contained within the opaque buffer object handle.
 * So,xrt_xocl::device::unmap is provided for symmetry but is a no-op.
 */
}

void*
device::
map(const ExecBufferObjectHandle& boh)
{
  auto bo = getExecBufferObject(boh);
  return bo->data;
}

void
device::
unmap(const ExecBufferObjectHandle& boh)
{
/*
 * Any BO allocated through xrt_xocl::hal2 is mapped by default and cannot be munmap'ed.
 * The unmapping happens as part of the buffer object handle going out of scope.
 * xrt_xocl::device::map() simply returns the already nmap'ed host pointer contained within the opaque buffer object handle.
 * So,xrt_xocl::device::unmap is provided for symmetry but is a no-op.
 */
}

int
device::
exec_buf(const ExecBufferObjectHandle& boh)
{
  auto bo = getExecBufferObject(boh);
  if (m_ops->mExecBuf(m_handle,bo->handle))
    throw std::runtime_error(std::string("failed to launch exec buffer '") + std::strerror(errno) + "'");
  return 0;
}

int
device::
exec_wait(int timeout_ms) const
{
  auto retval = m_ops->mExecWait(m_handle,timeout_ms);
  if (retval==-1) {
    // We should not treat interrupted syscall as an error
    if (errno == EINTR)
      retval = 0;
    else
      throw std::runtime_error(std::string("exec wait failed '") + std::strerror(errno) + "'");
  }
  return retval;
}

BufferObjectHandle
device::
import(const BufferObjectHandle& boh)
{
  if(!m_ops->mImportBO) {
    throw std::bad_alloc();
  }
  assert(0);

  BufferObject* bo = getBufferObject(boh);

  auto ubo = std::make_unique<BufferObject>();
  ubo->hostAddr = bo->hostAddr;
  ubo->size = bo->size;
  ubo->owner = m_handle;
  // Point to the parent exported bo; if the parent is itself an imported
  // bo point to its parent
  // Note that the max hierarchy depth is not more than 1
  ubo->parent = bo->parent ? bo->parent : boh;
  return BufferObjectHandle(ubo.release());
}


uint64_t
device::
getDeviceAddr(const BufferObjectHandle& boh)
{
  BufferObject* bo = getBufferObject(boh);
  return bo->deviceAddr;
}

bool
device::
is_imported(const BufferObjectHandle& boh) const
{
  auto bo = getBufferObject(boh);
  return bo->imported;
}

int
device::
getMemObjectFd(const BufferObjectHandle& boh)
{
  if (!m_ops->mExportBO)
    throw std::runtime_error("ExportBO function not found in FPGA driver. Please install latest driver");
  return m_ops->mExportBO(m_handle, getBufferObject(boh)->handle);
}

BufferObjectHandle
device::
getBufferFromFd(const int fd, size_t& size, unsigned flags)
{
  auto delBufferObject = [this](BufferObjectHandle::element_type* vbo) {
    BufferObject* bo = static_cast<BufferObject*>(vbo);
    XRT_DEBUGF("deleted buffer object device address(%p,%d)\n",bo->deviceAddr,bo->size);
    m_ops->mUnmapBO(m_handle, bo->handle, bo->hostAddr);
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  auto ubo = std::make_unique<BufferObject>();

  if (!m_ops->mImportBO)
    throw std::runtime_error("ImportBO function not found in FPGA driver. Please install latest driver");

  ubo->handle = m_ops->mImportBO(m_handle, fd, flags);
  if (ubo->handle == NULLBO)
    throw std::runtime_error("getBufferFromFd-Create XRT-BO: BOH handle is invalid");

  ubo->size = m_ops->mGetBOSize(m_handle, ubo->handle);
  size = ubo->size;
  ubo->owner = m_handle;
  ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  ubo->hostAddr = m_ops->mMapBO(m_handle, ubo->handle, true /*write*/);
  ubo->imported = true;

  return BufferObjectHandle(ubo.release(), delBufferObject);
}

void
device::
emplaceSVMBufferObjectMap(const BufferObjectHandle& boh, void* ptr)
{
  std::lock_guard<std::mutex> lk(m_mutex);
  auto itr = m_svmbomap.find(ptr);
  if (itr == m_svmbomap.end())
    m_svmbomap[ptr] = boh;
}

void
device::
eraseSVMBufferObjectMap(void* ptr)
{
  std::lock_guard<std::mutex> lk(m_mutex);
  auto itr = m_svmbomap.find(ptr);
  if (itr != m_svmbomap.end())
    m_svmbomap.erase(itr);
}

BufferObjectHandle
device::
svm_bo_lookup(void* ptr)
{
  std::lock_guard<std::mutex> lk(m_mutex);
  auto itr = m_svmbomap.find(ptr);
  if (itr != m_svmbomap.end())
    return (*itr).second;
  else
    throw std::runtime_error("svm_bo_lookup: The SVM pointer is invalid.");
}

//Stream
int
device::
createWriteStream(hal::StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream)
{
  xclQueueContext ctx;
  ctx.flags = flags;
  ctx.type = attr;
  ctx.route = route;
  ctx.flow = flow;
  return m_ops->mCreateWriteQueue(m_handle,&ctx,stream);
}

int
device::
createReadStream(hal::StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream)
{
  xclQueueContext ctx;
  ctx.flags = flags;
  ctx.type = attr;
  ctx.route = route;
  ctx.flow = flow;
  return m_ops->mCreateReadQueue(m_handle,&ctx,stream);
}

int
device::
closeStream(hal::StreamHandle stream)
{
  return m_ops->mDestroyQueue(m_handle,stream);
}

hal::StreamBuf
device::
allocStreamBuf(size_t size, hal::StreamBufHandle *buf)
{
  return m_ops->mAllocQDMABuf(m_handle,size,buf);
}

int
device::
freeStreamBuf(hal::StreamBufHandle buf)
{
  return m_ops->mFreeQDMABuf(m_handle,buf);
}

ssize_t
device::
writeStream(hal::StreamHandle stream, const void* ptr, size_t size, hal::StreamXferReq* request)
{
  //TODO:
  xclQueueRequest req;
  xclReqBuffer buffer;

  buffer.va = (uint64_t)ptr;
  buffer.len = size;
  buffer.buf_hdl = 0;

  req.bufs = &buffer;
  req.buf_num = 1;
//  req,op_code = XCL_QUEUE_WRITE;
//  req.bufs.buf = const_cast<char*>(ptr);
//  req.bufs.len = size;
  req.flag = request->flags;

  req.timeout = request->timeout;
  req.priv_data = request->priv_data;

  return m_ops->mWriteQueue(m_handle,stream,&req);
}

ssize_t
device::
readStream(hal::StreamHandle stream, void* ptr, size_t size, hal::StreamXferReq* request)
{
  xclQueueRequest req;
  xclReqBuffer buffer;

  buffer.va = (uint64_t)ptr;
  buffer.len = size;
  buffer.buf_hdl = 0;

  req.bufs = &buffer;
  req.buf_num = 1;
//  req,op_code = XCL_QUEUE_READ;
  req.flag = request->flags;

  req.timeout = request->timeout;
  req.priv_data = request->priv_data;
  return m_ops->mReadQueue(m_handle,stream,&req);
}

int
device::
pollStreams(hal::StreamXferCompletions* comps, int min, int max, int* actual, int timeout)
{
  xclReqCompletion* req = reinterpret_cast<xclReqCompletion*>(comps);
  return m_ops->mPollQueues(m_handle,min,max,req,actual,timeout);
}

int
device::
pollStream(hal::StreamHandle stream, hal::StreamXferCompletions* comps, int min, int max, int* actual, int timeout)
{
  xclReqCompletion* req = reinterpret_cast<xclReqCompletion*>(comps);
  return m_ops->mPollQueue(m_handle,stream,min,max,req,actual,timeout);
}

int
device::
setStreamOpt(hal::StreamHandle stream, int type, uint32_t val)
{
  return m_ops->mSetQueueOpt(m_handle,stream,type,val);
}

void
createDevices(hal::device_list& devices,
              const std::string& dll, void* driverHandle, unsigned int deviceCount)
{
  auto halops = std::make_shared<operations>(dll,driverHandle,deviceCount);
  for (unsigned int idx=0; idx<deviceCount; ++idx)
    devices.emplace_back(std::make_unique<xrt_xocl::hal2::device>(halops,idx));
}


}} // hal2,xrt
