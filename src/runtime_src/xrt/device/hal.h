// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrt_device_hal_h
#define xrt_device_hal_h

#include "xrt/config.h"
#include "xrt/util/task.h"
#include "xrt/util/event.h"
#include "xrt/util/range.h"
#include "xrt/util/uuid.h"
#include "core/common/device.h"
#include "core/include/xrt.h"
#include "core/include/xrt/xrt_device.h"
#include "core/include/deprecated/xcl_app_debug.h"

#include "core/include/xdp/app_debug.h"
#include "core/include/xdp/common.h"
#include "core/include/xdp/counters.h"
#include "core/include/xdp/trace.h"

#include "xrt/detail/ert.h"

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <iosfwd>

#ifdef _WIN32
# include "core/include/xrt/detail/windows/types.h"
# pragma warning( push )
# pragma warning ( disable : 4100 )
#endif
struct axlf;

namespace xrt_xocl {

namespace hal {

// Opaque types for various buffer objects
struct exec_buffer_object {};

using buffer_object_handle = xrt::bo;
using execbuffer_object_handle = std::shared_ptr<exec_buffer_object>;
using device_handle = xclDeviceHandle;

enum class verbosity_level : unsigned short
{
  quiet
 ,info
 ,warning
 ,error
};

enum class queue_type : unsigned short
{
  read=0 // queue used for DMA read  (device2host)
 ,write  // queue used for DMA write (host2device)
 ,misc   // queue used for non misc work (no actual hal)
 ,max=3
};

/**
 * Helper class to encapsulate return values from HAL operations.
 *
 * The HAL operation return value is valid if and only of the
 * HAL operations function is defined and was called.  Using this
 * class avoids client code littered with tests prior to call.
 */
template <typename ReturnValueType>
class operations_result
{
  ReturnValueType m_value;
  bool m_valid;

public:
  // implicit
  operations_result(ReturnValueType&& v)
    : m_value(std::move(v)),m_valid(true)
  {}

  operations_result()
    : m_value(0), m_valid(false)
  {}

  bool valid() const { return m_valid; }
  ReturnValueType get() const { return m_value; }
};

template <>
class operations_result<std::string>
{
  std::string m_value;
  bool m_valid;

public:
  operations_result(std::string&& s)
    : m_value(std::move(s))
    , m_valid(true)
  {}

  operations_result()
    : m_value("")
    , m_valid(false)
  {}

  bool valid() const { return m_valid; }
  std::string get() const { return m_value; }
};

template <>
class operations_result<void>
{
  bool m_valid;

public:
  // implicit
  operations_result(int)
    : m_valid(true)
  {}

  operations_result()
    : m_valid(false)
  {}

  bool valid() const { return m_valid; }
};

/**
 * Base class HAL device.
 *
 * A HAL device abstracts low level HAL driver APIs into some basic
 * methods that are implemented in concrete derived clases.
 *
 * Since the implementation of the abstracted methods depends on the
 * version of the HAL API, there will be one derived class per HAL
 * version API
 */
class device
{
public:
  device();
  virtual ~device();

  virtual void
  setup() {}

public:
  enum class direction { HOST2DEVICE, DEVICE2HOST };

  //TODO: Verify that this is the right place.
  enum class Domain
  {
    XRT_DEVICE_RAM
    ,XRT_DEVICE_BRAM
    ,XRT_DEVICE_PREALLOCATED_BRAM
    ,XRT_SHARED_VIRTUAL
    ,XRT_SHARED_PHYSICAL
    ,XRT_DEVICE_ONLY_MEM_P2P
    ,XRT_DEVICE_ONLY_MEM
    ,XRT_HOST_ONLY_MEM
  };

  /**
   * Open the device.
   *
   * @return True if device was opened, false if already open
   *
   * Throws if device could not be opened
   */
  virtual bool
  open() = 0;

  virtual void
  close() = 0;

  virtual xclDeviceHandle
  get_xcl_handle() const = 0;

  virtual xrt::device
  get_xrt_device() const = 0;

  virtual std::shared_ptr<xrt_core::device>
  get_core_device() const = 0;

  virtual void
  acquire_cu_context(const uuid& uuid,size_t cuidx,bool shared) {}

  virtual void
  release_cu_context(const uuid& uuid,size_t cuidx) {}

  virtual std::string
  getDriverLibraryName() const = 0;

