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

#include "xdp/profile/device/aie_trace_offload.h"
#include "xdp/profile/device/aie_trace_logger.h"

namespace xdp {

AIETraceOffload::AIETraceOffload(DeviceIntf* dInt,
                                 DeviceTraceLogger* dTraceLogger,
                                 uint64_t sleep_interval_ms,
                                 uint64_t trbuf_sz,
                                 bool     start_thread,
                                 uint64_t aie_trbuf_sz,
                                 AIETraceLogger* aieTraceLogger)
               : DeviceTraceOffload(dInt, dTraceLogger, sleep_interval_ms, trbuf_sz, start_thread),
                 m_aie_trbuf_alloc_sz(aie_trbuf_sz),
                 m_aie_trace_logger(aieTraceLogger)
{
  // Only question start_thread: start_offload

  // Nothing to do here for AIE
}

AIETraceOffload::~AIETraceOffload()
{
}

#if 0
void AIETraceOffload::offload_device_continuous()
{
#if 0
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
#endif

  DeviceTraceOffload::offload_device_continuous();
  read_aie_trace();
}
#endif

bool AIETraceOffload::read_trace_init(bool circ_buf)
{
  DeviceTraceOffload::read_trace_init(circ_buf);

  // reset flags
  m_aie_trbuf_full = false;

  init_aie_s2mm();	// multiple ?

  return m_initialized;
}

void AIETraceOffload::read_trace_end()
{
  DeviceTraceOffload::read_trace_end();

  // log aie trace buffer : Using AIETraceLogger ?
  // reset device_intf ts2mm

  reset_aie_s2mm();

  // iterate over all aie ts2mm
}

void AIETraceOffload::read_aie_trace()
{
  debug_stream
    << "AIETraceOffload::read_aie_trace " << std::endl;

  config_aie_s2mm_reader(dev_intf->getWordCountTs2mm(true, 0));
  while (1) {
    auto bytes = read_aie_trace_s2mm_partial();
//    aieTraceLogger->log ??
//    deviceTraceLogger->processTraceData(m_trace_vector);
//    m_trace_vector = {};

    if (m_aie_trbuf_sz == m_aie_trbuf_alloc_sz)
      m_aie_trbuf_full = true;

    if (bytes != m_trbuf_chunk_sz)
      break;
  }
}

uint64_t AIETraceOffload::read_aie_trace_s2mm_partial()
{
  if (m_aie_trbuf_offset >= m_aie_trbuf_sz)
    return 0;

  uint64_t nBytes = m_trbuf_chunk_sz;

  if ((m_aie_trbuf_offset + m_trbuf_chunk_sz) > m_aie_trbuf_sz)
    nBytes = m_aie_trbuf_sz - m_aie_trbuf_offset;

  debug_stream
    << "DeviceTraceOffload::read_aie_trace_s2mm_partial "
    << "Reading " << nBytes << " bytes " << std::endl;

  auto  start = std::chrono::steady_clock::now();
  void* host_buf = dev_intf->syncTraceBuf(m_aie_trbuf, m_aie_trbuf_offset, nBytes);
  auto  end = std::chrono::steady_clock::now();
  debug_stream
    << "Elapsed time in microseconds for sync : "
    << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
    << " µs" << std::endl;

  if (host_buf) {
    m_aie_trace_logger->addAIETraceData(host_buf, nBytes);
// copy this host_buf
//    dev_intf->parseTraceData(host_buf, nBytes, m_trace_vector);
    m_aie_trbuf_offset += nBytes;
    return nBytes;
  }
  return 0;
}

void AIETraceOffload::config_aie_s2mm_reader(uint64_t wordCount)
{
  m_aie_trbuf_sz = wordCount * TRACE_PACKET_SIZE;
  if(m_aie_trbuf_sz > m_aie_trbuf_alloc_sz) {
    m_aie_trbuf_sz = m_aie_trbuf_alloc_sz;
  }

#if 0
  auto bytes_written = wordCount * TRACE_PACKET_SIZE;
  auto bytes_read = m_trbuf_sz;

  // Start Offload from previous offset
  m_trbuf_offset = m_trbuf_sz;
#if 0
  if (m_trbuf_offset == m_trbuf_alloc_sz) {
    // end 
  }
#endif

  // End Offload at this offset
  m_trbuf_sz = bytes_written;
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
#endif

}

bool AIETraceOffload::init_aie_s2mm()
{
  debug_stream
    << "AIETraceOffload::init_aie_s2mm with size : " << m_aie_trbuf_alloc_sz
    << std::endl;

  /* If buffer is already allocated and still attempting to initialize again,
   * then reset the TS2MM IP and free the old buffer
   */
  if (m_aie_trbuf) {
    reset_aie_s2mm();
  }

  if (!m_aie_trbuf_alloc_sz)
    return false;

  m_aie_trbuf = dev_intf->allocTraceBuf(m_aie_trbuf_alloc_sz, dev_intf->getTS2MmMemIndex(true, 0));
  if (!m_aie_trbuf) {
    return false;
  }

  // Data Mover will write input stream to this address
  uint64_t bufAddr = dev_intf->getDeviceAddr(m_aie_trbuf);
  dev_intf->initTS2MM(m_aie_trbuf_alloc_sz, bufAddr, false);
  return true;
}

#if 0
// needed ?
void AIETraceOffload::reset_s2mm()
{
  DeviceTraceOffload::reset_s2mm();
  reset_aie_s2mm();
}
#endif

void AIETraceOffload::reset_aie_s2mm()
{
  debug_stream << "AIETraceOffload::reset_aie_s2mm" << std::endl;
  if (!m_aie_trbuf)
    return;
  dev_intf->resetTS2MM(true);
  dev_intf->freeTraceBuf(m_aie_trbuf); // multiple
  m_aie_trbuf = 0;
}

}

