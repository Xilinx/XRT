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

#ifndef xrt_device_device_h_
#define xrt_device_device_h_

#include "xrt/device/hal.h"
#include "xrt/util/range.h"

#include <set>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
namespace xrt {

/**
 * Runtime level device class.
 *
 * A device is a 1-1 mapping with a hal::device, but hides all HAL
 * layer functionality from clients.
 *
 */
class device
{
public:
  using verbosity_level = hal::verbosity_level;
  using BufferObjectHandle = hal::BufferObjectHandle;
  using ExecBufferObjectHandle = hal::ExecBufferObjectHandle;
  using direction = hal::device::direction;
  using memoryDomain = hal::device::Domain;
  using queue_type = hal::queue_type;
  using stream_handle = hal::StreamHandle;
  using stream_flags = hal::StreamFlags;
  using stream_attrs = hal::StreamAttributes;
  using stream_xfer_flags = hal::StreamXferFlags;
  using stream_buf = hal::StreamBuf;
  using stream_buf_handle = hal::StreamBufHandle;

  explicit
  device(std::unique_ptr<hal::device>&& hal)
    : m_hal(std::move(hal)), m_setup_done(false)
  {
  }

  device(device&& rhs)
    : m_hal(std::move(rhs.m_hal)), m_setup_done(rhs.m_setup_done)
  {}

  ~device()
  {}

  device(const device &dev) = delete;
  device& operator=(const device &dev) = delete;

  /**
   * Prepare a device for actual use.
   * For devices that support DMA threads, this function
   * should start the threads.
   */
  void
  setup()
  {
    m_hal->setup();
    m_setup_done = true;
  }

  std::string
  getDriverLibraryName() const
  {
    return m_hal->getDriverLibraryName();
  }

  std::string
  getName() const
  {
    return m_hal->getName();
  }

  unsigned int
  getBankCount() const
  {
    return m_hal->getBankCount();
  }

  size_t
  getDdrSize() const
  {
    return m_hal->getDdrSize();
  }

  size_t
  getAlignment() const
  {
    return m_hal->getAlignment();
  }

  /**
   * @return
   *   List of clock frequency from device info
   */
  range<const unsigned short*>
  getClockFrequencies() const
  {
    return m_hal->getClockFrequencies();
  }

  std::ostream&
  printDeviceInfo(std::ostream&) const;

  /**
   * Hack to accomodate sw_em missing device info
   */
  void
  copyDeviceInfo(const device* src)
  {
    m_hal->copyDeviceInfo(src->m_hal.get());
  }

  /**
   * Open a HAL device
   *
   * @param log
   *   If specified log ouput to this file
   * @param level
   *   Verbosity level for logging
   * @returns
   *   If open succeeds then true, false otherwise
   */
  bool
  open(const char* log=nullptr, verbosity_level level=verbosity_level::quiet)
  {
    return m_hal->open(log,level);
  }

  void
  close()
  {
    m_hal->close();
  }

  ExecBufferObjectHandle
  allocExecBuffer(size_t sz)
  {
    return m_hal->allocExecBuffer(sz);
  }

  BufferObjectHandle
  alloc(size_t sz, void* userptr)
  {
    auto bo = m_hal->alloc(sz,userptr);
    if (!bo) {
      bo = m_hal->alloc(sz);
    }
    return bo;
  }

  BufferObjectHandle
  alloc(size_t sz)
  { return m_hal->alloc(sz); }

  BufferObjectHandle
  alloc(size_t sz, memoryDomain domain, uint64_t memoryIndex, void* user_ptr)
  { return m_hal->alloc(sz, domain, memoryIndex, user_ptr); }

  /**
   * Allocate a new buffer object from an existing one by offsetting
   * host and device address
   *
   * @param bo
   *  The existing buffer object from which the new one will be created
   * @param sz
   *  The size to carve out of the existing buffer object when
   *  creating the new one
   * @param offset
   *  The offset to add to existing buffer object when creating
   *  the new one
   * @return
   *  A handle to the new buffer object
   */
  BufferObjectHandle
  alloc(const BufferObjectHandle& bo, size_t sz, size_t offset)
  { return m_hal->alloc(bo,sz,offset); }

  void*
  alloc_svm(size_t sz)
  { return m_hal->alloc_svm(sz); }

  void
  free(const BufferObjectHandle& bo)
  { m_hal->free(bo); }

  void
  free_svm(void* svm_ptr)
  { m_hal->free_svm(svm_ptr); }

