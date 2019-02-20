/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s): Hem C. Neema
 *          : Min Ma
 * ZNYQ HAL Driver layered on top of ZYNQ kernel driver
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

#include "shim.h"
#include <errno.h>

#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <vector>
#include <poll.h>
//#include "xclbin.h"
#include <assert.h>


#define GB(x)   ((size_t) (x) << 30)
#ifdef __aarch64__
#define BASE_ADDRESS 0xA0000000
#else
#define BASE_ADDRESS 0x40000000
#endif

static std::string parseCUStatus(unsigned val) {
  char delim = '(';
  std::string status;
  if ( val & 0x1) {
    status += delim;
    status += "START";
    delim = '|';
  }
  if ( val & 0x2) {
    status += delim;
    status += "DONE";
    delim = '|';
  }
  if ( val & 0x4) {
    status += delim;
    status += "IDLE";
    delim = '|';
  }
  if ( val & 0x8) {
    status += delim;
    status += "READY";
    delim = '|';
  }
  if ( val & 0x10) {
    status += delim;
    status += "RESTART";
    delim = '|';
  }
  if ( status.size())
    status += ')';
  else if ( val == 0x0)
    status = "(--)";
  else
    status = "(??)";
  return status;
}

// TODO: This code is copy from xclng/user_gem/shim.cpp. Considering to create a util library for X86 and ARM.
// Copy bytes word (32bit) by word.
//
// Neither memcpy, nor std::copy work as they become byte copying on
// some platforms
inline void* wordcopy(void *dst, const void* src, size_t bytes)
{
    // assert dest is 4 byte aligned
    assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

    using word = uint32_t;
    auto d = reinterpret_cast<word*>(dst);
    auto s = reinterpret_cast<const word*>(src);
    auto w = bytes/sizeof(word);

    for (size_t i=0; i<w; ++i)
    {
        d[i] = s[i];
    }

    return dst;
}

namespace ZYNQ {
ZYNQShim::ZYNQShim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity) :
    mBoardNumber(index),
    mVerbosity(verbosity)
{
  //TODO: Use board number
  mKernelFD = open("/dev/dri/renderD128", O_RDWR);
  if(mKernelFD) {
    mKernelControlPtr = (uint32_t*)mmap(0, 0x800000, PROT_READ | PROT_WRITE, MAP_SHARED, mKernelFD, 0);
    if (mKernelControlPtr == MAP_FAILED) {
        printf("Map failed \n");
        close(mKernelFD);
        mKernelFD = -1;
    }
//    printf("Compute Unit addr: %p\n", mKernelControlPtr);
  } else {
    printf("Cannot open /dev/dri/renderD128 \n");
  }
  if (logfileName && (logfileName[0] != '\0')) {
    mLogStream.open(logfileName);
    mLogStream << "FUNCTION, THREAD ID, ARG..." << std::endl;
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }
}
#ifndef __HWEM__
ZYNQShim::~ZYNQShim()
{
  //TODO
  if (mKernelFD > 0) {
    close(mKernelFD);
  }

  if (mLogStream.is_open()) {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    mLogStream.close();
  }
}
#endif


//TODO: UKP: return definition is not defined.
size_t ZYNQShim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  if(!hostBuf) {
    return -1;
  }

  if(XCL_ADDR_KERNEL_CTRL == space) {
    // Temp fix for offset issue. TODO: Umang
    if(offset >= BASE_ADDRESS )
      offset = offset - BASE_ADDRESS;
    wordcopy((char*)mKernelControlPtr + offset, hostBuf, size);
    return size;
  }
  return -1;
}

size_t ZYNQShim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{

  if(!hostBuf) {
    return -1;
  }

  if(XCL_ADDR_KERNEL_CTRL == space) {
    // Temp fix for offset issue. TODO: Umang
    if(offset >= BASE_ADDRESS )
      offset = offset - BASE_ADDRESS;
    wordcopy(hostBuf, (char*) mKernelControlPtr + offset, size );
    return size;
  }

  return -1;
}

unsigned int ZYNQShim::xclAllocBO(size_t size, xclBOKind domain, unsigned flags) {
  // TODO: unify xocl and zocl flags.
  //drm_zocl_create_bo info = { size, 0xffffffff, DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA };
  drm_zocl_create_bo info = { size, 0xffffffff, flags};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CREATE_BO, &info);
  if (mVerbosity == XCL_INFO) {
    std::cout  << "xclAllocBO result = " << result << std::endl;
    std::cout << "Handle " << info.handle << std::endl;
  }
  return info.handle;
}

