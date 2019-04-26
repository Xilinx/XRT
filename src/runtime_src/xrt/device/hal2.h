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

#ifndef xrt_device_hal2_h
#define xrt_device_hal2_h

#include "xrt/device/hal.h"
#include "xrt/device/halops2.h"
#include "xrt/device/PMDOperations.h"

#include "driver/include/ert.h"

#include <cassert>

#include <functional>
#include <type_traits>
#include <cstring>
#include <memory>
#include <map>

namespace xrt { namespace hal2 {

namespace hal  = xrt::hal;
namespace hal2 = xrt::hal2;
using BufferObjectHandle = hal::BufferObjectHandle;
using ExecBufferObjectHandle = hal::ExecBufferObjectHandle;
using svmbomap_type = std::map<void *, BufferObjectHandle>;
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

class device : public xrt::hal::device
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

  hal2::device_handle m_handle;
  hal2::device_info m_devinfo;

  struct BufferObject : hal::buffer_object
  {
    unsigned int handle = 0xffffffff;
    uint64_t deviceAddr = 0xffffffffffffffff;
    void *hostAddr = nullptr;
    size_t size = 0;
    size_t offset = 0;
    xclBOKind kind;
    unsigned int flags = 0;
    hal2::device_handle owner = nullptr;
    BufferObjectHandle parent = nullptr;
    bool imported = false;
  };

  struct ExecBufferObject : hal::exec_buffer_object
  {
    unsigned int handle = 0xffffffff;
    void* data = nullptr;
    size_t size = 0;
    hal2::device_handle owner = nullptr;
  };

  BufferObject*
  getBufferObject(const BufferObjectHandle& boh) const;

  ExecBufferObject*
  getExecBufferObject(const ExecBufferObjectHandle& boh) const;

  void
  openOrError() const
  {
    if (!m_handle)
      throw std::runtime_error("hal::device is not open");
  }

  int
  getDeviceInfo(hal2::device_info *info)  const
  {
    std::memset(info,0,sizeof(hal2::device_info));
#ifdef PMD_OCL
    assert(0);
    return 0;
#else
    return m_ops->mGetDeviceInfo(m_handle,info);
#endif
  }

  task::queue&
  get_queue(hal::queue_type qt)
  {
    return m_queue[static_cast<qtype>(qt)];
  }

  /**
   * emplace, erase, and find operations for m_svmbomap
   */
  virtual void
  emplaceSVMBufferObjectMap(const BufferObjectHandle& boh, void* ptr);

  virtual void
  eraseSVMBufferObjectMap(void* ptr);

  virtual BufferObjectHandle
  svm_bo_lookup(void* ptr);

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

#pragma GCC diagnostic push
#if __GNUC__  >= 7
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif
  template <typename F,typename ...Args>
  auto
  addTaskF(F&& f,hal::queue_type qt,Args&&... args) -> decltype(task::createF(m_queue,f,std::forward<Args>(args)...))
  {
    return task::createF(get_queue(qt),f,std::forward<Args>(args)...);
  }
#pragma GCC diagnostic pop
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

  virtual bool
  open(const char* log, hal::verbosity_level level)
  {
    bool retval = false;
    if (m_handle)
      throw std::runtime_error("device is already open");
#ifdef PMD_OCL
    assert(0);
#else
    m_handle=m_ops->mOpen(m_idx,log,static_cast<hal2::verbosity_level>(level));
    if (m_handle)
      retval = true;
#endif
    getDeviceInfo(&m_devinfo);
    return retval;
  }

  virtual void
  close()
  {
    if (m_handle) {
      m_ops->mClose(m_handle);
      m_handle=nullptr;
    }
  }

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
    return m_devinfo.mName;
  }

  virtual unsigned int
  getBankCount() const
  {
    return m_devinfo.mDDRBankCount;
  }

  virtual size_t
  getDdrSize() const override
  {
    return m_devinfo.mDDRSize;
  }

  virtual size_t
  getAlignment() const
  {
    openOrError();
    return m_devinfo.mDataAlignment;
  }

  virtual range<const unsigned short*>
  getClockFrequencies() const
  {
    return {m_devinfo.mOCLFrequency,m_devinfo.mOCLFrequency+4};
  }

  virtual std::ostream&
  printDeviceInfo(std::ostream& ostr) const;

  virtual size_t
  get_cdma_count() const
  {
    return m_devinfo.mNumCDMA;
  }

