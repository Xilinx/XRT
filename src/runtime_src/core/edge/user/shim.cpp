/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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
#include "system_linux.h"
#include "core/common/message.h"
#include "core/common/scheduler.h"
#include "core/common/xclbin_parser.h"
#include "core/common/config_reader.h"
#include "core/include/xcl_perfmon_parameters.h"
#include "core/common/bo_cache.h"
#include "core/common/config_reader.h"
#include "core/common/error.h"

#include <cerrno>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <cassert>
#include <cstdarg>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#ifndef __HWEM__
#include "plugin/xdp/hal_profile.h"
#include "plugin/xdp/hal_api_interface.h"
#include "plugin/xdp/hal_device_offload.h"
#endif

namespace {

constexpr size_t
operator"" _gb(unsigned long long value)
{
  return value << 30;
}

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

shim::
shim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity)
  : mCoreDevice(xrt_core::edge_linux::get_userpf_device(this, index))
  , mBoardNumber(index)
  , mVerbosity(verbosity)
  , mKernelClockFreq(100)
  , mCuMaps(128, nullptr)
{
  if (logfileName != nullptr)
    xclLog(XRT_WARNING, "XRT", "%s: logfileName is no longer supported", __func__);

  xclLog(XRT_INFO, "XRT", "%s", __func__);

  mKernelFD = open("/dev/dri/renderD128", O_RDWR);
  if (!mKernelFD) {
    xclLog(XRT_ERROR, "XRT", "%s: Cannot open /dev/dri/renderD128", __func__);
  }
  mCmdBOCache = std::make_unique<xrt_core::bo_cache>(this, xrt_core::config::get_cmdbo_cache());
  mDev = zynq_device::get_dev();
}

#ifndef __HWEM__
shim::
~shim()
{
  xclLog(XRT_INFO, "XRT", "%s", __func__);

  // The BO cache unmaps and releases all execbo, but this must
  // be done before the device (mKernelFD) is closed.
  mCmdBOCache.reset(nullptr);

  if (mKernelFD > 0) {
    close(mKernelFD);
  }

  for (auto p : mCuMaps) {
    if (p)
      (void) munmap(p, mCuMapSize);
  }
}
#endif

// This function is for internal mapping CU and debug IP only.
// For the future, this could be used to support any address aperture.
int
shim::
mapKernelControl(const std::vector<std::pair<uint64_t, size_t>>& offsets)
{
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
        drm_zocl_info_cu info = {offset_it->first, -1, -1};
        int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_CU, &info);
        if (result) {
          xclLog(XRT_ERROR, "XRT", "%s: Failed to find CU info 0x%lx", __func__, offset_it->first);
          return -1;
        }
        size_t psize = getpagesize();
        ptr = mmap(0, offset_it->second, PROT_READ | PROT_WRITE, MAP_SHARED, mKernelFD, info.apt_idx*psize);
        if (ptr == MAP_FAILED) {
          xclLog(XRT_ERROR, "XRT", "%s: Map failed for aperture 0x%lx, size 0x%lx", __func__, offset_it->first, offset_it->second);
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
void *
shim::
getVirtAddressOfApture(xclAddressSpace space, const uint64_t phy_addr, uint64_t& offset)
{
  void *vaddr = NULL;

  // If CU size is 64 Kb, then this is safe.  For Debug/Profile IPs,
  //  they may have 4K or 8K register space.  The profiling library
  //  will make sure that the offset will not be abused.
  uint64_t mask;
  if (space == XCL_ADDR_SPACE_DEVICE_PERFMON) {
    // Try small aperture.  If it fails, then try bigger apertures
    mask = 0xFFF;
    while (mask != 0x3FFF) {
      vaddr = mKernelControl[phy_addr & ~mask];
      if (vaddr) {
        offset = phy_addr & mask ;
        break ;
      }
      mask = (mask << 1) + 1;
    }
  }

  if (!vaddr) {
    // Try 64KB aperture
    mask = 0xFFFF;
    vaddr = mKernelControl[phy_addr & ~mask];
    offset = phy_addr & mask;
  }

  if (!vaddr)
    xclLog(XRT_ERROR, "XRT", "%s: Could not found the mapped address. Check if XCLBIN is loaded.", __func__);

  // If could not found the phy_addr in the mapping table, return will be NULL.
  return vaddr;
}

// For xclRead and xclWrite. The offset is comming from XCLBIN.
// It is the physical address on MPSoC.
// It consists of base address of the aperture and offset in the aperture
// Now the aceptable aperture are CUs and Debug IPs
size_t
shim::
xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  uint64_t off;
  void *vaddr = NULL;

  if (!hostBuf) {
    xclLog(XRT_ERROR, "XRT", "%s: Invalid hostBuf.", __func__);
    return -1;
  }

  vaddr = getVirtAddressOfApture(space, offset, off);
  if (!vaddr) {
    xclLog(XRT_ERROR, "XRT", "%s: Invalid offset.", __func__);
    return -1;
  }

  // Once reach here, vaddr and hostBuf should already be checked.
  wordcopy((char *)vaddr + off, hostBuf, size);
  return size;
}

size_t
shim::
xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  uint64_t off;
  void *vaddr = NULL;

  if (!hostBuf) {
    xclLog(XRT_ERROR, "XRT", "%s: Invalid hostBuf.", __func__);
    return -1;
  }

  vaddr = getVirtAddressOfApture(space, offset, off);
  if (!vaddr) {
    xclLog(XRT_ERROR, "XRT", "%s: Invalid offset.", __func__);
    return -1;
  }

  // Once reach here, vaddr and hostBuf should already be checked.
  wordcopy(hostBuf, (char *)vaddr + off, size);
  return size;
}

unsigned int
shim::
xclAllocBO(size_t size, int unused, unsigned flags)
{
  drm_zocl_create_bo info = { size, 0xffffffff, flags};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CREATE_BO, &info);

  xclLog(XRT_DEBUG, "XRT", "%s: size %ld, flags 0x%x", __func__, size, flags);
  xclLog(XRT_INFO, "XRT", "%s: ioctl return %d, bo handle %d", __func__, result, info.handle);

  return info.handle;
}

unsigned int
shim::
xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags)
{
  (void)flags;
  drm_zocl_userptr_bo info = {reinterpret_cast<uint64_t>(userptr), size, 0xffffffff, DRM_ZOCL_BO_FLAGS_USERPTR};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_USERPTR_BO, &info);

  xclLog(XRT_DEBUG, "XRT", "%s: userptr %p size %ld, flags 0x%x", __func__, userptr, size, DRM_ZOCL_BO_FLAGS_USERPTR);
  xclLog(XRT_INFO, "XRT", "%s: ioctl return %d, bo handle %d", __func__, result, info.handle);

  return info.handle;
}

