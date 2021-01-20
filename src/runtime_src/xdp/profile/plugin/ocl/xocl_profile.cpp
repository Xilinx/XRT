/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include "xocl_profile.h"
#include "ocl_profiler.h"
#include "xdp/profile/core/rt_profile.h"
#include "xdp/profile/profile_config.h"
#include "xdp/profile/device/tracedefs.h"
#include "xrt/device/hal.h"
#include "core/common/api/kernel_int.h"
#include "xclbin.h"
#include <regex>
#include <cctype>


namespace xdp { namespace xoclp {

// Index of CU used to execute command. This is not necessarily the
// proper CU since a command may have the option to execute on
// multiple CUs and only scheduler knows which one was picked
static unsigned int
get_cu_index(const xrt::run& run)
{
  auto& cumask = xrt_core::kernel_int::get_cumask(run);
  for (unsigned int bit = 0; bit < cumask.size(); ++bit)
    if (cumask.test(bit))
      return bit;
  return 0;
}

////////////////////////////////////////////////////////////////
// Compute unit profiling callbacks
////////////////////////////////////////////////////////////////

void get_cu_start(const xocl::execution_context* ctx, const xrt::run& run)
{
  //auto packet = cmd->get_packet();
  auto kernel = ctx->get_kernel();
  auto event = ctx->get_event();

  auto workGroupSize = kernel->get_wg_size();
  auto globalWorkDim = ctx->get_global_work_size();
  auto localWorkDim = ctx->get_local_work_size();

  auto contextId =  event->get_context()->get_uid();
  auto queue = event->get_command_queue();
  auto commandQueueId = queue->get_uid();
  auto device = queue->get_device();
  auto deviceName = device->get_name();
  auto deviceId = device->get_uid();
  auto program = kernel->get_program();
  auto programId = kernel->get_program()->get_uid();
  auto xclbin = program->get_xclbin(device);

  std::string xname = xclbin.project_name();
  std::string kname  = kernel->get_name();

  unsigned int cuIndex = get_cu_index(run);
  auto cu = device->get_compute_unit(cuIndex);
  uint64_t objId = reinterpret_cast<uint64_t>(cu);
  uint64_t eventId = reinterpret_cast<uint64_t>(event);
  std::string cuName = (cu) ? cu->get_name() : kname;

  XOCL_DEBUGF("get_cu_start: kernel=%s, CU=%s\n", kname.c_str(), cuName.c_str());

  auto rtp = OCLProfiler::Instance()->getProfileManager();
  rtp->logKernelExecution(objId, programId, eventId, xdp::RTUtil::START, kname, xname, contextId,
                          commandQueueId, deviceName, deviceId, globalWorkDim,
                          workGroupSize, localWorkDim, cuName);
}

void get_cu_done(const xocl::execution_context* ctx, const xrt::run& run)
{
  //auto packet = cmd->get_packet();
  auto kernel = ctx->get_kernel();
  auto event = ctx->get_event();

  auto workGroupSize = kernel->get_wg_size();
  auto globalWorkDim = ctx->get_global_work_size();
  auto localWorkDim = ctx->get_local_work_size();

  auto contextId =  event->get_context()->get_uid();
  auto queue = event->get_command_queue();
  auto commandQueueId = queue->get_uid();
  auto device = queue->get_device();
  auto deviceName = device->get_name();
  auto deviceId = device->get_uid();
  auto program = kernel->get_program();
  auto programId = kernel->get_program()->get_uid();
  auto xclbin = program->get_xclbin(device);

  std::string xname = xclbin.project_name();
  std::string kname  = kernel->get_name();

  unsigned int cuIndex = get_cu_index(run);
  auto cu = device->get_compute_unit(cuIndex);
  uint64_t objId = reinterpret_cast<uint64_t>(cu);
  uint64_t eventId = reinterpret_cast<uint64_t>(event);
  std::string cuName = (cu) ? cu->get_name() : kname;

  //auto offset = 1 + get_num_cu_masks(packet[0]) + 4 * get_cu_index(cmd);
  //uint64_t startTime = ((uint64_t)packet[offset+1] << 32) + packet[offset];
  //uint64_t endTime = ((uint64_t)packet[offset+3] << 32) + packet[offset+2];

  auto rtp = OCLProfiler::Instance()->getProfileManager();
  //double startTimeMsec = rtp->getTimestampMsec(startTime);
  //double endTimeMsec = rtp->getTimestampMsec(endTime);
  //double traceTimeMsec = rtp->getTraceTime();

  XOCL_DEBUGF("get_cu_done: kernel=%s, CU=%s\n", kname.c_str(), cuName.c_str());

  rtp->logKernelExecution(objId, programId, eventId, xdp::RTUtil::END, kname, xname, contextId,
                          commandQueueId, deviceName, deviceId, globalWorkDim,
                          workGroupSize, localWorkDim, cuName);
}

////////////////////////////////////////////////////////////////
// Platform
////////////////////////////////////////////////////////////////
namespace platform {

void
init(key )
{
  auto mgr = OCLProfiler::Instance()->getProfileManager();
  //mgr->setLoggingTraceUsec(0);
  for (int type=0; type < (int)XCL_PERF_MON_TOTAL_PROFILE; ++type)
    mgr->setLoggingTrace(type, false);
}

unsigned int
get_profile_num_slots(key k, const std::string& deviceName, xclPerfMonType type)
{
  auto platform = k;
  for (auto device : platform->get_device_range()) {
    std::string currDeviceName = device->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0)
      return device::getProfileNumSlots(device, type);
  }

