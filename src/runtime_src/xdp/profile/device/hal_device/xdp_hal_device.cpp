/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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
#include "core/common/system.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/xrt_profiling.h"

#include "core/include/experimental/xrt-next.h"
#include "core/include/experimental/xrt_device.h"

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
  std::string path = "";
  try {
    uint32_t size = 512;
    path = xrt_core::device_query<xrt_core::query::debug_ip_layout_path>(mXrtCoreDevice, size);
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving the information about Debug IP Layout Path.";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
  return path;
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
    std::string msg = "Error while retrieving the information about Number of Live Processes. Setting it to default value.";
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
  return xclWrite(mHalDevice, space, offset, hostBuf, size);
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
  return xclRead(mHalDevice, space, offset, hostBuf, size);
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif
}

// This uses mmap and is recommended way to access an XRT IP
int HalDevice::readXrtIP(uint32_t index, uint32_t offset, uint32_t *data)
{
  return xclRegRead(mHalDevice, index, offset, data);
}

#if defined(_WIN32) || defined(XDP_HWEMU_USING_HAL_BUILD)
int HalDevice::initXrtIP(const char * /*name*/, uint64_t /*base*/, uint32_t /*range*/)
{
  // The required APIs are missing from windows and hw emulation shim
  return -1;
}
#else
int HalDevice::initXrtIP(const char *name, uint64_t base, uint32_t range)
{
  // We cannot always get index from ip_layout
  // For some cases, this is determined by the driver
  int index = xclIPName2Index(mHalDevice, name);
  if (index < 0)
    return index;

  // A shared context is needed
  std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(mHalDevice);
  int ret = xclOpenContext(mHalDevice, device->get_xclbin_uuid().get(), index, true);
  if (ret < 0)
    return ret;

  // Open access to IP Registers. base should be > 0x10
  ret = xclIPSetReadRange(mHalDevice, index, base, range);
  if (ret < 0)
    return ret;

  return index;
}
#endif


int HalDevice::unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset)
{
  return xclUnmgdPread(mHalDevice, flags, buf, count, offset);
}


void HalDevice::getDebugIpLayout(char* buffer, size_t size, size_t* size_ret)
{
  std::vector<char> bufferData;
  try {
    bufferData = xrt_core::device_query<xrt_core::query::debug_ip_layout_raw>(mXrtCoreDevice);
    std::memcpy(buffer, bufferData.data(), bufferData.size()*sizeof(char));
  }
  catch (const xrt_core::query::no_such_key&) {
    //query is not implemented
  }
  catch (const std::exception&) {
    // error retrieving information
    std::string msg = "Error while retrieving Debug IP Layout information.";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
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
    std::string msg = "Error while retrieving the information about Device Clock Frequency. Setting it to default value.";
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
    std::string msg = "Error while retrieving the information about Trace Buffer. Setting it to default value.";
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
    std::string msg = "Error while retrieving the information about Trace Data.";
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
    std::string msg = "Error while retrieving the information about Host Max Read Bandwidth. Setting it to default value.";
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
    std::string msg = "Error while retrieving the information about Host Max Write Bandwidth. Setting it to default value.";
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
    std::string msg = "Error while retrieving the information about Kernel Max Read Bandwidth. Setting it to default value.";
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
    std::string msg = "Error while retrieving the information about Kernel Max Write Bandwidth. Setting it to default value.";
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
    std::string msg = "Error while retrieving the information about Sub Device Path.";
    xrt_core::message::send(severity_level::error, "XRT", msg);
  }
  return subDevicePath;
}

}