unsigned int
shim::
xclGetHostBO(uint64_t paddr, size_t size)
{
  drm_zocl_host_bo info = {paddr, size, 0xffffffff};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_GET_HOST_BO, &info);

  xclLog(XRT_DEBUG, "XRT", "%s: paddr 0x%lx, size %ld", __func__, paddr, size);
  xclLog(XRT_INFO, "XRT", "%s: ioctl return %d, bo handle %d", __func__, result, info.handle);

  return info.handle;
}

void
shim::
xclFreeBO(unsigned int boHandle)
{
  drm_gem_close closeInfo = {boHandle, 0};
  int result = ioctl(mKernelFD, DRM_IOCTL_GEM_CLOSE, &closeInfo);

  xclLog(XRT_DEBUG, "XRT", "%s: boHandle %d, ioctl return %d", __func__, boHandle, result);
}

int
shim::
xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
  drm_zocl_pwrite_bo pwriteInfo = { boHandle, 0, seek, size, reinterpret_cast<uint64_t>(src) };
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_PWRITE_BO, &pwriteInfo);

  xclLog(XRT_DEBUG, "XRT", "%s: boHandle %d, src %p, size %ld, seek %ld", __func__, boHandle, src, size, seek);
  xclLog(XRT_INFO, "XRT", "%s: ioctl return %d", __func__, result);

  return result;
}

int
shim::
xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
  drm_zocl_pread_bo preadInfo = { boHandle, 0, skip, size, reinterpret_cast<uint64_t>(dst) };
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo);

  xclLog(XRT_DEBUG, "XRT", "%s: boHandle %d, dst %p, size %ld, skip %ld", __func__, boHandle, dst, size, skip);
  xclLog(XRT_INFO, "XRT", "%s: ioctl return %d", __func__, result);

  return result;
}

void *
shim::
xclMapBO(unsigned int boHandle, bool write)
{
  drm_zocl_info_bo info = { boHandle, 0, 0 };
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_BO, &info);

  drm_zocl_map_bo mapInfo = { boHandle, 0, 0 };
  result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_MAP_BO, &mapInfo);
  if (result) {
    xclLog(XRT_ERROR, "XRT", "%s: ZOCL_MAP_BO ioctl return %d", __func__, result);
    return NULL;
  }

  void *ptr = mmap(0, info.size, (write ?(PROT_READ|PROT_WRITE) : PROT_READ ),
                   MAP_SHARED, mKernelFD, mapInfo.offset);

  xclLog(XRT_INFO, "XRT", "%s: mmap return %p", __func__, ptr);

  return ptr;
}

int
shim::
xclUnmapBO(unsigned int boHandle, void* addr)
{
  drm_zocl_info_bo info = { boHandle, 0, 0 };
  int ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_BO, &info);
  if (ret)
    return -errno;

  return munmap(addr, info.size);
}

int
shim::
xclGetDeviceInfo2(xclDeviceInfo2 *info)
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

  info->mDDRSize = 4_gb;
  info->mDataAlignment = BUFFER_ALIGNMENT;  //TODO:UKP

  info->mDDRBankCount = 1;
  info->mOCLFrequency[0] = mKernelClockFreq;

#if defined(__aarch64__)
  info->mNumCDMA = 1;
#else
  info->mNumCDMA = 0;
#endif

  std::string deviceName("edge");
  mVBNV.open("/etc/xocl.txt");
  if (mVBNV.is_open()) {
    mVBNV >> deviceName;
  }
  mVBNV.close();
  std::size_t length = deviceName.copy(info->mName, deviceName.length(),0);
  info->mName[length] = '\0';
  return 0;
}

int
shim::
xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  drm_zocl_sync_bo_dir zocl_dir;
  if (dir == XCL_BO_SYNC_BO_TO_DEVICE)
    zocl_dir = DRM_ZOCL_SYNC_BO_TO_DEVICE;
  else if (dir == XCL_BO_SYNC_BO_FROM_DEVICE)
    zocl_dir = DRM_ZOCL_SYNC_BO_FROM_DEVICE;
  else
    return -EINVAL;
  drm_zocl_sync_bo syncInfo = { boHandle, zocl_dir, offset, size };
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SYNC_BO, &syncInfo);

  xclLog(XRT_DEBUG, "XRT", "%s: boHandle %d, dir %d, size %ld, offset %ld", __func__, boHandle, dir, size, offset);
  xclLog(XRT_INFO, "XRT", "%s: ioctl return %d", __func__, result);

  return result;
}

int
shim::
xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
          size_t dst_offset, size_t src_offset)
{
  int ret = -EOPNOTSUPP;
#ifdef __aarch64__
  auto bo = mCmdBOCache->alloc<ert_start_copybo_cmd>();
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
  xclLog(XRT_INFO, "XRT", "%s: return %d", __func__, ret);
  return ret;
}


#ifndef __HWEM__
int
shim::
xclLoadXclBin(const xclBin *buffer)
{
  auto top = reinterpret_cast<const axlf*>(buffer);
  auto ret = xclLoadAxlf(top);

  if (!ret && !xrt_core::xclbin::is_pdi_only(top))
    mKernelClockFreq = xrt_core::xclbin::get_kernel_freq(top);

  xclLog(XRT_INFO, "XRT", "%s: return %d", __func__, ret);
  return ret;
}
#endif

int
shim::
xclLoadAxlf(const axlf *buffer)
{
  int ret = 0;
  unsigned int flags = DRM_ZOCL_PLATFORM_BASE;
  int off = 0;

  /*
   * If platform is a non-PR-platform, Following check will fail. Dont download
   * the partial bitstream
   *
   * If Platform is a PR-platform, Following check passes as enable_pr value is
   * true by default. Download the partial bitstream.
   *
   * If platform is a PR-platform, but v++ generated a full bitstream (using
   * some v++ param).  User need to add enable_pr=false in xrt.ini.
   */
  auto is_pr_platform = (buffer->m_header.m_mode == XCLBIN_PR ) ? true : false;
  auto is_pr_enabled = xrt_core::config::get_enable_pr(); //default value is true

  if (is_pr_platform && is_pr_enabled)
    flags = DRM_ZOCL_PLATFORM_PR;

    drm_zocl_axlf axlf_obj = {
      .za_xclbin_ptr = const_cast<axlf *>(buffer),
      .za_flags = flags,
      .za_ksize = 0,
      .za_kernels = NULL,
    };

  if (!xrt_core::xclbin::is_pdi_only(buffer)) {
    auto kernels = xrt_core::xclbin::get_kernels(buffer);
    /* Calculate size of kernels */
    for (auto& kernel : kernels) {
      axlf_obj.za_ksize += sizeof(kernel_info) + sizeof(argument_info) * kernel.args.size();
    }

    /* Check PCIe's shim.cpp for details of kernels binary */
    std::vector<char> krnl_binary(axlf_obj.za_ksize);
    axlf_obj.za_kernels = krnl_binary.data();
    for (auto& kernel : kernels) {
      auto krnl = reinterpret_cast<kernel_info *>(axlf_obj.za_kernels + off);
      if (kernel.name.size() > sizeof(krnl->name))
          return -EINVAL;
      std::strncpy(krnl->name, kernel.name.c_str(), sizeof(krnl->name)-1);
      krnl->name[sizeof(krnl->name)-1] = '\0';
      krnl->anums = kernel.args.size();

      int ai = 0;
      for (auto& arg : kernel.args) {
        if (arg.name.size() > sizeof(krnl->args[ai].name))
          return -EINVAL;
        std::strncpy(krnl->args[ai].name, arg.name.c_str(), sizeof(krnl->args[ai].name)-1);
        krnl->args[ai].name[sizeof(krnl->args[ai].name)-1] = '\0';
        krnl->args[ai].offset = arg.offset;
        krnl->args[ai].size   = arg.size;
        // XCLBIN doesn't define argument direction yet and it only support
        // input arguments.
        // Driver use 1 for input argument and 2 for output.
        // Let's refine this line later.
        krnl->args[ai].dir    = 1;
        ai++;
      }
      off += sizeof(kernel_info) + sizeof(argument_info) * kernel.args.size();
    }
  }

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_READ_AXLF, &axlf_obj);

  xclLog(XRT_INFO, "XRT", "%s: flags 0x%x, return %d", __func__, flags, ret);
  return ret;
}

