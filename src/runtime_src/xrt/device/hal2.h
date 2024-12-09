// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrt_device_hal2_h
#define xrt_device_hal2_h

#include "xrt/device/hal.h"

#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/detail/ert.h"

#include "core/common/device.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/query_requests.h"

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

using verbosity_level = xclVerbosityLevel;
using device_handle = xclDeviceHandle;
using device_info = xclDeviceInfo2;

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

  unsigned int m_idx;
  std::string m_filename;

  xrt::device m_handle;
  mutable boost::optional<hal2::device_info> m_devinfo;

  mutable std::mutex m_mutex;

  struct ExecBufferObject : hal::exec_buffer_object
  {
    std::unique_ptr<xrt_core::buffer_handle> handle;
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

  // helper function
  template<typename returnType, typename func>
  returnType
  return_or_default_on_throw(func&& f)
  {
    try {
      return std::forward<func>(f)();
    }   
    catch (...) {
      return {}; 
    }   
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
  device(unsigned int idx, std::string dll);
  ~device();

  /**
   * Prepare the hal2 device for actual use
   *
   * If the device supports DMA threads then they are started by
   * this function.
   */
  virtual void
  setup() override;

  /**
   * Open the device.
   *
   * @return True if device was opened, false if already open
   *
   * Throws if device could not be opened
   */
  virtual bool
  open() override;

  virtual void
  close() override;

  virtual xclDeviceHandle
  get_xcl_handle() const override
  {
    return m_handle; // cast to xclDeviceHandle
  }

  xrt::device
  get_xrt_device() const override
  {
    return m_handle;
  }

  std::shared_ptr<xrt_core::device>
  get_core_device() const override;

  bool
  is_nodma() const;

  virtual void
  acquire_cu_context(const uuid& uuid,size_t cuidx,bool shared) override;

  virtual void
  release_cu_context(const uuid& uuid,size_t cuidx) override;

  virtual task::queue*
  getQueue(hal::queue_type qt) override
  {
    return &m_queue[static_cast<qtype>(qt)];
  }

  virtual std::string
  getDriverLibraryName() const override
  {
    return m_filename;
  }

  virtual std::string
  getName() const override
  {
    return get_device_info()->mName;
  }

  virtual unsigned int
  getBankCount() const override
  {
    return get_device_info()->mDDRBankCount;
  }

  virtual size_t
  getDdrSize() const override
  {
    return get_device_info()->mDDRSize;
  }

  virtual size_t
  getAlignment() const override
  {
    return get_device_info()->mDataAlignment;
  }

  virtual range<const unsigned short*>
  getClockFrequencies() const override
  {
    return {get_device_info()->mOCLFrequency,get_device_info()->mOCLFrequency+4};
  }

  virtual std::ostream&
  printDeviceInfo(std::ostream& ostr) const override;

  virtual size_t
  get_cdma_count() const override
  {
    return get_device_info()->mNumCDMA;
  }

  virtual execbuffer_object_handle
  allocExecBuffer(size_t sz) override;

  virtual buffer_object_handle
  alloc(size_t sz, Domain domain, uint64_t memoryIndex, void* user_ptr) override;

  virtual buffer_object_handle
  alloc(const buffer_object_handle& bo, size_t sz, size_t offset) override;

  buffer_object_handle
  alloc_nodma(size_t sz, Domain domain, uint64_t memory_index, void* userptr);

  virtual void*
  alloc_svm(size_t sz) override;

  virtual void
  free_svm(void* svm_ptr) override;

  virtual event
  write(const buffer_object_handle& bo, const void* buffer, size_t sz, size_t offset,bool async) override;

  virtual event
  read(const buffer_object_handle& bo, void* buffer, size_t sz, size_t offset,bool async) override;

  virtual event
  sync(const buffer_object_handle& bo, size_t sz, size_t offset, direction dir, bool async) override;

  virtual event
  copy(const buffer_object_handle& dst_bo, const buffer_object_handle& src_bo, size_t sz, size_t dst_offset, size_t src_offset) override;

  virtual size_t
  read_register(size_t offset, void* buffer, size_t size) override;

  virtual size_t
  write_register(size_t offset, const void* buffer, size_t size) override;

  virtual void*
  map(const buffer_object_handle& bo) override;

  virtual void
  unmap(const buffer_object_handle& bo) override;

  virtual void*
  map(const execbuffer_object_handle& bo) override;

  virtual void
  unmap(const execbuffer_object_handle& bo) override;

  virtual int
  exec_buf(const execbuffer_object_handle& bo) override;

  virtual int
  exec_wait(int timeout_ms) const override;

public:
  virtual bool
  is_imported(const buffer_object_handle& boh) const override;

  virtual uint64_t
  getDeviceAddr(const buffer_object_handle& boh) override;

  virtual int
  getMemObjectFd(const buffer_object_handle& boh) override;

  virtual buffer_object_handle
  getBufferFromFd(const int fd, size_t& size, unsigned flags) override;

public:
  virtual hal::operations_result<int>
  loadXclBin(const xclBin* xclbin) override;

  virtual bool
  hasBankAlloc() const override
  {
    // PCIe DSAs have device DDRs which allow bank allocation/selection
    // Zynq PL based devices set device id to 0xffff.
    return (get_device_info()->mDeviceId != 0xffff);
  }

  virtual hal::operations_result<ssize_t>
  readKernelCtrl(uint64_t offset,void* hbuf,size_t size) override
  {
    return return_or_default_on_throw<ssize_t>([&]() {
        get_core_device()->xread(XCL_ADDR_KERNEL_CTRL, offset, hbuf, size);
        return static_cast<ssize_t>(size);
      }
    );
  }

  virtual hal::operations_result<ssize_t>
  writeKernelCtrl(uint64_t offset,const void* hbuf,size_t size) override
  {
    return return_or_default_on_throw<ssize_t>([&]() {
        get_core_device()->xwrite(XCL_ADDR_KERNEL_CTRL, offset, hbuf, size);
        return static_cast<ssize_t>(size);
      }
    );
  }

  virtual hal::operations_result<int>
  reClock2(unsigned short region, unsigned short *freqMHz) override
  {
    return return_or_default_on_throw<int>([&]() {
        get_core_device()->reclock(freqMHz);
        return 0;
      }
    );
  }

  // Following functions are profiling functions
  virtual hal::operations_result<size_t>
  clockTraining(xdp::MonitorType type) override
  {
    // xcl api of this function does nothing
    return {};
  }

  virtual hal::operations_result<uint32_t>
  countTrace(xdp::MonitorType type) override
  {
    // xcl api of this function does nothing
    return {};
  }

  virtual hal::operations_result<double>
  getDeviceClock() override
  {
    return return_or_default_on_throw<double>([&]() {
        return xrt_core::device_query<xrt_core::query::device_clock_freq_mhz>(get_core_device());
      }
    );
  }

  virtual hal::operations_result<double>
  getHostMaxRead() override
  {
    return return_or_default_on_throw<double>([&]() {
        return xrt_core::device_query<xrt_core::query::host_max_bandwidth_mbps>(get_core_device(), true);
      }
    );
  }

  virtual hal::operations_result<double>
  getHostMaxWrite() override
  {
    return return_or_default_on_throw<double>([&]() {
        return xrt_core::device_query<xrt_core::query::host_max_bandwidth_mbps>(get_core_device(), false);
      }
    );
  }

  virtual hal::operations_result<double>
  getKernelMaxRead() override
  {
    return return_or_default_on_throw<double>([&]() {
        return xrt_core::device_query<xrt_core::query::kernel_max_bandwidth_mbps>(get_core_device(), true);
      }
    );
  }

  virtual hal::operations_result<double>
  getKernelMaxWrite() override
  {
    return return_or_default_on_throw<double>([&]() {
        return xrt_core::device_query<xrt_core::query::kernel_max_bandwidth_mbps>(get_core_device(), false);
      }
    );
  }

  virtual hal::operations_result<void>
  xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) override
  {
    return return_or_default_on_throw<int>([&]() {
        get_core_device()->xread(space, offset, hostBuf, size);
        return static_cast<int>(size);
      }
    );
  }

  virtual hal::operations_result<void>
  xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) override
  {
    return return_or_default_on_throw<int>([&]() {
        get_core_device()->xwrite(space, offset, hostBuf, size);
        return static_cast<int>(size);
      }
    );
  }

  virtual hal::operations_result<ssize_t>
  xclUnmgdPread(unsigned /*flags*/, void *buf, size_t count, uint64_t offset) override
  {
    return return_or_default_on_throw<ssize_t>([&]() {
        get_core_device()->unmgd_pread(buf, count, offset);
        return 0;
      }
    );
  }

  virtual hal::operations_result<size_t>
  stopCounters(xdp::MonitorType type) override
  {
    // xcl api for this function does nothingm
    return {};
  }

  virtual hal::operations_result<size_t>
  stopTrace(xdp::MonitorType type) override
  {
    // xcl api for this function does nothing
    return {};
  }

  virtual hal::operations_result<void>
  setProfilingSlots(xdp::MonitorType type, uint32_t slots) override
  {
    // xcl api for this function does nothing
    return {};
  }

  virtual hal::operations_result<uint32_t>
  getProfilingSlots(xdp::MonitorType type) override
  {
    // xcl api for this function does nothing
    return {};
  }

  virtual hal::operations_result<void>
  getProfilingSlotName(xdp::MonitorType type, uint32_t slotnum,
                       char* slotName, uint32_t length) override
  {
    // xcl api for this function does nothing
    return {};
  }

  virtual hal::operations_result<uint32_t>
  getProfilingSlotProperties(xdp::MonitorType type, uint32_t slotnum) override
  {
    // xcl api for this function does nothing
    return {};
  }

  virtual hal::operations_result<void>
  configureDataflow(xdp::MonitorType type, unsigned *ip_config) override
  {
    // xcl api for this function does nothing
    return {};
  }


  virtual hal::operations_result<size_t>
  startCounters(xdp::MonitorType type) override
  {
    // xcl api for this function does nothing
    return {};
  }

  virtual hal::operations_result<size_t>
  startTrace(xdp::MonitorType type, uint32_t options) override
  {
    // xcl api for this function does nothing
    return {};
  }

  virtual void*
  getHalDeviceHandle() override {
    return m_handle;
  }

  virtual hal::operations_result<uint32_t>
  getNumLiveProcesses() override
  {
    return return_or_default_on_throw<uint32_t>([&]() {
        return xrt_core::device_query<xrt_core::query::num_live_processes>(get_core_device());
      }
    );
  }

  virtual hal::operations_result<std::string>
  getSubdevPath(const std::string& subdev, uint32_t idx) override
  {
    return return_or_default_on_throw<std::string>([&]() {
        xrt_core::query::sub_device_path::args arg = {subdev, idx};
        return xrt_core::device_query<xrt_core::query::sub_device_path>(get_core_device(), arg);
      }
    );
  }

  virtual hal::operations_result<std::string>
  getDebugIPlayoutPath() override
  {
    return return_or_default_on_throw<std::string>([&]() {
        constexpr unsigned int sysfs_max_path_length = 512;
        return xrt_core::device_query<xrt_core::query::debug_ip_layout_path>(get_core_device(), sysfs_max_path_length);
      }
    );
  }

  virtual hal::operations_result<int>
  getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz) override
  {
    return return_or_default_on_throw<int>([&]() {
        auto result = xrt_core::device_query<xrt_core::query::trace_buffer_info>(get_core_device(), nSamples);
        traceSamples = result.samples;
        traceBufSz = result.buf_size;
        return 0;
      }
    );
  }

  hal::operations_result<int>
  readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample) override
  {
    const int size = traceBufSz / 4;  // traceBufSz is in number of bytes
    std::vector<uint32_t> trace_data(size);
    xrt_core::query::read_trace_data::args arg = {traceBufSz, numSamples, ipBaseAddress, wordsPerSample};

    return return_or_default_on_throw<int>([&]() {
        trace_data = xrt_core::device_query<xrt_core::query::read_trace_data>(get_core_device(), arg);
        std::copy(trace_data.data(), trace_data.data() + size, static_cast<uint32_t*>(traceBuf));
        return static_cast<int>(traceBufSz);
      }
    );
  }

  hal::operations_result<void>
  getDebugIpLayout(char* buffer, size_t size, size_t* size_ret) override
  {
    return return_or_default_on_throw<int>([&]() {
        auto vec_data = xrt_core::device_query<xrt_core::query::debug_ip_layout_raw>(get_core_device());
        std::copy(vec_data.data(), vec_data.data() + size, buffer);
        return 0;
      }
    );
  }

  // Functions that use Hal operations
  // TODO : cleanup and remove these functions
  //        to completely remove Hal operations

  virtual hal::operations_result<size_t>
  getDeviceTime() override
  {
    return return_or_default_on_throw<ssize_t>([&]() {
        return get_core_device()->get_device_timestamp();
      }
    );
  }

  virtual hal::operations_result<std::string>
  getSysfsPath(const std::string& subdev, const std::string& entry) override
  {
    return return_or_default_on_throw<std::string>([&]() {
        return get_core_device()->get_sysfs_path(subdev, entry);
      }
    );
  }
};

}} // hal2,xrt

#endif
