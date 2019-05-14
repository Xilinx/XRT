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

/**
 * Copyright (C) 2015 Xilinx, Inc
 */

#include <shim.h>

int xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclExportBO(boHandle);
}

unsigned int xclImportBO(xclDeviceHandle handle, int boGlobalHandle, unsigned flags)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclImportBO(boGlobalHandle,flags);
}

int xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset) : -ENODEV;
}

int xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->resetProgram();
}

int xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  drv->resetProgram();
  return 0;
}

int xclLockDevice(xclDeviceHandle handle)
{
  return 0;
}

int xclUnlockDevice(xclDeviceHandle handle)
{
  return 0;
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst,
                 size_t size, size_t skip)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclReadBO(boHandle, dst, size, skip);
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, xclBOKind domain, unsigned flags)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAllocBO(size, domain, flags);
}


void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return NULL;
  return drv->xclMapBO(boHandle, write);
}

int xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSyncBO(boHandle, dir , size, offset);
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src,
                  size_t size, size_t seek)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWriteBO(boHandle, src, size, seek);
}
void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclFreeBO(boHandle);
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetBOProperties(boHandle, properties);
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclExecBuf(cmdBO);
}


//defining following two functions as they gets called in scheduler init call
int xclOpenContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned int ipIndex, bool shared)
  
{
  return 0;
}

int xclCloseContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned ipIndex)
{
  return 0;
}



int xclRegisterEventNotify(xclDeviceHandle handle, unsigned int userInterrupt, int fd)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclRegisterEventNotify(userInterrupt, fd) ;
}

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclExecWait(timeoutMilliSec) ;
}

int xclUpgradeFirmware(xclDeviceHandle handle, const char *fileName)
{
  return 0;
}

int xclBootFPGA(xclDeviceHandle handle)
{
  return 0;
}

uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->getPerfMonNumberSlots(type);
}

uint32_t xclGetProfilingSlotProperties(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return 0;
  return drv->getPerfMonProperties(type, slotnum);
}

void xclGetProfilingSlotName(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum,
		                     char* slotName, uint32_t length)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return;
  return drv->getPerfMonSlotName(type, slotnum, slotName, length);
}

size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type, void* debugResults)
{
  return 0;
}

unsigned xclProbe()
{
  if(!xclemulation::isXclEmulationModeHwEmuOrSwEmu())
  {
    std::string initMsg ="ERROR: [HW-EM 08] Please set XCL_EMULATION_MODE to \"hw_emu\" to run hardware emulation. ";
    std::cout<<initMsg<<std::endl;
    return 0;
  }

  unsigned int deviceIndex = 0;
  std::vector<std::tuple<xclDeviceInfo2,std::list<xclemulation::DDRBank> , bool, bool , FeatureRomHeader> > devicesInfo;
  getDevicesInfo(devicesInfo);
  if(devicesInfo.size() == 0)
    return 1;//old behavior
  for(auto &it:devicesInfo)
  {
    xclDeviceInfo2 info = std::get<0>(it);
    std::list<xclemulation::DDRBank> DDRBankList = std::get<1>(it);
    bool bUnified = std::get<2>(it);
    bool bXPR = std::get<3>(it);
    FeatureRomHeader fRomHeader = std::get<4>(it);
    xclhwemhal2::HwEmShim *handle = new xclhwemhal2::HwEmShim(deviceIndex,info,DDRBankList, bUnified, bXPR, fRomHeader);
    xclhwemhal2::devices[deviceIndex++] = handle;
  }

  return deviceIndex;
}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  //std::cout << "xclAllocUserPtrBO called.. " << handle << std::endl;
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return mNullBO;
  return drv->xclAllocUserPtrBO(userptr,size,flags);
}

xclDeviceHandle xclOpen(unsigned deviceIndex, const char *logfileName, xclVerbosityLevel level)
{
  xclDeviceInfo2 info;
  std::strcpy(info.mName, "xilinx:pcie-hw-em:7v3:1.0");
  info.mMagic = 0X586C0C6C;
  info.mHALMajorVersion = XCLHAL_MAJOR_VER;
  info.mHALMinorVersion = XCLHAL_MINOR_VER;
  info.mVendorId = 0x10ee;
  info.mDeviceId = 0x0000;
  info.mSubsystemVendorId = 0x0000;
  info.mDeviceVersion = 0x0000;
  info.mDDRSize = xclemulation::MEMSIZE_4G;
  info.mDataAlignment = DDR_BUFFER_ALIGNMENT;
  info.mDDRBankCount = 1;
  for(unsigned int i = 0; i < 4 ;i++)
    info.mOCLFrequency[i] = 200;

  std::list<xclemulation::DDRBank> DDRBankList;
  xclemulation::DDRBank bank;
  bank.ddrSize = xclemulation::MEMSIZE_4G;
  DDRBankList.push_back(bank);
  FeatureRomHeader fRomHeader;
  std::memset(&fRomHeader, 0, sizeof(FeatureRomHeader));


  xclhwemhal2::HwEmShim *handle = NULL;

  bool bDefaultDevice = false;
  std::map<unsigned int, xclhwemhal2::HwEmShim*>::iterator it = xclhwemhal2::devices.find(deviceIndex);
  if(it != xclhwemhal2::devices.end())
  {
    handle = (*it).second;
  }
  else
  {
    handle = new xclhwemhal2::HwEmShim(deviceIndex,info,DDRBankList, false, false,fRomHeader);
    bDefaultDevice = true;
  }

  if (!xclhwemhal2::HwEmShim::handleCheck(handle)) {
    delete handle;
    handle = 0;
  }
  if(handle)
  {
    handle->xclOpen(logfileName);
    if(bDefaultDevice)
    {
      std::string sDummyDeviceMsg ="CRITICAL WARNING: [HW-EM 08-0] Unable to find emconfig.json. Using default device \"xilinx:pcie-hw-em:7v3:1.0\"";
      handle->logMessage(sDummyDeviceMsg);
    }
  }
  return (xclDeviceHandle *)handle;
}


