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

/**
 * Copyright (C) 2015 Xilinx, Inc
 */

#include "shim.h"
#include "core/common/system.h"
#include "core/common/device.h"

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

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned flags)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAllocBO(size, unused, flags);
}


void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return NULL;
  return drv->xclMapBO(boHandle, write);
}

int xclUnmapBO(xclDeviceHandle handle, unsigned int boHandle, void* addr)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclUnmapBO(boHandle, addr);
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
int xclOpenContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned int ipIndex, bool shared)

{
  return 0;
}

int xclCloseContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned ipIndex)
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

unsigned xclProbe()
{
  if(!xclemulation::isXclEmulationModeHwEmuOrSwEmu())
  {
    std::string initMsg ="ERROR: [HW-EMU 08] Please set XCL_EMULATION_MODE to \"hw_emu\" to run hardware emulation. ";
    std::cout<<initMsg<<std::endl;
    return 0;
  }

  static int xclProbeCallCnt=0;
  static unsigned int deviceIndex = 0;

  //Ensure xclProbe is called only once as we load all the devices in the single go
  //xclProbe call happens during the load of the library, no need to explicit call

  if (xclProbeCallCnt == 1) {
    return deviceIndex;
  }

  std::vector<std::tuple<xclDeviceInfo2, std::list<xclemulation::DDRBank>, bool, bool, FeatureRomHeader, boost::property_tree::ptree> > devicesInfo;
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
    boost::property_tree::ptree platformData = std::get<5>(it);

    xclhwemhal2::HwEmShim *handle = new xclhwemhal2::HwEmShim(deviceIndex, info, DDRBankList, bUnified, bXPR, fRomHeader, platformData);
    xclhwemhal2::devices[deviceIndex++] = handle;
  }

  xclProbeCallCnt++;
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
  boost::property_tree::ptree platformData;

  xclhwemhal2::HwEmShim *handle = NULL;

  bool bDefaultDevice = false;
  std::map<unsigned int, xclhwemhal2::HwEmShim*>::iterator it = xclhwemhal2::devices.find(deviceIndex);
  if(it != xclhwemhal2::devices.end())
  {
    handle = (*it).second;
  }
  else
  {
    handle = new xclhwemhal2::HwEmShim(deviceIndex, info, DDRBankList, false, false, fRomHeader, platformData);
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
      std::string sDummyDeviceMsg ="CRITICAL WARNING: [HW-EMU 08-0] Unable to find emconfig.json. Using default device \"xilinx:pcie-hw-em:7v3:1.0\"";
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
  try {
    drv->xclClose();
    if (xclhwemhal2::HwEmShim::handleCheck(handle) && xclhwemhal2::devices.size() == 0)
      delete ((xclhwemhal2::HwEmShim *)handle);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
#ifdef DISABLE_DOWNLOAD_XCLBIN
  int ret = 0;
#else
  auto ret = drv->xclLoadXclBin(buffer);
#endif
  if (!ret) {
    auto device = xrt_core::get_userpf_device(drv);
    device->register_axlf(buffer);
#ifndef DISABLE_DOWNLOAD_XCLBIN
    ret = xrt_core::scheduler::init(handle, buffer);
#endif
  }
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

size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclGetDeviceTimestamp() : -1;
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

size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type, void* debugResults)
{
  return 0;
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

/*
 * API to get number of live processes.
 * Applicable only for System Flow as it supports Multiple processes on same device.
 * For Hardware Emulation, return 0
 */
uint32_t xclGetNumLiveProcesses(xclDeviceHandle handle)
{
    return 0;
}

/*
 * API to get path to the debug_ip_layout file. Needs to be implemented
 */

int xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclGetDebugIPlayoutPath(layoutPath, size) : -ENODEV;
}

int xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclGetTraceBufferInfo(nSamples, traceSamples, traceBufSz) : -ENODEV;
}

int xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclReadTraceData(traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample) : -ENODEV;
}


int xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = xclhwemhal2::HwEmShim::xclLogMsg(handle, level, tag, format, args);
  va_end(args);
  return ret;
}

int xclCreateProfileResults(xclDeviceHandle handle, ProfileResults** results)
{
  return 0;
}

int xclGetProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  return 0;
}

int xclDestroyProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  return 0;
}

void
xclGetDebugIpLayout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret)
{
  if(size_ret)
    *size_ret = 0;
  return;
}

int xclGetSubdevPath(xclDeviceHandle handle,  const char* subdev,
                        uint32_t idx, char* path, size_t size)
{
  return 0;
}
//Mapped CU register space for xclRegRead/Write()  
int xclRegWrite(xclDeviceHandle handle, uint32_t cu_index, uint32_t offset, uint32_t data)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclRegWrite(cu_index, offset, data) : -ENODEV;
}

int xclRegRead(xclDeviceHandle handle, uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclRegRead(cu_index, offset, datap) : -ENODEV;
}

int
xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
  return -ENOSYS;
}

//Get CU index from IP_LAYOUT section for corresponding kernel name
int xclIPName2Index(xclDeviceHandle handle, const char *name)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclIPName2Index(name) : -ENODEV;
}