int
shim::
xclExportBO(unsigned int boHandle)
{
  drm_prime_handle info = {boHandle, DRM_RDWR, -1};
  // Since Linux 4.6, drm_prime_handle_to_fd_ioctl respects O_RDWR.
  int result = ioctl(mKernelFD, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
  if (result) {
    xclLog(XRT_WARNING, "XRT", "%s: DRM prime handle to fd faied with DRM_RDWR. Try default flags.", __func__);
    info.flags = 0;
    result = ioctl(mKernelFD, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
  }

  xclLog(XRT_INFO, "XRT", "%s: boHandle %d, ioctl return %ld, fd %d", __func__, boHandle, result, info.fd);

  return !result ? info.fd : result;
}

unsigned int
shim::
xclImportBO(int fd, unsigned flags)
{
  drm_prime_handle info = {0xffffffff, flags, fd};
  int result = ioctl(mKernelFD, DRM_IOCTL_PRIME_FD_TO_HANDLE, &info);
  if (result) {
    xclLog(XRT_ERROR, "XRT", "%s: FD to handle IOCTL failed", __func__);
  }

  xclLog(XRT_INFO, "XRT", "%s: fd %d, flags %x, ioctl return %d, bo handle %d", __func__, fd, flags, result, info.handle);

  return !result ? info.handle : 0xffffffff;
}

unsigned int
shim::
xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
  drm_zocl_info_bo info = {boHandle, 0, 0};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_BO, &info);
  properties->handle = info.handle;
  properties->flags  = DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA;
  properties->size   = info.size;
  properties->paddr  = info.paddr;

  xclLog(XRT_DEBUG, "XRT", "%s: boHandle %d, size %x, paddr 0x%lx", __func__, boHandle, info.size, info.paddr);

  return result;
}

bool
shim::
isGood() const
{
  if(mKernelFD < 0)
    return false;

  return true;
}

shim *
shim::
handleCheck(void *handle)
{
  // Sanity checks
  if (!handle)
    return 0;

  if (!((shim *) handle)->isGood()) {
    return 0;
  }

  return (shim *) handle;
}

int
shim::
xclExecBuf(unsigned int cmdBO)
{
  drm_zocl_execbuf exec = {0, cmdBO};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_EXECBUF, &exec);
  xclLog(XRT_DEBUG, "XRT", "%s: cmdBO handle %d, ioctl return %d", __func__, cmdBO, result);
  if (result == -EDEADLK)
      xclLog(XRT_ERROR, "XRT", "CU might hang, please reset device");
  return result;
}

int
shim::
xclExecWait(int timeoutMilliSec)
{
  std::vector<pollfd> uifdVector;
  pollfd info = {mKernelFD, POLLIN, 0};
  uifdVector.push_back(info);
  return poll(&uifdVector[0], uifdVector.size(), timeoutMilliSec);
}

uint
shim::
xclGetNumLiveProcesses()
{
  return 0;
}

std::string
shim::
xclGetSysfsPath(const std::string& entry)
{
  return zynq_device::get_dev()->get_sysfs_path(entry);
}

int
shim::
xclGetDebugIPlayoutPath(char* layoutPath, size_t size)
{
  std::string path = xclGetSysfsPath("debug_ip_layout");
  if (path.size() >= size)
    return -EINVAL;
  std::strncpy(layoutPath, path.c_str(), size);
  return 0;
}

int
shim::
xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  // On Zynq, we are currently storing 2 samples per packet in the FIFO
  traceSamples = nSamples/2;
  traceBufSz = sizeof(uint32_t) * nSamples;
  return 0;
}

int
shim::
xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  uint32_t *buffer = (uint32_t*)traceBuf;
  for(uint32_t i = 0 ; i < numSamples; i++) {
    // Read only one 32-bit value. Later (in xdp layer) assemble two 32-bit values to form one trace sample.
    // Here numSamples is the total number of reads required
    xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, ipBaseAddress + 0x1000, (buffer + i), sizeof(uint32_t));
  }
  wordsPerSample = 2;
  return 0;
}

int
shim::
xclSKGetCmd(xclSKCmd *cmd)
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

int
shim::
xclSKCreate(unsigned int boHandle, uint32_t cu_idx)
{
  int ret;
  drm_zocl_sk_create scmd = {cu_idx, boHandle};

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SK_CREATE, &scmd);

  return ret;
}

int
shim::
xclSKReport(uint32_t cu_idx, xrt_scu_state state)
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

int
shim::
xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  unsigned int flags = shared ? ZOCL_CTX_SHARED : ZOCL_CTX_EXCLUSIVE;
  int ret;

  drm_zocl_ctx ctx = {
    .uuid_ptr = reinterpret_cast<uint64_t>(xclbinId),
    .uuid_size = sizeof (uuid_t) * sizeof (char),
    .cu_index = ipIndex,
    .flags = flags,
    .handle = 0,
    .op = ZOCL_CTX_OP_ALLOC_CTX,
  };

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  return ret ? -errno : ret;
}

int
shim::
xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex)
{
  std::lock_guard<std::mutex> l(mCuMapLock);
  int ret;

  if (ipIndex < mCuMaps.size()) {
    // Make sure no MMIO register space access when CU is released.
    uint32_t *p = mCuMaps[ipIndex];
    if (p) {
      (void) munmap(p, mCuMapSize);
      mCuMaps[ipIndex] = nullptr;
    }
  }

  drm_zocl_ctx ctx = {
    .uuid_ptr = reinterpret_cast<uint64_t>(xclbinId),
    .uuid_size = sizeof (uuid_t) * sizeof (char),
    .cu_index = ipIndex,
    .flags = 0,
    .handle = 0,
    .op = ZOCL_CTX_OP_FREE_CTX,
  };

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  // return ret ? -errno : ret; // wait for PR-3018 to be merged  return ret ? -errno : ret;
  return 0;
}

