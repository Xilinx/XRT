// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.
#include "shim.h"
#include "core/include/shim_int.h"
#include "core/include/xdp/app_debug.h"
#include "core/include/deprecated/xcl_app_debug.h"

#include "core/common/device.h"
#include "core/common/system.h"
#include "plugin/xdp/device_offload.h"
#include "plugin/xdp/hal_trace.h"
#include "plugin/xdp/pl_deadlock.h"

namespace {

// Wrap handle check to throw on error
static xclhwemhal2::HwEmShim*
get_shim_object(xclDeviceHandle handle)
{
  if (auto shim = xclhwemhal2::HwEmShim::handleCheck(handle))
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

void
register_xclbin(xclDeviceHandle handle, const xrt::xclbin& xclbin)
{
  auto shim = get_shim_object(handle);
  shim->register_xclbin(xclbin);
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


int xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
  try {
    auto shim = xclhwemhal2::HwEmShim::handleCheck(handle);
    if (!shim)
      return -1;

    auto shared = shim->xclExportBO(boHandle);
    auto ptr = static_cast<xclhwemhal2::HwEmShim::shared_object*>(shared.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get_code();
  }
}

unsigned int
xclImportBO(xclDeviceHandle handle, int boGlobalHandle, unsigned flags)
{
  try {
    auto shim = xclhwemhal2::HwEmShim::handleCheck(handle);
    if (!shim)
      return static_cast<unsigned int>(mNullBO);

    auto bo = shim->xclImportBO(boGlobalHandle, flags);
    auto ptr = static_cast<xclhwemhal2::HwEmShim::buffer_object*>(bo.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return static_cast<unsigned int>(ex.get_code());
  }
}

int xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclCopyBO", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset) : -ENODEV;
  }) ;
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
  return xdp::hw_emu::trace::buffer_transfer_profiling_wrapper("xclReadBO",
                                                               size,
                                                               false,
                                                               [=]{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return static_cast<size_t>(-EINVAL);
  return drv->xclReadBO(boHandle, dst, size, skip);
  }) ;
}

unsigned int
xclAllocBO(xclDeviceHandle handle, size_t size, int, unsigned int flags)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclAllocBO", [=] {
    try {
      auto shim = xclhwemhal2::HwEmShim::handleCheck(handle);
      if (!shim)
        return static_cast<unsigned int>(mNullBO);

      auto bo = shim->xclAllocBO(size, flags);
      auto ptr = static_cast<xclhwemhal2::HwEmShim::buffer_object*>(bo.get());
      return ptr->detach_handle();
    }
    catch (const xrt_core::error& ex) {
      xrt_core::send_exception_message(ex.what());
      return static_cast<unsigned int>(mNullBO);
    }
  }) ;
}


void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclMapBO", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return static_cast<void*>(nullptr);
  return drv->xclMapBO(boHandle, write);
  }) ;
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
  return xdp::hw_emu::trace::buffer_transfer_profiling_wrapper("xclSyncBO",
    size, (dir == XCL_BO_SYNC_BO_TO_DEVICE), [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return static_cast<int>(-EINVAL);
  return drv->xclSyncBO(boHandle, dir , size, offset);
  });
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src,
                  size_t size, size_t seek)
{
  return xdp::hw_emu::trace::buffer_transfer_profiling_wrapper("xclWriteBO",
    size, true, [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return static_cast<size_t>(-EINVAL);
  return drv->xclWriteBO(boHandle, src, size, seek);
  }) ;
}
void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle)
{
  xdp::hw_emu::trace::profiling_wrapper("xclFreeBO", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclFreeBO(boHandle);
  }) ;
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclGetBOProperties", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetBOProperties(boHandle, properties);
  }) ;
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclExecBuf", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclExecBuf(cmdBO);
  }) ;
}

