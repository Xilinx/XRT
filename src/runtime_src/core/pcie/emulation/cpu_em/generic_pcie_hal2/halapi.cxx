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

  xclcpuemhal2::CpuemShim *handle = NULL;
  bool bDefaultDevice = false;
  std::map<unsigned int, xclcpuemhal2::CpuemShim*>::iterator it = xclcpuemhal2::devices.find(deviceIndex);
  if(it != xclcpuemhal2::devices.end())
  {
    handle = (*it).second;
  }
  else
  {
    handle = new xclcpuemhal2::CpuemShim(deviceIndex, info, DDRBankList, false, false, fRomHeader, platformData);
    bDefaultDevice = true;
  }

  if (!xclcpuemhal2::CpuemShim::handleCheck(handle)) {
    delete handle;
    handle = 0;
  }
  if (handle) {
    handle->xclOpen(logfileName);
    if (bDefaultDevice)
    {
      std::string sDummyDeviceMsg = "CRITICAL WARNING: [SW-EM 09-0] Unable to find emconfig.json. Using default device \"xilinx:pcie-hw-em:7v3:1.0\"";
      if (xclemulation::config::getInstance()->isInfosToBePrintedOnConsole())
        std::cout << sDummyDeviceMsg << std::endl;
    }
  }
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
  auto ret = drv->xclLoadXclBin(buffer);
  if (!ret) {
    auto device = xrt_core::get_userpf_device(drv);
    device->register_axlf(buffer);
    if (xclemulation::is_sw_emulation() && xrt_core::config::get_flag_kds_sw_emu())
      ret = xrt_core::scheduler::init(handle, buffer);
  }
  return ret;
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
  bool p2pBuffer = false;
  std::string fileName("");
  return drv->xclAllocDeviceBuffer2(size, domain, flags,p2pBuffer,fileName);
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
//  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
//  if (!drv)
//    return 0.0;
  return 0.0;
}


double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle)
{
//  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
//  if (!drv)
//    return 0.0;
  return 0.0;
}


double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
//  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
//  if (!drv)
//    return 0.0;
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

unsigned xclProbe()
{
  if(!xclemulation::isXclEmulationModeHwEmuOrSwEmu())
  {
    std::string initMsg ="ERROR: [SW-EM 09] Please set XCL_EMULATION_MODE to \"sw_emu\" to run software emulation. ";
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

  std::vector<std::tuple<xclDeviceInfo2,std::list<xclemulation::DDRBank> ,bool, bool, FeatureRomHeader, boost::property_tree::ptree> > devicesInfo;
  getDevicesInfo(devicesInfo);

  if(devicesInfo.size() == 0)
    return 1;

  for(auto &it: devicesInfo)
  {
    xclDeviceInfo2 info = std::get<0>(it);
    std::list<xclemulation::DDRBank> DDRBankList = std::get<1>(it);
    bool bUnified = std::get<2>(it);
    bool bXPR = std::get<3>(it);
    FeatureRomHeader fRomHeader = std::get<4>(it);
    boost::property_tree::ptree platformData = std::get<5>(it);

    xclcpuemhal2::CpuemShim *handle = new xclcpuemhal2::CpuemShim(deviceIndex, info, DDRBankList, bUnified, bXPR, fRomHeader, platformData);
    xclcpuemhal2::devices[deviceIndex++] = handle;
  }

  xclProbeCallCnt++;
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

unsigned int xclImportBO(xclDeviceHandle handle, int boGlobalHandle,unsigned flags)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclImportBO(boGlobalHandle,flags);
}

int xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
    xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
    return drv ? drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset) : -ENODEV;
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst,
                 size_t size, size_t skip)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclReadBO(boHandle, dst, size, skip);
}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return mNullBO;
  return drv->xclAllocUserPtrBO(userptr,size,flags);
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned flags)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAllocBO(size, unused, flags);
}


void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return NULL;
  return drv->xclMapBO(boHandle, write);
}

int xclUnmapBO(xclDeviceHandle handle, unsigned int boHandle, void* addr)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclUnmapBO(boHandle, addr);
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
  return drv->xclGetBOProperties(boHandle, properties);
}

//QDMA Support

int xclCreateWriteQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclCreateWriteQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclCreateReadQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclCreateReadQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclDestroyQueue(xclDeviceHandle handle, uint64_t q_hdl)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclDestroyQueue(q_hdl) : -ENODEV;
}

void *xclAllocQDMABuf(xclDeviceHandle handle, size_t size, uint64_t *buf_hdl)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclAllocQDMABuf(size, buf_hdl) : NULL;
}

int xclFreeQDMABuf(xclDeviceHandle handle, uint64_t buf_hdl)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclFreeQDMABuf(buf_hdl) : -ENODEV;
}

ssize_t xclWriteQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
	return drv ? drv->xclWriteQueue(q_hdl, wr) : -ENODEV;
}

ssize_t xclReadQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
	return drv ? drv->xclReadQueue(q_hdl, wr) : -ENODEV;
}

int xclPollCompletion(xclDeviceHandle handle, int min_compl, int max_compl, xclReqCompletion *comps, int* actual, int timeout)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclPollCompletion(min_compl, max_compl, comps, actual, timeout) : -ENODEV;
}

ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset)
{
  return -ENOSYS;
}

ssize_t xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset)
{
  return -ENOSYS;
}

/*
 * API to get number of live processes.
 * Applicable only for System Flow as it supports Multiple processes on same device.
 * For CPU emulation, return 0
 */
uint32_t xclGetNumLiveProcesses(xclDeviceHandle handle)
{
  return 0;
}

int xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size)
{
  return -1;
}

int xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  return -1;
}

int xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  return -1;
}



int xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = xclcpuemhal2::CpuemShim::xclLogMsg(handle, level, tag, format, args);
  va_end(args);
  return ret;
}

//Added below calls as a fix for CR-1034151
int xclOpenContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclOpenContext(xclbinId, ipIndex, shared) : -ENODEV;
}

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclExecWait(timeoutMilliSec) : -ENODEV;
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclExecBuf(cmdBO) : -ENODEV;
}

int xclCloseContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned ipIndex)
{
  xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(handle);
  return drv ? drv->xclCloseContext(xclbinId, ipIndex) : -ENODEV;
}

// Restricted read/write on IP register space
int xclRegWrite(xclDeviceHandle, uint32_t, uint32_t, uint32_t)
{
  return 1;
}

int xclRegRead(xclDeviceHandle, uint32_t, uint32_t, uint32_t*)
{
  return 1;
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

int
xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
  return -ENOSYS;
}