  virtual ExecBufferObjectHandle
  allocExecBuffer(size_t sz);

  virtual BufferObjectHandle
  alloc(size_t sz);

  virtual BufferObjectHandle
  alloc(size_t sz,void* userptr);

  virtual BufferObjectHandle
  alloc(size_t sz, Domain domain, uint64_t memoryIndex, void* user_ptr);

  virtual BufferObjectHandle
  alloc(const BufferObjectHandle& bo, size_t sz, size_t offset);

  virtual void*
  alloc_svm(size_t sz);

  virtual BufferObjectHandle
  import(const BufferObjectHandle& bo);

  virtual void
  free(const BufferObjectHandle& bo);

  virtual void
  free_svm(void* svm_ptr);

  virtual event
  write(const BufferObjectHandle& bo, const void* buffer, size_t sz, size_t offset,bool async);

  virtual event
  read(const BufferObjectHandle& bo, void* buffer, size_t sz, size_t offset,bool async);

  virtual event
  sync(const BufferObjectHandle& bo, size_t sz, size_t offset, direction dir, bool async);

  virtual event
  copy(const BufferObjectHandle& dst_bo, const BufferObjectHandle& src_bo, size_t sz, size_t dst_offset, size_t src_offset);

  virtual void
  fill_copy_pkt(const BufferObjectHandle& dst_boh, const BufferObjectHandle& src_boh
                ,size_t sz, size_t dst_offset, size_t src_offset,ert_start_copybo_cmd* pkt);

  virtual size_t
  read_register(size_t offset, void* buffer, size_t size);

  virtual size_t
  write_register(size_t offset, const void* buffer, size_t size);

  virtual void*
  map(const BufferObjectHandle& bo);

  virtual void
  unmap(const BufferObjectHandle& bo);

  virtual void*
  map(const ExecBufferObjectHandle& bo);

  virtual void
  unmap(const ExecBufferObjectHandle& bo);

  virtual int
  exec_buf(const ExecBufferObjectHandle& bo);

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

public:
  virtual bool
  is_imported(const BufferObjectHandle& boh) const;

  virtual uint64_t
  getDeviceAddr(const BufferObjectHandle& boh);

  virtual int
  getMemObjectFd(const BufferObjectHandle& boh);

  virtual BufferObjectHandle
  getBufferFromFd(const int fd, size_t& size, unsigned flags);

public:
  virtual hal::operations_result<int>
  lockDevice()
  {
    if (!m_ops->mLockDevice)
      return hal::operations_result<int>();
    return m_ops->mLockDevice(m_handle);
  }

  virtual hal::operations_result<int>
  unlockDevice()
  {
    if (!m_ops->mUnlockDevice)
      return hal::operations_result<int>();
    return m_ops->mUnlockDevice(m_handle);
  }

  virtual hal::operations_result<int>
  loadXclBin(const xclBin* xclbin)
  {
    if (!m_ops->mLoadXclBin)
      return hal::operations_result<int>();

    hal::operations_result<int> ret = m_ops->mLoadXclBin(m_handle,xclbin);
    // refresh device info on successful load
    if (!ret.get())
      getDeviceInfo(&m_devinfo);

    return ret;
  }

  virtual bool
  hasBankAlloc() const
  {
    // PCIe DSAs have device DDRs which allow bank allocation/selection
    // Zynq PL based devices set device id to 0xffff.
    return (m_devinfo.mDeviceId != 0xffff);
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
  writeHostEvent(xclPerfMonEventType type, xclPerfMonEventID id)
  {
    if (!m_ops->mWriteHostEvent)
      return hal::operations_result<void>();
    m_ops->mWriteHostEvent(m_handle,type,id);
    return hal::operations_result<void>(0);
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

  virtual hal::operations_result<std::string>
  getSysfsPath(const std::string& subdev, const std::string& entry)
  {
    if (!m_ops->mGetSysfsPath)
      return hal::operations_result<std::string>();
    size_t max_path = 256;
    char path_buf[max_path];
    if (m_ops->mGetSysfsPath(m_handle, subdev.c_str(), entry.c_str(), path_buf, max_path)) {
      return hal::operations_result<std::string>();
    }
    path_buf[max_path - 1] = '\0';
    std::string sysfs_path = std::string(path_buf);
    return sysfs_path;
  }
};

}} // hal2,xrt

#endif
