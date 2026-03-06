// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#include "shim.h"
#include "core/include/shim_int.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/include/xdp/app_debug.h"
#include "core/include/deprecated/xcl_app_debug.h"
#include "xcl_graph.h"

namespace {

// Wrap handle check to throw on error
static xclswemuhal2::SwEmuShim*
get_shim_object(xclDeviceHandle handle)
{
  if (auto shim = xclswemuhal2::SwEmuShim::handleCheck(handle))
    return shim;

  throw xrt_core::error("Invalid shim handle");
}

} // namespace

////////////////////////////////////////////////////////////////
// Implementation of internal SHIM APIs
////////////////////////////////////////////////////////////////
namespace xrt::shim_int {

std::unique_ptr<xrt_core::hwctx_handle>
create_hw_context(xclDeviceHandle handle,
                  const xrt::uuid& xclbin_uuid,
                  const xrt::hw_context::cfg_param_type& cfg_param,
    xrt::hw_context::access_mode mode)
{
    auto shim = get_shim_object(handle);
    return shim->create_hw_context(xclbin_uuid, cfg_param, mode);
}

std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(xclDeviceHandle handle, size_t size, unsigned int flags)
{
  auto shim = get_shim_object(handle);
  return shim->xclAllocBO(size, flags);
}

// alloc_userptr_bo()
std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(xclDeviceHandle handle, void* userptr, size_t size, unsigned int flags)
{
  auto shim = get_shim_object(handle);
  return shim->xclAllocUserPtrBO(userptr, size, flags);
}

std::unique_ptr<xrt_core::buffer_handle>
import_bo(xclDeviceHandle handle, xrt_core::shared_handle::export_handle ehdl)
{
  auto shim = get_shim_object(handle);
  return shim->xclImportBO(ehdl, 0);
}

} // xrt::shim_int

xclDeviceHandle xclOpen(unsigned deviceIndex, const char *logfileName, xclVerbosityLevel level)
{
  xclDeviceInfo2 info;
  std::strcpy(info.mName, "xilinx:pcie-hw-em:7v3:1.0");
  info.mMagic = 0X586C0C6C;
  info.mHALMajorVersion = XCLHAL_MAJOR_VER;
  info.mHALMinorVersion = XCLHAL_MINOR_VER;
  info.mMinTransferSize = 32;
  info.mVendorId = 0x10ee;
  info.mDeviceId = 0x0000;
  info.mSubsystemId = 0xffff;
  info.mSubsystemVendorId = 0x0000;
  info.mDeviceVersion = 0x0000;
  info.mDDRSize = xclemulation::MEMSIZE_4G;
  info.mDataAlignment = DDR_BUFFER_ALIGNMENT;
  info.mDDRBankCount = 1;
  for(unsigned int i = 0; i < 4 ;i++)
    info.mOCLFrequency[i] = 200;

#if defined(__aarch64__)
  info.mNumCDMA = 1;
#else
  info.mNumCDMA = 0;
#endif

  std::string deviceName("");
  std::ifstream mVBNV;
  mVBNV.open("/etc/xocl.txt");
  if (mVBNV.is_open()) {
    mVBNV >> deviceName;
  }
  mVBNV.close();

  if (deviceName.empty()) {
    mVBNV.open("platform_desc.txt");
    if (mVBNV.is_open()) {
      mVBNV >> deviceName;
    }
    mVBNV.close();
  }

  if (!deviceName.empty()) {
    std::size_t length = deviceName.copy(info.mName, deviceName.length(), 0);
    info.mName[length] = '\0';
  }

  std::list<xclemulation::DDRBank> DDRBankList;
  xclemulation::DDRBank bank;
  bank.ddrSize = xclemulation::MEMSIZE_4G;
  DDRBankList.push_back(bank);
  FeatureRomHeader fRomHeader;
  std::memset(&fRomHeader, 0, sizeof(FeatureRomHeader));

  xclswemuhal2::SwEmuShim *handle = nullptr;
  bool bDefaultDevice = false;
  std::map<unsigned int, xclswemuhal2::SwEmuShim*>::iterator it = xclswemuhal2::devices.find(deviceIndex);
  if(it != xclswemuhal2::devices.end())
  {
    handle = (*it).second;
  }
  else
  {
    handle = new xclswemuhal2::SwEmuShim(deviceIndex,info,DDRBankList,false,false,fRomHeader);
    bDefaultDevice = true;
    xclswemuhal2::devices[deviceIndex++] = handle;
  }

  if (!xclswemuhal2::SwEmuShim::handleCheck(handle)) {
    delete handle;
    handle = 0;
  }
  if (handle) {
    handle->xclOpen(logfileName);
  }
  return (xclDeviceHandle *)handle;
}

