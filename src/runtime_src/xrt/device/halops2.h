/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef xrt_device_halops2_h
#define xrt_device_halops2_h

#include "driver/include/xclhal2.h"
#include <string>

/**
 * This file provides a C++ API into a HAL user shim C library.
 *
 * At most one function with a particular name can have "C" linkage.
 * This means that xclhal1.h and xclhal2.h cannot be included in a
 * single compilation unit.  Alas, this header cannot be included
 * along with another header for a different HAL library.
 */

namespace xrt { namespace hal2 {

/* TBD */
typedef xclVerbosityLevel     verbosity_level;
typedef xclDeviceHandle       device_handle;
typedef xclDeviceInfo2        device_info;
typedef xclPerfMonType        perfmon_type;
typedef xclPerfMonEventType   perfmon_event_type;
typedef xclPerfMonEventID     perfmon_event_id;

class operations
{
public:
  operations(const std::string &fileName, void *fileHandle, unsigned int count);
  ~operations();

private:

  /* TBD */
  typedef unsigned (* probeFuncType)();
  typedef xclDeviceHandle (* openFuncType)(unsigned deviceIndex, const char *logFileName,
                                           xclVerbosityLevel level);
  typedef void (* closeFuncType)(xclDeviceHandle handle);
  //typedef void (* freeDeviceBufferType)(xclDeviceHandle handle, unsigned int boHandle);
  typedef int (* loadBitstreamFuncType)(xclDeviceHandle handle, const char *fileName);
  typedef int (* loadXclBinFuncType)(xclDeviceHandle handle, const xclBin *buffer);
  typedef unsigned int (*allocBOFuncType) (xclDeviceHandle handle, size_t size, xclBOKind domain, unsigned flags);
  typedef unsigned int (*allocUserPtrBOFuncType) (xclDeviceHandle handle, void* userptr, size_t size, unsigned flags);

  typedef unsigned int (*importBOFuncType)(xclDeviceHandle handle, int fd, unsigned flags);
  typedef unsigned int (*exportBOFuncType)(xclDeviceHandle handle, unsigned int boHandle);
  typedef int (*getBOPropertiesFuncType)(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties*);
  typedef unsigned int (*execBOFuncType)(xclDeviceHandle handle, unsigned int cmdBO);
  typedef int (*execWaitFuncType)(xclDeviceHandle handle, int timeoutMS);

  typedef void (* freeBOFuncType)(xclDeviceHandle handle, unsigned int boHandle);
  typedef size_t (* writeBOFuncType)(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek);
  typedef size_t (* readBOFuncType)(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip);
  typedef int (* syncBOFuncType)(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir,
                                 size_t size, size_t offset);
  typedef int (* copyBOFuncType)(xclDeviceHandle handle, unsigned int dstBoHandle, unsigned int srcBoHandle,
                                 size_t size, size_t dst_offset, size_t src_offset);

  typedef void* (* mapBOFuncType)(xclDeviceHandle handle, unsigned int boHandle, bool write);

  typedef int (* reClock2FuncType)(xclDeviceHandle handle, unsigned short region,
                                   const unsigned short *targetFreqMHz);
  //These are for readControl() and writeControl() functions
  typedef size_t (* writeFuncType)(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset,
                                   const void *hostBuf, size_t size);
  typedef size_t (* readFuncType)(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset,
                                  void *hostbuf, size_t size);

  typedef int (* lockDeviceFuncType)(xclDeviceHandle handle);
  typedef int (* unlockDeviceFuncType)(xclDeviceHandle handle);

  typedef int (* getDeviceInfoFuncType)(xclDeviceHandle handle, xclDeviceInfo2 *info);