  virtual std::string
  getName() const = 0;

  virtual unsigned int
  getBankCount() const = 0;

  virtual size_t
  getDdrSize() const = 0;

  virtual size_t
  getAlignment() const = 0;

  //virtual std::vector<unsigned short>
  virtual range<const unsigned short*>
  getClockFrequencies() const = 0;

  virtual std::ostream&
  printDeviceInfo(std::ostream&) const = 0;

  virtual size_t
  get_cdma_count() const = 0;

  virtual execbuffer_object_handle
  allocExecBuffer(size_t sz) = 0;

  /**
   * Allocate buffer object in specified memory bank index
   *
   * The bank index is an index into mem topology array and not
   * necessarily the logical bank number used in the host code.
   */
  virtual buffer_object_handle
  alloc(size_t sz, Domain domain, uint64_t memoryIndex, void* user_ptr) = 0;

  virtual buffer_object_handle
  alloc(const buffer_object_handle& bo, size_t sz, size_t offset) = 0;

  virtual void*
  alloc_svm(size_t sz) = 0;

#if 0
  virtual void
  free(const buffer_object_handle& bo) = 0;
#endif

  virtual void
  free_svm(void* svm_ptr) = 0;

  virtual event
  write(const buffer_object_handle& bo, const void* buffer, size_t sz, size_t offset,bool async) = 0;

  virtual event
  read(const buffer_object_handle& bo, void* buffer, size_t sz, size_t offset, bool async) = 0;

  virtual event
  sync(const buffer_object_handle& bo, size_t sz, size_t offset, direction dir, bool async) = 0;

  virtual event
  copy(const buffer_object_handle& dst_bo, const buffer_object_handle& src_bo, size_t sz,
       size_t dst_offset, size_t src_offset) = 0;

  virtual size_t
  read_register(size_t offset, void* buffer, size_t size) = 0;

  virtual size_t
  write_register(size_t offset, const void* buffer, size_t size) = 0;

  virtual void*
  map(const buffer_object_handle& bo) = 0;

  virtual void
  unmap(const buffer_object_handle& bo) = 0;

  virtual void*
  map(const execbuffer_object_handle& bo) = 0;

  virtual void
  unmap(const execbuffer_object_handle& bo) = 0;

  virtual int
  exec_buf(const execbuffer_object_handle& bo)
  {
    throw std::runtime_error("exec_buf not supported");
  }

  virtual int
  exec_wait(int timeout_ms) const
  {
    throw std::runtime_error("exec_wait not supported");
  }

public:
  /**
   * @returns
   *   True of this buffer object is imported from another device,
   *   false otherwise
   */
  virtual bool
  is_imported(const buffer_object_handle& boh) const = 0;

