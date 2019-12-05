#include "ocl_device_offload.h"
#include "xdp/profile/device/tracedefs.h"
#include <iostream>

namespace xdp {

OclDeviceOffload::OclDeviceOffload(xdp::DeviceIntf* dInt,
                                   std::shared_ptr<RTProfile> ProfileMgr,
                                   xocl::device* xoclDevice,
                                   std::string device_name,
                                   std::string binary_name,
                                   uint64_t sleep_interval_ms,
                                   bool start_thread)
                                   : status(DeviceOffloadStatus::IDLE),
                                     sleep_interval_ms(sleep_interval_ms),
                                     dev_intf(dInt),
                                     prof_mgr(std::move(ProfileMgr)),
                                     xocl_dev(xoclDevice),
                                     device_name(std::move(device_name)),
                                     binary_name(std::move(binary_name))
                                     
{
  xrt_dev = xoclDevice->get_xrt_device();
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
    offload_trace();
    offload_counters();
    // Sleep for a specified time
    std::this_thread::sleep_for (std::chrono::milliseconds(sleep_interval_ms));
  }
  // Do a final flush
  offload_trace();
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

void OclDeviceOffload::offload_trace()
{
  if (dev_intf->hasFIFO()) {
    read_trace_fifo();
  } else {
    read_trace_s2mm();
  }
}

void OclDeviceOffload::offload_counters()
{
}

void OclDeviceOffload::read_trace_fifo()
{
  do {
    dev_intf->readTrace(m_type, m_trace_vector);
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
  config_s2mm_reader(dev_intf->getWordCountTs2mm());
  while (1) {
    auto bytes = read_trace_s2mm_partial();
    prof_mgr->logDeviceTrace(device_name, binary_name, m_type, m_trace_vector, false);
    m_trace_vector = {};
    if (bytes != m_trbuf_chunk_sz)
      break;
  }
}

void* OclDeviceOffload::sync_trace_buf(uint64_t offset, uint64_t bytes)
{
  if (!m_trbuf)
    return nullptr;
  auto addr = xrt_dev->map(m_trbuf);
  xrt_dev->sync(m_trbuf, bytes, offset, xrt::hal::device::direction::DEVICE2HOST, false);
  return static_cast<char*>(addr) + offset;
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

  void* host_buf = sync_trace_buf(m_trbuf_offset, nBytes);
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
  debug_stream << "OclDeviceOffload::init_s2mm" << std::endl;
  /* If buffer is already allocated and still attempting to initialize again,
   * then reset the TS2MM IP and free the old buffer
   */
  if(m_trbuf) {
    reset_s2mm();
  }

  uint64_t trbuf_sz = 0;
  try {
    trbuf_sz = xdp::xoclp::platform::get_ts2mm_buf_size();
    auto memory_sz = xdp::xoclp::platform::device::getMemSizeBytes(xocl_dev, dev_intf->getTS2MmMemIndex());
    if (memory_sz > 0 && trbuf_sz > memory_sz) {
      std::string msg = "Trace Buffer size is too big for Memory Resource. Using " + std::to_string(memory_sz)
                        + " Bytes instead.";
      xrt::message::send(xrt::message::severity_level::XRT_WARNING, msg);
      trbuf_sz = memory_sz;
    }
    m_trbuf = xrt_dev->alloc(trbuf_sz, xrt::hal::device::Domain::XRT_DEVICE_RAM, dev_intf->getTS2MmMemIndex(), nullptr);
    // XRT bug. We can't read from memory space we haven't written to
    xrt_dev->sync(m_trbuf, trbuf_sz, 0, xrt::hal::device::direction::HOST2DEVICE, false);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    xrt::message::send(xrt::message::severity_level::XRT_WARNING, TS2MM_WARN_MSG_ALLOC_FAIL);
    return false;
  }
  // Data Mover will write input stream to this address
  uint64_t bufAddr = xrt_dev->getDeviceAddr(m_trbuf);

  dev_intf->initTS2MM(trbuf_sz, bufAddr);
  return true;
}

void OclDeviceOffload::reset_s2mm()
{
  debug_stream << "OclDeviceOffload::reset_s2mm" << std::endl;
  if (!m_trbuf)
    return;
  dev_intf->resetTS2MM();
  xrt_dev->free(m_trbuf);
  m_trbuf = nullptr;
}

}