void xclClose(xclDeviceHandle handle)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return ;
  drv->xclClose();
  if (xclswemuhal2::SwEmuShim::handleCheck(handle) && xclswemuhal2::devices.size() == 0) {
    delete ((xclswemuhal2::SwEmuShim*)handle);
  }
}


int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetDeviceInfo2(info);
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
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
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclAllocDeviceBuffer(size);
}

uint64_t xclAllocDeviceBuffer2(xclDeviceHandle handle, size_t size, xclMemoryDomains domain,
                               unsigned flags)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  bool p2pBuffer = false;
  std::string fileName("");
  return drv->xclAllocDeviceBuffer2(size, domain, flags,p2pBuffer,fileName);
}

void xclFreeDeviceBuffer(xclDeviceHandle handle, uint64_t buf)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return;
  return drv->xclFreeDeviceBuffer(buf);
}


size_t xclCopyBufferHost2Device(xclDeviceHandle handle, uint64_t dest, const void *src, size_t size, size_t seek)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclCopyBufferHost2Device(dest, src, size, seek);
}


size_t xclCopyBufferDevice2Host(xclDeviceHandle handle, void *dest, uint64_t src, size_t size, size_t skip)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclCopyBufferDevice2Host(dest, src, size, skip);
}

size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclWrite(space, offset, hostBuf, size);
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
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
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  drv->resetProgram();
  return 0;
}

int xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
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

size_t xclPerfMonStartCounters(xclDeviceHandle handle, xdp::MonitorType type)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonStopCounters(xclDeviceHandle handle, xdp::MonitorType type)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonReadCounters(xclDeviceHandle handle, xdp::MonitorType type, xdp::CounterResults& counterResults)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}

size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type, void* debugResults)
{
  return 0;
}

size_t xclPerfMonClockTraining(xclDeviceHandle handle, xdp::MonitorType type)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonStartTrace(xclDeviceHandle handle, xdp::MonitorType type, uint32_t startTrigger)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonStopTrace(xclDeviceHandle handle, xdp::MonitorType type)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, xdp::MonitorType type)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


size_t xclPerfMonReadTrace(xclDeviceHandle handle, xdp::MonitorType type, xdp::TraceEventsVector& traceVector)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return 0;
}


double xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  return 0.0;
}


double xclGetHostReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 0.0;
}


double xclGetHostWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 0.0;
}


double xclGetKernelReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 0.0;
}


double xclGetKernelWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 0.0;
}


size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}


void xclSetProfilingNumberSlots(xclDeviceHandle handle, xdp::MonitorType type, uint32_t numSlots)
{
  return;
}

uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, xdp::MonitorType type)
{
  return 0;
}

void xclGetProfilingSlotName(xclDeviceHandle handle, xdp::MonitorType type, uint32_t slotnum,
		                     char* slotName, uint32_t length)
{
  return;
}

