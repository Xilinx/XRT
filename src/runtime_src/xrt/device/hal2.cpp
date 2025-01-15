// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2020 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#include "hal2.h"

#include "core/common/api/bo.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/common/scope_guard.h"
#include "core/common/system.h"
#include "core/common/thread.h"
#include "core/include/xrt/detail/ert.h"

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
  xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
}

inline void
send_exception_message(const std::string& msg)
{
  send_exception_message(msg.c_str());
}

}

namespace xrt_xocl { namespace hal2 {

device::
device(unsigned int idx, std::string dll)
  : m_idx(idx)
  , m_filename{std::move(dll)}
  , m_devinfo{}
{}

device::
~device()
{
  if (is_emulation()) {
    try {
      // xsim will not shutdown unless there is a guaranteed call to xclClose
      close();
    }
    catch (...) {
      // dtor cannot throw, and  close() can throw
    }
  }

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

  m_handle = xrt::device{m_idx};

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
  if (m_handle)
    m_handle.reset();
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

device::ExecBufferObject*
device::
getExecBufferObject(const execbuffer_object_handle& boh) const
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
  get_core_device()->get_device_info(dinfo);
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
  if (auto cdev = m_handle.get_handle())
    return cdev;

  // xrt::device has not been created yet
  throw std::runtime_error("Internal Error : device has not been opened\n");
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
  try {
    get_core_device()->open_context(uuid, cuidx, shared);
  }
  catch (const std::exception&) {
    throw;
  }
  catch (...) {
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
  try {
    get_core_device()->close_context(uuid, cuidx);
  }
  catch (const std::exception&) {
    throw;
  }
  catch (...) {
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
  m_handle.load_xclbin(xclbin);

  // refresh device info on successful load
  std::lock_guard<std::mutex> lk(m_mutex);
  m_devinfo = boost::none;

  return 0;
}

execbuffer_object_handle
device::
allocExecBuffer(size_t sz)
{
  auto delBufferObject = [this](execbuffer_object_handle::element_type* ebo) {
    ExecBufferObject* bo = static_cast<ExecBufferObject*>(ebo);
    XRT_DEBUG(std::cout,"deleted exec buffer object\n");
    bo->handle->unmap(bo->data);
    delete bo;
  };

  auto ubo = std::make_unique<ExecBufferObject>();
  try {
    ubo->handle = get_core_device()->alloc_bo(sz, XCL_BO_FLAGS_EXECBUF);
  }
  catch (const std::exception&) {
    throw;
  }
  catch (...) {
    throw std::bad_alloc();
  }

  ubo->size = sz;
  ubo->owner = m_handle;
  ubo->data = ubo->handle->map(xrt_core::buffer_handle::map_type::write);
  if (!ubo->data || ubo->data == (void*)(-1))
    throw std::runtime_error(std::string("map failed: ") + std::strerror(errno));
  return execbuffer_object_handle(ubo.release(),delBufferObject);
}

buffer_object_handle
device::
alloc(size_t sz, Domain domain, uint64_t memory_index, void* userptr)
{
  xrt::bo::flags flags = xrt::bo::flags::cacheable;
  if(domain==Domain::XRT_DEVICE_ONLY_MEM_P2P)
    flags = xrt::bo::flags::p2p;
  else if (domain == Domain::XRT_DEVICE_ONLY_MEM)
    flags = xrt::bo::flags::device_only;
  else if (domain == Domain::XRT_HOST_ONLY_MEM)
    flags = xrt::bo::flags::host_only;
  else if (domain == Domain::XRT_DEVICE_RAM && is_nodma())
    flags = xrt::bo::flags::normal;

  auto bo = userptr
    ? xrt::bo{m_handle, userptr, sz, flags, static_cast<xrt::memory_group>(memory_index)}
    : xrt::bo{m_handle, sz, flags, static_cast<xrt::memory_group>(memory_index)};

  XRT_DEBUGF("allocated buffer object device address(%p,%d)\n",bo.address(), bo.size());

  return bo;
}

buffer_object_handle
device::
alloc(const buffer_object_handle& boh, size_t sz, size_t offset)
{
  return xrt::bo{boh, sz, offset};
}

void*
device::
alloc_svm(size_t sz)
{
  auto bo = xrt::bo{m_handle, sz, xrt::bo::flags::svm, 0};
  auto host_addr = bo.map();

  std::lock_guard<std::mutex> lk(m_mutex);
  m_svmbomap[host_addr] = std::move(bo);

  return host_addr;
}

void
device::
free_svm(void* svm_ptr)
{
  std::lock_guard<std::mutex> lk(m_mutex);
  auto itr = m_svmbomap.find(svm_ptr);
  if (itr != m_svmbomap.end())
    m_svmbomap.erase(itr);
  else
    throw std::runtime_error("svm_bo_lookup: The SVM pointer is invalid.");
}

event
device::
write(const buffer_object_handle& boh, const void* src, size_t sz, size_t offset, bool async)
{
  const_cast<buffer_object_handle&>(boh).write(src, sz, offset);
  return event(typed_event<int>(0));
}

event
device::
read(const buffer_object_handle& boh, void* dst, size_t sz, size_t offset, bool async)
{
  const_cast<buffer_object_handle&>(boh).read(dst, sz, offset);
  return event(typed_event<int>(0));
}

event
device::
sync(const buffer_object_handle& boh, size_t sz, size_t offset, direction dir1, bool async)
{
  auto dir = (dir1 == direction::HOST2DEVICE) ? XCL_BO_SYNC_BO_TO_DEVICE : XCL_BO_SYNC_BO_FROM_DEVICE;
  const_cast<buffer_object_handle&>(boh).sync(dir, sz, offset);
  return event(typed_event<int>(0));
}

event
device::
copy(const buffer_object_handle& dst_boh, const buffer_object_handle& src_boh, size_t sz, size_t dst_offset, size_t src_offset)
{
  auto& dst = const_cast<buffer_object_handle&>(dst_boh);
  dst.copy(src_boh, sz, src_offset, dst_offset);
  return event(typed_event<int>(0));
}

size_t
device::
read_register(size_t offset, void* buffer, size_t size)
{
  get_core_device()->xread(XCL_ADDR_KERNEL_CTRL, offset, buffer, size);
  return size;
}

size_t
device::
write_register(size_t offset, const void* buffer, size_t size)
{
  get_core_device()->xwrite(XCL_ADDR_KERNEL_CTRL, offset, buffer, size);
  return size;
}

void*
device::
map(const buffer_object_handle& boh)
{
  auto& bo = const_cast<buffer_object_handle&>(boh);
  return bo.map();
}

void
device::
unmap(const buffer_object_handle& boh)
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
map(const execbuffer_object_handle& boh)
{
  auto bo = getExecBufferObject(boh);
  return bo->data;
}

void
device::
unmap(const execbuffer_object_handle& boh)
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
exec_buf(const execbuffer_object_handle& boh)
{
  auto bo = getExecBufferObject(boh);
  try {
    get_core_device()->exec_buf(bo->handle.get());
    return 0;
  }
  catch (const std::exception&) {
    throw;
  }
  catch (...) {
    throw std::runtime_error(std::string("failed to launch exec buffer '") + std::strerror(errno) + "'");
  }
}

int
device::
exec_wait(int timeout_ms) const
{
  auto retval = get_core_device()->exec_wait(timeout_ms);
  if (retval==-1) {
    // We should not treat interrupted syscall as an error
    if (errno == EINTR)
      retval = 0;
    else
      throw std::runtime_error(std::string("exec wait failed '") + std::strerror(errno) + "'");
  }
  return retval;
}

uint64_t
device::
getDeviceAddr(const buffer_object_handle& boh)
{
  return boh.address();
}

bool
device::
is_imported(const buffer_object_handle& boh) const
{
  return xrt_core::bo::is_imported(boh);
}

int
device::
getMemObjectFd(const buffer_object_handle& boh)
{
#ifndef _WIN32
  auto& bo = const_cast<buffer_object_handle&>(boh);
  return bo.export_buffer();
#else
  throw std::runtime_error("ExportBO function not supported on windows");
#endif
}

buffer_object_handle
device::
getBufferFromFd(int fd, size_t& size, unsigned flags)
{
#ifndef _WIN32
  auto export_handle = static_cast<xclBufferExportHandle>(fd);
  xrt::bo bo{m_handle, export_handle};
  size = bo.size();
  return bo;
#else
  throw std::runtime_error("ImportBO function not supported on windows");
#endif
}

void
createDevices(hal::device_list& devices,
              const std::string& dll, unsigned int deviceCount)
{
  for (unsigned int idx=0; idx<deviceCount; ++idx)
    devices.emplace_back(std::make_unique<xrt_xocl::hal2::device>(idx, dll));
}


}} // hal2,xrt
