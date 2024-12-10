/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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


#include "xdp_hal_device.h"
#include "core/common/time.h"
#include "core/common/ishim.h"
#include "core/common/system.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"

#include "core/include/xrt/experimental/xrt-next.h"
#include "core/include/xrt/experimental/xrt_device.h"

#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include<iostream>

#ifdef _WIN32
#pragma warning (disable : 4267 4244)
/* 4267 : Disable warning for conversion of size_t to int in return statements in read/write methods */
/* 4244 : Disable warning for conversion of uint64_t to unsigned int in "flag" argument in xclAllocBO */
#endif

namespace xdp {

using severity_level = xrt_core::message::severity_level;

HalDevice::HalDevice(void* halDeviceHandle)
          : Device(),
            mHalDevice(halDeviceHandle)
{
  mXrtCoreDevice = xrt_core::get_userpf_device(mHalDevice);
}

HalDevice::~HalDevice()
{
}

std::string HalDevice::getDebugIPlayoutPath()
{
  return util::getDebugIpLayoutPath(mXrtCoreDevice->get_user_handle());
}
uint32_t HalDevice::getNumLiveProcesses()
{
  uint32_t liveProcessesOnDevice = 0;
  try {
    liveProcessesOnDevice = xrt_core::device_query<xrt_core::query::num_live_processes>(mXrtCoreDevice);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving number of live processes. Using default value.";
    xrt_core::message::send(severity_level::warning, "XRT", msg);
  }
  return liveProcessesOnDevice;
}
int HalDevice::write(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  try{
    mXrtCoreDevice->xwrite(space, offset, hostBuf, size);
  }
  catch(const std::exception&){
    std::string msg = "Profiling will not be available. Reason: xwrite failed";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
  return 0;
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif
}
int HalDevice::read(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  try{
    mXrtCoreDevice->xread(space, offset, hostBuf, size);
  }
  catch(const std::exception&){
    std::string msg = "Profiling will not be available. Reason: xread failed";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
  return 0;
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif
}

int HalDevice::unmgdRead(unsigned , void *buf, size_t count, uint64_t offset)
{
  try{
    mXrtCoreDevice->unmgd_pread(buf, count, offset);
  }
  catch(const std::exception& ex){
    xrt_core::message::send(severity_level::error, "XRT", ex.what());
  }
  return 0;
}


std::vector<char> HalDevice::getDebugIpLayout()
{
  std::vector<char> bufferData;
  try {
    bufferData = xrt_core::device_query<xrt_core::query::debug_ip_layout_raw>(mXrtCoreDevice);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving debug IP layout.";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
  return bufferData;
 }

double HalDevice::getDeviceClock()
{
  double deviceClockFreqMHz = 0.0;
  try {
    deviceClockFreqMHz = xrt_core::device_query<xrt_core::query::device_clock_freq_mhz>(mXrtCoreDevice);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving device clock frequency. Using default value.";
    xrt_core::message::send(severity_level::warning, "XRT", msg);
  }
  return deviceClockFreqMHz;
}

uint64_t HalDevice::getTraceTime()
{
  return xrt_core::time_ns();
}

int HalDevice::getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  try {
    auto traceBufInfo = xrt_core::device_query<xrt_core::query::trace_buffer_info>(mXrtCoreDevice, nSamples);
    traceSamples = traceBufInfo.samples;
    traceBufSz = traceBufInfo.buf_size;
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving trace buffer information. Using default value.";
    xrt_core::message::send(severity_level::warning, "XRT", msg);
  }
  return 0;
}

int HalDevice::readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  std::vector<uint32_t> traceData(traceBufSz);
  xrt_core::query::read_trace_data::args traceDataArgs = {traceBufSz, numSamples, ipBaseAddress, wordsPerSample};
  try {
    traceData = xrt_core::device_query<xrt_core::query::read_trace_data>(mXrtCoreDevice, traceDataArgs);
    std::memcpy(traceBuf, traceData.data(), traceData.size()*sizeof(uint32_t));
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving trace data.";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
  return 0;
}

size_t HalDevice::alloc(size_t size, uint64_t memoryIndex)
{
  uint64_t flags = memoryIndex;
  flags |= XCL_BO_FLAGS_CACHEABLE;

  xrt_bos.push_back(xrt::bo(mHalDevice, size, flags, memoryIndex));
  return xrt_bos.size();
}

void HalDevice::free(size_t)
{
  return;
}

void* HalDevice::map(size_t id)
{
  if(!id) return nullptr;
  size_t boIndex = id - 1;
  return xrt_bos[boIndex].map();
}

void HalDevice::unmap(size_t)
{
  return;
}

void HalDevice::sync(size_t id, size_t size, size_t offset, direction d, bool )
{
  if(!id) return;
  size_t boIndex = id - 1;
  xclBOSyncDirection dir = (d == direction::DEVICE2HOST) ? XCL_BO_SYNC_BO_FROM_DEVICE : XCL_BO_SYNC_BO_TO_DEVICE;

  xrt_bos[boIndex].sync(dir, size, offset);
}

xclBufferExportHandle HalDevice::exportBuffer(size_t id)
{
  if(!id) return static_cast<xclBufferExportHandle>(XRT_NULL_BO_EXPORT);
  size_t boIndex = id - 1;

  return (xrt_bos[boIndex].export_buffer());
}

uint64_t HalDevice::getBufferDeviceAddr(size_t id)
{
  if(!id) return 0;
  size_t boIndex = id - 1;

  return xrt_bos[boIndex].address();
}

double HalDevice::getHostMaxBwRead()
{
  double hostMaxReadBW = 0.0;
  try {
    hostMaxReadBW = xrt_core::device_query<xrt_core::query::host_max_bandwidth_mbps>(mXrtCoreDevice, true);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented 
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving host max read bandwidth. Using default value.";
    xrt_core::message::send(severity_level::warning, "XRT", msg);
  }
  return hostMaxReadBW;
}

double HalDevice::getHostMaxBwWrite()
{
  double hostMaxWriteBW = 0.0;
  try {
    hostMaxWriteBW = xrt_core::device_query<xrt_core::query::host_max_bandwidth_mbps>(mXrtCoreDevice, false);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving host max write bandwidth. Using default value.";
    xrt_core::message::send(severity_level::warning, "XRT", msg);
  }
   return hostMaxWriteBW;
}

double HalDevice::getKernelMaxBwRead()
{
  double kernelMaxReadBW = 0.0;
  try {
    kernelMaxReadBW = xrt_core::device_query<xrt_core::query::kernel_max_bandwidth_mbps>(mXrtCoreDevice, true);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving kernel max read bandwidth. Using default value.";
    xrt_core::message::send(severity_level::warning, "XRT", msg);
  }
  return kernelMaxReadBW;
}

double HalDevice::getKernelMaxBwWrite()
{
  double kernelMaxWriteBW = 0.0;
  try {
    kernelMaxWriteBW = xrt_core::device_query<xrt_core::query::kernel_max_bandwidth_mbps>(mXrtCoreDevice, false);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving kernel max write bandwidth. Using default value.";
    xrt_core::message::send(severity_level::warning, "XRT", msg);
  }
  return kernelMaxWriteBW;
}

std::string HalDevice::getSubDevicePath(std::string& subdev, uint32_t index)
{
  std::string subDevicePath = "" ;
  xrt_core::query::sub_device_path::args subDevicePathArgs = {subdev, index};
  try {
    subDevicePath = xrt_core::device_query<xrt_core::query::sub_device_path>(mXrtCoreDevice, subDevicePathArgs);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving sub device path.";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
  return subDevicePath;
}

}
