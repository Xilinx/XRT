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

#ifndef xrt_device_hal2_h
#define xrt_device_hal2_h

#include "xrt/device/hal.h"
#include "xrt/device/halops2.h"

#include "experimental/xrt_device.h"
#include "experimental/xrt_bo.h"

#include "ert.h"

#include <cassert>

#include <functional>
#include <type_traits>
#include <cstring>
#include <memory>
#include <map>
#include <array>
#include <mutex>

#include <boost/optional/optional.hpp>

namespace xrt_xocl { namespace hal2 {

namespace hal  = xrt_xocl::hal;
namespace hal2 = xrt_xocl::hal2;
using buffer_object_handle = hal::buffer_object_handle;
using execbuffer_object_handle = hal::execbuffer_object_handle;
using svmbomap_type = std::map<void *, buffer_object_handle>;
using svmbomap_value_type = svmbomap_type::value_type;
using svmbomap_iterator_type = svmbomap_type::iterator;

/**
 * HAL device for hal 2.0.
 *
 * HAL2 supports asynchronous operation via a producer-consumer
 * queue (task queue), and a number of task workers (consumers). The
 * implementation of the abstracted methods pushes tasks on the queue
 * and the tasks are consumed by the workers. A task is HAL API function.
 *
 * The number of workers supported is defined by the HAL implementation
 * and aquired through the HAL API.
 */

class device : public xrt_xocl::hal::device
{
  // separate queues for read,write, and misc operations
  // primarily done so that independent operations can be serviced
  // by a worker simultaneously
  using qtype = std::underlying_type<hal::queue_type>::type;
  std::array<task::queue,static_cast<qtype>(hal::queue_type::max)> m_queue;
  std::vector<std::thread> m_workers;
  svmbomap_type m_svmbomap;

  std::shared_ptr<hal2::operations> m_ops;
  unsigned int m_idx;

  xrt::device m_handle;
  mutable boost::optional<hal2::device_info> m_devinfo;

  mutable std::mutex m_mutex;

  struct ExecBufferObject : hal::exec_buffer_object
  {
    xclBufferHandle handle;
    void* data = nullptr;
    size_t size = 0;
    hal2::device_handle owner = nullptr;
  };

  ExecBufferObject*
  getExecBufferObject(const execbuffer_object_handle& boh) const;

  hal2::device_info*
  get_device_info() const;

  bool
  open_nolock();

  void
  close_nolock();

  hal2::device_info*
  get_device_info_nolock() const;

  task::queue&
  get_queue(hal::queue_type qt)
  {
    return m_queue[static_cast<qtype>(qt)];
  }

public:
  /**
   * Schedule a task to be executed by a worker thread
   *
   * This function is wired to support only member functions of this class.
   * If other functions are to be added as task, additional overloads will
   * be required to make appropriate calls to task::create.
   *
   * For example:
   *  auto ev = addTask(&hal::device::copyBufferDevice2Host,dest,src,size,skip)
   *
   * @param F
   *   The task member function pointer
   * @param Args
   *   The task arguments
   * @return
   *  Typed event to wait on and retrieve the task return value
   */
  template <typename F,typename ...Args>
  auto
  addTaskM(F&& f,hal::queue_type qt,Args&&... args) -> decltype(task::createM(m_queue,f,*this,std::forward<Args>(args)...))
  {
    return task::createM(get_queue(qt),f,*this,std::forward<Args>(args)...);
  }

#ifdef __GNUC__
# pragma GCC diagnostic push
# if __GNUC__ >= 7
#  pragma GCC diagnostic ignored "-Wnoexcept-type"
# endif
#endif
  template <typename F,typename ...Args>
  auto
  addTaskF(F&& f,hal::queue_type qt,Args&&... args) -> decltype(task::createF(m_queue,f,std::forward<Args>(args)...))
  {
    return task::createF(get_queue(qt),f,std::forward<Args>(args)...);
  }
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif
public:
  device(std::shared_ptr<hal2::operations> ops, unsigned int idx);
  ~device();

  /**
   * Prepare the hal2 device for actual use
   *
   * If the device supports DMA threads then they are started by
   * this function.
   */
  void
  setup();

  /**
   * Open the device.
   *
   * @return True if device was opened, false if already open
   *
   * Throws if device could not be opened
   */
  virtual bool
  open();

  virtual void
  close();

  virtual xclDeviceHandle
  get_xcl_handle() const
  {
    return m_handle; // cast to xclDeviceHandle
  }

  xrt::device
  get_xrt_device() const
  {
    return m_handle;
  }

  std::shared_ptr<xrt_core::device>
  get_core_device() const;

  bool
  is_nodma() const;

  virtual void
  acquire_cu_context(const uuid& uuid,size_t cuidx,bool shared);