void xclClose(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return ;
  drv->xclClose();
  if (xclhwemhal2::HwEmShim::handleCheck(handle) && xclhwemhal2::devices.size() == 0) {
    delete ((xclhwemhal2::HwEmShim *)handle);
  }
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  auto ret = drv->xclLoadXclBin(buffer);
  if (!ret)
      ret = xrt_core::scheduler::init(handle, buffer);
  return ret;
}

size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclWrite(space, offset, hostBuf, size);
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclRead(space, offset, hostBuf, size);
}


int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetDeviceInfo2(info);
}

unsigned int xclVersion ()
{
  return 2;
}

void xclWriteHostEvent(xclDeviceHandle handle, xclPerfMonEventType type, xclPerfMonEventID id)
{
  return ;
}

size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetDeviceTimestamp();
}

double xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetDeviceClockFreqMHz();
}

double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetReadMaxBandwidthMBps();
}

double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetWriteMaxBandwidthMBps();
}

void xclSetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type, uint32_t numSlots)
{
  // TODO: set number of slots on monitor
  return;
}

size_t xclPerfMonClockTraining(xclDeviceHandle handle, xclPerfMonType type)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonClockTraining();
}

void xclPerfMonConfigureDataflow(xclDeviceHandle handle, xclPerfMonType type, unsigned *ip_config)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return;
  return drv->xclPerfMonConfigureDataflow(type, ip_config);
}

size_t xclPerfMonStartCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonStartCounters();
}

size_t xclPerfMonStopCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonStopCounters();
}

size_t xclPerfMonReadCounters(xclDeviceHandle handle, xclPerfMonType type, xclCounterResults& counterResults)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonReadCounters(type,counterResults);
}

size_t xclPerfMonStartTrace(xclDeviceHandle handle, xclPerfMonType type, uint32_t startTrigger)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonStartTrace(startTrigger);
}

size_t xclPerfMonStopTrace(xclDeviceHandle handle, xclPerfMonType type)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonStopTrace();
}

uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, xclPerfMonType type)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonGetTraceCount(type);
}

size_t xclPerfMonReadTrace(xclDeviceHandle handle, xclPerfMonType type, xclTraceResultsVector& traceVector)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclPerfMonReadTrace(type,traceVector);
}

ssize_t xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclUnmgdPwrite(flags, buf, count, offset) : -ENODEV;
}

ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclUnmgdPread(flags, buf, count, offset) : -ENODEV;
}


//QDMA Support
//

int xclCreateWriteQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclCreateWriteQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclCreateReadQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclCreateReadQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclDestroyQueue(xclDeviceHandle handle, uint64_t q_hdl)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclDestroyQueue(q_hdl) : -ENODEV;
}

void *xclAllocQDMABuf(xclDeviceHandle handle, size_t size, uint64_t *buf_hdl)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclAllocQDMABuf(size, buf_hdl) : NULL;
}

int xclFreeQDMABuf(xclDeviceHandle handle, uint64_t buf_hdl)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclFreeQDMABuf(buf_hdl) : -ENODEV;
}

ssize_t xclWriteQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
	return drv ? drv->xclWriteQueue(q_hdl, wr) : -ENODEV;
}

ssize_t xclReadQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
	return drv ? drv->xclReadQueue(q_hdl, wr) : -ENODEV;
}
int xclPollCompletion(xclDeviceHandle handle, int min_compl, int max_compl, xclReqCompletion *comps, int* actual, int timeout)
{
   xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclPollCompletion(min_compl, max_compl, comps, actual, timeout) : -ENODEV;
}

/*
 * API to get number of live processes. 
 * Applicable only for System Flow as it supports Multiple processes on same device.
 * For Hardware Emulation, return 0
 */
uint xclGetNumLiveProcesses(xclDeviceHandle handle)
{
    return 0;
}

int xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = xclhwemhal2::HwEmShim::xclLogMsg(handle, level, tag, format, args);
  va_end(args);
  return ret;
}

