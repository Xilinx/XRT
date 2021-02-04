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

namespace xdp {

DeviceTraceOffload::DeviceTraceOffload(DeviceIntf* dInt,
				       DeviceTraceLogger* dTraceLogger,
				       uint64_t sleep_interval_ms,
				       uint64_t trbuf_sz,
				       bool start_thread,
				       bool e_trace)
                   : continuous(start_thread),
		     enable_trace(e_trace),
		     sleep_interval_ms(sleep_interval_ms),
                     m_trbuf_alloc_sz(trbuf_sz),
                     dev_intf(dInt),
                     deviceTraceLogger(dTraceLogger)
{
  // Select appropriate reader
  if(has_fifo()) {
    m_read_trace = std::bind(&DeviceTraceOffload::read_trace_fifo, this);
  } else {
    m_read_trace = std::bind(&DeviceTraceOffload::read_trace_s2mm, this);
  }

  m_prev_clk_train_time = std::chrono::system_clock::now();

  if (start_thread) {
    start_offload(OffloadThreadType::TRACE);
  }
}

DeviceTraceOffload::~DeviceTraceOffload()
{
  stop_offload();
  if (offload_thread.joinable()) {
    offload_thread.join();
  }
}

void DeviceTraceOffload::offload_device_continuous()
{
  if (!m_initialized && !read_trace_init(true))
    return;

  while (should_continue()) {
    train_clock();
    m_read_trace();
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }

  // Do a final read
  m_read_trace();
  read_trace_end();
}

void DeviceTraceOffload::train_clock_continuous()
{
  while (should_continue()) {
    train_clock();
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }
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

  if (type == OffloadThreadType::TRACE)
    offload_thread = std::thread(&DeviceTraceOffload::offload_device_continuous, this);
  else if (type == OffloadThreadType::CLOCK_TRAIN)
    offload_thread = std::thread(&DeviceTraceOffload::train_clock_continuous, this);
}

void DeviceTraceOffload::stop_offload()
{
  std::lock_guard<std::mutex> lock(status_lock);
  status = OffloadThreadStatus::STOPPING;
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

void DeviceTraceOffload::read_trace_fifo()
{
  debug_stream
    << "DeviceTraceOffload::read_trace_fifo " << std::endl;

  uint32_t num_packets = 0;

#ifndef _WIN32
  do {
#endif
    m_trace_vector = {};
    dev_intf->readTrace(m_trace_vector);
    deviceTraceLogger->processTraceData(m_trace_vector);
    num_packets += m_trace_vector.mLength;
#ifndef _WIN32
  } while (m_trace_vector.mLength != 0);
#endif

  // Check if fifo is full
  if (!m_trbuf_full) {
    auto property = dev_intf->getMonitorProperties(XCL_PERF_MON_FIFO, 0);
    auto fifo_size = GetDeviceTraceBufferSize(property);

    if (num_packets >= fifo_size && enable_trace) {
      m_trbuf_full = true;

      // Send warning message
      const char* msg = "Trace FIFO is full because of too many events. Device trace could be incomplete. Please use 'coarse' option for data_transfer_trace or turn off Stall profiling";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);

    }

  }
}

bool DeviceTraceOffload::read_trace_init(bool circ_buf)
{
  // reset flags
  m_trbuf_full = false;

  if (has_ts2mm()) {
    m_initialized = init_s2mm(circ_buf);
  } else if (has_fifo()) {
    m_initialized = true;
  } else {
    m_initialized = false;
  }
  return m_initialized;
}

void DeviceTraceOffload::read_trace_end()
{
  // Trace logger will clear it's state and add approximations 
  // for pending events
  m_trace_vector = {};
  deviceTraceLogger->endProcessTraceData(m_trace_vector);
  if (dev_intf->hasTs2mm()) {
    reset_s2mm();
    m_initialized = false;
  }
}

void DeviceTraceOffload::read_trace_s2mm()
{
  debug_stream
    << "DeviceTraceOffload::read_trace_s2mm " << std::endl;

  config_s2mm_reader(dev_intf->getWordCountTs2mm());
  while (1) {
    auto bytes = read_trace_s2mm_partial();
    deviceTraceLogger->processTraceData(m_trace_vector);
    m_trace_vector = {};

    if (m_trbuf_sz == m_trbuf_alloc_sz && m_use_circ_buf == false) {
      m_trbuf_full = true;

      // Send warning message
      const char* msg = "Trace buffer is full. Device trace could be incomplete.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg) ;
    }

    if (bytes != m_trbuf_chunk_sz)
      break;
  }
}

uint64_t DeviceTraceOffload::read_trace_s2mm_partial()
{
  if (m_trbuf_offset >= m_trbuf_sz)
    return 0;

  uint64_t nBytes = m_trbuf_chunk_sz;

  if ((m_trbuf_offset + m_trbuf_chunk_sz) > m_trbuf_sz)
    nBytes = m_trbuf_sz - m_trbuf_offset;

  debug_stream
    << "DeviceTraceOffload::read_trace_s2mm_partial "
    <<"Reading " << nBytes << " bytes " << std::endl;

  auto start = std::chrono::steady_clock::now();
  void* host_buf = dev_intf->syncTraceBuf( m_trbuf, m_trbuf_offset, nBytes);
  auto end = std::chrono::steady_clock::now();
  debug_stream
    << "Elapsed time in microseconds for sync : "
    << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
    << " µs" << std::endl;

  if (host_buf) {
    dev_intf->parseTraceData(host_buf, nBytes, m_trace_vector);
    m_trbuf_offset += nBytes;
    return nBytes;
  }
  return 0;
}

void DeviceTraceOffload::config_s2mm_reader(uint64_t wordCount)
{
  auto bytes_written = wordCount * TRACE_PACKET_SIZE;
  auto bytes_read = m_rollover_count*m_trbuf_alloc_sz + m_trbuf_sz;

  // Offload cannot keep up with the DMA
  if (bytes_written > bytes_read + m_trbuf_alloc_sz) {
    // Don't read any data
    m_trbuf_offset = m_trbuf_sz;
    debug_stream
      << "ERROR: Circular buffer overwrite detected "
      << " bytes written : " << bytes_written << " bytes_read : " << bytes_read
      << std::endl;
    stop_offload();
    return;
  }

  // Start Offload from previous offset
  m_trbuf_offset = m_trbuf_sz;
  if (m_trbuf_offset == m_trbuf_alloc_sz) {
    if (!m_use_circ_buf) {
      stop_offload();
      return;
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
    << " Written : " << wordCount * 8
    << " rollover count : " << m_rollover_count
    << std::endl;
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
  if (dev_intf->hasTs2mm()) {
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
  }

  // Data Mover will write input stream to this address
  uint64_t bufAddr = dev_intf->getDeviceAddr(m_trbuf);
  dev_intf->initTS2MM(m_trbuf_alloc_sz, bufAddr, m_use_circ_buf);
  return true;
}

void DeviceTraceOffload::reset_s2mm()
{
  debug_stream << "DeviceTraceOffload::reset_s2mm" << std::endl;
  if (!m_trbuf)
    return;
  dev_intf->resetTS2MM();
  dev_intf->freeTraceBuf(m_trbuf);
  m_trbuf = 0;
}

}