  virtual void
  release_cu_context(const uuid& uuid,size_t cuidx);

  virtual task::queue*
  getQueue(hal::queue_type qt)
  {
    return &m_queue[static_cast<qtype>(qt)];
  }

  virtual std::string
  getDriverLibraryName() const
  {
    return m_ops->getFileName();
  }

  virtual std::string
  getName() const
  {
    return get_device_info()->mName;
  }

  virtual unsigned int
  getBankCount() const
  {
    return get_device_info()->mDDRBankCount;
  }

  virtual size_t
  getDdrSize() const override
  {
    return get_device_info()->mDDRSize;
  }

  virtual size_t
  getAlignment() const
  {
    return get_device_info()->mDataAlignment;
  }

  virtual range<const unsigned short*>
  getClockFrequencies() const
  {
    return {get_device_info()->mOCLFrequency,get_device_info()->mOCLFrequency+4};
  }

  virtual std::ostream&
  printDeviceInfo(std::ostream& ostr) const;

  virtual size_t
  get_cdma_count() const
  {
    return get_device_info()->mNumCDMA;
  }

  virtual execbuffer_object_handle
  allocExecBuffer(size_t sz);

  virtual buffer_object_handle
  alloc(size_t sz, Domain domain, uint64_t memoryIndex, void* user_ptr);

  virtual buffer_object_handle
  alloc(const buffer_object_handle& bo, size_t sz, size_t offset);

  buffer_object_handle
  alloc_nodma(size_t sz, Domain domain, uint64_t memory_index, void* userptr);

  virtual void*
  alloc_svm(size_t sz);

  virtual void
  free_svm(void* svm_ptr);

  virtual event
  write(const buffer_object_handle& bo, const void* buffer, size_t sz, size_t offset,bool async);

  virtual event
  read(const buffer_object_handle& bo, void* buffer, size_t sz, size_t offset,bool async);

  virtual event
  sync(const buffer_object_handle& bo, size_t sz, size_t offset, direction dir, bool async);

  virtual event
  copy(const buffer_object_handle& dst_bo, const buffer_object_handle& src_bo, size_t sz, size_t dst_offset, size_t src_offset);

  virtual void
  fill_copy_pkt(const buffer_object_handle& dst_boh, const buffer_object_handle& src_boh
                ,size_t sz, size_t dst_offset, size_t src_offset,ert_start_copybo_cmd* pkt);

  virtual size_t
  read_register(size_t offset, void* buffer, size_t size);

  virtual size_t
  write_register(size_t offset, const void* buffer, size_t size);

  virtual void*
  map(const buffer_object_handle& bo);

  virtual void
  unmap(const buffer_object_handle& bo);

  virtual void*
  map(const execbuffer_object_handle& bo);

  virtual void
  unmap(const execbuffer_object_handle& bo);

  virtual int
  exec_buf(const execbuffer_object_handle& bo);

  virtual int
  exec_wait(int timeout_ms) const;

public:

  virtual int
  createWriteStream(hal::StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream);

  virtual int
  createReadStream(hal::StreamFlags flags, hal::StreamAttributes attr, uint64_t route, uint64_t flow, hal::StreamHandle *stream);

  virtual int
  closeStream(hal::StreamHandle stream);

  virtual hal::StreamBuf
  allocStreamBuf(size_t size, hal::StreamBufHandle *buf);

  virtual int
  freeStreamBuf(hal::StreamBufHandle buf);

  virtual ssize_t
  writeStream(hal::StreamHandle stream, const void* ptr, size_t size, hal::StreamXferReq* req);

  virtual ssize_t
  readStream(hal::StreamHandle stream, void* ptr, size_t size, hal::StreamXferReq* req);

  virtual int
  pollStreams(hal::StreamXferCompletions* comps, int min, int max, int* actual, int timeout);

  virtual int
  pollStream(hal::StreamHandle stream, hal::StreamXferCompletions* comps, int min, int max, int* actual, int timeout);

  virtual int
  setStreamOpt(hal::StreamHandle stream, int type, uint32_t val);

public:
  virtual bool
  is_imported(const buffer_object_handle& boh) const;

  virtual uint64_t
  getDeviceAddr(const buffer_object_handle& boh);

  virtual int
  getMemObjectFd(const buffer_object_handle& boh);

  virtual buffer_object_handle
  getBufferFromFd(const int fd, size_t& size, unsigned flags);

public:
  virtual hal::operations_result<int>
  loadXclBin(const xclBin* xclbin);

  virtual bool
  hasBankAlloc() const
  {
    // PCIe DSAs have device DDRs which allow bank allocation/selection
    // Zynq PL based devices set device id to 0xffff.
    return (get_device_info()->mDeviceId != 0xffff);
  }

