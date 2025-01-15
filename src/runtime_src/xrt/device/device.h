// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2020 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrt_device_device_h_
#define xrt_device_device_h_

#include "xrt/config.h"
#include "xrt/device/hal.h"
#include "xrt/util/range.h"
#include "core/common/device.h"
#include "core/include/xdp/common.h"
#include "core/include/xrt/detail/ert.h"
#include "core/include/xrt/detail/xclbin.h"

#include <set>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

// Opaque handle to xrt_xocl::device
// The handle can be static_cast to xrt_xocl::device
struct xrt_device {};

namespace xrt_xocl {

/**
 * Runtime level device class.
 *
 * A device is a 1-1 mapping with a hal::device, but hides all HAL
 * layer functionality from clients.
 *
 */
class device : public xrt_device
{
public:
  using callback_function_type = std::function<void()>;
  using verbosity_level = hal::verbosity_level;
  using buffer_object_handle = hal::buffer_object_handle;
  using execbuffer_object_handle = hal::execbuffer_object_handle;
  using direction = hal::device::direction;
  using memoryDomain = hal::device::Domain;
  using queue_type = hal::queue_type;
  using device_handle = hal::device_handle;

  explicit
  device(std::unique_ptr<hal::device>&& hal)
    : m_hal(std::move(hal)), m_setup_done(false)
  {
  }

  device(device&& rhs)
    : m_hal(std::move(rhs.m_hal)), m_setup_done(rhs.m_setup_done)
  {}

  virtual ~device()
  {}

  device(const device &dev) = delete;
  device& operator=(const device &dev) = delete;

  // Transisition to xrt_core::device and eliminate
  // this class, but until done, provide access to
  // the core device.
  //
  // Function throws if core device is not loaded, which
  // is the case before the shim library is loaded
  std::shared_ptr<xrt_core::device>
  get_core_device() const
  {
    return m_hal->get_core_device();
  }

  void
  add_close_callback(callback_function_type fcn)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_close_callbacks.push_back(fcn);
  }

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

  size_t
  get_cdma_count() const
  {
    return m_hal->get_cdma_count();
  }

  /**
   * Open a HAL device
   *
   * @param log
   *   If specified log ouput to this file
   * @param level
   *   Verbosity level for logging
   * @returns
   *   If open was opened then true, false if device was already open
   *
   * Throws if device could not be opened
   */
  bool
  open()
  {
    return m_hal->open();
  }

  XRT_EXPORT
  void
  close();

  xclDeviceHandle
  get_xcl_handle() const
  { return m_hal->get_xcl_handle(); }

  xrt::device
  get_xrt_device() const
  { return m_hal->get_xrt_device(); }

  void
  acquire_cu_context(const uuid& uuid,size_t cuidx,bool shared)
  { m_hal->acquire_cu_context(uuid,cuidx,shared); }

  void
  release_cu_context(const uuid& uuid,size_t cuidx)
  { m_hal->release_cu_context(uuid,cuidx); }

  void
  acquire_cu_context(size_t cuidx,bool shared)
  { acquire_cu_context(m_uuid,cuidx,shared); }

  void
  release_cu_context(size_t cuidx)
  { release_cu_context(m_uuid,cuidx); }

  execbuffer_object_handle
  allocExecBuffer(size_t sz)
  {
    return m_hal->allocExecBuffer(sz);
  }

  buffer_object_handle
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
  buffer_object_handle
  alloc(const buffer_object_handle& bo, size_t sz, size_t offset)
  { return m_hal->alloc(bo,sz,offset); }

  void*
  alloc_svm(size_t sz)
  { return m_hal->alloc_svm(sz); }

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
  write(const buffer_object_handle& bo, const void* buffer, size_t sz, size_t offset,bool async=false)
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
  read(const buffer_object_handle& bo, void* buffer, size_t sz, size_t offset,bool async=false)
  { return m_hal->read(bo,buffer,sz,offset,async); }

