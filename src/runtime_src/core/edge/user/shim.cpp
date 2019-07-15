/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s): Hem C. Neema
 *          : Min Ma
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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
#include "shim-profile.h"
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
#include "core/common/message.h"
#include "core/common/scheduler.h"
#include "core/common/xclbin_parser.h"
#include "core/common/bo_cache.h"
#include "core/common/config_reader.h"
#include <assert.h>

#define GB(x)   ((size_t) (x) << 30)

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

// TODO: This code is copy from core/pcie/linux/shim.cpp. Considering to create a util library for X86 and ARM.
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
    profiling(nullptr),
    mBoardNumber(index),
    mVerbosity(verbosity)
{
  profiling = new ZYNQShimProfiling(this);
  //TODO: Use board number
  mKernelFD = open("/dev/dri/renderD128", O_RDWR);
  if (!mKernelFD) {
    printf("Cannot open /dev/dri/renderD128 \n");
  }
  if (logfileName && (logfileName[0] != '\0')) {
    mLogStream.open(logfileName);
    mLogStream << "FUNCTION, THREAD ID, ARG..." << std::endl;
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }
  mCmdBOCache = new xrt_core::bo_cache(this, xrt_core::config::get_cmdbo_cache());
}

#ifndef __HWEM__
ZYNQShim::~ZYNQShim()
{
  delete mCmdBOCache;
  delete profiling;
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

// This function is for internal mapping CU and debug IP only.
// For the future, this could be used to support any address aperture.
int ZYNQShim::mapKernelControl(const std::vector<std::pair<uint64_t, size_t>>& offsets) {
  void *ptr = NULL;

  if (offsets.size() == 0) {
    // The offsets list is empty. Just skip mapping.
    return 0;
  }

  auto offset_it = offsets.begin();
  auto end = offsets.end();

  while (offset_it != end) {
    // This (~0xFF) is the KDS mask
    if ((offset_it->first & (~0xFF)) != (-1UL & ~0xFF)) {
      auto it = mKernelControl.find(offset_it->first);
      if (it == mKernelControl.end()) {
        drm_zocl_info_cu info = {offset_it->first, -1};
        int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_CU, &info);
        if (result) {
            printf("failed to find CU info 0x%lx\n", offset_it->first);
            return -1;
        }
        size_t psize = getpagesize();
        ptr = mmap(0, offset_it->second, PROT_READ | PROT_WRITE, MAP_SHARED, mKernelFD, info.apt_idx*psize);
        if (ptr == MAP_FAILED) {
            printf("Map failed for aperture 0x%lx, size 0x%lx\n", offset_it->first, offset_it->second);
            return -1;
        }
        mKernelControl.insert(it, std::pair<uint64_t, uint32_t *>(offset_it->first, (uint32_t *)ptr));
      }
    }
    offset_it++;
  }

  return 0;
}

// This function is for internal use only.
// It is used to find CUs or Debug IP's virtual address.
void *ZYNQShim::getVirtAddressOfApture(xclAddressSpace space, const uint64_t phy_addr, uint64_t& offset)
{
    void *vaddr = NULL;

    // If CU size is 64 Kb, then this is safe.  For Debug/Profile IPs,
    //  they may have 4K or 8K register space.  The profiling library
    //  will make sure that the offset will not be abused.
    uint64_t mask = (space == XCL_ADDR_SPACE_DEVICE_PERFMON) ? 0x1FFF : 0xFFFF;

    vaddr  = mKernelControl[phy_addr & ~mask];
    offset = phy_addr & mask;

    if (!vaddr)
        std::cout  << "Could not found the mapped address. Check if XCLBIN is loaded." << std::endl;

    // If could not found the phy_addr in the mapping table, return will be NULL.
    return vaddr;
}

// For xclRead and xclWrite. The offset is comming from XCLBIN.
// It is the physical address on MPSoC.
// It consists of base address of the aperture and offset in the aperture
// Now the aceptable aperture are CUs and Debug IPs
size_t ZYNQShim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  uint64_t off;
  void *vaddr = NULL;

  if (!hostBuf) {
    std::cout  << "Invalid hostBuf." << std::endl;
    return -1;
  }

  vaddr = getVirtAddressOfApture(space, offset, off);
  if (!vaddr) {
    std::cout  << "Invalid offset." << std::endl;
    return -1;
  }

  // Once reach here, vaddr and hostBuf should already be checked.
  wordcopy((char *)vaddr + off, hostBuf, size);
  return size;
}