unsigned int ZYNQShim::xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags) {
    (void)flags;
    drm_zocl_userptr_bo info = {reinterpret_cast<uint64_t>(userptr), size, 0xffffffff, DRM_ZOCL_BO_FLAGS_USERPTR};
    int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_USERPTR_BO, &info);
    if (mVerbosity == XCL_INFO) {
        std::cout  << "xclAllocUserPtrBO result = " << result << std::endl;
        std::cout << "Handle " << info.handle << std::endl;
    }
    return info.handle;
}

unsigned int ZYNQShim::xclGetHostBO(uint64_t paddr, size_t size) {
  drm_zocl_host_bo info = { paddr, size, 0xffffffff };
  //std::cout  << "xclGetHostBO paddr " << std::hex << paddr << std::dec << std::endl;
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_GET_HOST_BO, &info);
  if (mVerbosity == XCL_INFO) {
    std::cout  << "xclGetHostBO result = " << result << std::endl;
    std::cout << "Handle " << info.handle << std::endl;
  }
  return info.handle;
}

void ZYNQShim::xclFreeBO(unsigned int boHandle)
{
  drm_gem_close closeInfo = {boHandle, 0};
  int result = ioctl(mKernelFD, DRM_IOCTL_GEM_CLOSE, &closeInfo);
  if (mVerbosity == XCL_INFO) {
    mLogStream << "xclFreeBO result = " << result << std::endl;
  }
}



int ZYNQShim::xclGetBOInfo(uint64_t handle)
{
    int result = 0;
//  drm_zocl_info_bo info = { handle, 0, 0 };
//  result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_BO, &info);
//  if (mVerbosity == XCL_INFO) {
//    mLogStream << "result = " << result << std::endl;
//    mLogStream << "Handle " << info.handle << std::endl;
//    mLogStream << "Size " << info.size << std::endl;
//    mLogStream << "Physical " << std::hex << info.paddr << std::dec << std::endl;
//  }
  return result;
}

int ZYNQShim::xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
  drm_zocl_pwrite_bo pwriteInfo = { boHandle, 0, seek, size, reinterpret_cast<uint64_t>(src) };
  return ioctl(mKernelFD, DRM_IOCTL_ZOCL_PWRITE_BO, &pwriteInfo);
}

int ZYNQShim::xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip) {
  drm_zocl_pread_bo preadInfo = { boHandle, 0, skip, size, reinterpret_cast<uint64_t>(dst) };
  return ioctl(mKernelFD, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo);
}

void *ZYNQShim::xclMapBO(unsigned int boHandle, bool write)
{
  drm_zocl_info_bo info = { boHandle, 0, 0 };
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_BO, &info);

  drm_zocl_map_bo mapInfo = { boHandle, 0, 0 };
  result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_MAP_BO, &mapInfo);
  if (result)
    return NULL;

  void *ptr = mmap(0, info.size, (write ?(PROT_READ|PROT_WRITE) : PROT_READ ),
          MAP_SHARED, mKernelFD, mapInfo.offset);

  return ptr;
}

int ZYNQShim::xclGetDeviceInfo2(xclDeviceInfo2 *info)
{
  std::memset(info, 0, sizeof(xclDeviceInfo2));

  info->mMagic = 0X586C0C6C;
  info->mHALMajorVersion = XCLHAL_MAJOR_VER;
  info->mHALMajorVersion = XCLHAL_MINOR_VER;
  info->mMinTransferSize = 32;
  info->mVendorId = 0x10ee;   // TODO: UKP
  info->mDeviceId = 0xffff;   // TODO: UKP
  info->mSubsystemId = 0xffff;
  info->mSubsystemVendorId = 0xffff;
  info->mDeviceVersion = 0xffff;

  info->mDDRSize = GB(4);
  info->mDataAlignment = BUFFER_ALIGNMENT;  //TODO:UKP

  info->mDDRBankCount = 1;
  info->mOCLFrequency[0] = 100;

  std::string deviceName;
  // Mike add the VBNV in the platform image.
  mVBNV.open("/etc/xocl.txt");
  if (mVBNV.is_open()) {
      mVBNV >> deviceName;
  } else {
      printf("Can not open /etc/xocl.txt. The device name not found. \n");
  }
  mVBNV.close();
  std::size_t length = deviceName.copy(info->mName, deviceName.length(),0);
  info->mName[length] = '\0';
  return 0;
}