  /**
   * Sync sz bytes at offset to/from device
   *
   * This function transfers data between host and device.
   *
   * @param
   */
  event // ssize_t
  sync(const buffer_object_handle& bo, size_t sz, size_t offset, direction dir, bool async=true)
  { return m_hal->sync(bo,sz,offset,dir,async); }

  /**
   * Copy sz bytes at offset from device to device/host
   *
   * This function transfers data between device and device/host.
   *
   * @param
   */
  event // ssize_t
  copy(const buffer_object_handle& dst_bo, const buffer_object_handle& src_bo, size_t sz, size_t dst_offset, size_t src_offset)
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
  map(const buffer_object_handle& bo)
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
  unmap(const buffer_object_handle& bo)
  {
    // TODO: We need to track if user fully unmapped the object or only
    // part of the BO before calling release.
    release(bo);
    m_hal->unmap(bo);
  }

  void*
  map(const execbuffer_object_handle& bo)
  { return m_hal->map(bo); }

  void
  unmap(const execbuffer_object_handle& bo)
  { m_hal->unmap(bo); }

  /**
   * Submit exec buffer to device.
   *
   * @returns
   *   0 on success, throws on error.
   */
  int
  exec_buf(const execbuffer_object_handle& bo)
  { return m_hal->exec_buf(bo); }

  int
  exec_wait(int timeout_ms) const
  { return m_hal->exec_wait(timeout_ms); }

public:
  /**
   * @returns
   *   True of this buffer object is imported from another device,
   *   false otherwise
   */
  virtual bool
  is_imported(const buffer_object_handle& boh) const
  {
    return m_hal->is_imported(boh);
  }

  /**
   * Get the device address of a buffer object
   *
   * @param boh
   *   Handle to buffer object
   * @returns
   *   Device side address of buffer object
   * @throws
   *   std::runtime_error if buffer object is unknown to this device
   */
  uint64_t
  getDeviceAddr(const buffer_object_handle& boh)
  {
    return m_hal->getDeviceAddr(boh);
  }

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
  int getMemObjectFd(const buffer_object_handle& boh)
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
  buffer_object_handle getBufferFromFd(const int fd, size_t& size, unsigned flags)
  {
    //TODO: check for the device match, Success/Fail
    return m_hal->getBufferFromFd(fd, size, flags);
  }

#if 0
  // read what ever is in bo, copy to user
  void
  read_cache(const buffer_object_handle& bo,void* user);

