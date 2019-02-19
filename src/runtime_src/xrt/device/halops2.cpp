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

#include "halops2.h"
#include <dlfcn.h>

namespace xrt { namespace hal2 {

operations::
operations(const std::string &fileName, void *fileHandle, unsigned int count)
  : mFileName(fileName)
  ,mDriverHandle(fileHandle)
  ,mDeviceCount(count)
  ,mProbe(0)
  ,mOpen(0)
  ,mClose(0)
  ,mLoadXclBin(0)
  ,mAllocBO(0)
  ,mAllocUserPtrBO(0)
  ,mImportBO(0)
  ,mExportBO(0)
  ,mGetBOProperties(0)
  ,mExecBuf(0)
  ,mExecWait(0)
  ,mOpenContext(0)
  ,mCloseContext(0)
  ,mFreeBO(0)
  ,mWriteBO(0)
  ,mReadBO(0)
  ,mSyncBO(0)
  ,mCopyBO(0)
  ,mMapBO(0)
  ,mWrite(0)
  ,mRead(0)
  ,mReClock2(0)
  ,mLockDevice(0)
  ,mUnlockDevice(0)
  ,mGetDeviceInfo(0)
  ,mGetDeviceTime(0)
  ,mGetDeviceClock(0)
  ,mGetDeviceMaxRead(0)
  ,mGetDeviceMaxWrite(0)
  ,mSetProfilingSlots(0)
  ,mGetProfilingSlots(0)
  ,mGetProfilingSlotName(0)
  ,mGetProfilingSlotProperties(0)
  ,mClockTraining(0)
  ,mStartCounters(0)
  ,mStopCounters(0)
  ,mReadCounters(0)
  ,mStartTrace(0)
  ,mStopTrace(0)
  ,mCountTrace(0)
  ,mReadTrace(0)
  ,mWriteHostEvent(0)
  ,mDebugReadIPStatus(0)
  ,mCreateWriteQueue(0)
  ,mCreateReadQueue(0)
  ,mDestroyQueue(0)
  ,mAllocQDMABuf(0)
  ,mFreeQDMABuf(0)
  ,mWriteQueue(0)
  ,mReadQueue(0)
  ,mPollQueues(0)
{
  mProbe = (probeFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclProbe");
  if (!mProbe)
    return;
  mOpen = (openFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclOpen");
  if (!mOpen)
    return;
  mClose = (closeFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclClose");;
  if (!mClose)
    return;

 // mLoadBitstream = (loadBitstreamFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclLoadBitstream");;

  mLoadXclBin = (loadXclBinFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclLoadXclBin");

  mAllocBO = (allocBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclAllocBO");
  if(!mAllocBO)
    return;

  mAllocUserPtrBO = (allocUserPtrBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclAllocUserPtrBO");
  mImportBO = (importBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclImportBO");
  mExportBO = (exportBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclExportBO");

  mGetBOProperties = (getBOPropertiesFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetBOProperties");
  mExecBuf = (execBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclExecBuf");
  mExecWait = (execWaitFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclExecWait");

  mOpenContext = (openContextFuncType)dlsym(const_cast<void*>(mDriverHandle), "xclOpenContext");
  mCloseContext = (closeContextFuncType)dlsym(const_cast<void*>(mDriverHandle), "xclCloseContext");

  mFreeBO   = (freeBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclFreeBO");
  if(!mFreeBO)
    return;
  mWriteBO  = (writeBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclWriteBO");
  if(!mWriteBO)
    return;
  mReadBO   = (readBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclReadBO");
  if(!mReadBO)
    return;

  mSyncBO   = (syncBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclSyncBO");
  mCopyBO   = (copyBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclCopyBO");
  mMapBO    = (mapBOFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclMapBO");

  mWrite    = (writeFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclWrite");
  if(!mWrite)
    return;
  mRead     = (readFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclRead");
  if(!mRead)
    return;

  mReClock2 = (reClock2FuncType)dlsym(const_cast<void *>(mDriverHandle), "xclReClock2");
  mLockDevice = (lockDeviceFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclLockDevice");
  mUnlockDevice = (unlockDeviceFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclUnlockDevice");
  mGetDeviceInfo = (getDeviceInfoFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetDeviceInfo2");

  mCreateWriteQueue = (createWriteQueueFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclCreateWriteQueue");
  mCreateReadQueue = (createReadQueueFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclCreateReadQueue");
  mDestroyQueue = (destroyQueueFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclDestroyQueue");
  mAllocQDMABuf = (allocQDMABufFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclAllocQDMABuf");
  mFreeQDMABuf = (freeQDMABufFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclFreeQDMABuf");
  mWriteQueue = (writeQueueFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclWriteQueue");
  mReadQueue = (readQueueFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclReadQueue");
  mPollQueues = (pollQueuesFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPollCompletion");

  // Profiling Functions
  mGetDeviceTime = (getDeviceTimeFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetDeviceTimestamp");
  mGetDeviceClock = (getDeviceClockFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetDeviceClockFreqMHz");
  mGetDeviceMaxRead = (getDeviceMaxReadFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetReadMaxBandwidthMBps");
  mGetDeviceMaxWrite = (getDeviceMaxWriteFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetWriteMaxBandwidthMBps");
  mSetProfilingSlots = (setSlotFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclSetProfilingNumberSlots");
  mGetProfilingSlots = (getSlotFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetProfilingNumberSlots");
  mGetProfilingSlotName = (getSlotNameFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetProfilingSlotName");
  mGetProfilingSlotProperties = (getSlotPropertiesFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclGetProfilingSlotProperties");
  mWriteHostEvent = (writeHostEventFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclWriteHostEvent");
  mClockTraining = (clockTrainingFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonClockTraining");
  mStartCounters = (startCountersFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStartCounters");
  mStopCounters = (stopCountersFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStopCounters");
  mReadCounters = (readCountersFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonReadCounters");
  mStartTrace = (startTraceFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStartTrace");
  mStopTrace = (stopTraceFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStopTrace");
  mCountTrace = (countTraceFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonGetTraceCount");
  mReadTrace = (readTraceFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonReadTrace");
  mWriteHostEvent = (writeHostEventFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclWriteHostEvent");
  mDebugReadIPStatus = (debugReadIPStatusFuncType)dlsym(const_cast<void *>(mDriverHandle), "xclDebugReadIPStatus");

}

operations::
~operations()
{
  dlclose(const_cast<void *>(mDriverHandle));
}

}} // hal2,xrt
