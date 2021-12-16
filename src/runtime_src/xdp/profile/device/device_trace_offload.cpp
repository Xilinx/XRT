/**
 * Copyright (C) 2019 Xilinx, Inc
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
                   : sleep_interval_ms(sleep_interval_ms),
                     m_trbuf_alloc_sz(trbuf_sz),
                     dev_intf(dInt),
                     deviceTraceLogger(dTraceLogger)
{
  // Select appropriate reader
  if(has_fifo()) {
    m_read_trace = std::bind(&DeviceTraceOffload::read_trace_fifo, this, std::placeholders::_1);
  } else {
    m_read_trace = std::bind(&DeviceTraceOffload::read_trace_s2mm, this, std::placeholders::_1);
  }

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
  if (!m_initialized && !read_trace_init(true)) {
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
    process_queue_lock.lock();
    if (!m_data_queue.empty()) {
      buf = std::move(m_data_queue.front());
      size = m_size_queue.front();
      m_data_queue.pop();
      m_size_queue.pop();
      q_read = true;
      q_empty = m_data_queue.empty();
    }
    process_queue_lock.unlock();

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
  if (m_trbuf_full)
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
  if (!m_trbuf_full) {
    auto property = dev_intf->getMonitorProperties(XCL_PERF_MON_FIFO, 0);
    auto fifo_size = GetDeviceTraceBufferSize(property);

    if (num_packets >= fifo_size)
      m_trbuf_full = true;

  }
}

bool DeviceTraceOffload::read_trace_init(bool circ_buf)
{
  // reset flags
  m_trbuf_full = false;
  trbuf_offload_done = false;

  if (has_ts2mm()) {
    m_initialized = init_s2mm(circ_buf);
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
  if (m_use_circ_buf && m_trbuf_sz == m_trbuf_alloc_sz) {
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
    << "DeviceTraceOffload::read_trace_s2mm " << std::endl;

  auto wordcount = dev_intf->getWordCountTs2mm();
  auto bytes_written = (wordcount - m_wordcount_old) * TRACE_PACKET_SIZE;

  // Don't read data if there's less than 512B trace
  if (!force && (bytes_written < TS2MM_MIN_READ_SIZE)) {
    debug_stream
      << "Skipping trace read. Amount of data: " << bytes_written << std::endl;
    return;
  }
  // There's enough data available
  m_wordcount_old = wordcount;

  if (!config_s2mm_reader(wordcount))
    return;

  uint64_t nBytes = m_trbuf_sz - m_trbuf_offset;

  auto start = std::chrono::steady_clock::now();
  void* host_buf = dev_intf->syncTraceBuf( m_trbuf, m_trbuf_offset, nBytes);
  auto end = std::chrono::steady_clock::now();
  debug_stream
    << "Elapsed time in microseconds for sync : "
    << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
    << " µs" << " nBytes : " << nBytes << std::endl;

  if (!host_buf)
    return;

  auto tmp = std::make_unique<char[]>(nBytes);
  std::memcpy(tmp.get(), host_buf, nBytes);
  // Push new data into queue for processing
  process_queue_lock.lock();
  m_data_queue.push(std::move(tmp));
  m_size_queue.push(nBytes);
  process_queue_lock.unlock();

  // Print warning if processing large amount of trace
  if (nBytes > TS2MM_WARN_BIG_BUF_SIZE && !m_trace_warn_big_done) {
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_BIG_BUF);
    m_trace_warn_big_done = true;
  }

  if (m_trbuf_sz == m_trbuf_alloc_sz && m_use_circ_buf == false)
    m_trbuf_full = true;
}

bool DeviceTraceOffload::config_s2mm_reader(uint64_t wordCount)
{
  if (trbuf_offload_done)
    return false;

  auto bytes_written = wordCount * TRACE_PACKET_SIZE;
  auto bytes_read = m_rollover_count*m_trbuf_alloc_sz + m_trbuf_sz;

  // Offload cannot keep up with the DMA
  if (bytes_written > bytes_read + m_trbuf_alloc_sz) {
    // Don't read any data
    m_trbuf_offset = m_trbuf_sz;
    trbuf_offload_done = true;

    // Add warnings and user markers
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_CIRC_BUF_OVERWRITE);
    xrt::profile::user_event events;
    events.mark("Trace Buffer Overwrite Detected");

    stop_offload();
    return false;
  }

  // Start Offload from previous offset
  m_trbuf_offset = m_trbuf_sz;
  if (m_trbuf_offset == m_trbuf_alloc_sz) {
    if (!m_use_circ_buf) {
      trbuf_offload_done = true;
      stop_offload();
      return false;
    }
    m_rollover_count++;
    m_trbuf_offset = 0;
  }

  // End Offload at this offset
  m_trbuf_sz = bytes_written - m_rollover_count*m_trbuf_alloc_sz;
  if (m_trbuf_sz > m_trbuf_alloc_sz) {
    m_trbuf_sz = m_trbuf_alloc_sz;
  }

  debug_stream
    << "DeviceTraceOffload::config_s2mm_reader "
    << "Reading from 0x"
    << std::hex << m_trbuf_offset << " to 0x" << m_trbuf_sz << std::dec
    << " Bytes Read : " << bytes_read
    << " Bytes Written : " << bytes_written
    << " Rollovers : " << m_rollover_count
    << std::endl;

  return true;
}

bool DeviceTraceOffload::init_s2mm(bool circ_buf)
{
  debug_stream
    << "DeviceTraceOffload::init_s2mm with size : " << m_trbuf_alloc_sz
    << std::endl;
  /* If buffer is already allocated and still attempting to initialize again,
   * then reset the TS2MM IP and free the old buffer
   */
  if (m_trbuf) {
    reset_s2mm();
  }

  if (!m_trbuf_alloc_sz)
    return false;

  m_trbuf = dev_intf->allocTraceBuf(m_trbuf_alloc_sz, dev_intf->getTS2MmMemIndex());
  if (!m_trbuf) {
    return false;
  }

  // Check if allocated buffer and sleep interval can keep up with offload
  auto tdma = dev_intf->getTs2mm();
  if (tdma->supportsCircBuf() && circ_buf) {
    if (sleep_interval_ms != 0) {
      m_circ_buf_cur_rate = m_trbuf_alloc_sz * (1000 / sleep_interval_ms);
      if (m_circ_buf_cur_rate >= m_circ_buf_min_rate)
        m_use_circ_buf = true;
    } else {
      m_use_circ_buf = true;
    }
  }

  // Data Mover will write input stream to this address
  m_trbuf_addr = dev_intf->getDeviceAddr(m_trbuf);
  dev_intf->initTS2MM(m_trbuf_alloc_sz, m_trbuf_addr, m_use_circ_buf);
  return true;
}

void DeviceTraceOffload::reset_s2mm()
{
  debug_stream << "DeviceTraceOffload::reset_s2mm" << std::endl;
  if (!m_trbuf)
    return;

  // Need to re-inititlize datamover with circular buffer off for reset to work properly
  if (m_use_circ_buf)
    dev_intf->initTS2MM(0, m_trbuf_addr, 0);

  dev_intf->resetTS2MM();
  dev_intf->freeTraceBuf(m_trbuf);
  m_trbuf = 0;
}

}
