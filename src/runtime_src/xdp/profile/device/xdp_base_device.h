

#ifndef _XDP_PROIFLE_XDP_BASE_DEVICE_H
#define _XDP_PROIFLE_XDP_BASE_DEVICE_H

#include<string>
#include "core/include/xrt.h"

namespace xdp {


// interface class
class Device
{
public:
  Device() {}
  ~Device() {}

  virtual std::string getDebugIPlayoutPath() = 0;
  virtual uint32_t getNumLiveProcesses() = 0;
  virtual int write(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) = 0;
  virtual int read(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) = 0;
  virtual int unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset) = 0;

  virtual double getDeviceClock() = 0;

  virtual int getTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz) = 0;
  virtual int readTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample) = 0;

//  virtual uint64_t getDeviceAddr(unsigned int) = 0;
   // alloc
   // sync
   // getDeviceAddr
   // map
   // sync
   // munmap ?
   // free
};
}

#endif 
