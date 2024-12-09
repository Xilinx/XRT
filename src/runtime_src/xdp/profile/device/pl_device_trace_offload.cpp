/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include "xdp/profile/device/pl_device_trace_offload.h"
#include "xdp/profile/device/pl_device_trace_logger.h"
#include "xrt/experimental/xrt_profile.h"

namespace xdp {

PLDeviceTraceOffload::
PLDeviceTraceOffload
  ( PLDeviceIntf* dInt
  , PLDeviceTraceLogger* dTraceLogger
  , uint64_t sleep_interval_ms
  , uint64_t trbuf_sz
  )
  : dev_intf(dInt)
  , deviceTraceLogger(dTraceLogger)
  , sleep_interval_ms(sleep_interval_ms)
  , m_prev_clk_train_time(std::chrono::system_clock::now())
  , m_process_trace(false)
  , m_process_trace_done(false)
{
  // Select appropriate reader
  if (has_fifo()) {
    m_read_trace = std::bind(&PLDeviceTraceOffload::read_trace_fifo, this, std::placeholders::_1);
  } else {
    m_read_trace = std::bind(&PLDeviceTraceOffload::read_trace_s2mm, this, std::placeholders::_1);
  }

  ts2mm_info.num_ts2mm = dev_intf->getNumberTS2MM();
  ts2mm_info.full_buf_size = trbuf_sz;
}

PLDeviceTraceOffload::
~PLDeviceTraceOffload()
{
  stop_offload();
  if (offload_thread.joinable()) {
    offload_thread.join();
  }
  if (process_thread.joinable()) {
    process_thread.join();
  }
}

void PLDeviceTraceOffload::
offload_device_continuous()
{
  if (!m_initialized) {
    offload_finished();
    return;
  }

  while (should_continue()) {
    train_clock();
    // Can't flush datamover in middle of offload
    m_read_trace(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }

  // Do final forced read
  // Note : Passing "true" also flushes and resets the datamover
  m_read_trace(true);

  // Stop processing thread
  m_process_trace = false;
  while (!m_process_trace_done);

  // Clear all state and add approximations
  read_trace_end();

  // Tell external plugin that offload has finished
  offload_finished();
}

void PLDeviceTraceOffload::
train_clock_continuous()
{
  while (should_continue()) {
    train_clock();
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }

  offload_finished();
}

void PLDeviceTraceOffload::
process_trace_continuous()
{
  if (!has_ts2mm())
    return;

  m_process_trace = true;
  m_process_trace_done = false;
  while (m_process_trace)
  {
    process_trace();
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }
  // One last time
  process_trace();
  m_process_trace_done = true;
}

void PLDeviceTraceOffload::
process_trace()
{
  if (!has_ts2mm())
    return;

  bool q_read = false;
  bool q_empty = true;
  std::unique_ptr<unsigned char[]> buf;
  uint64_t size = 0;
  do {
    q_read=false;
    ts2mm_info.process_queue_lock.lock();
    if (!ts2mm_info.data_queue.empty()) {
      buf = std::move(ts2mm_info.data_queue.front());
      size = ts2mm_info.size_queue.front();
      ts2mm_info.data_queue.pop();
      ts2mm_info.size_queue.pop();
      q_read = true;
      q_empty = ts2mm_info.data_queue.empty();
    }
    if (ts2mm_info.size_queue.size() > TS2MM_QUEUE_SZ_WARN_THRESHOLD) {
      std::call_once(ts2mm_queue_warning_flag, [](){
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_QUEUE_SZ);
      });
    }
    ts2mm_info.process_queue_lock.unlock();

    // Processing takes a lot more time compared to everything else
    if (q_read) {
      debug_stream << "Process " << size << " bytes of trace" << std::endl;
      deviceTraceLogger->processTraceData(buf.get(), size) ;
      buf.reset();
    }
  } while (!q_empty);
}

bool PLDeviceTraceOffload::
should_continue()
{
  std::lock_guard<std::mutex> lock(status_lock);
  return status == OffloadThreadStatus::RUNNING;
}

void PLDeviceTraceOffload::
start_offload(OffloadThreadType type)
{
  if (status == OffloadThreadStatus::RUNNING)
    return;

  std::lock_guard<std::mutex> lock(status_lock);
  status = OffloadThreadStatus::RUNNING;

  if (type == OffloadThreadType::TRACE) {
    offload_thread = std::thread(&PLDeviceTraceOffload::offload_device_continuous, this);
    process_thread = std::thread(&PLDeviceTraceOffload::process_trace_continuous, this);
  } else if (type == OffloadThreadType::CLOCK_TRAIN) {
    offload_thread = std::thread(&PLDeviceTraceOffload::train_clock_continuous, this);
  }

}

void PLDeviceTraceOffload::
stop_offload()
{
  std::lock_guard<std::mutex> lock(status_lock);
  if (status == OffloadThreadStatus::STOPPED) return ;
  status = OffloadThreadStatus::STOPPING;
}

void PLDeviceTraceOffload::
offload_finished()
{
  std::lock_guard<std::mutex> lock(status_lock);
  if (status == OffloadThreadStatus::STOPPED) return ;
  status = OffloadThreadStatus::STOPPED;
}

void PLDeviceTraceOffload::
train_clock()
{
  auto now = std::chrono::system_clock::now();
  auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_prev_clk_train_time).count();

