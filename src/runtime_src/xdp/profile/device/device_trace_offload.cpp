/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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

#define XDP_SOURCE

#include "xdp/profile/device/device_trace_offload.h"
#include "xdp/profile/device/device_trace_logger.h"
#include "core/common/message.h"
#include "experimental/xrt_profile.h"

namespace xdp {

DeviceTraceOffload::DeviceTraceOffload(DeviceIntf* dInt,
                                       DeviceTraceLogger* dTraceLogger,
                                       uint64_t sleep_interval_ms,
                                       uint64_t trbuf_sz)
                   : dev_intf(dInt),
                     deviceTraceLogger(dTraceLogger),
                     sleep_interval_ms(sleep_interval_ms)
                     
{
  // Select appropriate reader
  if(has_fifo()) {
    m_read_trace = std::bind(&DeviceTraceOffload::read_trace_fifo, this, std::placeholders::_1);
  } else {
    m_read_trace = std::bind(&DeviceTraceOffload::read_trace_s2mm, this, std::placeholders::_1);
  }

  ts2mm_info.num_ts2mm = dev_intf->getNumberTS2MM();
  ts2mm_info.full_buf_size = trbuf_sz;

  // Initialize internal variables
  m_prev_clk_train_time = std::chrono::system_clock::now();
  m_process_trace = false;
  m_process_trace_done = false;
}

DeviceTraceOffload::~DeviceTraceOffload()
{
  stop_offload();
  if (offload_thread.joinable()) {
    offload_thread.join();
  }
  if (process_thread.joinable()) {
    process_thread.join();
  }
}

void DeviceTraceOffload::offload_device_continuous()
{
  std::vector<uint64_t> buf_sizes;
  if(!ts2mm_info.buffers.empty()) {
    buf_sizes.resize(ts2mm_info.num_ts2mm);
    for(size_t i = 0; i < ts2mm_info.num_ts2mm; i++) {
       buf_sizes[i] = ts2mm_info.buffers[i].buf_size;
    }
  }
  if (!m_initialized && !read_trace_init(true, buf_sizes)) {
    offload_finished();
    return;
  }

  while (should_continue()) {
    train_clock();
    m_read_trace(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }

  // Do final forced reads
  m_read_trace(true);
  read_leftover_circular_buf();

  // Stop processing thread
  m_process_trace = false;
  while (!m_process_trace_done);

  // Clear all state and add approximations
  read_trace_end();

  // Tell external plugin that offload has finished
  offload_finished();
}

void DeviceTraceOffload::train_clock_continuous()
{
  while (should_continue()) {
    train_clock();
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }

  offload_finished();
}

void DeviceTraceOffload::process_trace_continuous()
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

void DeviceTraceOffload::process_trace()
{
  if (!has_ts2mm())
    return;

  bool q_read = false;
  bool q_empty = true;
  std::unique_ptr<char[]> buf;
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
    ts2mm_info.process_queue_lock.unlock();

    // Processing takes a lot more time compared to everything else
    if (q_read) {
      debug_stream << "Process " << size << " bytes of trace" << std::endl;
      deviceTraceLogger->processTraceData(buf.get(), size) ;
      buf.reset();
    }
  } while (!q_empty);
}

bool DeviceTraceOffload::should_continue()
{
  std::lock_guard<std::mutex> lock(status_lock);
  return status == OffloadThreadStatus::RUNNING;
}

void DeviceTraceOffload::start_offload(OffloadThreadType type)
{
  if (status == OffloadThreadStatus::RUNNING)
    return;

  std::lock_guard<std::mutex> lock(status_lock);
  status = OffloadThreadStatus::RUNNING;

  if (type == OffloadThreadType::TRACE) {
    offload_thread = std::thread(&DeviceTraceOffload::offload_device_continuous, this);
    process_thread = std::thread(&DeviceTraceOffload::process_trace_continuous, this);
  } else if (type == OffloadThreadType::CLOCK_TRAIN) {
    offload_thread = std::thread(&DeviceTraceOffload::train_clock_continuous, this);
  }

}

void DeviceTraceOffload::stop_offload()
{
  std::lock_guard<std::mutex> lock(status_lock);
  if (status == OffloadThreadStatus::STOPPED) return ;
  status = OffloadThreadStatus::STOPPING;
}

void DeviceTraceOffload::offload_finished()
{
  std::lock_guard<std::mutex> lock(status_lock);
  if (status == OffloadThreadStatus::STOPPED) return ;
  status = OffloadThreadStatus::STOPPED;
}

void DeviceTraceOffload::train_clock()
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

void DeviceTraceOffload::read_trace_fifo(bool)
{
  debug_stream
    << "DeviceTraceOffload::read_trace_fifo " << std::endl;

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

    if (num_packets >= fifo_size)
      fifo_full = true;

  }
}

