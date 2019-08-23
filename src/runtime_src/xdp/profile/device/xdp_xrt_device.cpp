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


#include "xdp_xrt_device.h"

namespace xdp {

XrtDevice::XrtDevice(xrt::device* xrtDev)
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

double XrtDevice::getDeviceClock()
{
  return mXrtDevice->getDeviceClock().get();
}

int XrtDevice::getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  return mXrtDevice->getTraceBufferInfo(nSamples, traceSamples, traceBufSz).get();
}

int XrtDevice::readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  return mXrtDevice->readTraceData(traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample).get();
}

}

