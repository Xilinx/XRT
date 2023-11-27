/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_PROFILE_DEVICE_TRACE_OFFLOAD_H_
#define XDP_PROFILE_DEVICE_TRACE_OFFLOAD_H_

#include "core/common/message.h"
#include "xdp/config.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/device_trace_logger.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace xdp {

enum class OffloadThreadStatus {
  IDLE,
  RUNNING,
  STOPPING,
  STOPPED
};

enum class OffloadThreadType {
  TRACE,
  CLOCK_TRAIN
};

struct TraceBufferInfo {
  size_t   bufId;
  uint64_t alloc_size;
  uint64_t used_size;
  uint64_t offset;
  uint64_t address;
  uint64_t prv_wordcount;
  uint32_t rollover_count;
  bool     full;
  bool     offload_done;
  bool     big_trace_warn_done;
  
  TraceBufferInfo()
    : bufId(0),
      alloc_size(0),
      used_size(0),
      offset(0),
      address(0),
      prv_wordcount(0),
      rollover_count(0),
      full(false),
      offload_done(false),
      big_trace_warn_done(false)
  {}
       
};

struct Ts2mmInfo {
  size_t   num_ts2mm;
  uint64_t full_buf_size;

  std::vector<TraceBufferInfo> buffers;

  //Circular Buffer Tracking
  bool use_circ_buf;
  // 100 mb of trace per second
  uint64_t circ_buf_min_rate = TS2MM_DEF_BUF_SIZE * 100;
  uint64_t circ_buf_cur_rate;

  std::queue<std::unique_ptr<unsigned char[]>> data_queue;
  std::queue<uint64_t> size_queue;
  std::mutex process_queue_lock;

  Ts2mmInfo()
    : num_ts2mm(0),
      full_buf_size(0),
      use_circ_buf(false),
      circ_buf_cur_rate(0)
  {}
  
};

class DeviceTraceLogger;

#define debug_stream \
if(!m_debug); else std::cout

class DeviceTraceOffload {
public:
  XDP_CORE_EXPORT
  DeviceTraceOffload(DeviceIntf* dInt, DeviceTraceLogger* dTraceLogger,
                     uint64_t offload_sleep_ms, uint64_t trbuf_sz);
  XDP_CORE_EXPORT
  virtual ~DeviceTraceOffload();
  XDP_CORE_EXPORT
  void start_offload(OffloadThreadType type);
  XDP_CORE_EXPORT
  void stop_offload();
  XDP_CORE_EXPORT
  virtual bool read_trace_init(bool circ_buf, const std::vector<uint64_t>&);
  XDP_CORE_EXPORT
  virtual void read_trace_end();
  XDP_CORE_EXPORT
  void train_clock();
  XDP_CORE_EXPORT
  void process_trace();
  XDP_CORE_EXPORT
  bool trace_buffer_full();

public:
  bool has_fifo() {
    return dev_intf->hasFIFO();
  };

  bool has_ts2mm() {
    return dev_intf->hasTs2mm();
  };

  void read_trace() {
    m_read_trace(true);
  };

  bool using_circular_buffer( uint64_t& min_offload_rate,
                              uint64_t& requested_offload_rate) {
    min_offload_rate = ts2mm_info.circ_buf_min_rate;
    requested_offload_rate = ts2mm_info.circ_buf_cur_rate;
    return ts2mm_info.use_circ_buf;
  };

  inline OffloadThreadStatus get_status() {
    std::lock_guard<std::mutex> lock(status_lock);
    return status;
  };

  inline bool continuous_offload() { return continuous ; }
  inline void set_continuous(bool value = true) { continuous = value ; }

private:
  void read_trace_fifo(bool force=true);
  void read_trace_s2mm(bool force=true);
  uint64_t read_trace_s2mm_partial();
  bool init_s2mm(bool circ_buf, const std::vector<uint64_t> &);
  void reset_s2mm();
  bool should_continue();
  void train_clock_continuous();
  void offload_device_continuous();
  void offload_finished();
  void process_trace_continuous();
  bool sync_and_log(uint64_t index);

protected:
  DeviceIntf* dev_intf;
  bool m_initialized = false;
  bool m_debug = false; /* Enable Output stream for log */

private:
  DeviceTraceLogger* deviceTraceLogger;
  std::function<void(bool)> m_read_trace;
  Ts2mmInfo ts2mm_info;

  // fifo doesn't support circular buffer mode
  bool fifo_full = false;

  // Continuous offload
  std::mutex status_lock;
  uint64_t sleep_interval_ms;
  OffloadThreadStatus status = OffloadThreadStatus::IDLE;
  std::thread offload_thread;
  std::thread process_thread;
  bool continuous = false;

  // Clock Training Params
  bool m_force_clk_train = true;
  std::chrono::time_point<std::chrono::system_clock> m_prev_clk_train_time;

  // Internal flags to end trace processing thread
  std::atomic<bool> m_process_trace;
  std::atomic<bool> m_process_trace_done;

  // Internal flags to keep track of warnings
  std::once_flag ts2mm_queue_warning_flag;
  std::once_flag fifo_full_warning_flag;
  std::once_flag ts2mm_full_warning_flag;
};

}

#endif
