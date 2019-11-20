#include <iostream>
#include "xdp_hal_plugin_interface.h"
#include "xdp_hal_plugin.h"

#include "hal_profiler.h"


namespace xdp {

void alloc_bo_start(void* payload) {
//    std::cout << "alloc_bo_start" << std::endl;
    return;
}

void alloc_bo_end(void* payload) {
//    std::cout << "alloc_bo_end" << std::endl;
    return;
}

void free_bo_start(void* payload) {
//    std::cout << "free_bo_start" << std::endl;
    return;
}

void free_bo_end(void* payload) {
//    std::cout << "free_bo_end" << std::endl;
    return;
}

void write_bo_start(void* payload) {
//    std::cout << "write_bo_start" << std::endl;
    return;
}

void write_bo_end(void* payload) {
//    std::cout << "write_bo_end" << std::endl;
    return;
}

void read_bo_start(void* payload) {
//    std::cout << "read_bo_start" << std::endl;
    return;
}

void read_bo_end(void* payload) {
//    std::cout << "read_bo_end" << std::endl;
    return;
}

void map_bo_start(void* payload) {
//    std::cout << "map_bo_start" << std::endl;
    return;
}

void map_bo_end(void* payload) {
//    std::cout << "map_bo_end" << std::endl;
    return;
}

void sync_bo_start(void* payload) {
//    std::cout << "sync_bo_start" << std::endl;
    return;
}

void sync_bo_end(void* payload) {
//    std::cout << "sync_bo_end" << std::endl;
    return;
}

void unmgd_read_start(void* payload) {
//  std::cout << "unmgd_read_start" << std::endl;

  UnmgdPreadPwriteCBPayload* pLoad = reinterpret_cast<UnmgdPreadPwriteCBPayload*>(payload);
  (void)pLoad;
  return;
}

void unmgd_read_end(void* payload) {
//  std::cout << "unmgd_read_end" << std::endl;
  return;
}

void unmgd_write_start(void* payload) {
//  std::cout << "unmgd_write_start" << std::endl;
  UnmgdPreadPwriteCBPayload* pLoad = reinterpret_cast<UnmgdPreadPwriteCBPayload*>(payload);
  (void)pLoad;
  return;
}

void unmgd_write_end(void* payload) {
//  std::cout << "unmgd_write_end" << std::endl;
  return;
}

void read_start(void* payload) {
//  std::cout << "read_start" << std::endl;
  ReadWriteCBPayload* pLoad = reinterpret_cast<ReadWriteCBPayload*>(payload);
  (void)pLoad; 
  return;
}

void read_end(void* payload) {
//  std::cout << "read_end" << std::endl;
  return;
}

void write_start(void* payload) {

//  std::cout << "write_start" << std::endl;

  ReadWriteCBPayload* pLoad = reinterpret_cast<ReadWriteCBPayload*>(payload);
  (void)pLoad; 
  return;
}


void write_end(void* payload) {
//    std::cout << "write_end" << std::endl;
    return;
}

void start_device_profiling_from_hal(void* payload)
{
  //std::cout << " start_device_profiling_from_hal " << std::endl;

  // HAL pointer
  xclDeviceHandle handle = ((CBPayload*)payload)->deviceHandle;

  HALProfiler::Instance()->startProfiling(handle);
}

void create_profile_results(void* payload)
{
  //std::cout << " create_profiling_result " << std::endl;

  ProfileResultsCBPayload* payld = (ProfileResultsCBPayload*)payload;
  xclDeviceHandle handle = payld->basePayload.deviceHandle; // HAL pointer

  HALProfiler::Instance()->createProfileResults(handle, payld->results);
}

void get_profile_results(void* payload)
{
  //std::cout << " get_profiling_result " << std::endl;

  ProfileResultsCBPayload* payld = (ProfileResultsCBPayload*)payload;
  xclDeviceHandle handle = payld->basePayload.deviceHandle; // HAL pointer

  HALProfiler::Instance()->getProfileResults(handle, payld->results);
}

void destroy_profile_results(void* payload)
{
  //std::cout << " destroy_profiling_result " << std::endl;

  ProfileResultsCBPayload* payld = (ProfileResultsCBPayload*)payload;
  xclDeviceHandle handle = payld->basePayload.deviceHandle; // HAL pointer

  HALProfiler::Instance()->destroyProfileResults(handle, payld->results);
}

void unknown_cb_type(void* payload) {
//    std::cout << "unknown_cb_type" << std::endl;
    return;
}

} //  xdp

void hal_level_xdp_cb_func(HalCallbackType cb_type, void* payload)
{
  switch (cb_type) {
    case HalCallbackType::START_DEVICE_PROFILING:
      xdp::start_device_profiling_from_hal(payload);
      break;
    case HalCallbackType::CREATE_PROFILE_RESULTS:
      xdp::create_profile_results(payload);
      break;
    case HalCallbackType::GET_PROFILE_RESULTS:
      xdp::get_profile_results(payload);
      break;
    case HalCallbackType::DESTROY_PROFILE_RESULTS:
      xdp::destroy_profile_results(payload);
      break;
    case HalCallbackType::ALLOC_BO_START:
      xdp::alloc_bo_start(payload);
      break;
    case HalCallbackType::ALLOC_BO_END:
      xdp::alloc_bo_end(payload);
      break;
    case HalCallbackType::FREE_BO_START:
      xdp::free_bo_start(payload);
      break;
    case HalCallbackType::FREE_BO_END:
      xdp::free_bo_end(payload);
      break;
    case HalCallbackType::WRITE_BO_START:
      xdp::write_bo_start(payload);
      break;
    case HalCallbackType::WRITE_BO_END:
      xdp::write_bo_end(payload);
      break;
    case HalCallbackType::READ_BO_START:
      xdp::read_bo_start(payload);
      break;
    case HalCallbackType::READ_BO_END:
      xdp::read_bo_end(payload);
      break;
    case HalCallbackType::MAP_BO_START:
      xdp::map_bo_start(payload);
      break;
    case HalCallbackType::MAP_BO_END:
      xdp::map_bo_end(payload);
      break;
    case HalCallbackType::SYNC_BO_START:
      xdp::sync_bo_start(payload);
      break;
    case HalCallbackType::SYNC_BO_END:
      xdp::sync_bo_end(payload);
      break;
    case HalCallbackType::UNMGD_READ_START:
      xdp::unmgd_read_start(payload);
      break;
    case HalCallbackType::UNMGD_READ_END:
      xdp::unmgd_read_end(payload);
      break;
    case HalCallbackType::UNMGD_WRITE_START:
      xdp::unmgd_write_start(payload);
      break;
    case HalCallbackType::UNMGD_WRITE_END:
      xdp::unmgd_write_end(payload);
      break;
    case HalCallbackType::READ_START:
      xdp::read_start(payload);
      break;
    case HalCallbackType::READ_END:
      xdp::read_end(payload);
      break;
    case HalCallbackType::WRITE_START:
      xdp::write_start(payload);
      break;
    case HalCallbackType::WRITE_END:
      xdp::write_end(payload);
      break;
    default: 
      xdp::unknown_cb_type(payload);
      break;
  }
  return;
}