  // read what ever is in user, copy to bo
  void
  write_cache(const buffer_object_handle& bo,void* user);
#endif

private:
  void retain(const buffer_object_handle& bo)
  {
    std::lock_guard<std::mutex> buflk(m_mutex);
    m_buffers.push_back(bo);
  }
  void release(const buffer_object_handle& bo)
  {
    std::lock_guard<std::mutex> buflk(m_mutex);
    auto itr = std::find_if(m_buffers.begin(),m_buffers.end(),
                            [&bo](const buffer_object_handle& boh) {
                              return bo == boh;
                            });
    if (itr==m_buffers.end())
      throw std::runtime_error("Buffer object not mapped");
    m_buffers.erase(itr);
  }

public:
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
    m_uuid = xclbin->m_header.uuid;
    return m_hal->loadXclBin(xclbin);
  }

  bool
  hasBankAlloc() const
  {
    return m_hal->hasBankAlloc();
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
  clockTraining(xdp::MonitorType type)
  {
    return m_hal->clockTraining(type);
  }

  hal::operations_result<uint32_t>
  countTrace(xdp::MonitorType type)
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
  getHostMaxRead()
  {
    return m_hal->getHostMaxRead();
  }

  hal::operations_result<double>
  getHostMaxWrite()
  {
    return m_hal->getHostMaxWrite();
  }

  hal::operations_result<double>
  getKernelMaxRead()
  {
    return m_hal->getKernelMaxRead();
  }

  hal::operations_result<double>
  getKernelMaxWrite()
  {
    return m_hal->getKernelMaxWrite();
  }

  hal::operations_result<size_t>
  readCounters(xdp::MonitorType type, xdp::CounterResults& result)
  {
    return m_hal->readCounters(type,result);
  }

  hal::operations_result<size_t>
  debugReadIPStatus(xclDebugReadType type, void* result)
  {
    return m_hal->debugReadIPStatus(type, result);
  }

  hal::operations_result<size_t>
  readTrace(xdp::MonitorType type, xdp::TraceEventsVector& vec)
  {
    return m_hal->readTrace(type,vec);
  }

  hal::operations_result<void>
  xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
  {
    return m_hal->xclRead(space, offset, hostBuf, size);
  }

  hal::operations_result<void>
  xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
  {
    return m_hal->xclWrite(space, offset, hostBuf, size);
  }

  hal::operations_result<ssize_t>
  xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset)
  {
    return m_hal->xclUnmgdPread(flags, buf, count, offset);
  }

  hal::operations_result<void>
  setProfilingSlots(xdp::MonitorType type, uint32_t slots)
  {
    return m_hal->setProfilingSlots(type, slots);
  }

  hal::operations_result<uint32_t>
  getProfilingSlots(xdp::MonitorType type)
  {
    return m_hal->getProfilingSlots(type);
  }

  hal::operations_result<void>
  getProfilingSlotName(xdp::MonitorType type, uint32_t slotnum,
                       char* slotName, uint32_t length)
  {
    return m_hal->getProfilingSlotName(type, slotnum, slotName, length);
  }

  hal::operations_result<uint32_t>
  getProfilingSlotProperties(xdp::MonitorType type, uint32_t slotnum)
  {
    return m_hal->getProfilingSlotProperties(type, slotnum);
  }

  hal::operations_result<void>
  configureDataflow(xdp::MonitorType type, unsigned *ip_config)
  {
    return m_hal->configureDataflow(type, ip_config);
  }

  hal::operations_result<size_t>
  startCounters(xdp::MonitorType type)
  {
    return m_hal->startCounters(type);
  }

  hal::operations_result<size_t>
  startTrace(xdp::MonitorType type, uint32_t options)
  {
    return m_hal->startTrace(type,options);
  }

  hal::operations_result<size_t>
  stopCounters(xdp::MonitorType type)
  {
    return m_hal->stopCounters(type);
  }

  hal::operations_result<size_t>
  stopTrace(xdp::MonitorType type)
  {
    return m_hal->stopTrace(type);
  }

  hal::operations_result<uint32_t>
  getNumLiveProcesses()
  {
    return m_hal->getNumLiveProcesses();
  }

  hal::operations_result<std::string>
  getSysfsPath(const std::string& subdev, const std::string& entry)
  {
    return m_hal->getSysfsPath(subdev, entry);
  }

  hal::operations_result<std::string>
  getSubdevPath(const std::string& subdev, uint32_t idx)
  {
    return m_hal->getSubdevPath(subdev, idx);
  }

  hal::operations_result<std::string>
  getDebugIPlayoutPath()
  {
    return m_hal->getDebugIPlayoutPath();
  }

  hal::operations_result<int>
  getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
  {
    return m_hal->getTraceBufferInfo(nSamples, traceSamples, traceBufSz);
  }

  hal::operations_result<int>
  readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
  {
    return m_hal->readTraceData(traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample);
  }

  hal::operations_result<void>
  getDebugIpLayout(char* buffer, size_t size, size_t* size_ret)
  {
    return m_hal->getDebugIpLayout(buffer, size, size_ret);
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
  std::vector<buffer_object_handle> m_buffers;
  std::vector<callback_function_type> m_close_callbacks;
  mutable std::mutex m_mutex;
  xrt_xocl::uuid m_uuid;
  bool m_setup_done;
};

/**
 * Construct xrt_xocl::device objects from matching hal devices
 *
 * @param pred
 *   Unary predicate to limit construction of xrt_xocl::devices from
 *   xrt_xocl::hal::devices that match the predicate.  The predicate
 *   is called with a std::string representing the hal device
 *   driver name.
 * @return
 *   Rvalue container of xrt_xocl::device objects.
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
  return devices;
}

/**
 * Construct xrt_xocl::device objects from all hal devices
 *
 * @return
 *   Rvalue container of xrt_xocl::device objects
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