  // If not found, return the timestamp of the first device
  auto device = platform->get_device_range()[0];
  return device::getProfileNumSlots(device.get(), type);
}

cl_int
get_profile_slot_name(key k, const std::string& deviceName, xclPerfMonType type,
		              unsigned int slotnum, std::string& slotName)
{
  auto platform = k;
  for (auto device : platform->get_device_range()) {
    std::string currDeviceName = device->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0)
      return device::getProfileSlotName(device, type, slotnum, slotName);
  }

  // If not found, return the timestamp of the first device
  auto device = platform->get_device_range()[0];
  return device::getProfileSlotName(device.get(), type, slotnum, slotName);
}

cl_int
get_trace_slot_name(key k, const std::string& deviceName, xclPerfMonType type,
		              unsigned int slotnum, std::string& slotName)
{
  auto platform = k;
  for (auto device : platform->get_device_range()) {
    std::string currDeviceName = device->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0)
      return device::getTraceSlotName(device, type, slotnum, slotName);
  }

  // If not found, return the timestamp of the first device
  auto device = platform->get_device_range()[0];
  return device::getTraceSlotName(device.get(), type, slotnum, slotName);
}

unsigned int
get_trace_slot_properties(key k, const std::string& deviceName, xclPerfMonType type,
		                      unsigned int slotnum)
{
  auto platform = k;
  for (auto device : platform->get_device_range()) {
    std::string currDeviceName = device->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0)
      return device::getTraceSlotProperties(device, type, slotnum);
  }

  // If not found, return the timestamp of the first device
  auto device = platform->get_device_range()[0];
  return device::getTraceSlotProperties(device.get(), type, slotnum);
}

unsigned int
get_profile_slot_properties(key k, const std::string& deviceName, xclPerfMonType type,
		              unsigned int slotnum)
{
  auto platform = k;
  for (auto device : platform->get_device_range()) {
    std::string currDeviceName = device->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0)
      return device::getProfileSlotProperties(device, type, slotnum);
  }

  // If not found, return the timestamp of the first device
  auto device = platform->get_device_range()[0];
  return device::getProfileSlotProperties(device.get(), type, slotnum);
}

cl_int
get_profile_kernel_name(key k, const std::string& deviceName, const std::string& cuName, std::string& kernelName)
{
  auto platform = k;
  for (auto device_id : platform->get_device_range()) {
    std::string currDeviceName = device_id->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0) {
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto currCUName = cu->get_name();
        if (currCUName.compare(cuName) == 0) {
          kernelName = cu->get_kernel_name();
        }
      }
    }
  }
  return 0;
}