  /**
   * Write sz bytes from buffer to host memory at offset in buffer
   * object.
   *
   * This function is simply a memcpy from buffer to buffer object.
   *
   * @param bo
   *   Handle to buffer object to write to
   * @param sz
   *   Number of bytes to write
   * @param offset
   *   Number of bytes to offset into buffer object host memory before
   *   writing
   * @return
   *   Event with ssize_t value with actual number of bytes written
   */
  // write from bo at offset,sz to device
  event // ssize_t
  write(const BufferObjectHandle& bo, const void* buffer, size_t sz, size_t offset,bool async=false)
  { return m_hal->write(bo,buffer,sz,offset,async); }

  /**
   * Read sz bytes from buffer object host memory at offset to
   * buffer.
   *
   * This function is simply a memcpy from buffer object to buffer.
   *
   * @param bo
   *   Handle to buffer object to read from
   * @param sz
   *   Number of bytes to read
   * @param offset
   *   Number of bytes to offset buffer object host memory before reading
   * @return
   *   Event with ssize_t value type with actual number of bytes read
   */
  event // ssize_t
  read(const BufferObjectHandle& bo, void* buffer, size_t sz, size_t offset,bool async=false)
  { return m_hal->read(bo,buffer,sz,offset,async); }

  /**
   * Sync sz bytes at offset to/from device
   *
   * This function transfers data between host and device.
   *
   * @param
   */
  event // ssize_t
  sync(const BufferObjectHandle& bo, size_t sz, size_t offset, direction dir, bool async=true)
  { return m_hal->sync(bo,sz,offset,dir,async); }

  /**
   * Copy sz bytes at offset from device to device/host
   *
   * This function transfers data between device and device/host.
   *
   * @param
   */
  event // ssize_t
  copy(const BufferObjectHandle& dst_bo, const BufferObjectHandle& src_bo, size_t sz, size_t dst_offset, size_t src_offset)
  { return m_hal->copy(dst_bo,src_bo,sz,dst_offset,src_offset); }

  /**
   * Read a device register
   *
   * @param offset
   *  The offset of the device register
   * @param buffer
   *  The buffer to write the read data into
   * @param size
   *  The number of bytes to read and the size of the buffer
   * @return
   *  Number of bytes successfully read
   */
  size_t read_register(size_t offset, void* buffer, size_t size)
  { return m_hal->read_register(offset,buffer,size); }

  /**
   * Write to a device register
   *
   * @param offset
   *  The offset of the device register
   * @param buffer
   *  The buffer with data to write the device register
   * @param size
   *  The number of bytes to write and the size of the buffer
   * @return
   *  Number of bytes successfully written
   */
  size_t write_register(size_t offset, const void* buffer, size_t size)
  { return m_hal->write_register(offset,buffer,size); }

  /**
   * Map device memory at offset to host memory buffer object at offset.
   * Return host's view to the bo. Note that map does not ensure refresh
   * of host's view of the data -- use sync() for that.
   *
   * @param bo
   *   Handle to buffer object to map
   * @return
   *   Event with void* host ptr at requested offset
   */
  void*
  map(const BufferObjectHandle& bo)
  {
    void* p = m_hal->map(bo);
    retain(bo);
    return p;
  }

  /**
   * Unmap buffer object host memory at offset to device memory at
   * offset. Note that unmap does not ensure flush of host's view
   * to device -- use sync() for that.
   *
   * @param bo
   *   Handle to buffer object to map
   */
  void
  unmap(const BufferObjectHandle& bo)
  {
    // TODO: We need to track if user fully unmapped the object or only
    // part of the BO before calling release.
    release(bo);
    m_hal->unmap(bo);
  }

  void*
  map(const ExecBufferObjectHandle& bo)
  { return m_hal->map(bo); }

  void
  unmap(const ExecBufferObjectHandle& bo)
  { m_hal->unmap(bo); }

  int
  exec_buf(const ExecBufferObjectHandle& bo)
  { return m_hal->exec_buf(bo); }

  int
  exec_wait(int timeout_ms) const
  { return m_hal->exec_wait(timeout_ms); }

public:
//  //TODO: BufferObject accessors. These are intermediary functions.
  uint64_t getDeviceAddr(const BufferObjectHandle& boh)
  {
      //TODO: check for the device match
    return m_hal->getDeviceAddr(boh);
  }
//
//  void* getHostAddr(const BufferObjectHandle& boh)
//  {
//      //TODO: check for the device match
//    return m_hal->getHostAddr(boh);
//  }