int
shim::
xclRegRW(bool rd, uint32_t ipIndex, uint32_t offset, uint32_t *datap)
{
  std::lock_guard<std::mutex> l(mCuMapLock);

  if (ipIndex >= mCuMaps.size()) {
    xclLog(XRT_ERROR, "XRT", "%s: invalid CU index: %d", __func__, ipIndex);
    return -EINVAL;
  }
  if (offset >= mCuMapSize || (offset & (sizeof(uint32_t) - 1)) != 0) {
    xclLog(XRT_ERROR, "XRT", "%s: invalid CU offset: %d", __func__, offset);
    return -EINVAL;
  }

  if (mCuMaps[ipIndex] == nullptr) {
    drm_zocl_info_cu info = {0, -1, (int)ipIndex};
    int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_CU, &info);
    void *p = mmap(0, mCuMapSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                   mKernelFD, info.apt_idx * getpagesize());
    if (p != MAP_FAILED)
      mCuMaps[ipIndex] = (uint32_t *)p;
  }

  uint32_t *cumap = mCuMaps[ipIndex];
  if (cumap == nullptr) {
    xclLog(XRT_ERROR, "XRT", "%s: can't map CU: %d", __func__, ipIndex);
    return -EINVAL;
  }

  if (rd)
    *datap = cumap[offset / sizeof(uint32_t)];
  else
    cumap[offset / sizeof(uint32_t)] = *datap;
  return 0;
}

int
shim::
xclRegRead(uint32_t ipIndex, uint32_t offset, uint32_t *datap)
{
  return xclRegRW(true, ipIndex, offset, datap);
}

int
shim::
xclRegWrite(uint32_t ipIndex, uint32_t offset, uint32_t data)
{
  return xclRegRW(false, ipIndex, offset, &data);
}

int
shim::
xclIPName2Index(const char *name)
{
  std::string errmsg;
  std::vector<char> buf;
  const uint64_t bad_addr = 0xffffffffffffffff;

  mDev->sysfs_get("ip_layout", errmsg, buf);
  if (!errmsg.empty()) {
    xclLog(XRT_ERROR, "XRT", "can't read ip_layout sysfs node: %s",
           errmsg.c_str());
    return -EINVAL;
  }
  if (buf.empty())
    return -ENOENT;

  const ip_layout *map = (ip_layout *)buf.data();
  if(map->m_count < 0) {
    xclLog(XRT_ERROR, "XRT", "invalid ip_layout sysfs node content");
    return -EINVAL;
  }

  uint64_t addr = bad_addr;
  int i;
  for(i = 0; i < map->m_count; i++) {
    if (strncmp((char *)map->m_ip_data[i].m_name, name,
                sizeof(map->m_ip_data[i].m_name)) == 0) {
      addr = map->m_ip_data[i].m_base_address;
      break;
    }
  }

  if (i == map->m_count)
    return -ENOENT;
  if (addr == bad_addr)
    return -EINVAL;

  std::vector<std::string> custat;
  mDev->sysfs_get("kds_custat", errmsg, custat);
  if (!errmsg.empty()) {
    xclLog(XRT_ERROR, "XRT", "can't read kds_custat sysfs node: %s",
           errmsg.c_str());
    return -EINVAL;
  }

  uint32_t idx = 0;
  for (auto& line : custat) {
    // convert and compare parsed hex address CU[@0x[0-9]+]
    size_t pos = line.find("0x");
    if (pos == std::string::npos)
      continue;
    if (static_cast<unsigned long>(addr) ==
        std::stoul(line.substr(pos).c_str(), 0, 16)) {
      return idx;
    }
    ++idx;
  }

  return -ENOENT;
}

int
shim::
xclOpenIPInterruptNotify(uint32_t ipIndex, unsigned int flags)
{
  int ret;

  drm_zocl_ctx ctx = {
    .uuid_ptr = 0,
    .uuid_size = 0,
    .cu_index = ipIndex,
    .flags = flags,
    .handle = 0,
    .op = ZOCL_CTX_OP_OPEN_GCU_FD,
  };

  xclLog(XRT_DEBUG, "XRT", "%s: IP index %d, flags 0x%x", __func__, ipIndex, flags);
  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  return ret;
}

int
shim::
xclCloseIPInterruptNotify(int fd)
{
  xclLog(XRT_DEBUG, "XRT", "%s: fd %d", __func__, fd);
  close(fd);
  return 0;
}

inline int
shim::
xclLog(xrtLogMsgLevel level, const char* tag, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = xclLogMsg(level, tag, format, args);
  va_end(args);

  return ret;
}

int
shim::
xclLogMsg(xrtLogMsgLevel level, const char* tag, const char* format, va_list args)
{
  static auto verbosity = xrt_core::config::get_verbosity();
  if (level <= verbosity) {
    va_list args_bak;
    // vsnprintf will mutate va_list so back it up
    va_copy(args_bak, args);
    int len = std::vsnprintf(nullptr, 0, format, args_bak);
    va_end(args_bak);

    if (len < 0) {
      //illegal arguments
      std::string err_str = "ERROR: Illegal arguments in log format string. ";
      err_str.append(std::string(format));
      xrt_core::message::send((xrt_core::message::severity_level)level, tag,
                              err_str);
      return len;
    }
    ++len; //To include null terminator

    std::vector<char> buf(len);
    len = std::vsnprintf(buf.data(), len, format, args);

    if (len < 0) {
      //error processing arguments
      std::string err_str =
        "ERROR: When processing arguments in log format string. ";
      err_str.append(std::string(format));
      xrt_core::message::send((xrt_core::message::severity_level)level, tag,
                              err_str.c_str());
      return len;
    }
    xrt_core::message::send((xrt_core::message::severity_level)level, tag,
                            buf.data());
  }

  return 0;
}

size_t
shim::
xclDebugReadCheckers(xclDebugCheckersResults* aCheckerResults)
{
  size_t size = 0;

  uint64_t statusRegisters[] = {
    LAPC_OVERALL_STATUS_OFFSET,

    LAPC_CUMULATIVE_STATUS_0_OFFSET, LAPC_CUMULATIVE_STATUS_1_OFFSET,
    LAPC_CUMULATIVE_STATUS_2_OFFSET, LAPC_CUMULATIVE_STATUS_3_OFFSET,

    LAPC_SNAPSHOT_STATUS_0_OFFSET, LAPC_SNAPSHOT_STATUS_1_OFFSET,
    LAPC_SNAPSHOT_STATUS_2_OFFSET, LAPC_SNAPSHOT_STATUS_3_OFFSET
  };

  uint64_t baseAddress[XLAPC_MAX_NUMBER_SLOTS];
  uint32_t numSlots = getIPCountAddrNames(LAPC, baseAddress, nullptr, nullptr, nullptr, nullptr, XLAPC_MAX_NUMBER_SLOTS);
  uint32_t temp[XLAPC_STATUS_PER_SLOT];
  aCheckerResults->NumSlots = numSlots;
  snprintf(aCheckerResults->DevUserName, 256, "%s", " ");
  for (uint32_t s = 0; s < numSlots; ++s) {
    for (int c=0; c < XLAPC_STATUS_PER_SLOT; c++)
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER, baseAddress[s]+statusRegisters[c], &temp[c], 4);

    aCheckerResults->OverallStatus[s]      = temp[XLAPC_OVERALL_STATUS];
    std::copy(temp+XLAPC_CUMULATIVE_STATUS_0, temp+XLAPC_SNAPSHOT_STATUS_0, aCheckerResults->CumulativeStatus[s]);
    std::copy(temp+XLAPC_SNAPSHOT_STATUS_0, temp+XLAPC_STATUS_PER_SLOT, aCheckerResults->SnapshotStatus[s]);
  }

  return size;
}