int ZYNQShim::xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  drm_zocl_sync_bo_dir zocl_dir;
  if (dir == XCL_BO_SYNC_BO_TO_DEVICE)
      zocl_dir = DRM_ZOCL_SYNC_BO_TO_DEVICE;
  else if (dir == XCL_BO_SYNC_BO_FROM_DEVICE)
      zocl_dir = DRM_ZOCL_SYNC_BO_FROM_DEVICE;
  else
      return -EINVAL;
  drm_zocl_sync_bo syncInfo = { boHandle, zocl_dir, offset, size };
  return ioctl(mKernelFD, DRM_IOCTL_ZOCL_SYNC_BO, &syncInfo);
}

#ifndef __HWEM__
int ZYNQShim::xclLoadXclBin(const xclBin *buffer)
{
  int ret = 0;
  const char *xclbininmemory = reinterpret_cast<char *> (const_cast<xclBin*> (buffer));
  if (mLogStream.is_open()) {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buffer << std::endl;
  }

  if (!memcmp(xclbininmemory, "xclbin2", 8)) {
    ret = xclLoadAxlf(reinterpret_cast<const axlf*> (xclbininmemory));
  } else {
    if (mLogStream.is_open()) {
      mLogStream << "xclLoadXclBin don't support legacy xclbin format." << std::endl;
    }
  }

//
//  std::cout << "CU Status:\n";
//  for (unsigned i = 0; i < 4; i++) {
//    xclRead(XCL_ADDR_KERNEL_CTRL, i * 4096, static_cast<void *>(buf), 16);
//    std::cout << "  " << std::setw(7) << i << ":      0x" << std::hex << buf[0] << std::dec << " " << parseCUStatus(buf[0]) << "\n";
//  }
//
//#if defined(XCLBIN_DOWNLOAD)
//  drm_zocl_pcap_download obj = { const_cast<xclBin *>(buffer) };
//  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_PCAP_DOWNLOAD, &obj);
//  if ( 0 != ret) {
//    std::cout << __func__ << " PCAP download failed. Error code:  " << ret << std::endl;
//  }
//  std::cout << __func__ << " PCAP download successful. return code:  " << ret << std::endl;
//  std::cout << "CU Status:\n";
//  for (unsigned i = 0; i < 4; i++) {
//    xclRead(XCL_ADDR_KERNEL_CTRL, i * 4096, static_cast<void *>(buf), 16);
//    std::cout << "  " << std::setw(7) << i << ":      0x" << std::hex << buf[0] << std::dec << " " << parseCUStatus(buf[0]) << "\n";
//  }
//#endif

  return ret;
}
#endif

int ZYNQShim::xclLoadAxlf(const axlf *buffer)
{
	int ret = 0;
	if (mLogStream.is_open()) {
		mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buffer << std::endl;
	}

#if defined(XCLBIN_DOWNLOAD)
  drm_zocl_pcap_download obj = { const_cast<axlf *>(buffer) };
  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_PCAP_DOWNLOAD, &obj);
#endif

	drm_zocl_axlf axlf_obj = { const_cast<axlf *>(buffer) };
	ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_READ_AXLF, &axlf_obj);

	return ret;
}

int ZYNQShim::xclExportBO(unsigned int boHandle)
{
  drm_prime_handle info = {boHandle, 0, -1};
  int result = ioctl(mKernelFD, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
  if (mVerbosity == XCL_INFO) {
    mLogStream << "xclExportBO result = " << result << std::endl;
  }
  return !result ? info.fd : result;
}

unsigned int ZYNQShim::xclImportBO(int fd, unsigned flags)
{
  drm_prime_handle info = {0xffffffff, flags, fd};
  int result = ioctl(mKernelFD, DRM_IOCTL_PRIME_FD_TO_HANDLE, &info);
  if (result) {
      std::cout << __func__ << " ERROR: FD to handle IOCTL failed" << std::endl;
  }
  if (mVerbosity == XCL_INFO) {
    mLogStream << "xclImportBO result = " << result << std::endl;
  }
  return !result ? info.handle : 0xffffffff;
}

unsigned int ZYNQShim::xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
  drm_zocl_info_bo info = {boHandle, 0, 0};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_BO, &info);
  properties->handle = info.handle;
  properties->flags  = DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA;
  properties->size   = info.size;
  properties->paddr  = info.paddr;
  properties->domain = XCL_BO_DEVICE_RAM; // currently all BO domains are XCL_BO_DEVICE_RAM
  return result;
}