  /**
   * Export FD of buffer object handle on this device.
   * The importing device will create another
   * buffer object (using FD) linked to buffer object on this device.
   * Note that the imported
   * bo's data will not be automatically flushed to importing device -- use
   * on the importing device sync() for that.
   *
   * @param boh
   *   Handle to buffer object for which FD is obtained
   *   Reference to FD variable to store the value into
   * @return
   *   Fd as integer
   */
  int getMemObjectFd(const BufferObjectHandle& boh)
  {
    //TODO: check for the device match, Success/Fail
    return m_hal->getMemObjectFd(boh);
  }

  /**
   * Import buffer assigned to the FD on another device.
   * BO is created on this device linked to FD/BO on another device.
   * Note that the imported
   * bo's data will not be automatically flushed to importing device -- use
   * on the importing device sync() for that.
   *
   * @param fd
   *   FD of buffer on another device which is to be imported
   *   Reference to size variable to store the value into
   *   Reference to buffer object handle to store the value into
   * @param size
   *   Sets size of the buffer imported
   * @param flags
   *   which DDR to import the buffer on
   * @return
   *   Handle to imported buffer
   */
  BufferObjectHandle getBufferFromFd(const int fd, size_t& size, unsigned flags)
  {
    //TODO: check for the device match, Success/Fail
    return m_hal->getBufferFromFd(fd, size, flags);
  }

  /**
   * Import a bo into this device. The importing device will create another
   * bo which will internally track the imported bo. Note that the imported
   * bo's data will not be automatically flushed to importing device -- use
   * on the importing device sync() for that.
   *
   * @param bo
   *   Handle to buffer object to map
   * @return
   *   BufferObjectHandle usable in the context of importing device
   */

  BufferObjectHandle
  import(const BufferObjectHandle& bo)
  {
    return m_hal->import(bo);
  }

#if 0
  // read what ever is in bo, copy to user
  void
  read_cache(const BufferObjectHandle& bo,void* user);

  // read what ever is in user, copy to bo
  void
  write_cache(const BufferObjectHandle& bo,void* user);
#endif

//Streaming APIs
  int 
  createWriteStream(hal::StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream)
  { 
    return m_hal->createWriteStream(flags, attr, route, flow, stream);
  }

  int 
  createReadStream(hal::StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream)
  {
    return m_hal->createReadStream(flags, attr, route, flow, stream);
  };

  int 
  closeStream(hal::StreamHandle stream)
  {
    return m_hal->closeStream(stream);
  };

  hal::StreamBuf
  allocStreamBuf(size_t size, hal::StreamBufHandle *buf)
  {
    return m_hal->allocStreamBuf(size, buf);
  };

  int 
  freeStreamBuf(hal::StreamBufHandle buf)
  {
    return m_hal->freeStreamBuf(buf);
  };

  ssize_t 
  writeStream(hal::StreamHandle stream, const void* ptr, size_t offset, size_t size, hal::StreamXferFlags flags)
  {
    return m_hal->writeStream(stream, ptr, offset, size, flags);
  };

  ssize_t 
  readStream(hal::StreamHandle stream, void* ptr, size_t offset, size_t size, hal::StreamXferFlags flags)
  {
    return m_hal->readStream(stream, ptr, offset, size, flags);
  };

//End Streaming APIs
#ifdef PMD_OCL
public:
  StreamHandle openStream(unsigned depth, unsigned q, direction dir)
  {
    return m_hal->openStream(depth, q, dir);
  }
  void closeStream(StreamHandle strm)
  {
    return m_hal->closeStream(strm);
  }
  unsigned send(StreamHandle strm, PacketObject *pkts, unsigned count)
  {
    return m_hal->send(strm, pkts, count);
  }
  unsigned recv(StreamHandle strm, PacketObject *pkts, unsigned count)
  {
    return m_hal->recv(strm, pkts, count);
  }
  PacketObject acquirePacket()
  {
    return m_hal->acquirePacket();
  }
  void releasePacket(PacketObject pkt)
  {
    return m_hal->releasePacket(pkt);
  }
#endif

private:
  void retain(const BufferObjectHandle& bo)
  {
    std::lock_guard<std::mutex> buflk(m_buffers_mutex);
    m_buffers.push_back(bo);
  }
  void release(const BufferObjectHandle& bo)
  {
    std::lock_guard<std::mutex> buflk(m_buffers_mutex);
    auto itr = std::find_if(m_buffers.begin(),m_buffers.end(),
                            [&bo](const BufferObjectHandle& boh) {
                              return bo.get() == boh.get();
                            });
    if (itr==m_buffers.end())
      throw std::runtime_error("Buffer object not mapped");
    m_buffers.erase(itr);
  }

public:
  /**
   * Lock the device
   */
  hal::operations_result<int>
  lockDevice()
  {
    return m_hal->lockDevice();
  }