  virtual hal::operations_result<ssize_t>
  readKernelCtrl(uint64_t offset,void* hbuf,size_t size)
  {
    if (!m_ops->mRead)
      return hal::operations_result<ssize_t>();
    return m_ops->mRead(m_handle,XCL_ADDR_KERNEL_CTRL,offset,hbuf,size);
  }

  virtual hal::operations_result<ssize_t>
  writeKernelCtrl(uint64_t offset,const void* hbuf,size_t size)
  {
    if (!m_ops->mWrite)
      return hal::operations_result<ssize_t>();
    return m_ops->mWrite(m_handle,XCL_ADDR_KERNEL_CTRL,offset,hbuf,size);
  }

  virtual hal::operations_result<int>
  reClock2(unsigned short region, unsigned short *freqMHz)
  {
    if (!m_ops->mReClock2)
      return hal::operations_result<int>();
    return m_ops->mReClock2(m_handle, region, freqMHz);
  }

  // Following functions are profiling functions
  virtual hal::operations_result<size_t>
  clockTraining(xclPerfMonType type)
  {
    if (!m_ops->mClockTraining)
      return hal::operations_result<size_t>();
    return m_ops->mClockTraining(m_handle,type);
  }

  virtual hal::operations_result<uint32_t>
  countTrace(xclPerfMonType type)
  {
    if (!m_ops->mCountTrace)
      return hal::operations_result<uint32_t>();
    return m_ops->mCountTrace(m_handle,type);
  }

  virtual hal::operations_result<double>
  getDeviceClock()
  {
    if (!m_ops->mGetDeviceClock)
      return hal::operations_result<double>();
    return m_ops->mGetDeviceClock(m_handle);
  }

  virtual hal::operations_result<size_t>
  getDeviceTime()
  {
    if (!m_ops->mGetDeviceTime)
      return hal::operations_result<size_t>();
    return m_ops->mGetDeviceTime(m_handle);
  }

  virtual hal::operations_result<double>
  getDeviceMaxRead()
  {
    if (!m_ops->mGetDeviceMaxRead)
      return hal::operations_result<double>();
    return m_ops->mGetDeviceMaxRead(m_handle);
  }

  virtual hal::operations_result<double>
  getDeviceMaxWrite()
  {
    if (!m_ops->mGetDeviceMaxWrite)
      return hal::operations_result<double>();
    return m_ops->mGetDeviceMaxWrite(m_handle);
  }

  virtual hal::operations_result<size_t>
  readCounters(xclPerfMonType type, xclCounterResults& results)
  {
    if (!m_ops->mReadCounters)
      return hal::operations_result<size_t>();
    return m_ops->mReadCounters(m_handle,type,results);
  }

  virtual hal::operations_result<size_t>
  debugReadIPStatus(xclDebugReadType type, void* results)
  {
    if (!m_ops->mDebugReadIPStatus)
      return hal::operations_result<size_t>();
    return m_ops->mDebugReadIPStatus(m_handle, type, (void*)results);
  }

  virtual hal::operations_result<size_t>
  readTrace(xclPerfMonType type, xclTraceResultsVector& vec)
  {
    if (!m_ops->mReadTrace)
      return hal::operations_result<size_t>();
    return m_ops->mReadTrace(m_handle,type, vec);
  }

  virtual hal::operations_result<void>
  xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
  {
    if (!m_ops->mRead)
      return hal::operations_result<void>();
    m_ops->mRead(m_handle, space, offset, hostBuf, size);
    return hal::operations_result<void>(0);
  }

  virtual hal::operations_result<void>
  xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
  {
    if (!m_ops->mWrite)
      return hal::operations_result<void>();
    m_ops->mWrite(m_handle, space, offset, hostBuf, size);
    return hal::operations_result<void>(0);
  }

  virtual hal::operations_result<ssize_t>
  xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset)
  {
    if (!m_ops->mUnmgdPread)
      return hal::operations_result<ssize_t>();
    return m_ops->mUnmgdPread(m_handle, flags, buf, count, offset);
  }

  virtual hal::operations_result<void>
  setProfilingSlots(xclPerfMonType type, uint32_t slots)
  {
    if (!m_ops->mSetProfilingSlots)
      return hal::operations_result<void>();
    m_ops->mSetProfilingSlots(m_handle,type,slots);
    return hal::operations_result<void>(0);
  }

  virtual hal::operations_result<uint32_t>
  getProfilingSlots(xclPerfMonType type)
  {
    if (!m_ops->mGetProfilingSlots)
      return hal::operations_result<uint32_t>();
    return m_ops->mGetProfilingSlots(m_handle,type);
  }

