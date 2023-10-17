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


HalDevice::HalDevice(void* halDeviceHandle)
          : Device(),
            mHalDevice(halDeviceHandle)
{
}

HalDevice::~HalDevice()
{
}

std::string HalDevice::getDebugIPlayoutPath()
{
  char layoutPath[512];
  xclGetDebugIPlayoutPath(mHalDevice, layoutPath, 512);
  return std::string(layoutPath);
}
uint32_t HalDevice::getNumLiveProcesses()
{
  return xclGetNumLiveProcesses(mHalDevice);
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
  xclGetDebugIpLayout(mHalDevice, buffer, size, size_ret);
}

double HalDevice::getDeviceClock()
{
  return xclGetDeviceClockFreqMHz(mHalDevice);
}

uint64_t HalDevice::getTraceTime()
{
  return xrt_core::time_ns();
}

int HalDevice::getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  return xclGetTraceBufferInfo(mHalDevice, nSamples, traceSamples, traceBufSz);
}

int HalDevice::readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  return xclReadTraceData(mHalDevice, traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample);
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
  return xclGetHostReadMaxBandwidthMBps(mHalDevice);
}

double HalDevice::getHostMaxBwWrite()
{
   return xclGetHostWriteMaxBandwidthMBps(mHalDevice);
}

double HalDevice::getKernelMaxBwRead()
{
  return xclGetKernelReadMaxBandwidthMBps(mHalDevice);
}

double HalDevice::getKernelMaxBwWrite()
{
   return xclGetKernelWriteMaxBandwidthMBps(mHalDevice);
}

std::string HalDevice::getSubDevicePath(std::string& subdev, uint32_t index)
{
  constexpr size_t maxSz = 256;
  char buffer[maxSz];
  buffer[maxSz - 1] = '\0';
  xclGetSubdevPath(mHalDevice, subdev.c_str(), index, buffer, maxSz);

  return std::string(buffer);
}

}
