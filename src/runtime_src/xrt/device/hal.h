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

#ifndef xrt_device_hal_h
#define xrt_device_hal_h

#include "xrt/config.h"
#include "xrt/device/PMDOperations.h"
#include "xrt/util/task.h"
#include "xrt/util/event.h"
#include "xrt/util/range.h"
#include "xrt/util/uuid.h"
#include "core/include/xrt.h"

#include "xclperf.h"
#include "xcl_app_debug.h"
#include "xstream.h"
#include "ert.h"

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <iosfwd>

#ifdef _WIN32
# include "core/include/windows/types.h"
# pragma warning( push )
# pragma warning ( disable : 4100 )
#endif
struct axlf;

namespace xrt {

namespace hal {

// Opaque types for various buffer objects
struct buffer_object {};
struct exec_buffer_object {};

using BufferObjectHandle = std::shared_ptr<buffer_object>;
using ExecBufferObjectHandle = std::shared_ptr<exec_buffer_object>;
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

//typedef rte_mbuf * PacketObject;
typedef void* PacketObject;
typedef uint64_t StreamHandle;
typedef void*    StreamBuf;
typedef uint64_t StreamBufHandle;
typedef uint32_t StreamAttributes;
typedef uint32_t StreamXferFlags;
typedef uint64_t StreamFlags;

using StreamXferReq = stream_xfer_req;
using StreamXferCompletions = streams_poll_req_completions;
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
#ifdef PMD_OCL
    ,XRT_DEVICE_REGISTER
#endif
    ,XRT_DEVICE_PREALLOCATED_BRAM
    ,XRT_SHARED_VIRTUAL
    ,XRT_SHARED_PHYSICAL
    ,XRT_DEVICE_ONLY_MEM_P2P
    ,XRT_DEVICE_ONLY_MEM
    ,XRT_HOST_ONLY_MEM
  };

  virtual bool
  open(const char* log, verbosity_level l) = 0;

  virtual void
  close() = 0;

  virtual device_handle
  get_handle() const = 0;

  virtual std::string
  get_bdf() const = 0;

  virtual void
  acquire_cu_context(const uuid& uuid,size_t cuidx,bool shared) {}

  virtual void
  release_cu_context(const uuid& uuid,size_t cuidx) {}

  // Hack to copy hw_em device info to sw_em device info
  // Should not be necessary when we move to sw_emu
  virtual void
  copyDeviceInfo(const device* src) {}

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

  virtual ExecBufferObjectHandle
  allocExecBuffer(size_t sz) = 0;

  virtual BufferObjectHandle
  alloc(size_t sz) = 0;

  virtual BufferObjectHandle
  alloc(size_t sz,void* userptr) = 0;

  /**
   * Allocate buffer object in specified memory bank index
   *
   * The bank index is an index into mem topology array and not
   * necessarily the logical bank number used in the host code.
   */
  virtual BufferObjectHandle
  alloc(size_t sz, Domain domain, uint64_t memoryIndex, void* user_ptr) = 0;

  virtual BufferObjectHandle
  alloc(const BufferObjectHandle& bo, size_t sz, size_t offset) = 0;

  virtual void*
  alloc_svm(size_t sz) = 0;

  virtual BufferObjectHandle
  import(const BufferObjectHandle& bo) = 0;

  virtual void
  free(const BufferObjectHandle& bo) = 0;

  virtual void
  free_svm(void* svm_ptr) = 0;

  virtual event
  write(const BufferObjectHandle& bo, const void* buffer, size_t sz, size_t offset,bool async) = 0;

  virtual event
  read(const BufferObjectHandle& bo, void* buffer, size_t sz, size_t offset, bool async) = 0;

  virtual event
  sync(const BufferObjectHandle& bo, size_t sz, size_t offset, direction dir, bool async) = 0;

  virtual event
  copy(const BufferObjectHandle& dst_bo, const BufferObjectHandle& src_bo, size_t sz,
       size_t dst_offset, size_t src_offset) = 0;

  virtual void
  fill_copy_pkt(const BufferObjectHandle& dst_boh, const BufferObjectHandle& src_boh
                ,size_t sz, size_t dst_offset, size_t src_offset,ert_start_copybo_cmd* pkt) = 0;

  virtual size_t
  read_register(size_t offset, void* buffer, size_t size) = 0;

  virtual size_t
  write_register(size_t offset, const void* buffer, size_t size) = 0;

  virtual void*
  map(const BufferObjectHandle& bo) = 0;

  virtual void
  unmap(const BufferObjectHandle& bo) = 0;

  virtual void*
  map(const ExecBufferObjectHandle& bo) = 0;