size_t
shim::
xclDebugReadCounters(xclDebugCountersResults* aCounterResults)
{
  size_t size = 0;

  uint64_t spm_offsets[] = {
    XAIM_SAMPLE_WRITE_BYTES_OFFSET,
    XAIM_SAMPLE_WRITE_TRANX_OFFSET,
    XAIM_SAMPLE_READ_BYTES_OFFSET,
    XAIM_SAMPLE_READ_TRANX_OFFSET,
    XAIM_SAMPLE_OUTSTANDING_COUNTS_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_DATA_OFFSET,
    XAIM_SAMPLE_LAST_READ_ADDRESS_OFFSET,
    XAIM_SAMPLE_LAST_READ_DATA_OFFSET
  };

  uint64_t spm_upper_offsets[] = {
    XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET,
    XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET,
    XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET,
    XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET,
    XAIM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET
  };

  // Read all metric counters
  uint64_t baseAddress[XAIM_MAX_NUMBER_SLOTS];
  uint8_t mPerfmonProperties[XAIM_MAX_NUMBER_SLOTS] = {} ;
  uint32_t numSlots = getIPCountAddrNames(AXI_MM_MONITOR, baseAddress, nullptr, mPerfmonProperties, nullptr, nullptr, XAIM_MAX_NUMBER_SLOTS);

  uint32_t temp[XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT];

  aCounterResults->NumSlots = numSlots;
  snprintf(aCounterResults->DevUserName, 256, "%s", " ");
  for (uint32_t s=0; s < numSlots; s++) {
    uint32_t sampleInterval;
    // Read sample interval register to latch the sampled metric counters
    size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress[s] + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, 4);

    // If applicable, read the upper 32-bits of the 64-bit debug counters
    if (mPerfmonProperties[s] & XAIM_64BIT_PROPERTY_MASK) {
      for (int c = 0 ; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                baseAddress[s] + spm_upper_offsets[c],
                &temp[c], 4) ;
      }
      aCounterResults->WriteBytes[s]    = ((uint64_t)(temp[0])) << 32 ;
      aCounterResults->WriteTranx[s]    = ((uint64_t)(temp[1])) << 32 ;
      aCounterResults->ReadBytes[s]     = ((uint64_t)(temp[2])) << 32 ;
      aCounterResults->ReadTranx[s]     = ((uint64_t)(temp[3])) << 32 ;
      aCounterResults->OutStandCnts[s]  = ((uint64_t)(temp[4])) << 32 ;
      aCounterResults->LastWriteAddr[s] = ((uint64_t)(temp[5])) << 32 ;
      aCounterResults->LastWriteData[s] = ((uint64_t)(temp[6])) << 32 ;
      aCounterResults->LastReadAddr[s]  = ((uint64_t)(temp[7])) << 32 ;
      aCounterResults->LastReadData[s]  = ((uint64_t)(temp[8])) << 32 ;
    }

    for (int c=0; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++)
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+spm_offsets[c], &temp[c], 4);

    aCounterResults->WriteBytes[s]    |= temp[0];
    aCounterResults->WriteTranx[s]    |= temp[1];
    aCounterResults->ReadBytes[s]     |= temp[2];
    aCounterResults->ReadTranx[s]     |= temp[3];
    aCounterResults->OutStandCnts[s]  |= temp[4];
    aCounterResults->LastWriteAddr[s] |= temp[5];
    aCounterResults->LastWriteData[s] |= temp[6];
    aCounterResults->LastReadAddr[s]  |= temp[7];
    aCounterResults->LastReadData[s]  |= temp[8];
  }
  return size;
}

size_t
shim::
xclDebugReadAccelMonitorCounters(xclAccelMonitorCounterResults* samResult)
{
  size_t size = 0;

  /*
    Here should read the version number
    and return immediately if version
    is not supported
  */

  uint64_t sam_offsets[] = {
    XAM_ACCEL_EXECUTION_COUNT_OFFSET,
    XAM_ACCEL_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_STALL_INT_OFFSET,
    XAM_ACCEL_STALL_STR_OFFSET,
    XAM_ACCEL_STALL_EXT_OFFSET,
    XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_TOTAL_CU_START_OFFSET
  };

  uint64_t sam_upper_offsets[] = {
    XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET,
    XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_STALL_INT_UPPER_OFFSET,
    XAM_ACCEL_STALL_STR_UPPER_OFFSET,
    XAM_ACCEL_STALL_EXT_UPPER_OFFSET,
    XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET
  };

  // Read all metric counters
  uint64_t baseAddress[XAM_MAX_NUMBER_SLOTS] = {0};
  uint8_t  accelmonProperties[XAM_MAX_NUMBER_SLOTS] = {0};
  uint8_t  accelmonMajorVersions[XAM_MAX_NUMBER_SLOTS] = {0};
  uint8_t  accelmonMinorVersions[XAM_MAX_NUMBER_SLOTS] = {0};

  uint32_t numSlots = getIPCountAddrNames(ACCEL_MONITOR, baseAddress, nullptr, accelmonProperties,
                                          accelmonMajorVersions, accelmonMinorVersions, XAM_MAX_NUMBER_SLOTS);

  uint32_t temp[XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] = {0};

  samResult->NumSlots = numSlots;
  snprintf(samResult->DevUserName, 256, "%s", " ");
  for (uint32_t s=0; s < numSlots; s++) {
    uint32_t sampleInterval;
    // Read sample interval register to latch the sampled metric counters
    size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress[s] + XAM_SAMPLE_OFFSET,
                    &sampleInterval, 4);

    bool hasDataflow = (cmpMonVersions(accelmonMajorVersions[s],accelmonMinorVersions[s],1,1) < 0) ? true : false;

    // If applicable, read the upper 32-bits of the 64-bit debug counters
    if (accelmonProperties[s] & XAM_64BIT_PROPERTY_MASK) {
      for (int c = 0 ; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                baseAddress[s] + sam_upper_offsets[c],
                &temp[c], 4) ;
      }
      samResult->CuExecCount[s]      = ((uint64_t)(temp[0])) << 32;
      samResult->CuExecCycles[s]     = ((uint64_t)(temp[1])) << 32;
      samResult->CuStallExtCycles[s] = ((uint64_t)(temp[2])) << 32;
      samResult->CuStallIntCycles[s] = ((uint64_t)(temp[3])) << 32;
      samResult->CuStallStrCycles[s] = ((uint64_t)(temp[4])) << 32;
      samResult->CuMinExecCycles[s]  = ((uint64_t)(temp[5])) << 32;
      samResult->CuMaxExecCycles[s]  = ((uint64_t)(temp[6])) << 32;
      samResult->CuStartCount[s]     = ((uint64_t)(temp[7])) << 32;

      if(hasDataflow) {
        uint64_t dfTmp[2] = {0};
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_BUSY_CYCLES_UPPER_OFFSET, &dfTmp[0], 4);
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET, &dfTmp[1], 4);

        samResult->CuBusyCycles[s]      = dfTmp[0] << 32;
        samResult->CuMaxParallelIter[s] = dfTmp[1] << 32;
      }
    }

    for (int c=0; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++)
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+sam_offsets[c], &temp[c], 4);

    samResult->CuExecCount[s]      |= temp[0];
    samResult->CuExecCycles[s]     |= temp[1];
    samResult->CuStallExtCycles[s] |= temp[2];
    samResult->CuStallIntCycles[s] |= temp[3];
    samResult->CuStallStrCycles[s] |= temp[4];
    samResult->CuMinExecCycles[s]  |= temp[5];
    samResult->CuMaxExecCycles[s]  |= temp[6];
    samResult->CuStartCount[s]     |= temp[7];

    if(hasDataflow) {
      uint64_t dfTmp[2] = {0};
      xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_BUSY_CYCLES_OFFSET, &dfTmp[0], 4);
      xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_MAX_PARALLEL_ITER_OFFSET, &dfTmp[1], 4);

      samResult->CuBusyCycles[s]      |= dfTmp[0] << 32;
      samResult->CuMaxParallelIter[s] |= dfTmp[1] << 32;
    } else {
      samResult->CuBusyCycles[s]      = samResult->CuExecCycles[s];
      samResult->CuMaxParallelIter[s] = 1;
    }
  }

  return size;
}

