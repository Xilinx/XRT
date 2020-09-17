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


#include "xdp_hal_device.h"
#include "core/common/time.h"
#include "core/common/xrt_profiling.h"
#include "core/include/experimental/xrt-next.h"
#include "core/include/experimental/xrt_device.h"

#ifdef _WIN32
#pragma warning (disable : 4267 4244)
/* 4267 : Disable warning for conversion of size_t to int in return statements in read/write methods */
/* 4244 : Disable warning for conversion of uint64_t to unsigned int in "flag" argument in xclAllocBO */
#endif

namespace xdp {


HalDevice::HalDevice(void* halDeviceHandle)
          : Device(),
            mHalDevice(halDeviceHandle)
{}

HalDevice::~HalDevice()
{}

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

  xrtBufferHandle boHandle = xrtBOAlloc(xrtDeviceOpenFromXcl(mHalDevice), size, flags, memoryIndex);
  if(nullptr == boHandle) {
    throw std::bad_alloc();
  }
  mBOHandles.push_back(boHandle);

  void* ptr = xrtBOMap(boHandle);
  mMappedBO.push_back(ptr);
  return mBOHandles.size();
}

void HalDevice::free(size_t id)
{
  if(!id) return;
  size_t boIndex = id - 1;
  xrtBOFree(mBOHandles[boIndex]);
}

void* HalDevice::map(size_t id)
{
  if(!id) return nullptr;
  size_t boIndex = id - 1;
  return mMappedBO[boIndex];
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
  xrtBOSync(mBOHandles[boIndex], dir, size, offset);
}

uint64_t HalDevice::getDeviceAddr(size_t id)
{
  if(!id) return 0;
  size_t boIndex = id - 1;

  return xrtBOAddress(mBOHandles[boIndex]);
}

double HalDevice::getMaxBwRead()
{
  return 9600.0;
}

double HalDevice::getMaxBwWrite()
{
  return 9600.0;
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

