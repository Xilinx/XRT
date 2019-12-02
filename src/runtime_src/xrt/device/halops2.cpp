/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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
#include "core/common/dlfcn.h"

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
  ,mUnmapBO(0)
  ,mWrite(0)
  ,mRead(0)
  ,mUnmgdPread(0)
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
  ,mConfigureDataflow(0)
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
  ,mGetNumLiveProcesses(0)
  ,mGetSysfsPath(0)
{
  mProbe = (probeFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclProbe");
  mOpen = (openFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclOpen");
  mClose = (closeFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclClose");;

 // mLoadBitstream = (loadBitstreamFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclLoadBitstream");;

  mLoadXclBin = (loadXclBinFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclLoadXclBin");
  mAllocBO = (allocBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclAllocBO");
  mAllocUserPtrBO = (allocUserPtrBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclAllocUserPtrBO");
  mImportBO = (importBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclImportBO");
  mExportBO = (exportBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclExportBO");
  mGetBOProperties = (getBOPropertiesFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetBOProperties");
  mExecBuf = (execBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclExecBuf");
  mExecWait = (execWaitFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclExecWait");

  mOpenContext = (openContextFuncType)xrt_core::dlsym(const_cast<void*>(mDriverHandle), "xclOpenContext");
  mCloseContext = (closeContextFuncType)xrt_core::dlsym(const_cast<void*>(mDriverHandle), "xclCloseContext");

  mFreeBO   = (freeBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclFreeBO");
  mWriteBO  = (writeBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclWriteBO");
  mReadBO   = (readBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclReadBO");
  mSyncBO   = (syncBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclSyncBO");
  mCopyBO   = (copyBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclCopyBO");
  mMapBO    = (mapBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclMapBO");
  mUnmapBO  = (unmapBOFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclUnmapBO");

  mWrite      = (writeFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclWrite");
  mRead       = (readFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclRead");
  mUnmgdPread = (unmgdPreadFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclUnmgdPread");

  mReClock2 = (reClock2FuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclReClock2");
  mLockDevice = (lockDeviceFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclLockDevice");
  mUnlockDevice = (unlockDeviceFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclUnlockDevice");
  mGetDeviceInfo = (getDeviceInfoFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetDeviceInfo2");

  mCreateWriteQueue = (createWriteQueueFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclCreateWriteQueue");
  mCreateReadQueue = (createReadQueueFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclCreateReadQueue");
  mDestroyQueue = (destroyQueueFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclDestroyQueue");
  mAllocQDMABuf = (allocQDMABufFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclAllocQDMABuf");
  mFreeQDMABuf = (freeQDMABufFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclFreeQDMABuf");
  mWriteQueue = (writeQueueFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclWriteQueue");
  mReadQueue = (readQueueFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclReadQueue");
  mPollQueues = (pollQueuesFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPollCompletion");

  // Profiling Functions
  mGetDeviceTime = (getDeviceTimeFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetDeviceTimestamp");
  mGetDeviceClock = (getDeviceClockFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetDeviceClockFreqMHz");
  mGetDeviceMaxRead = (getDeviceMaxReadFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetReadMaxBandwidthMBps");
  mGetDeviceMaxWrite = (getDeviceMaxWriteFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetWriteMaxBandwidthMBps");
  mSetProfilingSlots = (setSlotFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclSetProfilingNumberSlots");
  mGetProfilingSlots = (getSlotFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetProfilingNumberSlots");
  mGetProfilingSlotName = (getSlotNameFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetProfilingSlotName");
  mGetProfilingSlotProperties = (getSlotPropertiesFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetProfilingSlotProperties");
  mWriteHostEvent = (writeHostEventFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclWriteHostEvent");
  mClockTraining = (clockTrainingFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonClockTraining");
  mConfigureDataflow = (configureDataflowFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonConfigureDataflow");
  mStartCounters = (startCountersFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStartCounters");
  mStopCounters = (stopCountersFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStopCounters");
  mReadCounters = (readCountersFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonReadCounters");
  mStartTrace = (startTraceFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStartTrace");
  mStopTrace = (stopTraceFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonStopTrace");
  mCountTrace = (countTraceFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonGetTraceCount");
  mReadTrace = (readTraceFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonReadTrace");
  mWriteHostEvent = (writeHostEventFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclWriteHostEvent");
  mDebugReadIPStatus = (debugReadIPStatusFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclDebugReadIPStatus");
  mGetNumLiveProcesses = (xclGetNumLiveProcessesFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetNumLiveProcesses");
  mGetSysfsPath = (xclGetSysfsPathFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetSysfsPath");
  mGetDebugIPlayoutPath = (xclGetDebugIPlayoutPathFuncType)xrt_core::dlsym(const_cast<void*>(mDriverHandle), "xclGetDebugIPlayoutPath");
  mGetTraceBufferInfo = (xclGetTraceBufferInfoFuncType)xrt_core::dlsym(const_cast<void*>(mDriverHandle), "xclGetTraceBufferInfo");
  mReadTraceData = (xclReadTraceDataFuncType)xrt_core::dlsym(const_cast<void*>(mDriverHandle), "xclReadTraceData");
}

operations::
~operations()
{
  xrt_core::dlclose(const_cast<void *>(mDriverHandle));
}

}} // hal2,xrt
