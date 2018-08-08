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

#include "hal2.h"
#include "xrt/util/memory.h"
#include "xrt/util/thread.h"

#include <cstring> // for std::memcpy
#include <iostream>
#include <sys/mman.h> // for POSIX munmap

namespace xrt { namespace hal2 {

device::
device(std::shared_ptr<operations> ops, unsigned int idx)
  : m_ops(std::move(ops)), m_idx(idx), m_handle(nullptr), m_devinfo{}
{
}

device::
~device()
{
  close();
  for (auto& q : m_queue)
    q.stop();
  for (auto& t : m_workers)
    t.join();
}

std::ostream&
device::
printDeviceInfo(std::ostream& ostr) const
{
  if (!m_handle)
    throw std::runtime_error("Can't print device info, device is not open");
  ostr << "Name: " << m_devinfo.mName << "\n";
  ostr << "HAL v" << m_devinfo.mHALMajorVersion << "." << m_devinfo.mHALMinorVersion << "\n";
  ostr << "HAL vendor id: " << std::hex << m_devinfo.mVendorId << std::dec << "\n";
  ostr << "HAL device id: " << std::hex << m_devinfo.mDeviceId << std::dec << "\n";
  ostr << "HAL device v" << m_devinfo.mDeviceVersion << "\n";
  ostr << "HAL subsystem id: " << std::hex << m_devinfo.mSubsystemId << std::dec << "\n";
  ostr << "HAL subsystem vendor id: " << std::hex << m_devinfo.mSubsystemVendorId << std::dec << "\n";
  ostr << "HAL DDR size: " << std::hex << m_devinfo.mDDRSize << std::dec << "\n";
  ostr << "HAL Data alignment: " << m_devinfo.mDataAlignment << "\n";
  ostr << "HAL DDR free size: " << std::hex << m_devinfo.mDDRFreeSize << std::dec << "\n";
  ostr << "HAL Min transfer size: " << m_devinfo.mMinTransferSize << "\n";
  ostr << "HAL OnChip Temp: " << m_devinfo.mOnChipTemp << "\n";
  ostr << "HAL Fan Temp: " << m_devinfo.mFanTemp << "\n";
  ostr << "HAL Voltage: " << m_devinfo.mVInt << "\n";
  ostr << "HAL Current: " << m_devinfo.mCurrent << "\n";
  ostr << "HAL DDR count: " << m_devinfo.mDDRBankCount << "\n";
  ostr << "HAL OCL freq: " << m_devinfo.mOCLFrequency[0] << "\n";
  ostr << "HAL PCIe width: " << m_devinfo.mPCIeLinkWidth << "\n";
  ostr << "HAL PCIe speed: " << m_devinfo.mPCIeLinkSpeed << "\n";
  ostr << "HAL DMA threads: " << m_devinfo.mDMAThreads << "\n";
  return ostr;
}

void
device::
setup()
{
#ifndef PMD_OCL
  if (!m_workers.empty())
    return;

  openOrError();

  auto threads = config::get_dma_threads(); // number of bidirectional channels
  if (!threads)
    threads = m_devinfo.mDMAThreads;
  else
    threads = std::min(static_cast<unsigned short>(threads),m_devinfo.mDMAThreads);
  if (!threads) // Guard against drivers who do not set m_devinfo.mDMAThreads
    threads = 2;

  XRT_DEBUG(std::cout,"Creating ",2*threads," DMA worker threads\n");
  for (unsigned int i=0; i<threads; ++i) {
    // read and write queue workers
    m_workers.emplace_back(xrt::thread(task::worker2,std::ref(m_queue[static_cast<qtype>(hal::queue_type::read)]),"read"));
    m_workers.emplace_back(xrt::thread(task::worker2,std::ref(m_queue[static_cast<qtype>(hal::queue_type::write)]),"write"));
  }
  // single misc queue worker
  m_workers.emplace_back(xrt::thread(task::worker2,std::ref(m_queue[static_cast<qtype>(hal::queue_type::misc)]),"misc"));
#endif
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

ExecBufferObjectHandle
device::
allocExecBuffer(size_t sz)
{
  auto delBufferObject = [this](ExecBufferObjectHandle::element_type* ebo) {
    ExecBufferObject* bo = static_cast<ExecBufferObject*>(ebo);
    XRT_DEBUG(std::cout,"deleted exec buffer object\n");
    munmap(bo->data, bo->size);
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  auto ubo = xrt::make_unique<ExecBufferObject>();
  //ubo->handle = m_ops->mAllocBO(m_handle,sz,xclBOKind(0),(1<<31));  // 1<<31 xocl_ioctl.h
  ubo->handle = m_ops->mAllocBO(m_handle,sz,xclBOKind(0),(((uint64_t)1)<<31));  // 1<<31 xocl_ioctl.h
  if (ubo->handle == 0xffffffff)
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
    XRT_DEBUG(std::cout,"deleted buffer object device address(",bo->deviceAddr,",",bo->size,")\n");
    munmap(bo->hostAddr, bo->size);
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  xclBOKind kind = XCL_BO_DEVICE_RAM; //TODO: check default
  uint64_t flags = 0xFFFFFF; //TODO: check default, any bank.
  auto ubo = xrt::make_unique<BufferObject>();
  ubo->handle = m_ops->mAllocBO(m_handle, sz, kind, flags);
  if (ubo->handle == 0xffffffff)
    throw std::bad_alloc();

  ubo->kind = kind;
  ubo->size = sz;
  ubo->owner = m_handle;
  ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  ubo->hostAddr = m_ops->mMapBO(m_handle, ubo->handle, true /*write*/);
  return BufferObjectHandle(ubo.release(), delBufferObject);
}

BufferObjectHandle
device::
alloc(size_t sz,void* userptr)
{
  auto delBufferObject = [this](BufferObjectHandle::element_type* vbo) {
    BufferObject* bo = static_cast<BufferObject*>(vbo);
    XRT_DEBUG(std::cout,"deleted buffer object device address(",bo->deviceAddr,",",bo->size,")\n");
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  uint64_t flags = 0xFFFFFF; //TODO:check default
  auto ubo = xrt::make_unique<BufferObject>();
  ubo->handle = m_ops->mAllocUserPtrBO(m_handle, userptr, sz, flags);
  if (ubo->handle == 0xffffffff)
    throw std::bad_alloc();

  ubo->kind = XCL_BO_DEVICE_RAM;
  ubo->hostAddr = userptr;
  ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  ubo->size = sz;
  ubo->owner = m_handle;
  return BufferObjectHandle(ubo.release(), delBufferObject);
}

BufferObjectHandle
device::
alloc(size_t sz, Domain domain, uint64_t memory_index, void* userptr)
{
  const bool mmapRequired = (userptr == nullptr);
  auto delBufferObject = [mmapRequired, this](BufferObjectHandle::element_type* vbo) {
    BufferObject* bo = static_cast<BufferObject*>(vbo);
    XRT_DEBUG(std::cout,"deleted buffer object device address(",bo->deviceAddr,",",bo->size,")\n");
    if (bo->kind != XCL_BO_DEVICE_PREALLOCATED_BRAM) {
      if (mmapRequired)
        munmap(bo->hostAddr, bo->size);
      m_ops->mFreeBO(m_handle, bo->handle);
    }
    delete bo;
  };

  auto ubo = xrt::make_unique<BufferObject>();

  if (domain==Domain::XRT_DEVICE_PREALLOCATED_BRAM) {
    ubo->deviceAddr = memory_index;
    ubo->kind = XCL_BO_DEVICE_PREALLOCATED_BRAM;
    ubo->hostAddr = nullptr;
  }
  else {
    //uint64_t flags = (1<<memory_index);
    uint64_t flags = memory_index;
    xclBOKind kind = XCL_BO_DEVICE_RAM; //TODO: check default
    if(domain==Domain::XRT_DEVICE_P2P_RAM) {
      flags |= (1<<30);
    }
    if (userptr)
      ubo->handle = m_ops->mAllocUserPtrBO(m_handle, userptr, sz, flags);
    else
      ubo->handle = m_ops->mAllocBO(m_handle, sz, kind, flags);

    if (ubo->handle == 0xffffffff)
      throw std::bad_alloc();

    ubo->kind = XCL_BO_DEVICE_RAM;
    if (userptr)
      ubo->hostAddr = userptr;
    else
      ubo->hostAddr = m_ops->mMapBO(m_handle, ubo->handle, true /*write*/);

    ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  }
  ubo->size = sz;
  ubo->owner = m_handle;
  return BufferObjectHandle(ubo.release(), delBufferObject);
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

  auto ubo = xrt::make_unique<BufferObject>();
  ubo->handle = bo->handle;
  ubo->deviceAddr = bo->deviceAddr+offset;
  ubo->hostAddr = static_cast<char*>(bo->hostAddr)+offset;
  ubo->size = sz;
  ubo->offset = offset;
  ubo->kind = bo->kind;
  ubo->flags = bo->flags;
  ubo->owner = bo->owner;
  ubo->parent = boh;  // keep parent boh reference

  // Verify alignment based on hardware requirement
  auto alignment = getAlignment();
  if (reinterpret_cast<uintptr_t>(bo->hostAddr) % alignment || bo->deviceAddr % alignment)
    throw std::bad_alloc();

  XRT_DEBUG(std::cout,"allocated offset buffer object device address(",ubo->deviceAddr,",",ubo->size,")\n");
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
device::sync(const BufferObjectHandle& boh, size_t sz, size_t offset, direction dir1, bool async)
{
  xclBOSyncDirection dir = XCL_BO_SYNC_BO_TO_DEVICE;
  if(dir1 == direction::DEVICE2HOST)
    dir = XCL_BO_SYNC_BO_FROM_DEVICE;

  BufferObject* bo = getBufferObject(boh);

  if (async) {
    auto qt = (dir==XCL_BO_SYNC_BO_FROM_DEVICE) ? hal::queue_type::read : hal::queue_type::write;
    return event(addTaskF(m_ops->mSyncBO,qt,m_handle,bo->handle,dir,sz,offset));
  }
  return event(typed_event<int>(m_ops->mSyncBO(m_handle, bo->handle, dir, sz, offset+bo->offset)));
}

event
device::copy(const BufferObjectHandle& dst_boh, const BufferObjectHandle& src_boh, size_t sz, size_t dst_offset, size_t src_offset)
{
  BufferObject* dst_bo = getBufferObject(dst_boh);
  BufferObject* src_bo = getBufferObject(src_boh);
  return event(typed_event<int>(m_ops->mCopyBO(m_handle, dst_bo->handle, src_bo->handle, sz, dst_offset, src_offset)));
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
}

int
device::
exec_buf(const ExecBufferObjectHandle& boh)
{
  auto bo = getExecBufferObject(boh);
  return m_ops->mExecBuf(m_handle,bo->handle);
}

int
device::
exec_wait(int timeout_ms) const
{
  return m_ops->mExecWait(m_handle,timeout_ms);
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

  auto ubo = xrt::make_unique<BufferObject>();
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
    XRT_DEBUG(std::cout,"deleted buffer object device address(",bo->deviceAddr,",",bo->size,")\n");
    munmap(bo->hostAddr, bo->size);
    m_ops->mFreeBO(m_handle, bo->handle);
    delete bo;
  };

  auto ubo = xrt::make_unique<BufferObject>();

  if (!m_ops->mImportBO)
    throw std::runtime_error("ImportBO function not found in FPGA driver. Please install latest driver");

  ubo->handle = m_ops->mImportBO(m_handle, fd, flags);
  if (ubo->handle == 0xffffffff)
    throw std::runtime_error("getBufferFromFd-Create XRT-BO: BOH handle is invalid");


  ubo->kind = XCL_BO_DEVICE_RAM;
  ubo->size = m_ops->mGetBOSize(m_handle, ubo->handle);
  size = ubo->size;
  ubo->owner = m_handle;
  ubo->deviceAddr = m_ops->mGetDeviceAddr(m_handle, ubo->handle);
  ubo->hostAddr = m_ops->mMapBO(m_handle, ubo->handle, true /*write*/);

  return BufferObjectHandle(ubo.release(), delBufferObject);
}

void
device::
emplaceSVMBufferObjectMap(const BufferObjectHandle& boh, void* ptr)
{
  auto itr = m_svmbomap.find(ptr);
  if (itr == m_svmbomap.end())
    m_svmbomap[ptr] = boh;
}

void
device::
eraseSVMBufferObjectMap(void* ptr)
{
  auto itr = m_svmbomap.find(ptr);
  if (itr != m_svmbomap.end())
    m_svmbomap.erase(itr);
}

BufferObjectHandle
device::
svm_bo_lookup(void* ptr)
{
  auto itr = m_svmbomap.find(ptr);
  if (itr != m_svmbomap.end())
    return (*itr).second;
  else
    throw std::runtime_error("svm_bo_lookup: The SVM pointer is invalid.");
}

//Stream
int 
device::
createWriteStream(hal::StreamFlags flags, hal::StreamAttributes attr, hal::StreamHandle *stream)
{
  xclQueueContext ctx;
  ctx.flags = flags;
  ctx.type = attr;
  return m_ops->mCreateWriteQueue(&ctx,stream);
}

int 
device::
createReadStream(hal::StreamFlags flags, hal::StreamAttributes attr, hal::StreamHandle *stream)
{
  xclQueueContext ctx;
  ctx.flags = flags;
  ctx.type = attr;
  return m_ops->mCreateReadQueue(&ctx,stream);
}

int 
device::
closeStream(hal::StreamHandle stream) 
{
  return m_ops->mDestroyQueue(stream);
}

hal::StreamBuf
device::
allocStreamBuf(size_t size, hal::StreamBufHandle *buf)
{
  return m_ops->mAllocQDMABuf(size,buf);
}

int 
device::
freeStreamBuf(hal::StreamBufHandle buf)
{
  return m_ops->mFreeQDMABuf(buf);
}

ssize_t 
device::
writeStream(hal::StreamHandle stream, const void* ptr, size_t offset, size_t size, hal::StreamXferFlags flags) 
{
  //TODO:
  (void)offset;
  (void)flags;
  xclQueueRequest req;
//  req,op_code = XCL_QUEUE_WRITE;
//  req.bufs.buf = const_cast<char*>(ptr);
//  req.bufs.len = size;
//  req.flag = XCL_QUEUE_DEFAULT;
  return m_ops->mWriteQueue(stream,&req);
}

ssize_t 
device::
readStream(hal::StreamHandle stream, void* ptr, size_t offset, size_t size, hal::StreamXferFlags flags) 
{ 
  (void)offset;
  (void)flags;
  xclQueueRequest req;
//  req,op_code = XCL_QUEUE_READ;
//  req.bufs.buf = ptr;
//  req.bufs.len = size;
//  req.flag = XCL_QUEUE_DEFAULT;
  return m_ops->mReadQueue(stream,&req);
}

#ifdef PMD_OCL
void
createDevices(hal::device_list& devices,
              const std::string& dll, void* handle, unsigned int count, void* pmd)
{
  assert(0);
}
#else
void
createDevices(hal::device_list& devices,
              const std::string& dll, void* driverHandle, unsigned int deviceCount,void*)
{
  auto halops = std::make_shared<operations>(dll,driverHandle,deviceCount);
  for (unsigned int idx=0; idx<deviceCount; ++idx)
    devices.emplace_back(xrt::make_unique<xrt::hal2::device>(halops,idx));
}
#endif


}} // hal2,xrt