  // Clock training data is accurate upto 3 seconds
  // 500 ms is a reasonable time
  // No need of making it user configurable
  bool enough_time_passed = milliseconds >= 500 ? true: false;

  if (enough_time_passed || m_force_clk_train) {
    dev_intf->clockTraining(m_force_clk_train);
    m_prev_clk_train_time = now;
    debug_stream
      << "INFO Enough Time Passed.. Call Clock Training" << std::endl;
  }

  // Don't force continuous training for old IP
  m_force_clk_train = false;
}

void PLDeviceTraceOffload::
read_trace_fifo(bool)
{
  debug_stream
    << "PLDeviceTraceOffload::read_trace_fifo " << std::endl;

  // Disable using fifo as circular buffer
  if (fifo_full)
    return;

  uint64_t num_packets = 0;
  uint64_t numBytes = 0 ;
#ifndef _WIN32
  do {
#endif
    uint32_t* buf = nullptr ;
    numBytes = dev_intf->readTrace(buf) ; // Should allocate buf
    deviceTraceLogger->processTraceData(buf, numBytes) ;
    num_packets += numBytes / sizeof(uint64_t) ;
    if (buf)
      delete [] buf ;
#ifndef _WIN32
  } while (numBytes != 0);
#endif

  // Check if fifo is full
  if (!fifo_full) {
    auto fifo_size = dev_intf->getFifoSize();

    // hw emulation has infinite fifo
    if ((num_packets >= fifo_size) && (xdp::getFlowMode() == xdp::Flow::HW))
      fifo_full = true;
  }
}

bool PLDeviceTraceOffload::
read_trace_init(bool circ_buf, const std::vector<uint64_t> &buf_sizes)
{
  if (has_ts2mm()) {
    m_initialized = init_s2mm(circ_buf, buf_sizes);
  } else if (has_fifo()) {
    m_initialized = true;
  } else {
    m_initialized = false;
  }
  return m_initialized;
}

void PLDeviceTraceOffload::
read_trace_end()
{
  // Trace logger will clear it's state and add approximations 
  // for pending events
  deviceTraceLogger->endProcessTraceData();

  // Add event markers at end of trace data
  bool isFIFOFull = fifo_full;
  bool isTS2MMFull = (dev_intf->hasTs2mm() && trace_buffer_full()) ? true : false;
  deviceTraceLogger->addEventMarkers(isFIFOFull, isTS2MMFull);

  if (dev_intf->hasTs2mm()) {
    reset_s2mm();
    m_initialized = false;
  }
}