size_t
shim::
xclDebugReadStreamingCounters(xclStreamingDebugCountersResults* aCounterResults)
{
  size_t size = 0; // The amount of data read from the hardware

  // Get the base addresses of all the SSPM IPs in the debug IP layout
  uint64_t baseAddress[XASM_MAX_NUMBER_SLOTS];
  uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_MONITOR,
                                          baseAddress,
                                          nullptr, nullptr, nullptr, nullptr,
                                          XASM_MAX_NUMBER_SLOTS);

  // Fill up the portions of the return struct that are known by the runtime
  aCounterResults->NumSlots = numSlots ;
  snprintf(aCounterResults->DevUserName, 256, "%s", " ");

  // Fill up the return structure with the values read from the hardware
  uint64_t sspm_offsets[] = {
    XASM_NUM_TRANX_OFFSET,
    XASM_DATA_BYTES_OFFSET,
    XASM_BUSY_CYCLES_OFFSET,
    XASM_STALL_CYCLES_OFFSET,
    XASM_STARVE_CYCLES_OFFSET
  };

  for (unsigned int i = 0 ; i < numSlots ; ++i)
    {
      uint32_t sampleInterval ;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress[i] + XASM_SAMPLE_OFFSET,
                      &sampleInterval, sizeof(uint32_t));

      // Then read all the individual 64-bit counters
      unsigned long long int tmp[XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] ;

      for (unsigned int j = 0 ; j < XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; ++j)
        {
          size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                          baseAddress[i] + sspm_offsets[j],
                          &tmp[j], sizeof(unsigned long long int));
        }
      aCounterResults->StrNumTranx[i] = tmp[0] ;
      aCounterResults->StrDataBytes[i] = tmp[1] ;
      aCounterResults->StrBusyCycles[i] = tmp[2] ;
      aCounterResults->StrStallCycles[i] = tmp[3] ;
      aCounterResults->StrStarveCycles[i] = tmp[4] ;
    }
  return size;
}

size_t
shim::
xclDebugReadStreamingCheckers(xclDebugStreamingCheckersResults* aStreamingCheckerResults)
{
  size_t size = 0; // The amount of data read from the hardware

  // Get the base addresses of all the SPC IPs in the debug IP layout
  uint64_t baseAddress[XSPC_MAX_NUMBER_SLOTS];
  uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_PROTOCOL_CHECKER,
                                          baseAddress,
                                          nullptr, nullptr, nullptr, nullptr,
                                          XSPC_MAX_NUMBER_SLOTS);

  // Fill up the portions of the return struct that are known by the runtime
  aStreamingCheckerResults->NumSlots = numSlots ;
  snprintf(aStreamingCheckerResults->DevUserName, 256, "%s", " ");

  // Fill up the return structure with the values read from the hardware
  for (unsigned int i = 0 ; i < numSlots ; ++i)
    {
      uint32_t pc_asserted ;
      uint32_t current_pc ;
      uint32_t snapshot_pc ;

      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
                      baseAddress[i] + XSPC_PC_ASSERTED_OFFSET,
                      &pc_asserted, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
                      baseAddress[i] + XSPC_CURRENT_PC_OFFSET,
                      &current_pc, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
                      baseAddress[i] + XSPC_SNAPSHOT_PC_OFFSET,
                      &snapshot_pc, sizeof(uint32_t));

      aStreamingCheckerResults->PCAsserted[i] = pc_asserted;
      aStreamingCheckerResults->CurrentPC[i] = current_pc;
      aStreamingCheckerResults->SnapshotPC[i] = snapshot_pc;
    }
  return size;
}

uint32_t
shim::
getIPCountAddrNames(int type,
                    uint64_t* baseAddress,
                    std::string* portNames,
                    uint8_t* properties,
                    uint8_t* majorVersions,
                    uint8_t* minorVersions,
                    size_t size)
{
  debug_ip_layout *map;
  auto dev = zynq_device::get_dev() ;
  std::string entry_str = "debug_ip_layout";
  std::string path = dev->get_sysfs_path(entry_str);
  std::ifstream ifs(path.c_str(), std::ifstream::binary);
  uint32_t count = 0;
  char buffer[65536];
  if( ifs ) {
    //debug_ip_layout max size is 65536
    ifs.read(buffer, 65536);
    if (ifs.gcount() > 0) {
      map = (debug_ip_layout*)(buffer);
      for( unsigned int i = 0; i < map->m_count; i++ ) {
        if (count >= size) break;
        if (map->m_debug_ip_data[i].m_type == type) {
          if(baseAddress)baseAddress[count] = map->m_debug_ip_data[i].m_base_address;
          if(portNames) {
            // Fill up string with 128 characters (padded with null characters)
            portNames[count].assign(map->m_debug_ip_data[i].m_name, 128);
            // Strip away extraneous null characters
            portNames[count].assign(portNames[count].c_str());
          }
          if(properties) properties[count] = map->m_debug_ip_data[i].m_properties;
          if(majorVersions) majorVersions[count] = map->m_debug_ip_data[i].m_major;
          if(minorVersions) minorVersions[count] = map->m_debug_ip_data[i].m_minor;
          ++count;
        }
      }
    }
    ifs.close();
  }

  return count;
}