  /**
   * @returns
   *   The device address of a buffer object
   */
  virtual uint64_t
  getDeviceAddr(const buffer_object_handle& boh) = 0;

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
  virtual int
  getMemObjectFd(const buffer_object_handle& boh)
  {
    throw std::runtime_error("getMemObjectFd: HAL1 doesn't support DMA_BUF");
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
  virtual buffer_object_handle
  getBufferFromFd(const int fd, size_t& size, unsigned flags)
  {
    throw std::runtime_error("getBufferFromFd: HAL1 doesn't support DMA_BUF");
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

  virtual operations_result<int>
  loadXclBin(const axlf* xclbin)
  {
    return operations_result<int>();  // invalid result
  }

  /**
   * Check if bank allocation is supported
   *
   * @return
   *   true if bank allocation is supported, false otherwise
   */
  virtual bool
  hasBankAlloc() const
  {
    return false;
  }

  /**
   * Read kernel control register
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
  virtual operations_result<ssize_t>
  readKernelCtrl(uint64_t offset,void* hbuf,size_t size)
  {
    return operations_result<ssize_t>();
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
  virtual operations_result<ssize_t>
  writeKernelCtrl(uint64_t offset,const void* hbuf,size_t size)
  {
    return operations_result<ssize_t>();
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
  virtual operations_result<int>
  reClock(unsigned int freqMHz)
  {
    return operations_result<int>();
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
  virtual operations_result<int>
  reClock2(unsigned short  numClocks, unsigned short *freqMHz)
  {
    return operations_result<int>();
  }

  // Following functions are undocumented profiling functions
  virtual operations_result<size_t>
  clockTraining(xdp::MonitorType)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<uint32_t>
  countTrace(xdp::MonitorType)
  {
    return operations_result<uint32_t>();
  }

  virtual operations_result<double>
  getDeviceClock()
  {
    return operations_result<double>();
  }

  virtual operations_result<size_t>
  getDeviceTime()
  {
    return operations_result<size_t>();
  }

  virtual operations_result<double>
  getHostMaxRead()
  {
    return operations_result<double>();
  }

  virtual operations_result<double>
  getHostMaxWrite()
  {
    return operations_result<double>();
  }

  virtual operations_result<double>
  getKernelMaxRead()
  {
    return operations_result<double>();
  }

  virtual operations_result<double>
  getKernelMaxWrite()
  {
    return operations_result<double>();
  }

  virtual operations_result<size_t>
  readCounters(xdp::MonitorType, xdp::CounterResults&)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  debugReadIPStatus(xclDebugReadType type, void* results)
  {
    return operations_result<size_t>();
  }


  virtual operations_result<size_t>
  readTrace(xdp::MonitorType type, xdp::TraceEventsVector&)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<void>
  xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
  {
    return operations_result<void>();
  }

  virtual operations_result<void>
  xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
  {
    return operations_result<void>();
  }

  virtual operations_result<ssize_t>
  xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset)
  {
    return operations_result<ssize_t>();
  }

  virtual operations_result<void>
  setProfilingSlots(xdp::MonitorType type, uint32_t)
  {
    return operations_result<void>();
  }

  virtual operations_result<uint32_t>
  getProfilingSlots(xdp::MonitorType type)
  {
    return operations_result<uint32_t>();
  }

  virtual operations_result<void>
  getProfilingSlotName(xdp::MonitorType type, uint32_t slotnum,
                       char* slotName, uint32_t length)
  {
    return operations_result<void>();
  }

  virtual operations_result<uint32_t>
  getProfilingSlotProperties(xdp::MonitorType type, uint32_t slotnum)
  {
    return operations_result<uint32_t>();
  }

  virtual operations_result<void>
  configureDataflow(xdp::MonitorType, unsigned *ip_config)
  {
    return operations_result<void>();
  }

  virtual operations_result<size_t>
  startCounters(xdp::MonitorType)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  startTrace(xdp::MonitorType, uint32_t)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  stopCounters(xdp::MonitorType)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  stopTrace(xdp::MonitorType)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<uint32_t>
  getNumLiveProcesses()
  {
    return operations_result<uint32_t>();
  }

  virtual operations_result<std::string>
  getSysfsPath(const std::string& subdev, const std::string& entry)
  {
    return operations_result<std::string>();
  }

  virtual operations_result<std::string>
  getSubdevPath(const std::string& subdev, uint32_t idx)
  {
    return operations_result<std::string>();
  }

  virtual operations_result<std::string>
  getDebugIPlayoutPath()
  {
    return operations_result<std::string>();
  }

  virtual operations_result<int>
  getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
  {
    return operations_result<int>();
  }

  virtual operations_result<int>
  readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
  {
    return operations_result<int>();
  }

  virtual operations_result<void>
  getDebugIpLayout(char* buffer, size_t size, size_t* size_ret)
  {
    return operations_result<void>();
  }

  virtual task::queue*
  getQueue(hal::queue_type qt) {return nullptr; }

  virtual void*
  getHalDeviceHandle() {return nullptr;}
};


////////////////////////////////////////////////////////////////
// HAL level application functions and types
////////////////////////////////////////////////////////////////
using device_list = std::vector<std::unique_ptr<device>>;

XRT_EXPORT
device_list
loadDevices(const std::string& dirName);

XRT_EXPORT
device_list
loadDevices();

} // namespace hal

namespace hal2 {

/**
 * Populate hal device list with hal2 devices as probed by DLL
 *
 * @param devices
 *   List to populate with hal2 devices
 * @param dll
 *   Fill path o the dll (shim library) associated with these hal2 devices
 * @param handle
 *   Handle to the dll as was returned by dlopen
 * @param count
 *   Number of devices probed by the dll
 */
void
createDevices(hal::device_list&, const std::string&, unsigned int);

} // namespace hal2

} // xrt

#ifdef _WIN32
# pragma warning( push )
#endif

#endif