void PLDeviceTraceOffload::
read_trace_s2mm(bool force)
{
  for (uint64_t i = 0; i < ts2mm_info.num_ts2mm; i++) {
    auto& bd = ts2mm_info.buffers[i];

    if (bd.offload_done)
      continue;

    auto bytes_written = dev_intf->getWordCountTs2mm(i, force) * TRACE_PACKET_SIZE;
    auto bytes_read = bd.rollover_count * bd.alloc_size + bd.used_size;

    // Offload cannot keep up with the DMA
    if (bytes_written > bytes_read + bd.alloc_size) {
      // Don't read any data
      bd.offload_done = true;

       debug_stream
        << "ts2mm_ " << i << " Reading from 0x"
        << std::hex << bd.offset << " to 0x" << bd.used_size << std::dec
        << " Bytes Read : " << bytes_read
        << " Bytes Written : " << bytes_written
        << " Rollovers : " << bd.rollover_count
        << std::endl;

      // Add warnings and user markers
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_CIRC_BUF_OVERWRITE);
      xrt::profile::user_event events;
      events.mark("Trace Buffer Overwrite Detected");
      // Fatal condition. Abort offload
      stop_offload();
      return;
    }

    // Start Offload from previous offset
    bd.offset = bd.used_size;
    if (bd.offset == bd.alloc_size) {
      if (!ts2mm_info.use_circ_buf) {
        bd.offload_done = true;
        continue;
      }
      bd.rollover_count++;
      bd.offset = 0;
    }

    // End Offload at this offset
    // limit size to not cross circular buffer boundary
    uint64_t cir_buf_rollover_bytes = 0;
    bd.used_size = bytes_written - bd.rollover_count * bd.alloc_size;
    if (bd.used_size > bd.alloc_size) {
      cir_buf_rollover_bytes = bd.used_size - bd.alloc_size;
      bd.used_size = bd.alloc_size;
    }

    if (bd.offset != bd.used_size) {
      debug_stream
        << "ts2mm_" << i << " Reading from 0x"
        << std::hex << bd.offset << " to 0x" << bd.used_size << std::dec
        << " Bytes Read : " << bytes_read
        << " Bytes Written : " << bytes_written
        << " Rollovers : " << bd.rollover_count
        << std::endl;
    }

    if (!sync_and_log(i))
      continue;

    // Do another sync if we're crossing circular buffer boundary
    if (ts2mm_info.use_circ_buf && cir_buf_rollover_bytes) {
      // Start from 0
      bd.rollover_count++;
      bd.offset = 0;
      // End at leftover bytes
      bd.used_size = cir_buf_rollover_bytes;

      debug_stream
        << "Circular buffer boundary read from 0x0 to 0x: "
        << std::hex << cir_buf_rollover_bytes << std::dec << std::endl;

      sync_and_log(i);
    }
  }
}

bool PLDeviceTraceOffload::
sync_and_log(uint64_t index)
{
  auto& bd = ts2mm_info.buffers[index];

  // No data or invalid settings
  if (bd.offset >= bd.used_size)
    return false;

  uint64_t nBytes = bd.used_size - bd.offset;
  auto start = std::chrono::steady_clock::now();
  void* host_buf = dev_intf->syncTraceBuf(bd.bufId, bd.offset, nBytes);
  auto end = std::chrono::steady_clock::now();

  debug_stream
    << "ts2mm_" << index << " : sync : "
    << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
    << " µs" << " nBytes : " << nBytes << std::endl;

  if (!host_buf) {
    bd.offload_done = true;
    return false;
  }

  auto tmp = std::make_unique<unsigned char[]>(nBytes);
  std::memcpy(tmp.get(), host_buf, nBytes);
  // Push new data into queue for processing
  ts2mm_info.process_queue_lock.lock();
  ts2mm_info.data_queue.push(std::move(tmp));
  ts2mm_info.size_queue.push(nBytes);
  ts2mm_info.process_queue_lock.unlock();

  // Print warning if processing large amount of trace
  if (nBytes > TS2MM_WARN_BIG_BUF_SIZE && !bd.big_trace_warn_done) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_BIG_BUF);
    bd.big_trace_warn_done = true;
  }

  if (bd.used_size == bd.alloc_size && ts2mm_info.use_circ_buf == false)
    bd.full = true;

  return true;
}