size_t ZYNQShim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  uint64_t off;
  void *vaddr = NULL;

  if (!hostBuf) {
    std::cout  << "Invalid hostBuf." << std::endl;
    return -1;
  }

  vaddr = getVirtAddressOfApture(space, offset, off);
  if (!vaddr) {
    std::cout  << "Invalid offset." << std::endl;
    return -1;
  }

  // Once reach here, vaddr and hostBuf should already be checked.
  wordcopy(hostBuf, (char *)vaddr + off, size);
  return size;
}

unsigned int ZYNQShim::xclAllocBO(size_t size, int unused, unsigned flags) {
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

#if defined(__aarch64__)
  info->mNumCDMA = 1;
#else
  info->mNumCDMA = 0;
#endif

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

int ZYNQShim::xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                        size_t dst_offset, size_t src_offset)
{
  int ret = -EOPNOTSUPP;
#ifdef __aarch64__
    std::pair<const unsigned int, ert_start_copybo_cmd *const> bo = mCmdBOCache->alloc<ert_start_copybo_cmd>();
    ert_fill_copybo_cmd(bo.second, src_boHandle, dst_boHandle,
                        src_offset, dst_offset, size);

    ret = xclExecBuf(bo.first);
    if (ret) {
        mCmdBOCache->release(bo);
        return ret;
    }

    do {
        ret = xclExecWait(1000);
        if (ret == -1)
            break;
    }
    while (bo.second->state < ERT_CMD_STATE_COMPLETED);

    ret = (ret == -1) ? -errno : 0;
    if (!ret && (bo.second->state != ERT_CMD_STATE_COMPLETED))
        ret = -EINVAL;
    mCmdBOCache->release<ert_start_copybo_cmd>(bo);
#endif
  return ret;
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
  drm_prime_handle info = {boHandle, DRM_RDWR, -1};
  // Since Linux 4.6, drm_prime_handle_to_fd_ioctl respects O_RDWR.
  int result = ioctl(mKernelFD, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
  if (result) {
    std::cout << "WARNING: DRM prime handle to fd faied with DRM_RDWR. Try default flags." << std::endl;
    info.flags = 0;
    result = ioctl(mKernelFD, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
  }
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

uint ZYNQShim::xclGetNumLiveProcesses()
{
  return 0;
}

int ZYNQShim::xclGetSysfsPath(const char* subdev, const char* entry, char* sysfsPath, size_t size)
{
  // Until we have a programmatic way to determine what this directory
  //  is on Zynq platforms, this is hard-coded so we can test out
  //  debug and profile features.
  std::string path = "/sys/devices/platform/amba/amba:zyxclmm_drm/";
  path += entry ;

  if (path.length() >= size) return -1 ;

  // Since path.length() < size, we are sure to copy over the null
  //  terminating byte.
  strncpy(sysfsPath, path.c_str(), size) ;
  return 0 ;
}

int ZYNQShim::xclSKGetCmd(xclSKCmd *cmd)
{
  int ret;
  drm_zocl_sk_getcmd scmd;

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SK_GETCMD, &scmd);

  if (!ret) {
    cmd->opcode = scmd.opcode;
    cmd->start_cuidx = scmd.start_cuidx;
    cmd->cu_nums = scmd.cu_nums;
    cmd->xclbin_paddr = scmd.paddr;
    cmd->xclbin_size = scmd.size;
    snprintf(cmd->krnl_name, ZOCL_MAX_NAME_LENGTH, "%s", scmd.name);
  }

  return ret;
}

int ZYNQShim::xclSKCreate(unsigned int boHandle, uint32_t cu_idx)
{
  int ret;
  drm_zocl_sk_create scmd = {cu_idx, boHandle};

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SK_CREATE, &scmd);

  return ret;
}

int ZYNQShim::xclSKReport(uint32_t cu_idx, xrt_scu_state state)
{
  int ret;
  drm_zocl_sk_report scmd;

  switch (state) {
  case XRT_SCU_STATE_DONE:
    scmd.cu_state = ZOCL_SCU_STATE_DONE;
    break;
  default:
    return -EINVAL;
  }

  scmd.cu_idx = cu_idx;

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SK_REPORT, &scmd);

  return ret;
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

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned flags)
{
  //std::cout << "xclAllocBO called " << std::endl;
  //std::cout << "xclAllocBO size:  "  << size << std::endl;
  //std::cout << "xclAllocBO handle " << handle << std::endl;
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  //std::cout << "xclAllocBO handle check passed" << std::endl;
  return drv->xclAllocBO(size, unused, flags);
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
    auto ret = drv ? drv->xclLoadXclBin(buffer) : -ENODEV;
    if (ret) {
        printf("Load Xclbin Failed\n");
        return ret;
    }
    ret = xrt_core::scheduler::init(handle, buffer);
    if (ret) {
        printf("Scheduler init failed\n");
        return ret;
    }
    ret = drv->mapKernelControl(xrt_core::xclbin::get_cus_pair(buffer));
    if (ret) {
        printf("Map CUs Failed\n");
        return ret;
    }
    ret = drv->mapKernelControl(xrt_core::xclbin::get_dbg_ips_pair(buffer));
    if (ret) {
        printf("Map Debug IPs Failed\n");
        return ret;
    }
    return 0;
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

uint xclGetNumLiveProcesses(xclDeviceHandle handle)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return 0;
  return drv->xclGetNumLiveProcesses();
}

