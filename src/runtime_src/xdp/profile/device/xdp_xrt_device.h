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


#ifndef _XDP_PROIFLE_XDP_XRT_DEVICE_H
#define _XDP_PROIFLE_XDP_XRT_DEVICE_H

#include "xdp/config.h"
#include "xdp_base_device.h"
#include "xrt/device/device.h"

namespace xdp {


class XrtDevice : public xdp::Device
{
  xrt::device* mXrtDevice;
   
public:
  XDP_EXPORT
  XrtDevice(xrt::device* xrtDevice);
  virtual ~XrtDevice();

  virtual std::string getDebugIPlayoutPath();
  virtual uint32_t getNumLiveProcesses();
  virtual int write(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
  virtual int read(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
  virtual int unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset);

  virtual double getDeviceClock();
  virtual uint64_t getTraceTime();
  virtual int getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
  virtual int readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);
};
}

#endif 