bool PLDeviceTraceOffload::
init_s2mm(bool circ_buf, const std::vector<uint64_t> &buf_sizes)
{
  /* If buffer is already allocated and still attempting to initialize again,
   * then reset the TS2MM IP and free the old buffer
   */
  if (!ts2mm_info.buffers.empty())
    reset_s2mm();
  ts2mm_info.buffers.resize(ts2mm_info.num_ts2mm);

  if (buf_sizes.empty())
    return false;

  // Check if allocated buffer and sleep interval can keep up with offload
  if (dev_intf->supportsCircBufPL() && circ_buf) {
    if (sleep_interval_ms != 0) {
      ts2mm_info.circ_buf_cur_rate = buf_sizes.front() * (1000 / sleep_interval_ms);
      if (ts2mm_info.circ_buf_cur_rate >= ts2mm_info.circ_buf_min_rate)
        ts2mm_info.use_circ_buf = true;
    } else {
      ts2mm_info.use_circ_buf = true;
    }
  }

  for (uint64_t i = 0; i < ts2mm_info.num_ts2mm; i++) {
    auto& bd = ts2mm_info.buffers[i];
    bd.alloc_size = buf_sizes[i];

    bd.bufId = dev_intf->allocTraceBuf(bd.alloc_size, dev_intf->getTS2MmMemIndex(i));
    if (!bd.bufId)
      return false;

    // Data Mover will write input stream to this address
    bd.address = dev_intf->getTraceBufDeviceAddr(bd.bufId);
    dev_intf->initTS2MM(i, bd.alloc_size, bd.address, ts2mm_info.use_circ_buf);

    debug_stream
    << "PLDeviceTraceOffload::init_s2mm with each size : " << bd.alloc_size
    << " initiated " << i << " ts2mm " << std::endl;
  }
  return true;
}

void PLDeviceTraceOffload::
reset_s2mm()
{
  debug_stream << "PLDeviceTraceOffload::reset_s2mm" << std::endl;
  if (ts2mm_info.buffers.empty())
    return;

  for (uint64_t i = 0; i < ts2mm_info.num_ts2mm; i++) {
    // Need to re-initialize datamover with circular buffer off for reset to work properly
    if (ts2mm_info.use_circ_buf)
      dev_intf->initTS2MM(i, 0, ts2mm_info.buffers[i].address, 0);

    dev_intf->resetTS2MM(i);
    dev_intf->freeTraceBuf(ts2mm_info.buffers[i].bufId);
    ts2mm_info.buffers[i].bufId = 0;
  }
  ts2mm_info.buffers.clear();
}

bool PLDeviceTraceOffload::
trace_buffer_full()
{
  if (has_fifo()) {
    if (fifo_full) {
      // Throw warning for this offloader if we detect full fifo
      std::call_once(fifo_full_warning_flag, [](){
        xrt_core::message::send(xrt_core::message::severity_level::warning,
                                "XRT", FIFO_WARN_MSG);
      });
    }
    return fifo_full;
  }

  bool isFull = false;
  for (uint32_t i = 0 ; i < ts2mm_info.num_ts2mm && !isFull; i++) {
    isFull |= ts2mm_info.buffers[i].full;
  }
  // Throw warning for this offloader if we detect full buffer
  if (isFull)
    std::call_once(ts2mm_full_warning_flag, [](){
        xrt_core::message::send(xrt_core::message::severity_level::warning,
                                "XRT", TS2MM_WARN_MSG_BUF_FULL);
      });
  return isFull;
}

}