int xclGetSysfsPath(xclDeviceHandle handle, const char* subdev,
		    const char* entry, char* sysfsPath, size_t size)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetSysfsPath(subdev, entry, sysfsPath, size);
}

int xclSKGetCmd(xclDeviceHandle handle, xclSKCmd *cmd)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSKGetCmd(cmd);
}

int xclSKCreate(xclDeviceHandle handle, unsigned int boHandle, uint32_t cu_idx)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSKCreate(boHandle, cu_idx);
}

int xclSKReport(xclDeviceHandle handle, uint32_t cu_idx, xrt_scu_state state)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;

  return drv->xclSKReport(cu_idx, state);
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
  return 9600.0 ; // Needs to be adjusted to SoC value
}

double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 9600.0 ; // Needs to be adjusted to SoC value
}

void xclSetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type, uint32_t numSlots)
{
  // No longer supported at this level
  return;
}

uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->getProfilingNumberSlots(type);
}

void xclGetProfilingSlotName(xclDeviceHandle handle, xclPerfMonType type,
                             uint32_t slotnum, char* slotName, uint32_t length)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return;
  if (!(drv->profiling))
    return;
  drv->profiling->getProfilingSlotName(type, slotnum, slotName, length);
}

size_t xclPerfMonClockTraining(xclDeviceHandle handle, xclPerfMonType type)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return 1; // Not yet enabled
}

void xclPerfMonConfigureDataflow(xclDeviceHandle handle, xclPerfMonType type, unsigned *ip_config)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return;
  if (!(drv->profiling))
    return;
  return drv->profiling->xclPerfMonConfigureDataflow(type, ip_config);
}

size_t xclPerfMonStartCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->xclPerfMonStartCounters(type);
}

size_t xclPerfMonStopCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->xclPerfMonStopCounters(type);
}

size_t xclPerfMonReadCounters(xclDeviceHandle handle, xclPerfMonType type, xclCounterResults& counterResults)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->xclPerfMonReadCounters(type, counterResults);
}

size_t xclPerfMonStartTrace(xclDeviceHandle handle, xclPerfMonType type, uint32_t startTrigger)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->xclPerfMonStartTrace(type, startTrigger);
}

size_t xclPerfMonStopTrace(xclDeviceHandle handle, xclPerfMonType type)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->xclPerfMonStopTrace(type);
}

uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, xclPerfMonType type)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->xclPerfMonGetTraceCount(type);
}

size_t xclPerfMonReadTrace(xclDeviceHandle handle, xclPerfMonType type, xclTraceResultsVector& traceVector)
{
  ZYNQ::ZYNQShim *drv = ZYNQ::ZYNQShim::handleCheck(handle);
  if (!drv)
    return -ENODEV;
  if (!(drv->profiling))
    return -EINVAL;
  return drv->profiling->xclPerfMonReadTrace(type, traceVector);
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