  /**
   * Unlock the device
   */
  hal::operations_result<int>
  unlockDevice()
  {
    return m_hal->unlockDevice();
  }

  /**
   * Load an xclbin
   *
   * @param xclbin
   *   Pointer to an xclbin
   * @returns
   *   A pair <int,bool> where bool is set to true if
   *   and only if the return int value is valid. The
   *   return value is implementation dependent.
   */
  hal::operations_result<int>
  loadXclBin(const axlf* xclbin)
  {
    return m_hal->loadXclBin(xclbin);
  }

  /**
   * Load a bistream from a file
   *
   * @param fnm
   *   Full path to bitsream file
   * @returns
   *   A pair <int,bool> where bool is set to true if
   *   and only if the return int value is valid. The
   *   return value is implementation dependent.
   */
//  hal::operations_result<int>
//  loadBitstream(const char* fnm)
//  {
//    return m_hal->loadBitstream(fnm);
//  }

  bool
  hasBankAlloc() const
  {
    return m_hal->hasBankAlloc();
  }

  /**
   * Check if this device is an ARE device
   */
  bool
  is_xare_device() const
  {
    //return m_hal->isAreDevice();
    return (m_hal->getName().find("-xare")!=std::string::npos);
  }

  /**
   * Read kernel control register from device
   *
   * @param offset
   *   Read from offset
   * @param hbuf
   *   Read into provided buffer
   * @param size
   *   Bytes to read
   * @returns
   *   A pair <ssize_t,bool> where bool is set to true if
   *   and only if the return ssize_t value is valid. The
   *   return value is implementation dependent.
   */
  hal::operations_result<ssize_t>
  readKernelCtrl(uint64_t offset,void* hbuf,size_t size)
  {
    return m_hal->readKernelCtrl(offset,hbuf,size);
  }

  /**
   * Write to kernel control register
   *
   * @param offset
   *   Write to offset
   * @param hbuf
   *   Buffer with bytes to write
   * @param size
   *   Number of bytes to write
   * @returns
   *   A pair <ssize_t,bool> where bool is set to true if
   *   and only if the return ssize_t value is valid. The
   *   return value is implementation dependent.
   */
  hal::operations_result<ssize_t>
  writeKernelCtrl(uint64_t offset,const void* hbuf,size_t size)
  {
    return m_hal->writeKernelCtrl(offset,hbuf,size);
  }

  /**
   * Reset device program
   *
   * @param kind
   *   Type of set
   * @returns
   *   A pair <int,bool> where bool is set to true if
   *   and only if the return int value is valid. The
   *   return value is implementation dependent.
   */
  hal::operations_result<int>
  resetKernel()
  {
    return m_hal->resetKernel();
  }

  /**
   * Re-clock device at specified freq
   *
   * @param freqMHz
   *   Target frequency in MHz
   * @returns
   *   A pair <int,bool> where bool is set to true if
   *   and only if the return int value is valid. The
   *   return value is implementation dependent.
   */
  hal::operations_result<int>
  reClock(unsigned int freqMHz)
  {
    return m_hal->reClock(freqMHz);
  }

  /**
   * Re-clock2 OCL Kernel clocks at specified frequencies
   *
   * @param numClocks
   *   number of clocks
   * @param freqMHz
   *   pointer to an array of associated clock frequencies in MHz
   * @returns
   *   A pair <int,bool> where bool is set to true if
   *   and only if the return int value is valid. The
   *   return value is implementation dependent.
   */
  hal::operations_result<int>
  reClock2(unsigned short numClocks, unsigned short *freqMHz)
  {
    return m_hal->reClock2(numClocks, freqMHz);
  }


  // Following functions are undocumented profiling functions
  hal::operations_result<size_t>
  clockTraining(xclPerfMonType type)
  {
    return m_hal->clockTraining(type);
  }

  hal::operations_result<uint32_t>
  countTrace(xclPerfMonType type)
  {
    return m_hal->countTrace(type);
  }