unsigned xclProbe()
{
  if(!xclemulation::isXclEmulationModeHwEmuOrSwEmu())
  {
    std::string initMsg ="ERROR: [SW_EMU 09] Please set XCL_EMULATION_MODE to \"sw_emu\" to run software emulation. ";
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
  std::vector<std::tuple<xclDeviceInfo2,std::list<xclemulation::DDRBank> ,bool, bool, FeatureRomHeader> > devicesInfo;
  xclemulation::getDevicesInfo(devicesInfo);

  if(devicesInfo.size() == 0)
    return 1;

  std::string deviceName = "";
  std::ifstream mVBNV;
  mVBNV.open("/etc/xocl.txt");
  if (mVBNV.is_open()) {
    mVBNV >> deviceName;
  }
  mVBNV.close();

  if(deviceName.empty()) {
    mVBNV.open("platform_desc.txt");
    if (mVBNV.is_open()) {
      mVBNV >> deviceName;
    }
    mVBNV.close();
  }

  for(auto &it: devicesInfo)
  {
    xclDeviceInfo2 info = std::get<0>(it);
    std::list<xclemulation::DDRBank> DDRBankList = std::get<1>(it);
    bool bUnified = std::get<2>(it);
    bool bXPR = std::get<3>(it);
    FeatureRomHeader fRomHeader = std::get<4>(it);
    if (!deviceName.empty()) {
      std::size_t length = deviceName.copy(info.mName, deviceName.length(), 0);
      info.mName[length] = '\0';
    }
    xclswemuhal2::SwEmuShim *handle = new xclswemuhal2::SwEmuShim(deviceIndex,info,DDRBankList, bUnified, bXPR, fRomHeader);
    xclswemuhal2::devices[deviceIndex++] = handle;
  }

  xclProbeCallCnt++;
  return deviceIndex;
}

//########################################## HAL2 START ##########################################

unsigned int xclVersion ()
{
  return 2;
}

int
xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
  try {
    auto shim = xclswemuhal2::SwEmuShim::handleCheck(handle);
    if (!shim)
      return -1;

    auto shared = shim->xclExportBO(boHandle);
    auto ptr = static_cast<xclswemuhal2::SwEmuShim::shared_object*>(shared.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get_code();
  }
}

unsigned int
xclImportBO(xclDeviceHandle handle, int boGlobalHandle,unsigned flags)
{
  try {
    auto shim = xclswemuhal2::SwEmuShim::handleCheck(handle);
    if (!shim)
      return -1;

    auto bo = shim->xclImportBO(boGlobalHandle,flags);
    auto ptr = static_cast<xclswemuhal2::SwEmuShim::buffer_object*>(bo.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return static_cast<unsigned int>(ex.get_code());
  }

}

int xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
    xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
    return drv ? drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset) : -ENODEV;
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst,
                 size_t size, size_t skip)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclReadBO(boHandle, dst, size, skip);
}

unsigned int
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  try {
    auto shim = xclswemuhal2::SwEmuShim::handleCheck(handle);
    if (!shim)
      return static_cast<unsigned int>(mNullBO);

    auto bo = shim->xclAllocUserPtrBO(userptr,size,flags);
    auto ptr = static_cast<xclswemuhal2::SwEmuShim::buffer_object*>(bo.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return mNullBO;
  }
}

unsigned int
xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned flags)
{
  try {
    auto shim = xclswemuhal2::SwEmuShim::handleCheck(handle);
    if (!shim)
      return static_cast<unsigned int>(mNullBO);

    auto bo = shim->xclAllocBO(size, flags);
    auto ptr = static_cast<xclswemuhal2::SwEmuShim::buffer_object*>(bo.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return static_cast<unsigned int>(mNullBO);
  }
}


void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return nullptr;
  return drv->xclMapBO(boHandle, write);
}

int xclUnmapBO(xclDeviceHandle handle, unsigned int boHandle, void* addr)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclUnmapBO(boHandle, addr);
}

int xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSyncBO(boHandle, dir , size, offset);
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src,
                  size_t size, size_t seek)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWriteBO(boHandle, src, size, seek);
}
void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclFreeBO(boHandle);
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetBOProperties(boHandle, properties);
}

ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset)
{
  return -ENOSYS;
}

ssize_t
xclUnmgdPwrite(xclDeviceHandle handle, unsigned int flags, const void *buf, size_t size, uint64_t offset)
{
  return -ENOSYS;
}

int
xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
  return -ENOSYS;
}

int
xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t force)
{
  return -ENOSYS;
}

int
xclUpdateSchedulerStat(xclDeviceHandle handle)
{
  return -ENOSYS;
}

int
xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind)
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
  int ret = xclswemuhal2::SwEmuShim::xclLogMsg(handle, level, tag, format, args);
  va_end(args);
  return ret;
}

//Added below calls as a fix for CR-1034151
int xclOpenContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  return drv ? drv->xclOpenContext(xclbinId, ipIndex, shared) : -ENODEV;
}

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  return drv ? drv->xclExecWait(timeoutMilliSec) : -ENODEV;
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  return drv ? drv->xclExecBuf(cmdBO) : -ENODEV;
}

int xclCloseContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned ipIndex)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  return drv ? drv->xclCloseContext(xclbinId, ipIndex) : -ENODEV;
}