size_t
get_device_timestamp(key k, const std::string& deviceName)
{
  auto platform = k;
  for (auto device : platform->get_device_range()) {
    std::string currDeviceName = device->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0)
      return device::getTimestamp(device);
  }

  // If not found, return the timestamp of the first device
  auto device = platform->get_device_range()[0];
  return device::getTimestamp(device.get());
}

double
get_device_max_read(key k)
{
  auto platform = k;
  // TODO: this is not specific to a device; is that needed?
  double maxRead = 0.0;
  for (auto device : platform->get_device_range()) {
    double currMaxRead = device::getMaxRead(device);
    maxRead = std::max(currMaxRead,maxRead);
  }
  if (maxRead == 0.0)
    maxRead = 9600.0;
  return maxRead;
}

double
get_device_max_write(key k)
{
  auto platform = k;
  // TODO: this is not specific to a device; is that needed?
  double maxWrite = 0.0;
  for (auto device : platform->get_device_range()) {
    double currMaxWrite = device::getMaxWrite(device);
    maxWrite = std::max(currMaxWrite,maxWrite);
  }
  if (maxWrite == 0.0)
    maxWrite = 9600.0;
  return maxWrite;
}

cl_int
start_device_trace(key k, xclPerfMonType type, size_t numComputeUnits)
{
  auto platform = k;
  auto mgr = OCLProfiler::Instance()->getProfileManager();
  cl_int ret = CL_SUCCESS;
  if (isValidPerfMonTypeTrace(k,type)) {
    for (auto device : platform->get_device_range()) {
      if (device->is_active())
        ret |= device::startTrace(device,type, numComputeUnits);
    }
    mgr->setLoggingTrace(type, false);
  }
  return ret;
}

cl_int
stop_device_trace(key k, xclPerfMonType type)
{
  auto platform = k;
  cl_int ret = CL_SUCCESS;
  if (isValidPerfMonTypeTrace(k,type)) {
    for (auto device : platform->get_device_range()) {
      if (device->is_active())
        ret |= device::stopTrace(device,type);
    }
  }
  return ret;
}

cl_int
log_device_trace(key k, xclPerfMonType type, bool forceRead)
{
  auto platform = k;
  auto mgr = OCLProfiler::Instance()->getProfileManager();

  // Make sure we're not overlapping multiple calls to trace
  // NOTE: This can happen when we do the 'final log' called from the singleton deconstructor
  //       which is a different thread than the event scheduler.
  if (mgr->getLoggingTrace(type)) {
    //xdp::logprintf("Trace already being logged (type=%d)\n", type);
    return -1;
  }

  cl_int ret = CL_SUCCESS;
  if (isValidPerfMonTypeTrace(k,type)) {

    // Iterate over all devices
    mgr->setLoggingTrace(type, true);
    for (auto device : platform->get_device_range()) {
      if (device->is_active())
        ret |= device::logTrace(device,type, forceRead);
    }
    mgr->setLoggingTrace(type, false);
  }
  return ret;
}

cl_int
start_device_counters(key k, xclPerfMonType type)
{
  auto platform = k;
  cl_int ret = CL_SUCCESS;
  if (isValidPerfMonTypeCounters(k,type)) {
    for (auto device : platform->get_device_range()) {
      if (device->is_active()) {
        ret |= device::startCounters(device,type);
        // TODO: figure out why we need to start trace here for counters to always work (12/14/15, schuey)
        // ret |= device::startTrace(device,type, 1);
      }
    }
  }
  return ret;
}

cl_int
stop_device_counters(key k, xclPerfMonType type)
{
  auto platform = k;
  cl_int ret = CL_SUCCESS;
  if (isValidPerfMonTypeCounters(k,type)) {
    for (auto device : platform->get_device_range()) {
      if (device->is_active())
        ret |= device::stopCounters(device,type);
    }
  }
  return ret;
}

cl_int
log_device_counters(key k, xclPerfMonType type, bool firstReadAfterProgram,
                    bool forceRead)
{
  auto platform = k;
  cl_int ret = CL_SUCCESS;
  if (isValidPerfMonTypeCounters(k,type)) {
    for (auto device : platform->get_device_range()) {
      if (device->is_active())
        ret |= device::logCounters(device,type, firstReadAfterProgram, forceRead);
    }
  }
  return ret;
}

