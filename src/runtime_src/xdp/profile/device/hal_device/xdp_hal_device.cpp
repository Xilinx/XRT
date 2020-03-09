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

#ifdef _WIN32
#pragma warning (disable : 4267)
/* 4267 : Disable warning for conversion of size_t to int in return statements in read/write methods */
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

size_t HalDevice::alloc(size_t, uint64_t)
{
  return 0;
}

void HalDevice::free(size_t)
{

}

void* HalDevice::map(size_t)
{
  return nullptr;
}

void HalDevice::unmap(size_t)
{

}

void HalDevice::sync(size_t, size_t, size_t, direction, bool)
{

}

uint64_t HalDevice::getDeviceAddr(size_t)
{
  return 0;
}

}