  virtual void
  unmap(const ExecBufferObjectHandle& bo) = 0;

  virtual int
  exec_buf(const ExecBufferObjectHandle& bo)
  {
    throw std::runtime_error("exec_buf not supported");
  }

  virtual int
  exec_wait(int timeout_ms) const
  {
    throw std::runtime_error("exec_wait not supported");
  }

public:
  virtual int
  createWriteStream(StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream) = 0;

  virtual int
  createReadStream(StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream) = 0;

  virtual int
  closeStream(hal::StreamHandle stream) = 0;

  virtual StreamBuf
  allocStreamBuf(size_t size, hal::StreamBufHandle *buf) = 0;

  virtual int
  freeStreamBuf(hal::StreamBufHandle buf) = 0;

  virtual ssize_t
  writeStream(hal::StreamHandle stream, const void* ptr, size_t size, hal::StreamXferReq* req ) = 0;

  virtual ssize_t
  readStream(hal::StreamHandle stream, void* ptr, size_t size, hal::StreamXferReq* req) = 0;

  virtual int
  pollStreams(StreamXferCompletions* comps, int min, int max, int* actual, int timeout) = 0;

public:
  /**
   * @returns
   *   True of this buffer object is imported from another device,
   *   false otherwise
   */
  virtual bool
  is_imported(const BufferObjectHandle& boh) const = 0;

  /**
   * @returns
   *   The device address of a buffer object
   */
  virtual uint64_t
  getDeviceAddr(const BufferObjectHandle& boh) = 0;

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
  getMemObjectFd(const BufferObjectHandle& boh)
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
  virtual BufferObjectHandle
  getBufferFromFd(const int fd, size_t& size, unsigned flags)
  {
    throw std::runtime_error("getBufferFromFd: HAL1 doesn't support DMA_BUF");
  }

public:
  /**
   * Lock the device
   *
   * Fails if device is already locked
   */
  virtual operations_result<int>
  lockDevice()
  {
    return operations_result<int>(); // invalid result
  }

  /**
   * Unlock the device
   *
   * Unlocking an unlocked device is a no-op
   */
  virtual operations_result<int>
  unlockDevice()
  {
    return operations_result<int>(); // invalid result
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

  virtual operations_result<int>
  loadXclBin(const axlf* xclbin)
  {
    return operations_result<int>();  // invalid result
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
//  virtual operations_result<int>
//  loadBitstream(const char* fnm)
//  {
//    return operations_result<int>(); // invalid result
//  }

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
  clockTraining(xclPerfMonType)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<uint32_t>
  countTrace(xclPerfMonType)
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
  getDeviceMaxRead()
  {
    return operations_result<double>();
  }

  virtual operations_result<double>
  getDeviceMaxWrite()
  {
    return operations_result<double>();
  }

  virtual operations_result<size_t>
  readCounters(xclPerfMonType, xclCounterResults&)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  debugReadIPStatus(xclDebugReadType type, void* results)
  {
    return operations_result<size_t>();
  }


  virtual operations_result<size_t>
  readTrace(xclPerfMonType type, xclTraceResultsVector&)
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
  setProfilingSlots(xclPerfMonType type, uint32_t)
  {
    return operations_result<void>();
  }

  virtual operations_result<uint32_t>
  getProfilingSlots(xclPerfMonType type)
  {
    return operations_result<uint32_t>();
  }

  virtual operations_result<void>
  getProfilingSlotName(xclPerfMonType type, uint32_t slotnum,
                       char* slotName, uint32_t length)
  {
    return operations_result<void>();
  }

  virtual operations_result<uint32_t>
  getProfilingSlotProperties(xclPerfMonType type, uint32_t slotnum)
  {
    return operations_result<uint32_t>();
  }

  virtual operations_result<void>
  configureDataflow(xclPerfMonType, unsigned *ip_config)
  {
    return operations_result<void>();
  }

  virtual operations_result<size_t>
  startCounters(xclPerfMonType)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  startTrace(xclPerfMonType, uint32_t)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  stopCounters(xclPerfMonType)
  {
    return operations_result<size_t>();
  }

  virtual operations_result<size_t>
  stopTrace(xclPerfMonType)
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

XRT_EXPORT
void
load_xdp();

XRT_EXPORT
void
load_xdp_kernel_debug();

XRT_EXPORT
void
load_xdp_app_debug();

XRT_EXPORT
void
load_xdp_lop();
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
 * @param pmd (optional)
 */
void
createDevices(hal::device_list&,const std::string&,void*,unsigned int,void* pmd=nullptr);

} // namespace hal2

} // xrt

#ifdef _WIN32
# pragma warning( push )
#endif

#endif