bool DeviceTraceOffload::read_trace_init(bool circ_buf, const std::vector<uint64_t> &buf_sizes)
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

void DeviceTraceOffload::read_leftover_circular_buf()
{
  // If we use circular buffer then, final trace read
  // might stop at trace buffer boundry and to read the entire
  // trace, we need one last read
  if (ts2mm_info.use_circ_buf && ts2mm_info.buffers[0].used_size == ts2mm_info.full_buf_size) {
    debug_stream
      << "Try to read left over circular buffer data" << std::endl;
    m_read_trace(true);
  }
}

void DeviceTraceOffload::read_trace_end()
{
  // Trace logger will clear it's state and add approximations 
  // for pending events
  deviceTraceLogger->endProcessTraceData();
  if (dev_intf->hasTs2mm()) {
    reset_s2mm();
    m_initialized = false;
  }
}

void DeviceTraceOffload::read_trace_s2mm(bool force)
{
  debug_stream
    << "DeviceTraceOffload::read_trace_s2mm : number of ts2mm in design " << ts2mm_info.num_ts2mm << std::endl;

  for(uint64_t i = 0; i < ts2mm_info.num_ts2mm; i++) {
  auto wordcount = dev_intf->getWordCountTs2mm(i);
  auto bytes_written = (wordcount - ts2mm_info.buffers[i].prv_wordcount) * TRACE_PACKET_SIZE;

  // Don't read data if there's less than 512B trace
  if (!force && (bytes_written < TS2MM_MIN_READ_SIZE)) {
    debug_stream
      << "Skipping trace read. Amount of data: " << bytes_written << std::endl;
    return;
  }
  // There's enough data available
  ts2mm_info.buffers[i].prv_wordcount = wordcount;

  if (!config_s2mm_reader(i, wordcount))
    return;

  uint64_t nBytes = ts2mm_info.buffers[i].used_size - ts2mm_info.buffers[i].offset;

  auto start = std::chrono::steady_clock::now();
  void* host_buf = dev_intf->syncTraceBuf( ts2mm_info.buffers[i].buf, ts2mm_info.buffers[i].offset, nBytes);
  auto end = std::chrono::steady_clock::now();
  debug_stream
    << "For " << i << " ts2mm : Elapsed time in microseconds for sync : "
    << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
    << " µs" << " nBytes : " << nBytes << std::endl;

  if (!host_buf)
    return;

  auto tmp = std::make_unique<char[]>(nBytes);
  std::memcpy(tmp.get(), host_buf, nBytes);
  // Push new data into queue for processing
  ts2mm_info.process_queue_lock.lock();
  ts2mm_info.data_queue.push(std::move(tmp));
  ts2mm_info.size_queue.push(nBytes);
  ts2mm_info.process_queue_lock.unlock();

  // Print warning if processing large amount of trace
  if (nBytes > TS2MM_WARN_BIG_BUF_SIZE && !ts2mm_info.buffers[i].big_trace_warn_done) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_BIG_BUF);
    ts2mm_info.buffers[i].big_trace_warn_done = true;
  }

  if (ts2mm_info.buffers[i].used_size == ts2mm_info.buffers[i].buf_size && ts2mm_info.use_circ_buf == false)
    ts2mm_info.buffers[i].full = true;
  }
}