// Restricted read/write on IP register space
int xclRegWrite(xclDeviceHandle handle, uint32_t cu_index, uint32_t offset, uint32_t data)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  return drv ? drv->xclRegWrite(cu_index, offset, data) : -ENODEV;
}

int xclRegRead(xclDeviceHandle handle, uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  return drv ? drv->xclRegRead(cu_index, offset, datap) : -ENODEV;
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

//Get CU index from IP_LAYOUT section for corresponding kernel name
int xclIPName2Index(xclDeviceHandle handle, const char *name)
{
  xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
  return drv ? drv->xclIPName2Index(name) : -ENODEV;
}

// Temporary place holder for XRT shim level Graph APIs

void*
xclGraphOpen(xclDeviceHandle handle, const uuid_t xclbin_uuid, const char* graph, xrt::graph::access_mode am)
{
  try {
    xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
    xclGraphHandle graphHandle = new xclswemuhal2::GraphType(drv, graph);
    if (graphHandle) {
      auto ghPtr = (xclswemuhal2::GraphType*)graphHandle;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      if (drv) {
        drv->xrtGraphInit(graphHandle);
      }
      else {
        delete ghPtr;
        ghPtr = nullptr;
        return XRT_NULL_HANDLE;
      }
    }
    return graphHandle;
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

void
xclGraphClose(xclGraphHandle ghl)
{
  try {
    if (ghl) {
      auto ghPtr = (xclswemuhal2::GraphType*)ghl;
      delete ghPtr;
    }
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
}

int
xclGraphReset(xclGraphHandle ghl)
{
  return 0;
}

uint64_t
xclGraphTimeStamp(xclGraphHandle ghl)
{
  return 0;
}

int
xclGraphRun(xclGraphHandle gh, int iterations)
{
  try {
    if (gh) {
      auto ghPtr = (xclswemuhal2::GraphType*)gh;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      return drv ? drv->xrtGraphRun(gh, iterations) : -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphWaitDone(xclGraphHandle gh, int timeoutMilliSec)
{
  try {
    if (gh) {
      auto ghPtr = (xclswemuhal2::GraphType*)gh;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      return drv ? drv->xrtGraphWait(gh) : -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphWait(xclGraphHandle gh, uint64_t cycle)
{
  try {
    if (gh) {
      auto ghPtr = (xclswemuhal2::GraphType*)gh;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      return drv ? (cycle ? drv->xrtGraphTimedWait(gh,cycle) : drv->xrtGraphWait(gh) ): -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphSuspend(xclGraphHandle gh)
{
  return 0;
}

int
xclGraphResume(xclGraphHandle gh)
{
  try {
    if (gh) {
      auto ghPtr = (xclswemuhal2::GraphType*)gh;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      return drv ? drv->xrtGraphResume(gh) : -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphEnd(xclGraphHandle gh, uint64_t cycle)
{
  try {
    if (gh) {
      auto ghPtr = (xclswemuhal2::GraphType*)gh;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      return drv ? (cycle ? drv->xrtGraphTimedEnd(gh, cycle) : drv->xrtGraphEnd(gh) ) : -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphUpdateRTP(xclGraphHandle ghdl, const char* port, const char* buffer, size_t size)
{
  try {
    if (ghdl) {
      auto ghPtr = (xclswemuhal2::GraphType*)ghdl;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      return drv ? drv->xrtGraphUpdateRTP(ghdl, port, buffer, size) : -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclGraphReadRTP(xclGraphHandle ghdl, const char *port, char *buffer, size_t size)
{
  try {
    if (ghdl) {
      auto ghPtr = (xclswemuhal2::GraphType*)ghdl;
      auto drv = (ghPtr) ? ghPtr->getDeviceHandle() : nullptr;
      return drv ? drv->xrtGraphReadRTP(ghdl, port, buffer, size) : -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
}

int
xclAIEOpenContext(xclDeviceHandle handle, xrt::aie::access_mode am)
{
  return 0;
}

int
xclResetAIEArray(xclDeviceHandle handle)
{
  return 0;
}

int
xclGMIOWait(xclDeviceHandle handle, const char *gmioName)
{
  try {
    if (handle) {
      xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(handle);
      return drv ? drv->xrtGMIOWait(gmioName) : -1;
    }
    return -1;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -1;
  }
  return -1;
}

int
xclLoadXclBinMeta(xclDeviceHandle handle, const xclBin *buffer)
{
  return 0;
}
