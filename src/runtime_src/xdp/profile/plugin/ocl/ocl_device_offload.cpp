#include "ocl_device_offload.h"
#include "xdp/profile/device/tracedefs.h"
#include <iostream>
#include <chrono>

namespace xdp {

OclDeviceOffload::OclDeviceOffload(xdp::DeviceIntf* dInt,
                                   std::shared_ptr<RTProfile> ProfileMgr,
                                   std::string device_name,
                                   std::string binary_name,
                                   uint64_t sleep_interval_ms,
                                   uint64_t trbuf_sz,
                                   bool start_thread)
                                   : status(DeviceOffloadStatus::IDLE),
                                     sleep_interval_ms(sleep_interval_ms),
                                     m_trbuf_alloc_sz(trbuf_sz),
                                     dev_intf(dInt),
                                     prof_mgr(std::move(ProfileMgr)),
                                     device_name(std::move(device_name)),
                                     binary_name(std::move(binary_name))
                                     
{
  // Select appropriate reader
  if(dev_intf->hasFIFO()) {
    m_read_trace = std::bind(&OclDeviceOffload::read_trace_fifo, this);
  } else {
    m_read_trace = std::bind(&OclDeviceOffload::read_trace_s2mm, this);
  }

  if (start_thread) {
    start_offload();
  }
}

OclDeviceOffload::~OclDeviceOffload()
{
  stop_offload();
  if (offload_thread.joinable()) {
    offload_thread.join();
  }
}

void OclDeviceOffload::offload_device_continuous()
{
  // Initialization
  if (!read_trace_init())
    return;

  while (should_continue()) {
    // Offload and log trace and counters
    m_read_trace();
    // Sleep for a specified time
    std::this_thread::sleep_for (std::chrono::milliseconds(sleep_interval_ms));
  }
  // Do a final flush
  m_read_trace();
  read_trace_end();
}

bool OclDeviceOffload::should_continue()
{
  std::lock_guard<std::mutex> lock(status_lock);
  return status == DeviceOffloadStatus::RUNNING;
}

void OclDeviceOffload::start_offload()
{
  std::lock_guard<std::mutex> lock(status_lock);
  status = DeviceOffloadStatus::RUNNING;
  offload_thread = std::thread(&OclDeviceOffload::offload_device_continuous, this);
}

void OclDeviceOffload::stop_offload()
{
  std::lock_guard<std::mutex> lock(status_lock);
  status = DeviceOffloadStatus::STOPPING;
}

void OclDeviceOffload::read_trace_fifo()
{
  debug_stream
    << "OclDeviceOffload::read_trace_fifo " << std::endl;

  do {
    dev_intf->readTrace(m_trace_vector);
    prof_mgr->logDeviceTrace(device_name, binary_name, m_type, m_trace_vector, false);
    m_trace_vector = {};
  } while (m_trace_vector.mLength != 0);
}

bool OclDeviceOffload::read_trace_init()
{
  if (dev_intf->hasTs2mm()) {
    return init_s2mm();
  }
  return true;
}

void OclDeviceOffload::read_trace_end()
{
  // Trace logger will clear it's state and add approximations for pending
  // events 
  m_trace_vector = {};
  prof_mgr->logDeviceTrace(device_name, binary_name, m_type, m_trace_vector, true);
  if (dev_intf->hasTs2mm()) {
    reset_s2mm();
  }
}

void OclDeviceOffload::read_trace_s2mm()
{
  debug_stream
    << "OclDeviceOffload::read_trace_s2mm " << std::endl;

  config_s2mm_reader(dev_intf->getWordCountTs2mm());
  while (1) {
    auto bytes = read_trace_s2mm_partial();
    prof_mgr->logDeviceTrace(device_name, binary_name, m_type, m_trace_vector, false);
    m_trace_vector = {};
    if (bytes != m_trbuf_chunk_sz)
      break;
  }
}

uint64_t OclDeviceOffload::read_trace_s2mm_partial()
{
  if (m_trbuf_offset >= m_trbuf_sz)
    return 0;
  uint64_t nBytes = m_trbuf_chunk_sz;
  if ((m_trbuf_offset + m_trbuf_chunk_sz) > m_trbuf_sz)
    nBytes = m_trbuf_sz - m_trbuf_offset;

  debug_stream
    << "OclDeviceOffload::read_trace_s2mm_partial "
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

void OclDeviceOffload::config_s2mm_reader(uint64_t wordCount)
{
  // Start from previous offset
  m_trbuf_offset = m_trbuf_sz;
  m_trbuf_sz = wordCount * TRACE_PACKET_SIZE;
  m_trbuf_sz = (m_trbuf_sz > TS2MM_MAX_BUF_SIZE) ? TS2MM_MAX_BUF_SIZE : m_trbuf_sz;
  m_trbuf_chunk_sz = MAX_TRACE_NUMBER_SAMPLES * TRACE_PACKET_SIZE;

  debug_stream
    << "OclDeviceOffload::config_s2mm_reader "
    << "Reading from "
    << std::hex << m_trbuf_offset << " to " << m_trbuf_sz
    << std::dec << std::endl;
}

bool OclDeviceOffload::init_s2mm()
{
  debug_stream
    << "OclDeviceOffload::init_s2mm with size : " << m_trbuf_alloc_sz
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
    xrt::message::send(xrt::message::severity_level::XRT_WARNING, TS2MM_WARN_MSG_ALLOC_FAIL);
    return false;
  }

  // Data Mover will write input stream to this address
  uint64_t bufAddr = dev_intf->getDeviceAddr(m_trbuf);
  dev_intf->initTS2MM(m_trbuf_alloc_sz, bufAddr);
  return true;
}

void OclDeviceOffload::reset_s2mm()
{
  debug_stream << "OclDeviceOffload::reset_s2mm" << std::endl;
  if (!m_trbuf)
    return;
  dev_intf->resetTS2MM();
  dev_intf->freeTraceBuf(m_trbuf);
  m_trbuf = 0;
}

}