bool DeviceTraceOffload::config_s2mm_reader(uint64_t i, uint64_t wordCount)
{
  if (ts2mm_info.buffers[i].offload_done)
    return false;

  auto bytes_written = wordCount * TRACE_PACKET_SIZE;
  auto bytes_read = ts2mm_info.buffers[i].rollover_count*ts2mm_info.buffers[i].buf_size + ts2mm_info.buffers[i].used_size;

  // Offload cannot keep up with the DMA
  if (bytes_written > bytes_read + ts2mm_info.buffers[i].buf_size) {
    // Don't read any data
    ts2mm_info.buffers[i].offset = ts2mm_info.buffers[i].used_size;
    ts2mm_info.buffers[i].offload_done = true;

    // Add warnings and user markers
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_CIRC_BUF_OVERWRITE);
    xrt::profile::user_event events;
    events.mark("Trace Buffer Overwrite Detected");

    stop_offload();
    return false;
  }

  // Start Offload from previous offset
  ts2mm_info.buffers[i].offset = ts2mm_info.buffers[i].used_size;
  if (ts2mm_info.buffers[i].offset == ts2mm_info.full_buf_size) {
    if (!ts2mm_info.use_circ_buf) {
      ts2mm_info.buffers[i].offload_done = true;
      stop_offload();
      return false;
    }
    ts2mm_info.buffers[i].rollover_count++;
    ts2mm_info.buffers[i].offset = 0;
  }

  // End Offload at this offset
  ts2mm_info.buffers[i].used_size = bytes_written - ts2mm_info.buffers[i].rollover_count*ts2mm_info.full_buf_size;
  if (ts2mm_info.buffers[i].used_size > ts2mm_info.full_buf_size) {
    ts2mm_info.buffers[i].used_size = ts2mm_info.full_buf_size;
  }

  debug_stream
    << "DeviceTraceOffload::config_s2mm_reader for " << i << " ts2mm " 
    << "Reading from 0x"
    << std::hex << ts2mm_info.buffers[i].offset << " to 0x" << ts2mm_info.buffers[i].used_size << std::dec
    << " Bytes Read : " << bytes_read
    << " Bytes Written : " << bytes_written
    << " Rollovers : " << ts2mm_info.buffers[i].rollover_count
    << std::endl;

  return true;
}

bool DeviceTraceOffload::init_s2mm(bool circ_buf, const std::vector<uint64_t> &buf_sizes)
{
  debug_stream
    << "DeviceTraceOffload::init_s2mm with size : " << ts2mm_info.full_buf_size
    << std::endl;
  /* If buffer is already allocated and still attempting to initialize again,
   * then reset the TS2MM IP and free the old buffer
   */
  if (!ts2mm_info.buffers.empty()) {
    reset_s2mm();
  }

  if (!ts2mm_info.full_buf_size)
    return false;

  ts2mm_info.buffers.resize(ts2mm_info.num_ts2mm);

  // Check if allocated buffer and sleep interval can keep up with offload
  if (dev_intf->supportsCircBuf() && circ_buf) {
    if (sleep_interval_ms != 0) {
      ts2mm_info.circ_buf_cur_rate = buf_sizes[0] * (1000 / sleep_interval_ms);
      if (ts2mm_info.circ_buf_cur_rate >= ts2mm_info.circ_buf_min_rate)
        ts2mm_info.use_circ_buf = true;
    } else {
      ts2mm_info.use_circ_buf = true;
    }
  }

  for(uint64_t i = 0; i < ts2mm_info.num_ts2mm; i++) {
    ts2mm_info.buffers[i].buf_size = buf_sizes[i];

    ts2mm_info.buffers[i].buf = dev_intf->allocTraceBuf(ts2mm_info.buffers[i].buf_size, dev_intf->getTS2MmMemIndex(i));

    if (!ts2mm_info.buffers[i].buf) {
      return false;
    }

    // Data Mover will write input stream to this address
    ts2mm_info.buffers[i].address = dev_intf->getDeviceAddr(ts2mm_info.buffers[i].buf);
    dev_intf->initTS2MM(i, ts2mm_info.buffers[i].buf_size, ts2mm_info.buffers[i].address, ts2mm_info.use_circ_buf);
  debug_stream
    << "DeviceTraceOffload::init_s2mm with each size : " << ts2mm_info.buffers[i].buf_size << " initiated " << i << " ts2mm "
    << std::endl;
  }
  return true;
}

void DeviceTraceOffload::reset_s2mm()
{
  debug_stream << "DeviceTraceOffload::reset_s2mm" << std::endl;
  if (ts2mm_info.buffers.empty())
    return;

  for(uint64_t i = 0; i < ts2mm_info.num_ts2mm; i++) {
    // Need to re-initialize datamover with circular buffer off for reset to work properly
    if (ts2mm_info.use_circ_buf)
      dev_intf->initTS2MM(i, 0, ts2mm_info.buffers[i].address, 0);

    dev_intf->resetTS2MM(i);
    dev_intf->freeTraceBuf(ts2mm_info.buffers[i].buf);
    ts2mm_info.buffers[i].buf = 0;
  }
  ts2mm_info.buffers.clear();
}

}