bool ZYNQShim::isGood() const
{
  if(mKernelFD < 0)
    return false;

  return true;
}

ZYNQShim *ZYNQShim::handleCheck(void *handle)
{
  // Sanity checks
  if (!handle)
    return 0;
  //if (*(unsigned *)handle != TAG)
  //  return 0;
  if (!((ZYNQShim *) handle)->isGood()) {
    return 0;
  }

  return (ZYNQShim *) handle;
}

void ZYNQShim::xclWriteHostEvent(xclPerfMonEventType type, xclPerfMonEventID id) {
  //  if (mVerbosity == XCL_INFO)
  //    std::cout << "xclWriteHostEvent called (type = " << type << ", ID = " << id << ")" << std::endl;
  //
  //  uint32_t regValue = (id << 4) + (type & 0xF);
  //  xclWrite(XCL_ADDR_KERNEL_CTRL, ZYNQ_PERFMON_OFFSET, &regValue, 4);
}

int ZYNQShim::xclExecBuf(unsigned int cmdBO)
{
  drm_zocl_execbuf exec = {0, cmdBO};
  return ioctl(mKernelFD, DRM_IOCTL_ZOCL_EXECBUF, &exec);
}

int ZYNQShim::xclExecWait(int timeoutMilliSec)
{
  std::vector<pollfd> uifdVector;
  pollfd info = {mKernelFD, POLLIN, 0};
  uifdVector.push_back(info);
  return poll(&uifdVector[0], uifdVector.size(), timeoutMilliSec);
}

}
;
//end namespace ZYNQ
#ifndef __HWEM__
unsigned xclProbe()
{
  int fd = open("/dev/dri/renderD128", O_RDWR);
  if (fd < 0) {
    return 0;
  }
  drm_version version;
  std::memset(&version, 0, sizeof(version));
  version.name = new char[128];
  version.name_len = 128;
  version.desc = new char[512];
  version.desc_len = 512;
  version.date = new char[128];
  version.date_len = 128;

  int result = ioctl(fd, DRM_IOCTL_VERSION, &version);
  if (result)
      return 0;

  result = std::strncmp(version.name, "zocl", 4);
  close(fd);

  return (result == 0) ? 1 : 0;
}
#endif

xclDeviceHandle xclOpen(unsigned deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
  //std::cout << "xclOpen called" << std::endl;
  ZYNQ::ZYNQShim *handle = new ZYNQ::ZYNQShim(deviceIndex, logFileName, level);
  if (!ZYNQ::ZYNQShim::handleCheck(handle)) {
    delete handle;
    handle = 0;
  }
  return (xclDeviceHandle) handle;
}

void xclClose(xclDeviceHandle handle)
{
  //std::cout << "xclClose called" << std::endl;
  if (ZYNQ::ZYNQShim::handleCheck(handle)) {
    delete ((ZYNQ::ZYNQShim *) handle);
  }
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, xclBOKind domain, unsigned flags)
{
  //std::cout << "xclAllocBO called " << std::endl;
  //std::cout << "xclAllocBO size:  "  << size << std::endl;
  //std::cout << "xclAllocBO handle " << handle << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  //std::cout << "xclAllocBO handle check passed" << std::endl;
  return drv->xclAllocBO(size, domain, flags);
}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  //std::cout << "xclAllocUserPtrBO called.. " << handle << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAllocUserPtrBO(userptr, size, flags);
  //return 0xffffffff;
}

unsigned int xclGetHostBO(xclDeviceHandle handle, uint64_t paddr, size_t size)
{
  std::cout << "xclGetHostBO called.. " << handle << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetHostBO(paddr, size);
}

void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle)
{
  //std::cout << "xclFreeBO called" << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclFreeBO(boHandle);
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src,
                  size_t size, size_t seek)
{

  //std::cout << "xclWriteBO called" << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWriteBO(boHandle, src, size, seek);
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst,
                 size_t size, size_t skip)
{

  //std::cout << "xclReadBO called" << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclReadBO(boHandle, dst, size, skip);
}

void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  //std::cout << "xclMapBO called" << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return NULL;
  return drv->xclMapBO(boHandle, write);
}

int xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir,
              size_t size, size_t offset) {
  //std::cout << "xclSyncBO called.. " << handle << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSyncBO(boHandle, dir, size, offset);
}

int xclExportBO(xclDeviceHandle handle, unsigned int boHandle) {
  //std::cout << "xclExportBO called.. " << handle << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExportBO(boHandle);
}

unsigned int xclImportBO(xclDeviceHandle handle, int fd, unsigned flags) {
  //std::cout << "xclImportBO called.. " << handle << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclImportBO(fd, flags);
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
    if (!drv)
        return -EINVAL;
    return drv->xclLoadXclBin(buffer);
}

size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  //std::cout << "xclWrite called" << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWrite(space, offset, hostBuf, size);
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
//  //std::cout << "xclRead called" << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclRead(space, offset, hostBuf, size);
}

int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
  //std::cout << "xclGetDeviceInfo2 called" << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetDeviceInfo2(info);
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetBOProperties(boHandle, properties);
}

unsigned int xclVersion ()
{
  return 2;
}

void xclWriteHostEvent(xclDeviceHandle handle, xclPerfMonEventType type, xclPerfMonEventID id)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclWriteHostEvent(type, id);
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExecBuf(cmdBO) ;
}

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExecWait(timeoutMilliSec);
}

//
// TODO: pending implementations
//
int xclOpenContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  return 0;
}

int xclCloseContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned ipIndex)
{
  return 0;
}

size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}
double xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  return 0;
}
double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 0;
}
double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
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
void xclGetProfilingSlotName(xclDeviceHandle handle, xclPerfMonType type,
                             uint32_t slotnum, char* slotName, uint32_t length)
{
  return;
}
size_t xclPerfMonClockTraining(xclDeviceHandle handle, xclPerfMonType type)
{
  return 0;
}
size_t xclPerfMonStartCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  return 0;
}
size_t xclPerfMonStopCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  return 0;
}
size_t xclPerfMonReadCounters(xclDeviceHandle handle, xclPerfMonType type, xclCounterResults& counterResults)
{
  return 0;
}
size_t xclPerfMonStartTrace(xclDeviceHandle handle, xclPerfMonType type, uint32_t startTrigger)
{
  return 0;
}
size_t xclPerfMonStopTrace(xclDeviceHandle handle, xclPerfMonType type)
{
  return 0;
}
uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, xclPerfMonType type)
{
  return 0;
}
size_t xclPerfMonReadTrace(xclDeviceHandle handle, xclPerfMonType type, xclTraceResultsVector& traceVector)
{
  return 0;
}
size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type,
                            void* debugResults)
{
  return 0;
}
int xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  return 0;
}
int xclGetUsageInfo(xclDeviceHandle handle, xclDeviceUsage *info)
{
  return 0;
}
int xclGetErrorStatus(xclDeviceHandle handle, xclErrorStatus *info)
{
  return 0;
}
int xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
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
int xclUpgradeFirmware(xclDeviceHandle handle, const char *fileName)
{
  return 0;
}
int xclUpgradeFirmware2(xclDeviceHandle handle, const char *file1, const char* file2)
{
  return 0;
}
int xclUpgradeFirmwareXSpi(xclDeviceHandle handle, const char *fileName, int index)
{
  return 0;
}
int xclBootFPGA(xclDeviceHandle handle)
{
  return 0;
}
int xclRemoveAndScanFPGA()
{
  return 0;
}
ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf,
                      size_t size, uint64_t offset)
{
  return 0;
}
ssize_t xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf,
                       size_t size, uint64_t offset)
{
  return 0;
}
int xclRegisterInterruptNotify(xclDeviceHandle handle, unsigned int userInterrupt, int fd)
{
  return 0;
}
int xclCreateWriteQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, void **q_hdl)
{
  return -ENOSYS;
}
int xclCreateReadQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, void **q_hdl)
{
  return -ENOSYS;
}
int xclDestroyQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}
int xclModifyQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}
int xclStartQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}
int xclStopQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}
ssize_t xclWriteQueue(xclDeviceHandle handle, void *q_hdl, xclQueueRequest *wr_req)
{
  return -ENOSYS;
}
ssize_t xclReadQueue(xclDeviceHandle handle, void *q_hdl, xclQueueRequest *wr_req)
{
  return -ENOSYS;
}
