/**
 * Copyright (C) 2021 Xilinx, Inc
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

namespace xrt_xocl { namespace hal2 {

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
  ,mDebugReadIPStatus(0)
  ,mCreateWriteQueue(0)
  ,mCreateReadQueue(0)
  ,mDestroyQueue(0)
  ,mAllocQDMABuf(0)
  ,mFreeQDMABuf(0)
  ,mWriteQueue(0)
  ,mReadQueue(0)
  ,mPollQueues(0)
  ,mPollQueue(0)
  ,mSetQueueOpt(0)
  ,mGetDebugIpLayout(0)
  ,mGetNumLiveProcesses(0)
  ,mGetSysfsPath(0)
  ,mGetSubdevPath(0)
  ,mGetDebugIPlayoutPath(0)
  ,mGetTraceBufferInfo(0)
  ,mReadTraceData(0)
{
  mProbe = &xclProbe;
  mOpen = &xclOpen;
  mClose = &xclClose;

 // mLoadBitstream = &xclLoadBitstream;

  mLoadXclBin = &xclLoadXclBin;
  mAllocBO = &xclAllocBO;
  mAllocUserPtrBO = &xclAllocUserPtrBO;
  mImportBO = &xclImportBO;
  mExportBO = &xclExportBO;
  mGetBOProperties = &xclGetBOProperties;
  mExecBuf = &xclExecBuf;
  mExecWait = &xclExecWait;

  mOpenContext = &xclOpenContext;
  mCloseContext = &xclCloseContext;

  mFreeBO   = &xclFreeBO;
  mWriteBO  = &xclWriteBO;
  mReadBO   = &xclReadBO;
  mSyncBO   = &xclSyncBO;
  mCopyBO   = &xclCopyBO;
  mMapBO    = &xclMapBO;
  mUnmapBO  = &xclUnmapBO;

  mWrite      = &xclWrite;
  mRead       = &xclRead;
  mUnmgdPread = &xclUnmgdPread;

  mReClock2 = &xclReClock2;
  mLockDevice = &xclLockDevice;
  mUnlockDevice = &xclUnlockDevice;
  mGetDeviceInfo = &xclGetDeviceInfo2;

  mCreateWriteQueue = &xclCreateWriteQueue;
  mCreateReadQueue = &xclCreateReadQueue;
  mDestroyQueue = &xclDestroyQueue;
  mAllocQDMABuf = &xclAllocQDMABuf;
  mFreeQDMABuf = &xclFreeQDMABuf;
  mWriteQueue = &xclWriteQueue;
  mReadQueue = &xclReadQueue;
  mPollQueues = &xclPollCompletion;
  mPollQueue = &xclPollQueue;
  mSetQueueOpt = &xclSetQueueOpt;

#if 0
  // Profiling Functions
  mGetDeviceTime = &xclGetDeviceTimestamp;
  mGetDeviceClock = &xclGetDeviceClockFreqMHz;
  mGetDeviceMaxRead = &xclGetReadMaxBandwidthMBps;
  mGetDeviceMaxWrite = &xclGetWriteMaxBandwidthMBps;
  mSetProfilingSlots = &xclSetProfilingNumberSlots;
  mGetProfilingSlots = &xclGetProfilingNumberSlots;
  mGetProfilingSlotName = &xclGetProfilingSlotName;
  mGetProfilingSlotProperties = &xclGetProfilingSlotProperties;
  mClockTraining = &xclPerfMonClockTraining;
  mConfigureDataflow = &xclPerfMonConfigureDataflow;
  mStartCounters = &xclPerfMonStartCounters;
  mStopCounters = &xclPerfMonStopCounters;
  mReadCounters = &xclPerfMonReadCounters;
  mStartTrace = &xclPerfMonStartTrace;
  mStopTrace = &xclPerfMonStopTrace;
  mCountTrace = &xclPerfMonGetTraceCount;
  mReadTrace = &xclPerfMonReadTrace;
  mDebugReadIPStatus = &xclDebugReadIPStatus;
  mGetNumLiveProcesses = &xclGetNumLiveProcesses;
  mGetSysfsPath = &xclGetSysfsPath;
  mGetSubdevPath = &xclGetSubdevPath;
  mGetDebugIPlayoutPath = &xclGetDebugIPlayoutPath;
  mGetTraceBufferInfo = &xclGetTraceBufferInfo;
  mReadTraceData = &xclReadTraceData;

  mGetDebugIpLayout = &xclGetDebugIpLayout;
#endif
}

operations::
~operations()
{
}

}} // hal2,xrt