/*
 * Returns  1 if Version2 > Version1
 * Returns  0 if Version2 = Version1
 * Returns -1 if Version2 < Version1
 */
int
shim::
cmpMonVersions(unsigned int major1, unsigned int minor1,
               unsigned int major2, unsigned int minor2)
{
  if (major2 > major1)
    return 1;
  else if (major2 < major1)
    return -1;
  else if (minor2 > minor1)
    return 1;
  else if (minor2 < minor1)
    return -1;
  else return 0;
}

double
shim::
xclGetDeviceClockFreqMHz()
{
  xclDeviceInfo2  deviceInfo ;
  xclGetDeviceInfo2(&deviceInfo) ;
  unsigned short clockFreq = deviceInfo.mOCLFrequency[0] ;
  if (clockFreq == 0)
    clockFreq = 100 ;

  return (double)clockFreq;
}

int
shim::
xclErrorInject(uint16_t num, uint16_t driver, uint16_t  severity, uint16_t module, uint16_t eclass)
{
  int ret;
  drm_zocl_error_inject ecmd = {ZOCL_ERROR_OP_INJECT, num, driver, severity, module, eclass};

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_ERROR_INJECT, &ecmd);
  return ret;
}

int
shim::
xclErrorClear()
{
  int ret;
  drm_zocl_error_inject ecmd = {ZOCL_ERROR_OP_CLEAR_ALL};

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_ERROR_INJECT, &ecmd);
  return ret;
}

#ifdef XRT_ENABLE_AIE
zynqaie::Aie*
shim::
getAieArray()
{
  return aieArray.get();
}

void
shim::
registerAieArray()
{
//not registering AieArray in hw_emu as it is crashing in hw_emu. We can fix the
//issue once move to AIE-V2 is done
#ifndef __HWEM__
  aieArray = std::make_unique<zynqaie::Aie>(mCoreDevice);
#endif
}

bool
shim::
isAieRegistered()
{
  return (aieArray != nullptr);
}

int
shim::
getPartitionFd(drm_zocl_aie_fd &aiefd)
{
  int ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_AIE_FD, &aiefd);
  if (ret)
    return -errno;

  return 0;
}
#endif

} // end namespace ZYNQ

#ifndef __HWEM__
unsigned
xclProbe()
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

xclDeviceHandle
xclOpen(unsigned deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
  try {
    //std::cout << "xclOpen called" << std::endl;
    auto handle = new ZYNQ::shim(deviceIndex, logFileName, level);
    if (!ZYNQ::shim::handleCheck(handle)) {
      delete handle;
      handle = XRT_NULL_HANDLE;
    }
    return handle;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }

  return XRT_NULL_HANDLE;

}

void
xclClose(xclDeviceHandle handle)
{
  //std::cout << "xclClose called" << std::endl;
  if (ZYNQ::shim::handleCheck(handle)) {
    delete ((ZYNQ::shim *) handle);
  }
}

unsigned int
xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned flags)
{
  //std::cout << "xclAllocBO called " << std::endl;
  //std::cout << "xclAllocBO size:  "  << size << std::endl;
  //std::cout << "xclAllocBO handle " << handle << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  //std::cout << "xclAllocBO handle check passed" << std::endl;
  return drv->xclAllocBO(size, unused, flags);
}

unsigned int
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  //std::cout << "xclAllocUserPtrBO called.. " << handle << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAllocUserPtrBO(userptr, size, flags);
  //return 0xffffffff;
}

unsigned int
xclGetHostBO(xclDeviceHandle handle, uint64_t paddr, size_t size)
{
  //std::cout << "xclGetHostBO called.. " << handle << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetHostBO(paddr, size);
}

void
xclFreeBO(xclDeviceHandle handle, unsigned int boHandle)
{
  //std::cout << "xclFreeBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclFreeBO(boHandle);
}

size_t
xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src,
           size_t size, size_t seek)
{

  //std::cout << "xclWriteBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWriteBO(boHandle, src, size, seek);
}

size_t
xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst,
          size_t size, size_t skip)
{
  //std::cout << "xclReadBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclReadBO(boHandle, dst, size, skip);
}

void *
xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  //std::cout << "xclMapBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return NULL;
  return drv->xclMapBO(boHandle, write);
}

int
xclUnmapBO(xclDeviceHandle handle, unsigned int boHandle, void* addr)
{
  //std::cout << "xclMapBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclUnmapBO(boHandle, addr);
}

int
xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir,
          size_t size, size_t offset)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSyncBO(boHandle, dir, size, offset);
}

int
xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle,
          unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset);
}

int
xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
  //std::cout << "xclExportBO called.. " << handle << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExportBO(boHandle);
}

unsigned int
xclImportBO(xclDeviceHandle handle, int fd, unsigned flags)
{
  //std::cout << "xclImportBO called.. " << handle << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclImportBO(fd, flags);
}

int
xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
#ifndef __HWEM__
#ifdef ENABLE_HAL_PROFILING
  LOAD_XCLBIN_CB ;
#endif
#endif

  try {
    ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);

#ifndef __HWEM__
#ifdef ENABLE_HAL_PROFILING
  xdphal::flush_device(handle) ;
#endif
#endif

    auto ret = drv ? drv->xclLoadXclBin(buffer) : -ENODEV;
    if (ret) {
      printf("Load Xclbin Failed\n");

      return ret;
    }
    auto core_device = xrt_core::get_userpf_device(handle);

    core_device->register_axlf(buffer);

#ifdef XRT_ENABLE_AIE
    drv->registerAieArray();
#endif

    /* If PDI is the only section, return here */
    if (xrt_core::xclbin::is_pdi_only(buffer))
        return 0;


#ifndef __HWEM__
#ifdef ENABLE_HAL_PROFILING
  xdphal::update_device(handle) ;
#endif
#endif

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
#ifndef __HWEM__
    START_DEVICE_PROFILING_CB(handle);
#endif
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return 1;
  }
}

size_t
xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWrite(space, offset, hostBuf, size);
}

size_t
xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclRead(space, offset, hostBuf, size);
}

int
xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetDeviceInfo2(info);
}

int
xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetBOProperties(boHandle, properties);
}

unsigned int
xclVersion ()
{
  return 2;
}

int
xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExecBuf(cmdBO) ;
}

int
xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExecWait(timeoutMilliSec);
}

uint
xclGetNumLiveProcesses(xclDeviceHandle handle)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return 0;
  return drv->xclGetNumLiveProcesses();
}

int
xclGetSysfsPath(xclDeviceHandle handle, const char* subdev,
                const char* entry, char* sysfsPath, size_t size)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;

  std::string path = drv->xclGetSysfsPath(entry);
  if (path.size() >= size)
    return -EINVAL;

  std::strncpy(sysfsPath, path.c_str(), size);
  return 0;
}

