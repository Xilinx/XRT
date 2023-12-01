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


#include "xdp_client_device.h"
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


ClientDevice::ClientDevice(void* ClientDeviceHandle)
          : Device(),
            mClientDevice(ClientDeviceHandle)
{
}

ClientDevice::~ClientDevice()
{
}

std::string ClientDevice::getDebugIPlayoutPath()
{
  return "";
}
uint32_t ClientDevice::getNumLiveProcesses()
{
  return 0;
}
int ClientDevice::write(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  (void) space;
  (void) offset;
  (void) hostBuf;
  (void) size;
  return 0;
}

// This uses mmap and is recommended way to access an XRT IP
int ClientDevice::readXrtIP(uint32_t index, uint32_t offset, uint32_t *data)
{
  (void) offset;
  (void) data;
  return index;
}

#if defined(_WIN32) || defined(XDP_HWEMU_USING_HAL_BUILD)
int ClientDevice::initXrtIP(const char * /*name*/, uint64_t /*base*/, uint32_t /*range*/)
{
  // The required APIs are missing from windows and hw emulation shim
  return -1;
}
#else
int ClientDevice::initXrtIP(const char *name, uint64_t base, uint32_t range)
{
 (void) name;
 (void) base;
 (void) range;
 return 0;
}
#endif


int ClientDevice::unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset)
{
  (void) flags;
  (void) buf;
  (void) count;
  (void) offset;
  return 0;
}


void ClientDevice::getDebugIpLayout(char* buffer, size_t size, size_t* size_ret)
{
  (void) buffer;
  (void) size;
  (void) size_ret;
}

double ClientDevice::getDeviceClock()
{
  return 0;
}

uint64_t ClientDevice::getTraceTime()
{
  return 0;
}

int ClientDevice::getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  (void) nSamples;
  (void) traceSamples;
  (void) traceBufSz;
  return 0;
}

int ClientDevice::read(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) {
  (void) space;
  (void) offset; 
  (void) hostBuf;
  (void) size;
  return 0;
}


int ClientDevice::readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  (void) traceBuf;
  (void) traceBufSz;
  (void) numSamples;
  (void) ipBaseAddress;
  (void) wordsPerSample;
  return 0;
}

size_t ClientDevice::alloc(size_t size, uint64_t memoryIndex)
{
  (void) memoryIndex;
  return size;
}

void ClientDevice::free(size_t)
{
  return;
}

void* ClientDevice::map(size_t id)
{
  (void)id;
  return nullptr;
}

void ClientDevice::unmap(size_t)
{
  return;
}

void ClientDevice::sync(size_t id, size_t size, size_t offset, direction d, bool )
{
  (void) id;
  (void) size;
  (void) offset;
  (void) d;
}

xclBufferExportHandle ClientDevice::exportBuffer(size_t id)
{
  (void) id;
  return 0;
}

uint64_t ClientDevice::getBufferDeviceAddr(size_t id)
{
  (void) id;
  return 0;
}

double ClientDevice::getHostMaxBwRead()
{
  return 0;
}

double ClientDevice::getHostMaxBwWrite()
{
  return 0;
}

double ClientDevice::getKernelMaxBwRead()
{
  return 0;
}

double ClientDevice::getKernelMaxBwWrite()
{
   return 0;
}

std::string ClientDevice::getSubDevicePath(std::string& subdev, uint32_t index)
{
  (void) index;
  return subdev;
}

}