  typedef size_t (* getDeviceTimeFuncType)(xclDeviceHandle handle);
  typedef double (* getDeviceClockFuncType)(xclDeviceHandle handle);
  typedef double (* getDeviceMaxReadFuncType)(xclDeviceHandle handle);
  typedef double (* getDeviceMaxWriteFuncType)(xclDeviceHandle handle);
  typedef void (* setSlotFuncType)(xclDeviceHandle handle, xclPerfMonType type, uint32_t numSlots);
  typedef uint32_t (* getSlotFuncType)(xclDeviceHandle handle, xclPerfMonType type);
  typedef void (* getSlotNameFuncType)(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum,
                                       char* slotName, uint32_t length);
  typedef uint32_t (* getSlotPropertiesFuncType)(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum);
  typedef size_t (* clockTrainingFuncType)(xclDeviceHandle handle, xclPerfMonType type);
  typedef size_t (* startCountersFuncType)(xclDeviceHandle handle, xclPerfMonType type);
  typedef size_t (* stopCountersFuncType)(xclDeviceHandle handle, xclPerfMonType type);
  typedef size_t (* readCountersFuncType)(xclDeviceHandle handle, xclPerfMonType type,
                                          xclCounterResults& counterResults);
  typedef size_t (* startTraceFuncType)(xclDeviceHandle handle, xclPerfMonType type,
                                        uint32_t options);
  typedef size_t (* stopTraceFuncType)(xclDeviceHandle handle, xclPerfMonType type);
  typedef uint32_t (* countTraceFuncType)(xclDeviceHandle handle, xclPerfMonType type);
  typedef size_t (* readTraceFuncType)(xclDeviceHandle handle, xclPerfMonType type,
                                       xclTraceResultsVector& traceVector);
  typedef void (* writeHostEventFuncType)(xclDeviceHandle handle, xclPerfMonEventType type,
                                          xclPerfMonEventID id);
  typedef size_t (* debugReadIPStatusFuncType)(xclDeviceHandle handle, xclDebugReadType type,
                                               void* debugResults);

  typedef int (* openContextFuncType)(xclDeviceHandle handle, const uuid_t xclbinId, unsigned int ipIndex,
                                      bool shared);
  typedef int (* closeContextFuncType)(xclDeviceHandle handle, const uuid_t xclbinId, unsigned ipIndex);

  //Streaming
  typedef int     (*createWriteQueueFuncType)(xclDeviceHandle handle,xclQueueContext *q_ctx, uint64_t *q_hdl);
  typedef int     (*createReadQueueFuncType)(xclDeviceHandle handle,xclQueueContext *q_ctx, uint64_t *q_hdl);
  typedef int     (*destroyQueueFuncType)(xclDeviceHandle handle,uint64_t q_hdl);
  typedef void*   (*allocQDMABufFuncType)(xclDeviceHandle handle,size_t size, uint64_t *buf_hdl);
  typedef int     (*freeQDMABufFuncType)(xclDeviceHandle handle,uint64_t buf_hdl);
  typedef ssize_t (*writeQueueFuncType)(xclDeviceHandle handle,uint64_t q_hdl, xclQueueRequest *wr);
  typedef ssize_t (*readQueueFuncType)(xclDeviceHandle handle,uint64_t q_hdl, xclQueueRequest *wr);
  typedef int     (*pollQueuesFuncType)(xclDeviceHandle handle,int min, int max, xclReqCompletion* completions, int* actual, int timeout);
//End Streaming
//
#if 0
  typedef int (* loadBitstreamFuncType)(xclDeviceHandle handle, const char *fileName);
  typedef int (* loadXclBinFuncType)(xclDeviceHandle handle, const xclBin *buffer);
  typedef int (* resetProgramFuncType)(xclDeviceHandle handle, xclResetKind kind);
  typedef int (* reClockFuncType)(xclDeviceHandle handle, unsigned targetFreqMHz);

  typedef uint64_t (* allocDeviceBufferType)(xclDeviceHandle handle, size_t size);
  typedef uint64_t (* allocDeviceBufferType2)(xclDeviceHandle handle, size_t size, xclMemoryDomains domain,
                                              unsigned flags);
  typedef size_t (* copyBufferHost2DeviceType)(xclDeviceHandle handle, uint64_t dest, const void *src,
                                               size_t size, size_t seek);
  typedef size_t (* copyBufferDevice2HostType)(xclDeviceHandle handle, void *dest, uint64_t src,
                                               size_t size, size_t skip);
  typedef double (* getHostClockFuncType)(xclDeviceHandle handle);
#endif

private:
  const std::string mFileName;
  const void *mDriverHandle;
  const unsigned mDeviceCount;

public:

