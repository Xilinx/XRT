/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "xocl_profile.h"
#include "ocl_profiler.h"
#include "xdp/profile/config.h"

namespace xdp { namespace xoclp {

// Number of CU masks in packet
uint32_t
get_num_cu_masks(uint32_t header)
{
  return ((1 + (header >> 10)) & 0x3);
}

// Index of bit set to one on a 32 bit mask
uint32_t
get_cu_index_mask(uint32_t cumask)
{
  uint32_t cu_idx = 0;
  for (; (cumask & 0x1)==0; ++cu_idx, cumask >>= 1);
  return cu_idx;
}

// Index of CU used to execute command
unsigned int
get_cu_index(const xrt::command* cmd)
{
  auto& packet = cmd->get_packet();
  auto masks = get_num_cu_masks(packet[0]);

  for (unsigned int i=0; i < masks; ++i) {
    if (auto cumask = packet[i+1])
      return get_cu_index_mask(cumask) + 32*i;
  }
  return 0;
}

////////////////////////////////////////////////////////////////
// Compute unit profiling callbacks
////////////////////////////////////////////////////////////////

void get_cu_start(const xrt::command* cmd, const xocl::execution_context* ctx)
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

  unsigned int cuIndex = get_cu_index(cmd);
  auto cu = ctx->get_compute_unit(cuIndex);
  uint64_t objId = reinterpret_cast<uint64_t>(cu);
  uint64_t eventId = reinterpret_cast<uint64_t>(event);
  std::string cuName = (cu) ? cu->get_name() : kname;

  XOCL_DEBUGF("get_cu_start: kernel=%s, CU=%s\n", kname.c_str(), cuName.c_str());

  auto rtp = OCLProfiler::Instance()->getProfileManager();
  rtp->logKernelExecution(objId, programId, eventId, xdp::RTUtil::START, kname, xname, contextId,
                          commandQueueId, deviceName, deviceId, globalWorkDim,
                          workGroupSize, localWorkDim, cuName);
}

void get_cu_done(const xrt::command* cmd, const xocl::execution_context* ctx)
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