int xclExecBufWithWaitList(xclDeviceHandle handle, unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
    xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
    if (!drv)
      return -1;
    return drv->xclExecBuf(cmdBO,num_bo_in_wait_list,bo_wait_list);
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
  return xdp::hw_emu::trace::profiling_wrapper("xclExecWait", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclExecWait(timeoutMilliSec) ;
  }) ;
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

  //return xdp::hw_emu::trace::profiling_wrapper("xclProbe", [=] {

  if (xclProbeCallCnt == 1) {
    return deviceIndex;
  }

  std::vector<std::tuple<xclDeviceInfo2, std::list<xclemulation::DDRBank>, bool, bool, FeatureRomHeader, boost::property_tree::ptree> > devicesInfo;
  getDevicesInfo(devicesInfo);
  if(devicesInfo.size() == 0)
    return static_cast<unsigned int>(1);//old behavior
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
  //  }) ;
}

unsigned int
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclAllocUserPtrBO", [=] {
    try {
      auto shim = xclhwemhal2::HwEmShim::handleCheck(handle);
      if (!shim)
        return static_cast<unsigned int>(mNullBO);

      auto bo = shim->xclAllocUserPtrBO(userptr,size,flags);
      auto ptr = static_cast<xclhwemhal2::HwEmShim::buffer_object*>(bo.get());
      return ptr->detach_handle();
    }
    catch (const xrt_core::error& ex) {
      xrt_core::send_exception_message(ex.what());
      return static_cast<unsigned int>(mNullBO);
    }
  }) ;
}

xclDeviceHandle xclOpen(unsigned deviceIndex, const char *logfileName, xclVerbosityLevel level)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclOpen", [=] {
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
  }) ;
}


void xclClose(xclDeviceHandle handle)
{
  xdp::hw_emu::trace::profiling_wrapper("xclClose", [=] {
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
  }) ;
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclLoadXclbin", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  xdp::hw_emu::flush_device(handle);
  auto ret = drv->xclLoadXclBin(buffer);
  if (!ret) {
    auto device = xrt_core::get_userpf_device(drv);
    device->register_axlf(buffer);
    // Call update_device only when xclbin is loaded and registered successfully
    xdp::hw_emu::update_device(handle);
    xdp::hw_emu::pl_deadlock::update_device(handle);
    ret = xrt_core::scheduler::init(handle, buffer);
  }
  return ret;
  }) ;
}

size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclWrite", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return static_cast<size_t>(-1);
  return drv->xclWrite(space, offset, hostBuf, size);
  }) ;
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclRead", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return static_cast<size_t>(-1);
  return drv->xclRead(space, offset, hostBuf, size);
  }) ;
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
  return xdp::hw_emu::trace::profiling_wrapper("xclUnmgdPwrite", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclUnmgdPwrite(flags, buf, count, offset) : static_cast<ssize_t>(-ENODEV);
  }) ;
}

ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclUnmgdPread", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclUnmgdPread(flags, buf, count, offset) : static_cast<ssize_t>(-ENODEV);
  }) ;
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

double xclGetHostReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetHostReadMaxBandwidthMBps();
}

double xclGetHostWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetHostWriteMaxBandwidthMBps();
}

double xclGetKernelReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetKernelReadMaxBandwidthMBps();
}

double xclGetKernelWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetKernelWriteMaxBandwidthMBps();
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
  int ret = xclhwemhal2::HwEmShim::xclLogMsg(level, tag, format, args);
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
  return xdp::hw_emu::trace::profiling_wrapper("xclRegWrite", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclRegWrite(cu_index, offset, data) : -ENODEV;
  }) ;
}

int xclRegRead(xclDeviceHandle handle, uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
  return xdp::hw_emu::trace::profiling_wrapper("xclRegRead", [=] {
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclRegRead(cu_index, offset, datap) : -ENODEV;
  }) ;
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
xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  return -ENOSYS;
}

int
xclUpdateSchedulerStat(xclDeviceHandle handle)
{
  return -ENOSYS;
}

//Get CU index from IP_LAYOUT section for corresponding kernel name
int xclIPName2Index(xclDeviceHandle handle, const char *name)
{
  xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(handle);
  return drv ? drv->xclIPName2Index(name) : -ENODEV;
}
