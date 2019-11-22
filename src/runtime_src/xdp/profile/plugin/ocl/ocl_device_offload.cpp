#include "ocl_device_offload.h"
#include <iostream>

namespace xdp {

OclDeviceOffload::OclDeviceOffload(xdp::DeviceIntf* dInt,
                                   std::shared_ptr<RTProfile> ProfileMgr,
                                   std::string device_name,
                                   std::string binary_name,
                                   uint64_t sleep_interval_ms)
                                   : status(DeviceOffloadStatus::IDLE),
                                     sleep_interval_ms(sleep_interval_ms),
                                     dev_intf(dInt),
                                     prof_mgr(std::move(ProfileMgr)),
                                     device_name(std::move(device_name)),
                                     binary_name(std::move(binary_name))
                                     
{
  start_offload();
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
    read_trace_s2mm_partial();
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

void OclDeviceOffload::read_trace_end() {
  // Trace logger will clear it's state and add approximations for pending
  // events 
  m_trace_vector = {};
  prof_mgr->logDeviceTrace(device_name, binary_name, m_type, m_trace_vector, true);
}

void OclDeviceOffload::read_trace_s2mm_all() {
}

void OclDeviceOffload::read_trace_s2mm_partial() {
}

}