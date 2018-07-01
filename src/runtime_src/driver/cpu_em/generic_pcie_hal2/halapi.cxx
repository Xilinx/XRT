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
 
xclDeviceHandle xclOpen(unsigned deviceIndex, const char *logfileName, xclVerbosityLevel level)
{
  xclDeviceInfo2 info;
  std::strcpy(info.mName, "xilinx:pcie-hw-em:7v3:1.0");
  info.mMagic = 0X586C0C6C;
  info.mHALMajorVersion = XCLHAL_MAJOR_VER;
  info.mHALMajorVersion = XCLHAL_MINOR_VER;
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

  xclcpuemhal2::CpuemShim *handle = NULL;
  std::map<unsigned int, xclcpuemhal2::CpuemShim*>::iterator it = xclcpuemhal2::devices.find(deviceIndex);
  if(it != xclcpuemhal2::devices.end())
  {
    handle = (*it).second;
  }
  else
  {
    handle = new xclcpuemhal2::CpuemShim(deviceIndex,info,DDRBankList,false,false);
  }

  if (!xclcpuemhal2::CpuemShim::handleCheck(handle)) {
    delete handle;
    handle = 0;
  }
  if(handle)
    handle->xclOpen(logfileName);
  return (xclDeviceHandle *)handle;
}

void xclClose(xclDeviceHandle handle)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return ;
  drv->xclClose();
  if (xclcpuemhal2::CpuemShim::handleCheck(handle) && xclcpuemhal2::devices.size() == 0) {
    delete ((xclcpuemhal2::CpuemShim*)handle);
  }
}


int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetDeviceInfo2(info);
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclLoadXclBin(buffer);
}

uint64_t xclAllocDeviceBuffer(xclDeviceHandle handle, size_t size)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclAllocDeviceBuffer(size);
}

uint64_t xclAllocDeviceBuffer2(xclDeviceHandle handle, size_t size, xclMemoryDomains domain,
                               unsigned flags)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclAllocDeviceBuffer2(size, domain, flags);
}

void xclFreeDeviceBuffer(xclDeviceHandle handle, uint64_t buf)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return;
  return drv->xclFreeDeviceBuffer(buf);
}


size_t xclCopyBufferHost2Device(xclDeviceHandle handle, uint64_t dest, const void *src, size_t size, size_t seek)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclCopyBufferHost2Device(dest, src, size, seek);
}


size_t xclCopyBufferDevice2Host(xclDeviceHandle handle, void *dest, uint64_t src, size_t size, size_t skip)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclCopyBufferDevice2Host(dest, src, size, skip);
}

size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclWrite(space, offset, hostBuf, size);
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclRead(space, offset, hostBuf, size);
}

int xclUpgradeFirmware(xclDeviceHandle handle, const char *fileName)
{
  return 0;
}

int xclBootFPGA(xclDeviceHandle handle)
{
  return 0;
}

int xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  drv->resetProgram();
  return 0;
}

int xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
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

size_t xclPerfMonStartCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonStopCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonReadCounters(xclDeviceHandle handle, xclPerfMonType type, xclCounterResults& counterResults)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}

size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type, void* debugResults)
{
  return 0;
}

size_t xclPerfMonClockTraining(xclDeviceHandle handle, xclPerfMonType type)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0; 
}


size_t xclPerfMonStartTrace(xclDeviceHandle handle, xclPerfMonType type, uint32_t startTrigger)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonStopTrace(xclDeviceHandle handle, xclPerfMonType type)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, xclPerfMonType type)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonReadTrace(xclDeviceHandle handle, xclPerfMonType type, xclTraceResultsVector& traceVector)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


double xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return 0.0;
  return 0.0;
}


double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return 0.0;
  return 0.0;
}


double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return 0.0;
  return 0.0;
}


size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}


void xclSetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type, uint32_t numSlots)
{
  return;
}

uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type)
{
  return 0;
}

void xclGetProfilingSlotName(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum,
		                     char* slotName, uint32_t length)
{
  return;
}

void xclWriteHostEvent(xclDeviceHandle handle, xclPerfMonEventType type, xclPerfMonEventID id)
{
  return;
}

unsigned xclProbe()
{
  if(!xclemulation::isXclEmulationModeHwEmuOrSwEmu())
  {
    std::string initMsg ="ERROR: [SDx-EM 09] Please set XCL_EMULATION_MODE to \"sw_emu\" to run software emulation. ";
    std::cout<<initMsg<<std::endl;
    return 0;
  }

  unsigned int deviceIndex = 0;
  std::vector<std::tuple<xclDeviceInfo2,std::list<xclemulation::DDRBank> ,bool, bool> > devicesInfo;
  getDevicesInfo(devicesInfo);
  
  if(devicesInfo.size() == 0)
    return 1;
  
  std::vector<std::tuple<xclDeviceInfo2,std::list<xclemulation::DDRBank>, bool, bool > >::iterator start = devicesInfo.begin();
  std::vector<std::tuple<xclDeviceInfo2,std::list<xclemulation::DDRBank> ,bool, bool > >::iterator end = devicesInfo.end();
  for(;start != end; start++)
  {
    xclDeviceInfo2 info = std::get<0>(*start);
    std::list<xclemulation::DDRBank> DDRBankList = std::get<1>(*start);
    bool bUnified = std::get<2>(*start);
    bool bXPR = std::get<3>(*start);

    xclcpuemhal2::CpuemShim *handle = new xclcpuemhal2::CpuemShim(deviceIndex,info,DDRBankList, bUnified, bXPR);
    xclcpuemhal2::devices[deviceIndex++] = handle;
  }

  return deviceIndex;
}

//########################################## HAL2 START ##########################################

unsigned int xclVersion ()
{
  return 2;
}

int xclExportBO(xclDeviceHandle handle, unsigned int boHandle) 
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclExportBO(boHandle);
}

unsigned int xclImportBO(xclDeviceHandle handle, int boGlobalHandle) 
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclImportBO(boGlobalHandle);
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst,
                 size_t size, size_t skip)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclReadBO(boHandle, dst, size, skip);
}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, uint64_t flags)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return mNullBO;
  return drv->xclAllocUserPtrBO(userptr,size,flags); 
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, xclBOKind domain, uint64_t flags)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAllocBO(size, domain, flags);
}


void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return NULL;
  return drv->xclMapBO(boHandle, write);
}

int xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset) 
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSyncBO(boHandle, dir , size, offset);
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src,
                  size_t size, size_t seek)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWriteBO(boHandle, src, size, seek);
}
void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclFreeBO(boHandle);
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv ? drv->xclGetBOProperties(boHandle, properties) : -ENODEV;
}
