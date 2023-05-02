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


#ifndef _XDP_PROIFLE_XDP_BASE_DEVICE_H
#define _XDP_PROIFLE_XDP_BASE_DEVICE_H

#include<string>
#include "core/include/xrt.h"

namespace xdp {


// interface class
class Device
{

public:
  enum class direction { HOST2DEVICE, DEVICE2HOST };

public:
  Device() {}
  virtual ~Device() {}

  virtual std::string getDebugIPlayoutPath() = 0;
  virtual uint32_t getNumLiveProcesses() = 0;
  virtual int write(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) = 0;
  virtual int read(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) = 0;
  virtual int unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset) = 0;

  // Access to IP in IP_LAYOUT
  virtual int readXrtIP(uint32_t id, uint32_t offset, uint32_t *data) = 0;
  virtual int initXrtIP(const char *name, uint64_t base, uint32_t range) = 0;

  virtual void getDebugIpLayout(char* buffer, size_t size, size_t* size_ret) = 0;

  // Only device RAM
  virtual size_t alloc(size_t sz, uint64_t memoryIndex) = 0;
  virtual void   free(size_t id) = 0;

  virtual void*  map(size_t id) = 0;
  virtual void   unmap(size_t id) = 0;
  virtual void   sync(size_t id, size_t sz, size_t offset, direction dir, bool async=false) = 0;

  virtual xclBufferExportHandle exportBuffer(size_t id) = 0;
  virtual uint64_t              getBufferDeviceAddr(size_t id) = 0;

  virtual double getDeviceClock() = 0;
  virtual uint64_t getTraceTime() = 0;

  virtual int getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz) = 0;
  virtual int readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample) = 0;

  virtual double getHostMaxBwRead() = 0;
  virtual double getHostMaxBwWrite() = 0;
  virtual double getKernelMaxBwRead() = 0;
  virtual double getKernelMaxBwWrite() = 0;

  virtual void* getRawDevice() = 0 ;

  virtual std::string getSubDevicePath(std::string& subdev, uint32_t index) = 0;

};

}

#endif 
