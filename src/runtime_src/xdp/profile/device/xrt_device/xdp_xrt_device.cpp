/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

#include "xdp_xrt_device.h"
#include "core/common/time.h"

namespace xdp {

XrtDevice::XrtDevice(xrt_xocl::device* xrtDev)
          : Device(),
            mXrtDevice(xrtDev)
{}

XrtDevice::~XrtDevice()
{}

std::string XrtDevice::getDebugIPlayoutPath()
{
  return mXrtDevice->getDebugIPlayoutPath().get();
}
uint32_t XrtDevice::getNumLiveProcesses()
{
   return mXrtDevice->getNumLiveProcesses().get();
}
int XrtDevice::write(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  mXrtDevice->xclWrite(space, offset, hostBuf, size);
  return 0;
}
int XrtDevice::read(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  mXrtDevice->xclRead(space, offset, hostBuf, size);
  return 0;
}

int XrtDevice::unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset)
{
  mXrtDevice->xclUnmgdPread(flags, buf, count, offset);
  return 0;
}

void XrtDevice::getDebugIpLayout(char* buffer, size_t size, size_t* size_ret)
{
   mXrtDevice->getDebugIpLayout(buffer, size, size_ret);
}


double XrtDevice::getDeviceClock()
{
  return mXrtDevice->getDeviceClock().get();
}

uint64_t XrtDevice::getTraceTime()
{
  return xrt_core::time_ns();
}

int XrtDevice::getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  return mXrtDevice->getTraceBufferInfo(nSamples, traceSamples, traceBufSz).get();
}

int XrtDevice::readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  return mXrtDevice->readTraceData(traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample).get();
}

/*
 * Allocate Device buffer on DDR/HBM Bank
 * Return Val: 0 if unsuccessful, > 0 if successful
 * XDP BO Handle is just an index in BO vector
 * Actual XRT BO Handle is stored within this vector
 */
size_t XrtDevice::alloc(size_t sz, uint64_t memoryIndex)
{
  try {
    auto handle = mXrtDevice->alloc(sz, xrt_xocl::hal::device::Domain::XRT_DEVICE_RAM, memoryIndex, nullptr);
    m_bos.push_back(std::move(handle));
    return m_bos.size();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    return 0;
  }
}

/*
 * BO vector is emptied only at the destruction
 * User is responsible for freeing the allocated buffer
 */
void XrtDevice::free(size_t xdpBoHandle)
{
  if (!xdpBoHandle) return;
  auto idx = xdpBoHandle - 1;
  m_bos[idx] = xrt::bo{};
}

void* XrtDevice::map(size_t xdpBoHandle)
{
  if (!xdpBoHandle) return nullptr;
  auto idx = xdpBoHandle - 1;
  return mXrtDevice->map(m_bos[idx]);
}

void XrtDevice::unmap(size_t xdpBoHandle)
{
  if (!xdpBoHandle) return;
  auto idx = xdpBoHandle - 1;
  return mXrtDevice->unmap(m_bos[idx]);
}

void XrtDevice::sync(size_t xdpBoHandle, size_t sz, size_t offset, direction dir, bool async)
{
  if (!xdpBoHandle) return;
  auto idx = xdpBoHandle - 1;
  auto dir1 = xrt_xocl::hal::device::direction::HOST2DEVICE;
  if (dir == direction::DEVICE2HOST)
    dir1 = xrt_xocl::hal::device::direction::DEVICE2HOST;
  mXrtDevice->sync(m_bos[idx], sz, offset, dir1, async);
}

uint64_t XrtDevice::getDeviceAddr(size_t xdpBoHandle)
{
  if (!xdpBoHandle) return 0;
  auto idx = xdpBoHandle - 1;
  return mXrtDevice->getDeviceAddr(m_bos[idx]);
}

double XrtDevice::getMaxBwRead()
{
  return mXrtDevice->getDeviceMaxRead().get();
}

double XrtDevice::getMaxBwWrite()
{
  return mXrtDevice->getDeviceMaxWrite().get();
}

std::string XrtDevice::getSubDevicePath(std::string& subdev, uint32_t index)
{
  return mXrtDevice->getSubdevPath(subdev, index).get();
}

}

