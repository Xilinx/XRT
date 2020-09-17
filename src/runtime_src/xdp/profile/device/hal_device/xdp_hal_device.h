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


#ifndef _XDP_PROIFLE_XDP_HAL_DEVICE_H
#define _XDP_PROIFLE_XDP_HAL_DEVICE_H

#include<vector>
#include "core/include/xrt.h"
#include "core/include/experimental/xrt_bo.h"
#include "xdp/profile/device/xdp_base_device.h"

namespace xdp {


class HalDevice : public xdp::Device
{
  xclDeviceHandle mHalDevice;
  std::vector<xrtBufferHandle> mBOHandles;
  std::vector<void*>  mMappedBO;

public:
  HalDevice(void* halDeviceHandle);
  virtual ~HalDevice();

  virtual std::string getDebugIPlayoutPath();
  virtual uint32_t getNumLiveProcesses();
  virtual int write(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
  virtual int read(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
  virtual int unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset);

  virtual void getDebugIpLayout(char* buffer, size_t size, size_t* size_ret);

  virtual double getDeviceClock();
  virtual uint64_t getTraceTime();
  virtual int getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
  virtual int readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);

  virtual size_t alloc(size_t sz, uint64_t memoryIndex);
  virtual void free(size_t xdpBoHandle);
  virtual void* map(size_t xdpBoHandle);
  virtual void unmap(size_t xdpBoHandle);
  virtual void sync(size_t xdpBoHandle, size_t sz, size_t offset, direction dir, bool async=false);
  virtual uint64_t getDeviceAddr(size_t xdpBoHandle);
  virtual void* getRawDevice() { return mHalDevice ; }

  virtual double getMaxBwRead();
  virtual double getMaxBwWrite();

  virtual std::string getSubDevicePath(std::string& subdev, uint32_t index);
};
}

#endif 