  hal::operations_result<double>
  getDeviceClock()
  {
    return m_hal->getDeviceClock();
  }

  hal::operations_result<size_t>
  getDeviceTime()
  {
    return m_hal->getDeviceTime();
  }

  hal::operations_result<double>
  getDeviceMaxRead()
  {
    return m_hal->getDeviceMaxRead();
  }

  hal::operations_result<double>
  getDeviceMaxWrite()
  {
    return m_hal->getDeviceMaxWrite();
  }

  hal::operations_result<size_t>
  readCounters(xclPerfMonType type, xclCounterResults& result)
  {
    return m_hal->readCounters(type,result);
  }

  hal::operations_result<size_t>
  debugReadIPStatus(xclDebugReadType type, void* result)
  {
    return m_hal->debugReadIPStatus(type, result);
  }

  hal::operations_result<size_t>
  readTrace(xclPerfMonType type, xclTraceResultsVector& vec)
  {
    return m_hal->readTrace(type,vec);
  }

  hal::operations_result<void>
  setProfilingSlots(xclPerfMonType type, uint32_t slots)
  {
    return m_hal->setProfilingSlots(type, slots);
  }

  hal::operations_result<uint32_t>
  getProfilingSlots(xclPerfMonType type)
  {
    return m_hal->getProfilingSlots(type);
  }

  hal::operations_result<void>
  getProfilingSlotName(xclPerfMonType type, uint32_t slotnum,
                       char* slotName, uint32_t length)
  {
    return m_hal->getProfilingSlotName(type, slotnum, slotName, length);
  }

  hal::operations_result<void>
  writeHostEvent(xclPerfMonEventType type, xclPerfMonEventID id)
  {
    return m_hal->writeHostEvent(type, id);
  }

  hal::operations_result<size_t>
  startCounters(xclPerfMonType type)
  {
    return m_hal->startCounters(type);
  }

  hal::operations_result<size_t>
  startTrace(xclPerfMonType type, uint32_t options)
  {
    return m_hal->startTrace(type,options);
  }

  hal::operations_result<size_t>
  stopCounters(xclPerfMonType type)
  {
    return m_hal->stopCounters(type);
  }

  hal::operations_result<size_t>
  stopTrace(xclPerfMonType type)
  {
    return m_hal->stopTrace(type);
  }

  /**
   * Explicitly schedule an arbitrary function on the device's
   * task queue.
   */
  template <typename F,typename ...Args>
  event
  schedule(F&& f,queue_type qt,Args&&... args)
  {
    // Ensure that the worker threads are running
    if (!m_setup_done)
      setup();

    task::queue* q = m_hal->getQueue(qt);
    return task::createF(*q,f,std::forward<Args>(args)...);
  }

  /**
   * Explicitly schedule an arbitrary member function on the device's
   * task queue.
   */
  template <typename F,typename C,typename ...Args>
  event
  scheduleM(F&& f,C& c,queue_type qt,Args&&... args)
  {
    // Ensure that the worker threads are running
    if (!m_setup_done)
      setup();

    task::queue* q = m_hal->getQueue(qt);
    return task::createM(*q,f,c,std::forward<Args>(args)...);
  }

private:

  std::unique_ptr<hal::device> m_hal;
  std::vector<BufferObjectHandle> m_buffers;
  mutable std::mutex m_buffers_mutex;
  bool m_setup_done;
};

/**
 * Construct xrt::device objects from matching hal devices
 *
 * @param pred
 *   Unary predicate to limit construction of xrt::devices from
 *   xrt::hal::devices that match the predicate.  The predicate
 *   is called with a std::string representing the hal device
 *   driver name.
 * @return
 *   Rvalue container of xrt::device objects.
 */
template <typename UnaryPredicate>
std::vector<device>
loadDevices(UnaryPredicate pred)
{
  std::vector<device> devices;
  auto haldevices = hal::loadDevices();
  for (auto& hal : haldevices) { // unique_ptr<hal::device
    if (pred(hal->getDriverLibraryName()))
      devices.emplace_back(std::move(hal));
  }
  return std::move(devices);
}

/**
 * Construct xrt::device objects from all hal devices
 *
 * @return
 *   Rvalue container of xrt::device objects
 */
inline
std::vector<device>
loadDevices()
{
  auto pred = [](const std::string&) { return true; };
  return loadDevices(pred);
}

} // xrt

#endif