int
xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclGetDebugIPlayoutPath(layoutPath, size);
}

int
xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return (drv) ? drv->xclGetTraceBufferInfo(nSamples, traceSamples, traceBufSz) : -EINVAL;
}

int
xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return (drv) ? drv->xclReadTraceData(traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample) : -EINVAL;
}

double
xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return 0;
  return drv->xclGetDeviceClockFreqMHz();
}

int
xclSKGetCmd(xclDeviceHandle handle, xclSKCmd *cmd)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSKGetCmd(cmd);
}

int
xclSKCreate(xclDeviceHandle handle, unsigned int boHandle, uint32_t cu_idx)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSKCreate(boHandle, cu_idx);
}

int
xclSKReport(xclDeviceHandle handle, uint32_t cu_idx, xrt_scu_state state)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;

  return drv->xclSKReport(cu_idx, state);
}

/*
 * Context switch phase 1: support xclbin swap, no cu and shared checking
 */
int
xclOpenContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);

  return drv ? drv->xclOpenContext(xclbinId, ipIndex, shared) : -EINVAL;
}

int
xclCloseContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned ipIndex)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclCloseContext(xclbinId, ipIndex) : -EINVAL;
}

size_t
xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}

size_t
xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type,
                     void* debugResults)
{
  ZYNQ::shim* drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -1;
  switch (type) {
  case XCL_DEBUG_READ_TYPE_LAPC:
    return drv->xclDebugReadCheckers(reinterpret_cast<xclDebugCheckersResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_AIM:
    return drv->xclDebugReadCounters(reinterpret_cast<xclDebugCountersResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_AM:
    return drv->xclDebugReadAccelMonitorCounters(reinterpret_cast<xclAccelMonitorCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_ASM:
    return drv->xclDebugReadStreamingCounters(reinterpret_cast<xclStreamingDebugCountersResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_SPC:
    return drv->xclDebugReadStreamingCheckers(reinterpret_cast<xclDebugStreamingCheckersResults*>(debugResults));
  default:
    ;
  }
  return -1 ;
}

int
xclGetDebugProfileDeviceInfo(xclDeviceHandle handle, xclDebugProfileDeviceInfo* info)
{
  return 0;
}

int
xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  return 0;
}

int
xclGetUsageInfo(xclDeviceHandle handle, xclDeviceUsage *info)
{
  return 0;
}

int
xclGetErrorStatus(xclDeviceHandle handle, xclErrorStatus *info)
{
  return 0;
}

int
xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
  return 0;
}

int
xclLockDevice(xclDeviceHandle handle)
{
  return 0;
}

int
xclUnlockDevice(xclDeviceHandle handle)
{
  return 0;
}

int
xclUpgradeFirmware(xclDeviceHandle handle, const char *fileName)
{
  return 0;
}

int
xclUpgradeFirmware2(xclDeviceHandle handle, const char *file1, const char* file2)
{
  return 0;
}

int
xclUpgradeFirmwareXSpi(xclDeviceHandle handle, const char *fileName, int index)
{
  return 0;
}

int
xclBootFPGA(xclDeviceHandle handle)
{
  return 0;
}

int
xclRemoveAndScanFPGA()
{
  return 0;
}

ssize_t
xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf,
              size_t size, uint64_t offset)
{
  return -ENOSYS;
}

ssize_t
xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf,
               size_t size, uint64_t offset)
{
  return -ENOSYS;
}

int
xclRegisterInterruptNotify(xclDeviceHandle handle, unsigned int userInterrupt, int fd)
{
  return 0;
}

int
xclCreateWriteQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, void **q_hdl)
{
  return -ENOSYS;
}

int
xclCreateReadQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, void **q_hdl)
{
  return -ENOSYS;
}

int
xclDestroyQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}

int
xclModifyQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}

int
xclStartQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}

int
xclStopQueue(xclDeviceHandle handle, void *q_hdl)
{
  return -ENOSYS;
}

ssize_t
xclWriteQueue(xclDeviceHandle handle, void *q_hdl, xclQueueRequest *wr_req)
{
  return -ENOSYS;
}

ssize_t
xclReadQueue(xclDeviceHandle handle, void *q_hdl, xclQueueRequest *wr_req)
{
  return -ENOSYS;
}

int
xclCreateProfileResults(xclDeviceHandle handle, ProfileResults** results)
{
  int status = -1;
#ifndef __HWEM__
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  CREATE_PROFILE_RESULTS_CB(handle, results, status);
#endif
  return status;
}

int
xclGetProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  int status = -1;
#ifndef __HWEM__
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  GET_PROFILE_RESULTS_CB(handle, results, status);
#endif
  return status;
}

int
xclDestroyProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  int status = -1;
#ifndef __HWEM__
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  DESTROY_PROFILE_RESULTS_CB(handle, results, status);
#endif
  return status;
}


int
xclRegWrite(xclDeviceHandle handle, uint32_t ipIndex, uint32_t offset,
            uint32_t data)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclRegWrite(ipIndex, offset, data) : -ENODEV;
}

int
xclRegRead(xclDeviceHandle handle, uint32_t ipIndex, uint32_t offset,
           uint32_t *datap)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclRegRead(ipIndex, offset, datap) : -ENODEV;
}

int
xclIPName2Index(xclDeviceHandle handle, const char *name)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return (drv) ? drv->xclIPName2Index(name) : -ENODEV;
}

int
xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag,
          const char* format, ...)
{
  static auto verbosity = xrt_core::config::get_verbosity();
  if (level <= verbosity) {
    va_list args;
    va_start(args, format);
    int ret = -1;
    if (handle) {
      ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
      ret = drv ? drv->xclLogMsg(level, tag, format, args) : -ENODEV;
    } else {
      ret = ZYNQ::shim::xclLogMsg(level, tag, format, args);
    }
    va_end(args);

    return ret;
  }

  return 0;
}

int
xclOpenIPInterruptNotify(xclDeviceHandle handle, uint32_t ipIndex, unsigned int flags)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);

  return drv ? drv->xclOpenIPInterruptNotify(ipIndex, flags) : -EINVAL;
}

int
xclCloseIPInterruptNotify(xclDeviceHandle handle, int fd)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);

  return drv ? drv->xclCloseIPInterruptNotify(fd) : -EINVAL;
}

void
xclGetDebugIpLayout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret)
{
  if(size_ret)
    *size_ret = 0;
  return;
}

int
xclGetSubdevPath(xclDeviceHandle handle,  const char* subdev,
                 uint32_t idx, char* path, size_t size)
{
  return 0;
}

int
xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
  return 1; // -ENOSYS;
}

int
xclErrorInject(xclDeviceHandle handle, uint16_t num, uint16_t driver, uint16_t severity, uint16_t module, uint16_t eclass)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;

  return drv->xclErrorInject(num, driver, severity, module, eclass);
}

int
xclErrorClear(xclDeviceHandle handle)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;

  return drv->xclErrorClear();
}