  probeFuncType mProbe;
  openFuncType mOpen;
  closeFuncType mClose;
  //loadBitstreamFuncType mLoadBitstream;
  loadXclBinFuncType mLoadXclBin;
  allocBOFuncType mAllocBO;
  allocUserPtrBOFuncType mAllocUserPtrBO;
  importBOFuncType mImportBO;
  exportBOFuncType mExportBO;
  getBOPropertiesFuncType mGetBOProperties;

  execBOFuncType mExecBuf;
  execWaitFuncType mExecWait;

  openContextFuncType mOpenContext;
  closeContextFuncType mCloseContext;

  freeBOFuncType mFreeBO;
  writeBOFuncType mWriteBO;
  readBOFuncType mReadBO;
  syncBOFuncType mSyncBO;
  copyBOFuncType mCopyBO;
  mapBOFuncType mMapBO;
  writeFuncType mWrite;
  readFuncType mRead;
  reClock2FuncType mReClock2;
  lockDeviceFuncType mLockDevice;
  unlockDeviceFuncType mUnlockDevice;
  getDeviceInfoFuncType mGetDeviceInfo;

  getDeviceTimeFuncType mGetDeviceTime;
  getDeviceClockFuncType mGetDeviceClock;
  getDeviceMaxReadFuncType mGetDeviceMaxRead;
  getDeviceMaxWriteFuncType mGetDeviceMaxWrite;
  setSlotFuncType mSetProfilingSlots;
  getSlotFuncType mGetProfilingSlots;
  getSlotNameFuncType mGetProfilingSlotName;
  getSlotPropertiesFuncType mGetProfilingSlotProperties;
  clockTrainingFuncType mClockTraining;
  startCountersFuncType mStartCounters;
  stopCountersFuncType mStopCounters;
  readCountersFuncType mReadCounters;
  startTraceFuncType mStartTrace;
  stopTraceFuncType mStopTrace;
  countTraceFuncType mCountTrace;
  readTraceFuncType mReadTrace;
  writeHostEventFuncType mWriteHostEvent;
  debugReadIPStatusFuncType mDebugReadIPStatus;
//Streaming
  createWriteQueueFuncType mCreateWriteQueue;
  createReadQueueFuncType mCreateReadQueue;
  destroyQueueFuncType mDestroyQueue;
  allocQDMABufFuncType mAllocQDMABuf;
  freeQDMABufFuncType mFreeQDMABuf;
  writeQueueFuncType mWriteQueue;
  readQueueFuncType mReadQueue;
  pollQueuesFuncType mPollQueues;
//End Streaming

#if 0
  /* TBD */
  loadBitstreamFuncType mLoadBitstream;
  loadXclBinFuncType mLoadXclBin;
  resetProgramFuncType mResetProgram;
  reClockFuncType mReClock;
  copyBufferHost2DeviceType mCopyHost2Device;
  copyBufferDevice2HostType mCopyDevice2Host;
  allocDeviceBufferType mAllocDeviceBuffer;
  allocDeviceBufferType2 mAllocDeviceBuffer2;
  freeDeviceBufferType mFreeDeviceBuffer;
#endif

  const std::string&
  getFileName() const
  {
    return mFileName;
  }

  unsigned
  getDeviceCount() const
  {
    return mDeviceCount;
  }

  size_t
  mGetBOSize(xclDeviceHandle handle, unsigned int boHandle) const
  {
    xclBOProperties p;
    return mGetBOProperties(handle,boHandle,&p)
      ? -1
      : p.size;
  }

  uint64_t
  mGetDeviceAddr(xclDeviceHandle handle, unsigned int boHandle) const
  {
    xclBOProperties p;
    return mGetBOProperties(handle,boHandle,&p)
      ? -1
      : p.paddr;
  }

};

}} // hal2,xrt

#endif