  virtual hal::operations_result<void>
  getProfilingSlotName(xclPerfMonType type, uint32_t slotnum,
                       char* slotName, uint32_t length)
  {
    if (!m_ops->mGetProfilingSlotName)
      return hal::operations_result<void>();
    m_ops->mGetProfilingSlotName(m_handle,type,slotnum,slotName,length);
    return hal::operations_result<void>(0);
  }

  virtual hal::operations_result<uint32_t>
  getProfilingSlotProperties(xclPerfMonType type, uint32_t slotnum)
  {
    if (!m_ops->mGetProfilingSlotProperties)
      return hal::operations_result<uint32_t>();
    return m_ops->mGetProfilingSlotProperties(m_handle,type,slotnum);
  }

  virtual hal::operations_result<void>
  configureDataflow(xclPerfMonType type, unsigned *ip_config)
  {
    if (!m_ops->mConfigureDataflow)
      return hal::operations_result<void>();
    m_ops->mConfigureDataflow(m_handle,type, ip_config);
    return hal::operations_result<void>(0);
  }

  virtual hal::operations_result<size_t>
  startCounters(xclPerfMonType type)
  {
    if (!m_ops->mStartCounters)
      return hal::operations_result<size_t>();
    return m_ops->mStartCounters(m_handle,type);
  }

  virtual hal::operations_result<size_t>
  startTrace(xclPerfMonType type, uint32_t options)
  {
    if (!m_ops->mStartTrace)
      return hal::operations_result<size_t>();
    return m_ops->mStartTrace(m_handle,type,options);
  }

  virtual hal::operations_result<size_t>
  stopCounters(xclPerfMonType type)
  {
    if (!m_ops->mStopCounters)
      return hal::operations_result<size_t>();
    return m_ops->mStopCounters(m_handle,type);
  }

  virtual hal::operations_result<size_t>
  stopTrace(xclPerfMonType type)
  {
    if (!m_ops->mStopTrace)
      return hal::operations_result<size_t>();
    return m_ops->mStopTrace(m_handle,type);
  }

  virtual void*
  getHalDeviceHandle() {
    return m_handle;
  }

  virtual hal::operations_result<uint32_t>
  getNumLiveProcesses()
  {
    if(!m_ops->mGetNumLiveProcesses)
      return hal::operations_result<uint32_t>();
    return m_ops->mGetNumLiveProcesses(m_handle);
  }

  virtual hal::operations_result<std::string>
  getSysfsPath(const std::string& subdev, const std::string& entry)
  {
    if (!m_ops->mGetSysfsPath)
      return hal::operations_result<std::string>();
    constexpr size_t max_path = 256;
    char path_buf[max_path];
    if (m_ops->mGetSysfsPath(m_handle, subdev.c_str(), entry.c_str(), path_buf, max_path)) {
      return hal::operations_result<std::string>();
    }
    path_buf[max_path - 1] = '\0';
    std::string sysfs_path = std::string(path_buf);
    return sysfs_path;
  }

  virtual hal::operations_result<std::string>
  getSubdevPath(const std::string& subdev, uint32_t idx)
  {
    if (!m_ops->mGetSubdevPath)
      return hal::operations_result<std::string>();
    constexpr size_t max_path = 256;
    char path_buf[max_path];
    if (m_ops->mGetSubdevPath(m_handle, subdev.c_str(), idx, path_buf, max_path)) {
      return hal::operations_result<std::string>();
    }
    path_buf[max_path - 1] = '\0';
    std::string path = std::string(path_buf);
    return path;
  }

  virtual hal::operations_result<std::string>
  getDebugIPlayoutPath()
  {
    if(!m_ops->mGetDebugIPlayoutPath)
      return hal::operations_result<std::string>();

    const size_t maxLen = 512;
    char path[maxLen];
    if(m_ops->mGetDebugIPlayoutPath(m_handle, path, maxLen)) {
      return hal::operations_result<std::string>();
    }
    path[maxLen - 1] = '\0';
    std::string pathStr(path);
    return pathStr;
  }

  virtual hal::operations_result<int>
  getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
  {
    if(!m_ops->mGetTraceBufferInfo)
      return hal::operations_result<int>();
    return m_ops->mGetTraceBufferInfo(m_handle, nSamples, traceSamples, traceBufSz);
  }

  hal::operations_result<int>
  readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
  {
    if(!m_ops->mReadTraceData)
      return hal::operations_result<int>();
    return m_ops->mReadTraceData(m_handle, traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample);
  }

  hal::operations_result<void>
  getDebugIpLayout(char* buffer, size_t size, size_t* size_ret)
  {
    if(!m_ops->mGetDebugIpLayout) {
      return hal::operations_result<void>();
    }
    m_ops->mGetDebugIpLayout(m_handle, buffer, size, size_ret);
    return hal::operations_result<void>(0);
  }




};

}} // hal2,xrt

#endif