unsigned int
get_ddr_bank_count(key k, const std::string& deviceName)
{
  auto platform = k;
  for (auto device : platform->get_device_range()) {
    std::string currDeviceName = device->get_unique_name();
    if (currDeviceName.compare(deviceName) == 0)
      return device->get_ddr_bank_count();
  }
  // If not found, return 1
  return 1;
}

bool
isValidPerfMonTypeTrace(key , xclPerfMonType type)
{
  auto profiler = OCLProfiler::Instance();
  return ((profiler->deviceTraceProfilingOn() && (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_STR))
          || ((profiler->getPlugin()->getFlowMode() == xdp::RTUtil::HW_EM) && type == XCL_PERF_MON_ACCEL));
}

bool
isValidPerfMonTypeCounters(key , xclPerfMonType type)
{
  auto profiler = OCLProfiler::Instance();
  return ((profiler->deviceCountersProfilingOn() && (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_STR))
  || ((profiler->getPlugin()->getFlowMode() == xdp::RTUtil::HW_EM) && type == XCL_PERF_MON_ACCEL));
}

bool
is_ap_ctrl_chain(key k, const std::string& deviceName, const std::string& cu)
{
  auto platform = k;
  if (platform) {
    for (auto device : platform->get_device_range()) {
      std::string currDeviceName = device->get_unique_name();
      if (currDeviceName.compare(deviceName) == 0)
        return device::isAPCtrlChain(device, cu);
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////
// Device
////////////////////////////////////////////////////////////////
namespace device {

data*
get_data(key k);

DeviceIntf* get_device_interface(key k)
{
  if(!((OCLProfiler::Instance()->getPlugin()->getFlowMode() == xdp::RTUtil::DEVICE)
            || (OCLProfiler::Instance()->getPlugin()->getFlowMode() == xdp::RTUtil::HW_EM && OCLProfiler::Instance()->getPlugin()->getSystemDPAEmulation())) )
    return nullptr;

  auto  device = k;
  auto& device_data = OCLProfiler::Instance()->DeviceData;

  auto itr = device_data.find(device);
  if (itr == device_data.end()) {
    itr = device_data.emplace(k,(new data())).first;
  }
  return  &(itr->second->mDeviceIntf);
}

unsigned int
getProfileNumSlots(key k, xclPerfMonType type)
{
  auto device = k;
  auto device_interface = get_device_interface(device);
  if(device_interface) {
    return device_interface->getNumMonitors(type);
  }
  return device->get_xdevice()->getProfilingSlots(type).get();
}

cl_int
getProfileSlotName(key k, xclPerfMonType type, unsigned int index,
		           std::string& slotName)
{
  auto device = k;
  char name[128];

  auto device_interface = get_device_interface(device);
  if(device_interface) {
    device_interface->getMonitorName(type, index, name, 128);
  } else {
    device->get_xdevice()->getProfilingSlotName(type, index, name, 128);
  }
  slotName = name;
  return CL_SUCCESS;
}

cl_int
getTraceSlotName(key k, xclPerfMonType type, unsigned int index,
		           std::string& slotName)
{
  auto device = k;
  auto device_interface = get_device_interface(device);

  if (device_interface)
    slotName = device_interface->getTraceMonName(type, index);
  else
    slotName = "";

  return CL_SUCCESS;
}

unsigned int
getTraceSlotProperties(key k, xclPerfMonType type, unsigned int index)
{
  auto device = k;
  auto device_interface = get_device_interface(device);

  if (device_interface)
    return device_interface->getTraceMonProperty(type, index);
  else
    return device->get_xdevice()->getProfilingSlotProperties(type, index).get();
}

unsigned int
getProfileSlotProperties(key k, xclPerfMonType type, unsigned int index)
{
  auto device = k;
  auto device_interface = get_device_interface(device);
  if(device_interface) {
    return device_interface->getMonitorProperties(type, index);
  }
  return device->get_xdevice()->getProfilingSlotProperties(type, index).get();
}

cl_int
startTrace(key k, xclPerfMonType type, size_t /*numComputeUnits*/)
{
  auto device = k;
  auto xdevice = device->get_xdevice();
  auto data = get_data(k);
  auto profiler = OCLProfiler::Instance();
  auto profileMgr = profiler->getProfileManager();

  // Since clock training is performed in mStartTrace, let's record this time
  data->mLastTraceTrainingTime[type] = std::chrono::steady_clock::now();
  data->mPerformingFlush = false;
  data->mLastTraceNumSamples[type] = 0;

  // Start device trace if enabled
  xdp::RTUtil::e_device_trace deviceTrace = profileMgr->getTransferTrace();
  xdp::RTUtil::e_stall_trace stallTrace = profileMgr->getStallTrace();
  uint32_t traceOption = (deviceTrace == xdp::RTUtil::DEVICE_TRACE_COARSE) ? 0x1 : 0x0;
  if (deviceTrace != xdp::RTUtil::DEVICE_TRACE_OFF) traceOption   |= (0x1 << 1);
  if (stallTrace & xdp::RTUtil::STALL_TRACE_INT)    traceOption   |= (0x1 << 2);
  if (stallTrace & xdp::RTUtil::STALL_TRACE_STR)    traceOption   |= (0x1 << 3);
  if (stallTrace & xdp::RTUtil::STALL_TRACE_EXT)    traceOption   |= (0x1 << 4);
  XOCL_DEBUGF("Starting trace with option = 0x%x\n", traceOption);
  xdevice->startTrace(type, traceOption);

  // Get/set clock freqs
  double deviceClockMHz = xdevice->getDeviceClock().get();
  if (deviceClockMHz > 0) {
    profiler->setKernelClockFreqMHz(device->get_unique_name(), static_cast<unsigned int>(deviceClockMHz) );
    profileMgr->setDeviceClockFreqMHz( deviceClockMHz );
  }

  // Get the trace samples threshold
  data->mSamplesThreshold = profileMgr->getTraceSamplesThreshold();

  // Calculate interval for clock training
  data->mTrainingIntervalUsec = (uint32_t)(pow(2, 17) / deviceClockMHz);

  return CL_SUCCESS;
}

cl_int
stopTrace(key k, xclPerfMonType type)
{
  auto device = k;
  auto xdevice = device->get_xdevice();
  xdevice->stopTrace(type);
  return CL_SUCCESS;
}

size_t
getTimestamp(key k)
{
  auto device = k;
  if(OCLProfiler::Instance()->getPlugin()->getFlowMode() == xdp::RTUtil::HW_EM) {
      return device->get_xdevice()->getDeviceTime().get();
  }
  return 0;
}

double
getMaxRead(key k)
{
  auto device = k;
  auto device_interface = get_device_interface(device);
  if (device_interface)
    return device_interface->getMaxBwRead();
  return device->get_xdevice()->getDeviceMaxRead().get();
}

double
getMaxWrite(key k)
{
  auto device = k;
  auto device_interface = get_device_interface(device);
  if (device_interface)
    return device_interface->getMaxBwRead();
  return device->get_xdevice()->getDeviceMaxWrite().get();
}

void configureDataflow(key k, xclPerfMonType type)
{
  unsigned int num_slots = getProfileNumSlots(k, type);
  auto ip_config = std::make_unique <unsigned int []>(num_slots);
  for (unsigned int i=0; i < num_slots; i++) {
    std::string slot;
    getProfileSlotName(k, type, i, slot);
    ip_config[i] = isAPCtrlChain(k, slot) ? 1 : 0;
  }

  auto device = k;
  auto xdevice = device->get_xdevice();
  xdevice->configureDataflow(type, ip_config.get());
}

cl_int
startCounters(key k, xclPerfMonType type)
{
  auto data = get_data(k);
  auto device = k;
  auto xdevice = device->get_xdevice();

  data->mPerformingFlush = false;

  // Get/set clock freqs
  double deviceClockMHz = xdevice->getDeviceClock().get();
  if (deviceClockMHz > 0)
    OCLProfiler::Instance()->getProfileManager()->setDeviceClockFreqMHz( deviceClockMHz );

  xdevice->startCounters(type);

  data->mSampleIntervalMsec =
    OCLProfiler::Instance()->getProfileManager()->getSampleIntervalMsec();

  // Depends on Debug IP Layout data loaded in hal
  configureDataflow(k, XCL_PERF_MON_ACCEL);
  return CL_SUCCESS;
}

cl_int
stopCounters(key k, xclPerfMonType type)
{
  auto device = k;
  device->get_xdevice()->stopCounters(type);
  return CL_SUCCESS;
}

cl_int
logTrace(key k, xclPerfMonType type, bool forceRead)
{
  auto data = get_data(k);
  auto device = k;
  auto xdevice = device->get_xdevice();
  auto profilemgr = OCLProfiler::Instance()->getProfileManager();

  // Create unique name for device since system can have multiples of same device
  std::string device_name = device->get_unique_name();
  std::string binary_name = "binary";
  if (device->is_active())
    binary_name = device->get_xclbin().project_name();

  // Do clock training if enough time has passed
  // NOTE: once we start flushing FIFOs, we stop all training (no longer needed)
  std::chrono::steady_clock::time_point nowTime = std::chrono::steady_clock::now();

  if (!data->mPerformingFlush &&
      (nowTime - data->mLastTraceTrainingTime[type]) > std::chrono::microseconds(data->mTrainingIntervalUsec)) {
    xdevice->clockTraining(type);
    data->mLastTraceTrainingTime[type] = nowTime;
  }

  // Read and log when trace FIFOs are filled beyond specified threshold
  uint32_t numSamples = 0;
  if (!forceRead) {
    numSamples = xdevice->countTrace(type).get();
  }

  // Control how often we do clock training: if there are new samples, then don't train
  if (numSamples > data->mLastTraceNumSamples[type]) {
    data->mLastTraceTrainingTime[type] = nowTime;
  }
  data->mLastTraceNumSamples[type] = numSamples;

  if (forceRead || (numSamples > data->mSamplesThreshold)) {

    // warning : reading from the accelerator device only
    // read the device trace
    while (1)
    {
      xdevice->readTrace(type, data->mTraceVector);
      if (data->mTraceVector.mLength == 0)
        break;

      // log and write
      profilemgr->logDeviceTrace(device_name, binary_name, type, data->mTraceVector);
      data->mTraceVector.mLength = 0;

      // Only check repeatedly for trace buffer flush if HW emulation
      if (OCLProfiler::Instance()->getPlugin()->getFlowMode() != xdp::RTUtil::HW_EM)
        break;
    }
  }

  if (forceRead)
    data->mPerformingFlush = true;
  return CL_SUCCESS;
}

cl_int
logCounters(key k, xclPerfMonType type, bool firstReadAfterProgram, bool forceRead)
{
  auto data = get_data(k);
  auto device = k;
  auto xdevice = device->get_xdevice();

  //if (data->mPerformingFlush)
  //  return CL_SUCCESS;

  std::chrono::steady_clock::time_point nowTime = std::chrono::steady_clock::now();

  if (forceRead || ((nowTime - data->mLastCountersSampleTime) > std::chrono::milliseconds(data->mSampleIntervalMsec))) {
    //warning : reading from the accelerator device only
    //read the device profile
    xdevice->readCounters(type, data->mCounterResults);

    // Record counter data
    auto timeSinceEpoch = (std::chrono::steady_clock::now()).time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch);
    uint64_t timeNsec = value.count();

    // Create unique name for device since currently all devices are called fpga0
    std::string device_name = device->get_unique_name();
    std::string binary_name = device->get_xclbin().project_name();
    auto program = device->get_program();
    auto profiler = OCLProfiler::Instance();
    uint32_t program_id = 0;
    // kernel logger logs data in this format
    if (program && profiler && profiler->getPlugin()->getFlowMode() == xdp::RTUtil::DEVICE) {
      program_id = program->get_uid();
    }

    OCLProfiler::Instance()->getProfileManager()->logDeviceCounters(device_name, binary_name, program_id, type, data->mCounterResults,
                                                                         timeNsec, firstReadAfterProgram);

    //update the last time sample
    data->mLastCountersSampleTime = nowTime;
  }
  return CL_SUCCESS;
}

#if 0
cl_int
debugReadIPStatus(key k, xclDebugReadType type, void* aDebugResults)
{
  auto device = k;
  auto xdevice = device->get_xdevice();
  //warning : reading from the accelerator device only
  //read the device profile
  xdevice->debugReadIPStatus(type, aDebugResults);
  return CL_SUCCESS;
}
#endif

template <typename SectionType>
static SectionType*
getAxlfSection(const axlf* top, axlf_section_kind kind)
{
  if (auto header = xclbin::get_axlf_section(top, kind)) {
    auto begin = reinterpret_cast<const char*>(top) + header->m_sectionOffset ;
    return reinterpret_cast<SectionType*>(begin);
  }
  return nullptr;
}

bool
isAPCtrlChain(key k, const std::string& cu)
{
  auto device = k;
  if (!device)
    return false;
  size_t base_addr = 0;
  for (auto& xcu : device->get_cus()) {
    if (xcu->get_name().compare(cu) == 0)
      base_addr = xcu->get_base_addr();
  }
  auto ip_layout = device->get_axlf_section<const ::ip_layout*>(axlf_section_kind::IP_LAYOUT);
  if (!ip_layout)
    return false;
  for (int32_t count=0; count <ip_layout->m_count; ++count) {
    const auto& ip_data = ip_layout->m_ip_data[count];
    auto current = ip_data.m_base_address;
    if (current != base_addr || ip_data.m_type != IP_TYPE::IP_KERNEL)
      continue;
    if ((ip_data.properties >> IP_CONTROL_SHIFT) & AP_CTRL_CHAIN)
      return true;
  }
  return false;
}

/*
 * Return Memory size from index in xclbin mem topology
 */
uint64_t
getMemSizeBytes(key k, int idx)
{
  auto device = k;
  if (!device)
    return false;
  auto mem_topology = device->get_axlf_section<const ::mem_topology*>(axlf_section_kind::MEM_TOPOLOGY);
  if (mem_topology && idx < mem_topology->m_count) {
    return mem_topology->m_mem_data[idx].m_size * 1024;
  }
  return 0;
}

uint64_t
getPlramSizeBytes(key k)
{
  auto device = k;
  if (!device)
    return 0;

  auto mem_tp = device->get_axlf_section<const ::mem_topology*>(axlf_section_kind::MEM_TOPOLOGY);
  if(!mem_tp)
    return 0;

  auto m_count = mem_tp->m_count;
  for (int i=0; i < m_count; i++) {
    std::string mem_tag(reinterpret_cast<const char*>(mem_tp->m_mem_data[i].m_tag));
    // work-around boost indirect include confusion with boost::placeholders
    //std::transform(mem_tag.begin(), mem_tag.end(), mem_tag.begin(), std::tolower);
    std::transform(mem_tag.begin(), mem_tag.end(), mem_tag.begin(), [](char c){return (char) std::tolower(c);});
    if (mem_tag.find("plram") != std::string::npos)
      return mem_tp->m_mem_data[i].m_size * 1024;
  }
  return 0;
}

void
getMemUsageStats(key k, std::map<std::string, uint64_t>& stats)
{
  auto device = k;
  if (!device)
    return;

  auto mem_tp = device->get_axlf_section<const ::mem_topology*>(axlf_section_kind::MEM_TOPOLOGY);
  if(!mem_tp)
    return;

  auto name = device->get_unique_name();
  auto m_count = mem_tp->m_count;
  for (int i=0; i < m_count; i++) {
    std::string mem_tag(reinterpret_cast<const char*>(mem_tp->m_mem_data[i].m_tag));
    if (mem_tag.rfind("bank", 0) == 0)
        mem_tag = "DDR[" + mem_tag.substr(4,4) + "]";
    stats[name + "|" + mem_tag] = mem_tp->m_mem_data[i].m_used;
  }
}

data*
get_data(key k)
{
  // TODO: this used to come from RTProfile, now it comes from the plugin. Is this correct?
  auto profiler = OCLProfiler::Instance();
  auto& device_data = profiler->DeviceData;

  auto itr = device_data.find(k);
  if (itr==device_data.end()) {
    itr = device_data.emplace(k,(new data())).first;
  }
  return itr->second;
}

  }} // device/platform
}} // xoclp/xdp