  unsigned int cuIndex = get_cu_index(cmd);
  auto cu = ctx->get_compute_unit(cuIndex);
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
init(key k)
{
  auto mgr = OCLProfiler::Instance()->getProfileManager();
  //mgr->setLoggingTraceUsec(0);
  for (int type=0; type < (int)XCL_PERF_MON_TOTAL_PROFILE; ++type)
    mgr->setLoggingTrace(type, false);
}

unsigned
get_profile_num_slots(key k, std::string& deviceName, xclPerfMonType type)
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
get_profile_slot_name(key k, std::string& deviceName, xclPerfMonType type,
		              unsigned slotnum, std::string& slotName)
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

unsigned
get_profile_slot_properties(key k, std::string& deviceName, xclPerfMonType type,
		              unsigned slotnum)
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

cl_int
write_host_event(key k, xclPerfMonEventType type, xclPerfMonEventID id)
{
  XDP_LOG("Writing host event: type = %d, ID = %d\n", type, id);

  auto platform = k;
  for (auto device : platform->get_device_range())
    device::writeHostEvent(device, type, id);
  return 0;
}

size_t 
get_device_timestamp(key k, std::string& deviceName)
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
      ret |= device::startCounters(device,type);
      // TODO: figure out why we need to start trace here for counters to always work (12/14/15, schuey)
      ret |= device::startTrace(device,type, 1);
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
isValidPerfMonTypeTrace(key k, xclPerfMonType type)
{
  auto profiler = OCLProfiler::Instance();
  return ((profiler->deviceTraceProfilingOn() && (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_STR))
          || ((profiler->getPlugin()->getFlowMode() == xdp::RTUtil::HW_EM) && type == XCL_PERF_MON_ACCEL));
}

bool 
isValidPerfMonTypeCounters(key k, xclPerfMonType type)
{
  auto profiler = OCLProfiler::Instance();
  return ((profiler->deviceCountersProfilingOn() && (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_STR))
  || ((profiler->getPlugin()->getFlowMode() == xdp::RTUtil::HW_EM) && type == XCL_PERF_MON_ACCEL));
}

////////////////////////////////////////////////////////////////
// Device
////////////////////////////////////////////////////////////////
namespace device {

data*
get_data(key k);

void
init(key k)
{
  auto data = get_data(k);
  data->mPerformingFlush = false;
  auto nowTime = std::chrono::steady_clock::now();
  data->mLastCountersSampleTime = nowTime;
  for (int i=0; i < XCL_PERF_MON_TOTAL_PROFILE; ++i)
    data->mLastTraceTrainingTime[i] = nowTime;
}
  
cl_int
setProfileNumSlots(key k, xclPerfMonType type, unsigned numSlots)
{
  auto device = k;
  device->get_xrt_device()->setProfilingSlots(type, numSlots);
  return CL_SUCCESS;
}

unsigned
getProfileNumSlots(key k, xclPerfMonType type)
{
  auto device = k;
  return device->get_xrt_device()->getProfilingSlots(type).get();
}

cl_int
getProfileSlotName(key k, xclPerfMonType type, unsigned slotnum,
		           std::string& slotName)
{
  auto device = k;
  char name[128];
  device->get_xrt_device()->getProfilingSlotName(type, slotnum, name, 128);
  slotName = name;
  return CL_SUCCESS;
}

unsigned
getProfileSlotProperties(key k, xclPerfMonType type, unsigned slotnum)
{
  auto device = k;
  return device->get_xrt_device()->getProfilingSlotProperties(type, slotnum).get();
}

cl_int 
writeHostEvent(key k, xclPerfMonEventType type, xclPerfMonEventID id)
{
  auto device = k;

  // Only Zynq is supported in 2017.1
  // NOTE: Zynq device names are currently blank
  //std::string name = device->get_name();
  //if (name.find("zcu102") != std::string::npos) {
  //  XDP_LOG("  Writing host event to %s\n", name.c_str());
    device->get_xrt_device()->writeHostEvent(type, id);
  //}
  return CL_SUCCESS;
}

cl_int
startTrace(key k, xclPerfMonType type, size_t numComputeUnits)
{
  auto device = k;
  auto xdevice = device->get_xrt_device();
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
    profiler->setKernelClockFreqMHz(device->get_unique_name(), deviceClockMHz );
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
  device->get_xrt_device()->stopTrace(type);
  return CL_SUCCESS;
}

size_t 
getTimestamp(key k)
{
  auto device = k;
  return device->get_xrt_device()->getDeviceTime().get();
}

double 
getMaxRead(key k)
{
  auto device = k;
  return device->get_xrt_device()->getDeviceMaxRead().get();
}

double 
getMaxWrite(key k)
{
  auto device = k;
  return device->get_xrt_device()->getDeviceMaxWrite().get();
}

cl_int 
startCounters(key k, xclPerfMonType type)
{
  auto data = get_data(k);
  auto device = k;
  auto xdevice = device->get_xrt_device();

  data->mPerformingFlush = false;

  // Get/set clock freqs
  double deviceClockMHz = xdevice->getDeviceClock().get();
  if (deviceClockMHz > 0)
    OCLProfiler::Instance()->getProfileManager()->setDeviceClockFreqMHz( deviceClockMHz );

  xdevice->startCounters(type);
  data->mSampleIntervalMsec =
    OCLProfiler::Instance()->getProfileManager()->getSampleIntervalMsec();
  return CL_SUCCESS;
}

cl_int 
stopCounters(key k, xclPerfMonType type)
{
  auto device = k;
  device->get_xrt_device()->stopCounters(type);
  return CL_SUCCESS;
}

cl_int 
logTrace(key k, xclPerfMonType type, bool forceRead)
{
  auto data = get_data(k);
  auto device = k;
  auto xdevice = device->get_xrt_device();

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
    // Create unique name for device since system can have multiples of same device
    std::string device_name = device->get_unique_name();
    std::string binary_name = "binary";
    if (device->is_active())
      binary_name = device->get_xclbin().project_name();

    // warning : reading from the accelerator device only
    // read the device trace
    while (1)
    {
      xdevice->readTrace(type, data->mTraceVector);
      if (data->mTraceVector.mLength == 0)
        break;

      // log the device trace
      OCLProfiler::Instance()->getProfileManager()->logDeviceTrace(device_name, binary_name, type, data->mTraceVector);
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
  auto xdevice = device->get_xrt_device();

  //if (data->mPerformingFlush)
  //  return CL_SUCCESS;

  std::chrono::steady_clock::time_point nowTime = std::chrono::steady_clock::now();
  
  if (forceRead || ((nowTime - data->mLastCountersSampleTime) > std::chrono::milliseconds(data->mSampleIntervalMsec))) {
    //warning : reading from the accelerator device only
    //read the device profile
    xdevice->readCounters(type, data->mCounterResults);
    struct timespec now;
    int err = clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t timeNsec = (err < 0) ? 0 : (uint64_t) now.tv_sec * 1000000000UL + (uint64_t) now.tv_nsec;
    
    // Create unique name for device since currently all devices are called fpga0
    std::string device_name = device->get_unique_name();
    std::string binary_name = device->get_xclbin().project_name();

    OCLProfiler::Instance()->getProfileManager()->logDeviceCounters(device_name, binary_name, type, data->mCounterResults,
                                                                         timeNsec, firstReadAfterProgram);

    //update the last time sample
    data->mLastCountersSampleTime = nowTime;
  }
  return CL_SUCCESS;
}

cl_int
debugReadIPStatus(key k, xclDebugReadType type, void* aDebugResults)
{
  auto device = k;
  auto xdevice = device->get_xrt_device();
  //warning : reading from the accelerator device only
  //read the device profile
  xdevice->debugReadIPStatus(type, aDebugResults);
  return CL_SUCCESS;
}

data*
get_data(key k) 
{ 
  // TODO: this used to come from RTProfile, now it comes from the plugin. Is this correct?
  auto profiler = OCLProfiler::Instance();
  auto& device_data = profiler->DeviceData;

  auto itr = device_data.find(k);
  if (itr==device_data.end()) {
    itr = device_data.emplace(k,data()).first;
  }
  return &(*itr).second;
}

  }} // device/platform 
}} // xoclp/xdp


