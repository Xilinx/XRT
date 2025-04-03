// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#include "shim.h"
#include "system_linux.h"
#include "hwctx_object.h"

#include "core/include/shim_int.h"
#include "core/include/xdp/aim.h"
#include "core/include/xdp/am.h"
#include "core/include/xdp/asm.h"
#include "core/include/xdp/lapc.h"
#include "core/include/xdp/spc.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/include/deprecated/xcl_app_debug.h"

#include "core/edge/common/aie_parser.h"

#include "core/common/bo_cache.h"
#include "core/common/config_reader.h"
#include "core/common/config_reader.h"
#include "core/common/error.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/scheduler.h"
#include "core/common/xclbin_parser.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>

#include <regex>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "plugin/xdp/hal_profile.h"

#ifndef __HWEM__
#include "plugin/xdp/hal_api_interface.h"
#endif

#include "plugin/xdp/shim_callbacks.h"

#if defined(XRT_ENABLE_LIBDFX)
extern "C" {
#include <libdfx.h>
}
#endif

namespace {

template <typename ...Args>
void
xclLog(xrtLogMsgLevel level, const char* format, Args&&... args)
{
  auto slvl = static_cast<xrt_core::message::severity_level>(level);
  xrt_core::message::send(slvl, "XRT", format, std::forward<Args>(args)...);
}

constexpr size_t
operator"" _gb(unsigned long long value)
{
  return value << 30;
}

static ZYNQ::shim*
get_shim_object(xclDeviceHandle handle)
{
  if (auto shim = ZYNQ::shim::handleCheck(handle))
    return shim;

  throw xrt_core::error("Invalid shim handle");
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
//initializing static member
std::map<uint64_t, uint32_t *> shim::mKernelControl;

shim::
shim(unsigned index)
  : mCoreDevice(xrt_core::edge_linux::get_userpf_device(this, index))
  , mBoardNumber(index)
  , mKernelClockFreq(100)
  , mCuMaps(128, {nullptr, 0})
{
  xclLog(XRT_INFO, "%s", __func__);
  const std::string zocl_drm_device = "/dev/dri/" + get_render_devname();
  mKernelFD = open(zocl_drm_device.c_str(), O_RDWR);
  // Validity of mKernelFD is checked using handleCheck in every shim function

  mCmdBOCache = std::make_unique<xrt_core::bo_cache>(this, xrt_core::config::get_cmdbo_cache());
  mDev = zynq_device::get_dev();
}

shim::
~shim()
{

  xclLog(XRT_INFO, "%s", __func__);

  // Flush all of the profiling information from the device to the profiling
  // library before the device is closed (when profiling is enabled).
  xdp::finish_flush_device(this);

  // The BO cache unmaps and releases all execbo, but this must
  // be done before the device (mKernelFD) is closed.
  mCmdBOCache.reset(nullptr);

  if (mKernelFD > 0) {
    close(mKernelFD);
  }

  for (auto p : mCuMaps) {
    if (p.first)
      (void) munmap(p.first, p.second);
  }
}

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
        drm_zocl_info_cu info = {offset_it->first, -1, -1, 0};
        int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_CU, &info);
        if (result) {
          xclLog(XRT_ERROR, "%s: Failed to find CU info 0x%lx", __func__, offset_it->first);
          return -errno;
        }
        size_t psize = getpagesize();
        ptr = mmap(0, info.cu_size, PROT_READ | PROT_WRITE, MAP_SHARED, mKernelFD, info.apt_idx*psize);
        if (ptr == MAP_FAILED) {
          xclLog(XRT_ERROR, "%s: Map failed for aperture 0x%lx, size 0x%lx", __func__, offset_it->first, info.cu_size);
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
    xclLog(XRT_ERROR, "%s: Could not found the mapped address. Check if XCLBIN is loaded.", __func__);

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
    xclLog(XRT_ERROR, "%s: Invalid hostBuf.", __func__);
    return -1;
  }

  vaddr = getVirtAddressOfApture(space, offset, off);
  if (!vaddr) {
    xclLog(XRT_ERROR, "%s: Invalid offset.", __func__);
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
    xclLog(XRT_ERROR, "%s: Invalid hostBuf.", __func__);
    return -1;
  }

  vaddr = getVirtAddressOfApture(space, offset, off);
  if (!vaddr) {
    xclLog(XRT_ERROR, "%s: Invalid offset.", __func__);
    return -1;
  }

  // Once reach here, vaddr and hostBuf should already be checked.
  wordcopy(hostBuf, (char *)vaddr + off, size);
  return size;
}

std::unique_ptr<xrt_core::buffer_handle>
shim::
xclAllocBO(size_t size, unsigned flags, xrt_core::hwctx_handle* hwctx_hdl)
{
  drm_zocl_create_bo info = { size, 0xffffffff, flags};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CREATE_BO, &info);
 
  if (result)
    throw std::bad_alloc();

  xclLog(XRT_DEBUG, "%s: size %ld, flags 0x%x", __func__, size, flags);
  xclLog(XRT_INFO, "%s: ioctl return %d, bo handle %d", __func__, result, info.handle);

  return std::make_unique<buffer_object>(this, info.handle, hwctx_hdl);
}

std::unique_ptr<xrt_core::buffer_handle>
shim::
xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags, xrt_core::hwctx_handle* hwctx_hdl)
{
  flags |= DRM_ZOCL_BO_FLAGS_USERPTR;
  drm_zocl_userptr_bo info = {reinterpret_cast<uint64_t>(userptr), size, 0xffffffff, flags};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_USERPTR_BO, &info);

  if (result)
    throw std::bad_alloc();

  xclLog(XRT_DEBUG, "%s: userptr %p size %ld, flags 0x%x", __func__, userptr, size, flags);
  xclLog(XRT_INFO, "%s: ioctl return %d, bo handle %d", __func__, result, info.handle);

  return std::make_unique<buffer_object>(this, info.handle, hwctx_hdl);
}

unsigned int
shim::
xclGetHostBO(uint64_t paddr, size_t size)
{
  drm_zocl_host_bo info = {paddr, size, 0xffffffff};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_GET_HOST_BO, &info);

  xclLog(XRT_DEBUG, "%s: paddr 0x%lx, size %ld", __func__, paddr, size);
  xclLog(XRT_INFO, "%s: ioctl return %d, bo handle %d", __func__, result, info.handle);

  return info.handle;
}

void
shim::
xclFreeBO(unsigned int boHandle)
{
  drm_gem_close closeInfo = {boHandle, 0};
  int result = ioctl(mKernelFD, DRM_IOCTL_GEM_CLOSE, &closeInfo);

  xclLog(XRT_DEBUG, "%s: boHandle %d, ioctl return %d", __func__, boHandle, result);
}

int
shim::
xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
  drm_zocl_pwrite_bo pwriteInfo = { boHandle, 0, seek, size, reinterpret_cast<uint64_t>(src) };
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_PWRITE_BO, &pwriteInfo);

  xclLog(XRT_DEBUG, "%s: boHandle %d, src %p, size %ld, seek %ld", __func__, boHandle, src, size, seek);
  xclLog(XRT_INFO, "%s: ioctl return %d", __func__, result);

  return result ? -errno : result;
}

int
shim::
xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
  drm_zocl_pread_bo preadInfo = { boHandle, 0, skip, size, reinterpret_cast<uint64_t>(dst) };
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_PREAD_BO, &preadInfo);

  xclLog(XRT_DEBUG, "%s: boHandle %d, dst %p, size %ld, skip %ld", __func__, boHandle, dst, size, skip);
  xclLog(XRT_INFO, "%s: ioctl return %d", __func__, result);

  return result ? -errno : result;
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
    xclLog(XRT_ERROR, "%s: ZOCL_MAP_BO ioctl return %d", __func__, result);
    return NULL;
  }

  void *ptr = mmap(0, info.size, (write ?(PROT_READ|PROT_WRITE) : PROT_READ ),
                   MAP_SHARED, mKernelFD, mapInfo.offset);

  xclLog(XRT_INFO, "%s: mmap return %p", __func__, ptr);

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
  info->mTimeStamp = 0;

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

  xclLog(XRT_DEBUG, "%s: boHandle %d, dir %d, size %ld, offset %ld", __func__, boHandle, dir, size, offset);
  xclLog(XRT_INFO, "%s: ioctl return %d", __func__, result);

  return result ? -errno : result;
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

  auto boh = static_cast<buffer_object*>(bo.first.get());
  ret = xclExecBuf(boh->get_handle());
  if (ret) {
    mCmdBOCache->release(std::move(bo));
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

  mCmdBOCache->release(std::move(bo));
#endif
  xclLog(XRT_INFO, "%s: return %d", __func__, ret);
  return ret;
}


int
shim::
xclLoadXclBin(const xclBin *buffer)
{
  auto top = reinterpret_cast<const axlf*>(buffer);
  auto ret = xclLoadAxlf(top);

  if (!ret && !xrt_core::xclbin::is_aie_only(top))
    mKernelClockFreq = xrt_core::xclbin::get_kernel_freq(top);

  xclLog(XRT_INFO, "%s: return %d", __func__, ret);
  return ret;
}

#if defined(XRT_ENABLE_LIBDFX)
namespace libdfx {

static void
libdfxHelper(std::shared_ptr<xrt_core::device> core_dev, std::string& dtbo_path, int& fd)
{
  uint32_t slot_id = 0;
  static const std::string dtbo_dir_path = "/configfs/device-tree/overlays/";

  // root privileges are needed for loading and unloading dtbo and bitstream
  if (getuid() && geteuid())
    throw std::runtime_error("Root privileges required");

  /*
   * get dtbo_path of slot '0' for now, in future when we support multi slot we need
   * info about which slot this xclbin needs to be loaded
   * TODO: read slot id from xclbin or get it as an arg to this function
   */
  try {
    dtbo_path = xrt_core::device_query<xrt_core::query::dtbo_path>(core_dev, slot_id);
  }
  catch(const std::exception &e) {
    const std::string errmsg{"Query for dtbo path failed: "};
    throw std::runtime_error(errmsg + e.what());
  }
  if (!dtbo_path.empty()) {
    // remove existing libdfx node
    rmdir((dtbo_dir_path + dtbo_path).c_str());
    dtbo_path.clear();
    // close drm fd as zocl driver will be reloaded
    close(fd);
  }
  else {
    // bitstream is loaded for first time
    std::filesystem::directory_iterator end_itr;
    static const std::regex filter{".*_image_[0-9]+"};
    for (std::filesystem::directory_iterator itr( dtbo_dir_path ); itr != end_itr; ++itr) {
      if (!std::regex_match(itr->path().filename().string(), filter))
        continue;

      // remove existing libdfx node loaded by libdfx daemon
      rmdir((dtbo_dir_path + itr->path().filename().string()).c_str());
    }
  }
}

static void
copyBufferToFile(const std::string& file_path, const char* buf, uint64_t size)
{
  std::ofstream file(file_path, std::ios::out | std::ios::binary);

  if (!file)
    throw std::runtime_error("Failed to open " + file_path + " for writing xclbin section");

  file.write(buf, size);
  file.close();
}

static void
libdfxConfig(std::string& xclbin_dir_path, const axlf *top,
	     const axlf_section_header *bit_header, const axlf_section_header *overlay_header)
{
  // create a temp directory to extract bitstream and dtbo
  char dir[] = "/tmp/xclbin.XXXXXX";
  char *tmpdir = mkdtemp(dir);
  if (tmpdir == nullptr)
    throw std::runtime_error("Failed to create tmp directory for xclbin files extraction");

  xclbin_dir_path = tmpdir;
  // create a file with BITSTREAM section
  const std::string bit_file_path = xclbin_dir_path + "/xclbin.bit";
  auto bit_buffer = reinterpret_cast<const char *>(top) + bit_header->m_sectionOffset;
  copyBufferToFile(bit_file_path, bit_buffer, bit_header->m_sectionSize);

  // create a file with OVERLAY(dtbo) section
  const std::string overlay_file_path = xclbin_dir_path + "/xclbin.dtbo";
  auto overlay_buffer = reinterpret_cast<const char *>(top) + overlay_header->m_sectionOffset;
  copyBufferToFile(overlay_file_path, overlay_buffer, overlay_header->m_sectionSize);
}

// function for cleaning temp files
static void
libdfxClean(const std::string& file_path)
{
  try {
    if (std::filesystem::exists(std::filesystem::path(file_path)))
      std::filesystem::remove_all(std::filesystem::path(file_path));
  }
  catch(std::exception& ex) {
    xclLog(XRT_WARNING, "%s: unable to remove '%s' folder",__func__,file_path);
  }
}

static int
libdfxLoadAxlf(std::shared_ptr<xrt_core::device> core_dev, const axlf *top,
	       const axlf_section_header *overlay_header, int& fd, int flags, std::string& dtbo_path)
{
  static const std::string fpga_device = "/dev/fpga0";

  // check BITSTREAM section
  const axlf_section_header *bit_header = xclbin::get_axlf_section(top, axlf_section_kind::BITSTREAM);
  if (!bit_header)
    throw std::runtime_error("No BITSTREAM section in xclbin");

  //check if xclbin is already loaded
  try {
    if (core_dev->get_xclbin_uuid() == xrt::uuid(top->m_header.uuid) && !(flags & DRM_ZOCL_FORCE_PROGRAM)) {
      xclLog(XRT_WARNING, "%s: skipping as xclbin is already loaded", __func__);
      return 1;
    }
  }
  catch(const std::exception &e) {
    // can happen when no bitstream is loaded and xclbinid sysfs is not created
    // do nothing
  }

  libdfxHelper(core_dev, dtbo_path, fd);

  std::string xclbin_dir_path;
  libdfxConfig(xclbin_dir_path, top, bit_header, overlay_header);

  // call libdfx api to load bitstream and dtbo
  int dtbo_id = dfx_cfg_init(xclbin_dir_path.c_str(), fpga_device.c_str(), 0);
  if (dtbo_id <= 0) {
    libdfxClean(xclbin_dir_path);
    throw std::runtime_error("Failed to initialize config with libdfx api");
  }
  if (dfx_cfg_load(dtbo_id)){
    dfx_cfg_destroy(dtbo_id);
    libdfxClean(xclbin_dir_path);
    throw std::runtime_error("Failed to load bitstream, dtbo with libdfx api");
  }

  // save dtbo_path as load is successful
  dtbo_path = std::filesystem::path(xclbin_dir_path).filename().string()
			+ "_image_" + std::to_string(dtbo_id);

  // clean tmp files of libdfx
  dfx_cfg_destroy(dtbo_id);
  libdfxClean(xclbin_dir_path);

  // asynchronously check for drm device node
  const static int timeout_sec = 10;
  int count = 0;
  const std::string render_dev_dir{"/dev/dri/"};
  std::string zocl_drm_device;
  while (count++ < timeout_sec) {
    zocl_drm_device = render_dev_dir + get_render_devname();
    if (std::filesystem::exists(std::filesystem::path(zocl_drm_device)))
      break;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // create drm fd
  fd = open(zocl_drm_device.c_str(), O_RDWR);
  if (fd < 0) {
    dtbo_path.clear();
    throw std::runtime_error("Cannot create file descriptor with device " + zocl_drm_device);
  }

  return 0;
}

}
#endif

int
shim::
xclLoadAxlf(const axlf *buffer)
{
  int ret = 0;
  unsigned int flags = DRM_ZOCL_PLATFORM_BASE;
  int off = 0;
  std::string dtbo_path("");

#ifndef __HWEM__
  auto is_pr_platform = (buffer->m_header.m_mode == XCLBIN_PR || buffer->m_header.m_actionMask & AM_LOAD_PDI);
  auto is_flat_enabled = xrt_core::config::get_enable_flat(); //default value is false
  auto force_program = xrt_core::config::get_force_program_xclbin(); //default value is false
  auto overlay_header = xclbin::get_axlf_section(buffer, axlf_section_kind::OVERLAY);

  if (is_pr_platform)
    flags = DRM_ZOCL_PLATFORM_PR;
  /*
   * If its non-PR-platform and enable_flat=true in xrt.ini, download the full
   * bitstream. But if OVERLAY section is present in xclbin, userspace apis are
   * used to download full bitstream
   */
  else if (is_flat_enabled && !overlay_header) {
    if (!ZYNQ::shim::handleCheck(this)) {
      xclLog(XRT_ERROR, "%s: No DRM render device found", __func__);
      return -ENODEV;
    }
    flags = DRM_ZOCL_PLATFORM_FLAT;
  }

  if (force_program) {
    flags = flags | DRM_ZOCL_FORCE_PROGRAM;
  }

#if defined(XRT_ENABLE_LIBDFX)
  // if OVERLAY section is present use libdfx apis to load bitstream and dtbo(overlay)
  if(overlay_header) {
    try {
      // if xclbin is already loaded ret val is '1', dont call ioctl in this case
      if (libdfx::libdfxLoadAxlf(this->mCoreDevice, buffer, overlay_header, mKernelFD, flags, dtbo_path))
        return 0;
    }
    catch(const std::exception& e){
      xclLog(XRT_ERROR, "%s: loading xclbin with OVERLAY section failed: %s", __func__,e.what());
      return -EPERM;
    }
  }
#endif

#endif

    /* Get the AIE_METADATA and get the hw_gen information */
    uint8_t hw_gen = xrt_core::edge::aie::get_hw_gen(mCoreDevice.get());
    auto partition_id = xrt_core::edge::aie::full_array_id;

    drm_zocl_axlf axlf_obj = {
      .za_xclbin_ptr = const_cast<axlf *>(buffer),
      .za_flags = flags,
      .za_ksize = 0,
      .za_kernels = NULL,
      .za_slot_id = 0, // TODO Cleanup: Once uuid interface id available we need to remove this
      .za_dtbo_path = const_cast<char *>(dtbo_path.c_str()),
      .za_dtbo_path_len = static_cast<unsigned int>(dtbo_path.length()),
      .hw_gen = hw_gen,
      .partition_id = static_cast<unsigned int>(partition_id),
    };

  axlf_obj.kds_cfg.polling = xrt_core::config::get_ert_polling();
  std::vector<char> krnl_binary;
  auto xml_header = xclbin::get_axlf_section(buffer, axlf_section_kind::EMBEDDED_METADATA);
  if (xml_header) {
    auto kernels = xrt_core::xclbin::get_kernels(buffer);
    /* Calculate size of kernels */
    for (auto& kernel : kernels) {
      axlf_obj.za_ksize += sizeof(kernel_info) + sizeof(argument_info) * kernel.args.size();
    }

    /* Check PCIe's shim.cpp for details of kernels binary */
    krnl_binary.resize(axlf_obj.za_ksize);
    axlf_obj.za_kernels = krnl_binary.data();
    for (auto& kernel : kernels) {
      auto krnl = reinterpret_cast<kernel_info *>(axlf_obj.za_kernels + off);
      if (kernel.name.size() > sizeof(krnl->name))
          return -EINVAL;
      std::strncpy(krnl->name, kernel.name.c_str(), sizeof(krnl->name)-1);
      krnl->name[sizeof(krnl->name)-1] = '\0';
      krnl->range = kernel.range;
      krnl->anums = kernel.args.size();
      krnl->features = 0;
      if (kernel.sw_reset)
        krnl->features |= KRNL_SW_RESET;

      int ai = 0;
      for (auto& arg : kernel.args) {
        if (arg.name.size() > sizeof(krnl->args[ai].name)) {
          xclLog(XRT_ERROR, "%s: Argument name length %d>%d", __func__, arg.name.size(), sizeof(krnl->args[ai].name));
          return -EINVAL;
        }
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

  #ifdef __HWEM__
    if (!secondXclbinLoadCheck(this->mCoreDevice, buffer)) {
      return 0; // skipping to load the 2nd xclbin for hw_emu embedded designs
    }
  #endif

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_READ_AXLF, &axlf_obj);

  xclLog(XRT_INFO, "%s: flags 0x%x, return %d", __func__, flags, ret);
  return ret ? -errno : ret;
}

int
shim::
secondXclbinLoadCheck(std::shared_ptr<xrt_core::device> core_dev, const axlf *top) {
  try {
    static int xclbin_hw_emu_count = 0;

    if (core_dev->get_xclbin_uuid() != xrt::uuid(top->m_header.uuid)) {
      xclbin_hw_emu_count++;

      if (xclbin_hw_emu_count > 1) {
        xclLog(XRT_WARNING, "%s: Skipping as xclbin is already loaded. Only single XCLBIN load is supported for hw_emu embedded designs.", __func__);
        return 0;
      }
    } else {
      xclLog(XRT_INFO, "%s: Loading the XCLBIN", __func__);
    }
  }
  catch(const std::exception &e) {
    // do nothing
  }
  return 1;
}

std::unique_ptr<xrt_core::shared_handle>
shim::
xclExportBO(unsigned int boHandle)
{
  drm_prime_handle info = {boHandle, DRM_RDWR, -1};
  // Since Linux 4.6, drm_prime_handle_to_fd_ioctl respects O_RDWR.
  int result = ioctl(mKernelFD, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
  if (result) {
    xclLog(XRT_WARNING, "%s: DRM prime handle to fd faied with DRM_RDWR. Try default flags.", __func__);
    info.flags = 0;
    result = ioctl(mKernelFD, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
  }

  if (result)
    throw xrt_core::system_error(result, "failed to export bo");

  xclLog(XRT_INFO, "%s: boHandle %d, ioctl return %ld, fd %d", __func__, boHandle, result, info.fd);

  return std::make_unique<shared_object>(this, info.fd);
}

std::unique_ptr<xrt_core::buffer_handle>
shim::
xclImportBO(int fd, unsigned flags)
{
  drm_prime_handle info = {0xffffffff, flags, fd};
  int result = ioctl(mKernelFD, DRM_IOCTL_PRIME_FD_TO_HANDLE, &info);
  if (result)
    throw xrt_core::system_error(result, "ioctl failed to import bo");

  xclLog(XRT_INFO, "%s: fd %d, flags %x, ioctl return %d, bo handle %d", __func__, fd, flags, result, info.handle);

  return std::make_unique<buffer_object>(this, info.handle);
}

unsigned int
shim::
xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
  drm_zocl_info_bo info = {boHandle, 0, 0, 0};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_BO, &info);
  properties->handle = info.handle;
  properties->flags  = info.flags;
  properties->size   = info.size;
  properties->paddr  = info.paddr;

  xclLog(XRT_DEBUG, "%s: boHandle %d, size %x, paddr 0x%lx", __func__, boHandle, info.size, info.paddr);

  return result ? -errno : result;
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
handleCheck(void *handle, bool checkDrmFd /*= true*/)
{
  // Sanity checks
  if (!handle)
    return 0;

  if (checkDrmFd && !((shim *) handle)->isGood()) {
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
  xclLog(XRT_DEBUG, "%s: cmdBO handle %d, ioctl return %d", __func__, cmdBO, result);
  if (result == -EDEADLK)
      xclLog(XRT_ERROR, "CU might hang, please reset device");
  return result ? -errno : result;
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

// For DDR4: Typical Max BW = 19.25 GB/s
double
shim::
xclGetHostReadMaxBandwidthMBps()
{
  return 19250.00;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double
shim::
xclGetHostWriteMaxBandwidthMBps()
{
  return 19250.00;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double
shim::
xclGetKernelReadMaxBandwidthMBps()
{
  return 19250.00;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double
shim::
xclGetKernelWriteMaxBandwidthMBps()
{
  return 19250.00;
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
    cmd->bohdl = scmd.bohdl;
    cmd->meta_bohdl = scmd.meta_bohdl;
    memcpy(cmd->uuid, &scmd.uuid, sizeof(cmd->uuid));
    snprintf(cmd->krnl_name, ZOCL_MAX_NAME_LENGTH, "%s", scmd.name);
    cmd->slot_id = scmd.slot_id;
  }

  return ret ? -errno : ret;
}

int
shim::
xclAIEGetCmd(xclAIECmd *cmd)
{
  drm_zocl_aie_cmd scmd;

  int ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_AIE_GETCMD, &scmd);

  if (!ret) {
    cmd->opcode = scmd.opcode;
    cmd->size = scmd.size;
    snprintf(cmd->info, scmd.size, "%s", scmd.info);
  }

  return ret ? -errno : ret;
}

int
shim::
xclAIEPutCmd(xclAIECmd *cmd)
{
  int ret;
  drm_zocl_aie_cmd scmd;

  scmd.hw_ctx_id = 0;
  scmd.opcode = cmd->opcode;
  scmd.size = cmd->size;
  snprintf(scmd.info, cmd->size, "%s",cmd->info);
  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_AIE_PUTCMD, &scmd);
  return ret ? -errno : ret;
}

int
shim::
xclSKCreate(int *boHandle, uint32_t cu_idx)
{
  int ret;
  drm_zocl_sk_create scmd = {cu_idx, 0};

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SK_CREATE, &scmd);
  if(!ret) {
    *boHandle = scmd.handle;
  }

  return ret ? -errno : ret;
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
  case XRT_SCU_STATE_READY:
    scmd.cu_state = ZOCL_SCU_STATE_READY;
    break;
  case XRT_SCU_STATE_CRASH:
    scmd.cu_state = ZOCL_SCU_STATE_CRASH;
    break;
  case XRT_SCU_STATE_FINI:
    scmd.cu_state = ZOCL_SCU_STATE_FINI;
    break;
  default:
    return -EINVAL;
  }

  scmd.cu_idx = cu_idx;

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SK_REPORT, &scmd);

  return ret ? -errno : ret;
}

int
shim::
xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  unsigned int flags = shared ? ZOCL_CTX_SHARED : ZOCL_CTX_EXCLUSIVE;
  int ret;

  drm_zocl_ctx ctx = {0};
  ctx.uuid_ptr = reinterpret_cast<uint64_t>(xclbinId);
  ctx.uuid_size = sizeof (uuid_t) * sizeof (char);
  ctx.cu_index = ipIndex;
  ctx.flags = flags;
  ctx.op = ZOCL_CTX_OP_ALLOC_CTX;

  if (ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx))
    throw xrt_core::system_error(errno, "failed to open ip context");

  return 0;
}

// open_context() - aka xclOpenContextByName
xrt_core::cuidx_type
shim::
open_cu_context(const xrt_core::hwctx_handle* hwctx_hdl, const std::string& cuname)
{
  auto hwctx = static_cast<const zynqaie::hwctx_object*>(hwctx_hdl);
  auto shared = (hwctx->get_mode() != xrt::hw_context::access_mode::exclusive);

  if (!hw_context_enable) {
    // for legacy flow
    auto cuidx = mCoreDevice->get_cuidx(0, cuname);
    xclOpenContext(hwctx->get_xclbin_uuid().get(), cuidx.index, shared);
    return cuidx;
  }
  else {
    // This is for multi slot case
    unsigned int flags = shared ? ZOCL_CTX_SHARED : ZOCL_CTX_EXCLUSIVE;
    drm_zocl_open_cu_ctx  cu_ctx = {};
    cu_ctx.flags = flags;
    cu_ctx.hw_context = hwctx_hdl->get_slotidx();
    std:strncpy(cu_ctx.cu_name, cuname.c_str(), sizeof(cu_ctx.cu_name));
    cu_ctx.cu_name[sizeof(cu_ctx.cu_name) - 1] = 0;
    if (ioctl(mKernelFD, DRM_IOCTL_ZOCL_OPEN_CU_CTX, &cu_ctx))
      throw xrt_core::error("Failed to open cu context");

    return xrt_core::cuidx_type{cu_ctx.cu_index};
  }
}

void
shim::
close_cu_context(const xrt_core::hwctx_handle* hwctx_hdl, xrt_core::cuidx_type cuidx)
{
  auto hwctx = static_cast<const zynqaie::hwctx_object*>(hwctx_hdl);

  if (!hw_context_enable) {
    // for legacy flow
    if (xclCloseContext(hwctx->get_xclbin_uuid().get(), cuidx.index))
      throw xrt_core::system_error(errno, "failed to close cu context (" + std::to_string(cuidx.index) + ")");
  }
  else {
    // This is for multi slot case
    drm_zocl_close_cu_ctx cu_ctx = {};
    cu_ctx.hw_context = hwctx_hdl->get_slotidx();
    cu_ctx.cu_index = cuidx.index;
    if (ioctl(mKernelFD, DRM_IOCTL_ZOCL_CLOSE_CU_CTX, &cu_ctx))
      throw xrt_core::system_error(errno, "failed to close cu context (" + std::to_string(cuidx.index) + ")");
  }
}

int shim::prepare_hw_axlf(const axlf *buffer, struct drm_zocl_axlf *axlf_obj)
{
  int ret = 0;
  unsigned int flags = DRM_ZOCL_PLATFORM_BASE;
  int off = 0;
  std::string dtbo_path("");

#ifndef __HWEM__
  auto is_pr_platform = (buffer->m_header.m_mode == XCLBIN_PR || buffer->m_header.m_actionMask & AM_LOAD_PDI);
  auto is_flat_enabled = xrt_core::config::get_enable_flat(); //default value is false
  auto force_program = xrt_core::config::get_force_program_xclbin() || buffer->m_header.m_actionMask & AM_LOAD_PDI;
  auto overlay_header = xclbin::get_axlf_section(buffer, axlf_section_kind::OVERLAY);

  if (is_pr_platform)
    flags = DRM_ZOCL_PLATFORM_PR;
  /*
   * If its non-PR-platform and enable_flat=true in xrt.ini, download the full
   * bitstream. But if OVERLAY section is present in xclbin, userspace apis are
   * used to download full bitstream
   */
  else if (is_flat_enabled && !overlay_header) {
    if (!ZYNQ::shim::handleCheck(this)) {
      xclLog(XRT_ERROR, "%s: No DRM render device found", __func__);
      return -ENODEV;
    }
    flags = DRM_ZOCL_PLATFORM_FLAT;
  }

  if (force_program) {
    flags = flags | DRM_ZOCL_FORCE_PROGRAM;
  }

#if defined(XRT_ENABLE_LIBDFX)
  // if OVERLAY section is present use libdfx apis to load bitstream and dtbo(overlay)
  if(overlay_header) {
    try {
      // if xclbin is already loaded ret val is '1', dont call ioctl in this case
      if (libdfx::libdfxLoadAxlf(this->mCoreDevice, buffer, overlay_header, mKernelFD, flags, dtbo_path))
        return 0;
    }
    catch(const std::exception& e){
      xclLog(XRT_ERROR, "%s: loading xclbin with OVERLAY section failed: %s", __func__,e.what());
      return -EPERM;
    }
  }
#endif //XRT_ENABLE_LIBDFX

#endif //__HWEM__

/* Get the AIE_METADATA and get the hw_gen information */
  uint8_t hw_gen = xrt_core::edge::aie::get_hw_gen(mCoreDevice.get());
  auto part_info = xrt_core::edge::aie::get_partition_info(mCoreDevice.get(), buffer->m_header.uuid);
  auto partition_id = part_info.partition_id;

  axlf_obj->za_xclbin_ptr = const_cast<axlf *>(buffer),
  axlf_obj->za_flags = flags,
  axlf_obj->za_ksize = 0,
  axlf_obj->za_kernels = NULL,
  axlf_obj->za_slot_id = 0, // TODO Cleanup: Once uuid interface id available we need to remove this
  axlf_obj->za_dtbo_path = const_cast<char *>(dtbo_path.c_str()),
  axlf_obj->za_dtbo_path_len = static_cast<unsigned int>(dtbo_path.length()),
  axlf_obj->hw_gen = hw_gen,
  axlf_obj->partition_id = partition_id,
  axlf_obj->kds_cfg.polling = xrt_core::config::get_ert_polling();

  std::vector<char> krnl_binary;
  auto xml_header = xclbin::get_axlf_section(buffer, axlf_section_kind::EMBEDDED_METADATA);
  //return success even if there is no embedded metadata.AIE overlay xclbins
  //wont have embedded metadata
  if (!xml_header)
    return 0;
  auto kernels = xrt_core::xclbin::get_kernels(buffer);
  /* Calculate size of kernels */
  for (auto& kernel : kernels) {
    axlf_obj->za_ksize += sizeof(kernel_info) + sizeof(argument_info) * kernel.args.size();
  }

  /* Check PCIe's shim.cpp for details of kernels binary */
  krnl_binary.resize(axlf_obj->za_ksize);
  axlf_obj->za_kernels = krnl_binary.data();
  for (auto& kernel : kernels) {
    auto krnl = reinterpret_cast<kernel_info *>(axlf_obj->za_kernels + off);
    if (kernel.name.size() > sizeof(krnl->name))
        return -EINVAL;
    std::strncpy(krnl->name, kernel.name.c_str(), sizeof(krnl->name)-1);
    krnl->name[sizeof(krnl->name)-1] = '\0';
    krnl->range = kernel.range;
    krnl->anums = kernel.args.size();

    krnl->features = 0;
    if (kernel.sw_reset)
      krnl->features |= KRNL_SW_RESET;

    int ai = 0;
    for (auto& arg : kernel.args) {
      if (arg.name.size() > sizeof(krnl->args[ai].name)) {
        xclLog(XRT_ERROR, "%s: Argument name length %d>%d", __func__, arg.name.size(), sizeof(krnl->args[ai].name));
        return -EINVAL;
      }
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
  return 0;
}

int shim::load_hw_axlf(xclDeviceHandle handle, const xclBin *buffer, drm_zocl_create_hw_ctx *hw_ctx)
{
  drm_zocl_axlf axlf_obj = {};
  auto top = reinterpret_cast<const axlf*>(buffer);
  auto ret = prepare_hw_axlf(top, &axlf_obj);
  if (ret)
    return -errno;

  hw_ctx->axlf_ptr = &axlf_obj;
  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CREATE_HW_CTX, hw_ctx);
  if (ret)
    return -errno;

  auto core_device = xrt_core::get_userpf_device(handle);

  bool checkDrmFD = xrt_core::config::get_enable_flat() ? false : true;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle, checkDrmFD);

  if (!hw_context_enable)
    xdp::update_device(handle, false);
  #ifndef __HWEM__
    START_DEVICE_PROFILING_CB(handle);
  #endif

  return 0;
}

std::unique_ptr<xrt_core::hwctx_handle>
shim::
create_hw_context(xclDeviceHandle handle,
                  const xrt::uuid& xclbin_uuid,
                  const xrt::hw_context::cfg_param_type&,
                  xrt::hw_context::access_mode mode)
{
  const static int qos_val = 0;
  if (!hw_context_enable) {
    //for legacy flow
    return std::make_unique<zynqaie::hwctx_object>(this, 0, xclbin_uuid, mode);
  }
  else {
    // This is for multi slot case
    auto xclbin = mCoreDevice->get_xclbin(xclbin_uuid);
    auto buffer = reinterpret_cast<const axlf*>(xclbin.get_axlf());
    int rcode = 0;
    drm_zocl_create_hw_ctx hw_ctx = {};
    hw_ctx.qos = qos_val;
    auto shim = get_shim_object(handle);

    if(auto ret = shim->load_hw_axlf(handle, buffer, &hw_ctx)) {
      if (ret) {
        if (ret == -EOPNOTSUPP) {
          xclLog(XRT_ERROR, "XCLBIN does not match shell on the card.");
        }
        xclLog(XRT_ERROR, "See dmesg log for details. Err = %d", ret);
        throw xrt_core::error("Failed to create hardware context");
      }
    }
    //success
    mCoreDevice->register_axlf(buffer);

    auto hwctx_obj_ptr{std::make_unique<zynqaie::hwctx_object>(this, hw_ctx.hw_context, xclbin_uuid, mode)};
    hwctx_obj_ptr->init_aie(); // just to make sure Aie instance created only once

    return hwctx_obj_ptr;
  }
}

void
shim::
destroy_hw_context(xrt_core::hwctx_handle::slot_id slot)
{
  if (!hw_context_enable) {
    //for legacy flow, nothing to be done.
    return;
  }
  else {
    // This is for multi slot case
    drm_zocl_destroy_hw_ctx hw_ctx = {};
    hw_ctx.hw_context = slot;

    auto ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_DESTROY_HW_CTX, &hw_ctx);
    if (ret)
      throw xrt_core::system_error(errno, "Failed to destroy hardware context");
  }
}

void
shim::
register_xclbin(const xrt::xclbin&){
  xclLog(XRT_INFO, "%s: xclbin successfully registered for this device without loading the xclbin", __func__);
  hw_context_enable = true;
}

void
shim::
hwctx_exec_buf(const xrt_core::hwctx_handle* hwctx_hdl, xclBufferHandle boh) {
  if (!hw_context_enable) {
    //for legacy flow
    xclExecBuf(boh);

    return;
  }
  // This is for multi slot case
  auto hwctx = static_cast<const zynqaie::hwctx_object*>(hwctx_hdl);
  drm_zocl_hw_ctx_execbuf exec = {hwctx->get_slotidx(), boh};
  int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_HW_CTX_EXECBUF, &exec);
  xclLog(XRT_DEBUG, "%s: cmdBO handle %d, ioctl return %d", __func__, boh, result);
  if (result == -EDEADLK)
    xclLog(XRT_ERROR, "CU might hang, please reset device");
}

int
shim::
xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex)
{
  std::lock_guard<std::mutex> l(mCuMapLock);
  int ret;

  if (ipIndex < mCuMaps.size()) {
    // Make sure no MMIO register space access when CU is released.
    uint32_t *p = mCuMaps[ipIndex].first;
    if (p) {
      (void) munmap(p, mCuMaps[ipIndex].second);
      mCuMaps[ipIndex] = {nullptr, 0};
    }
  }

  drm_zocl_ctx ctx = {0};
  ctx.uuid_ptr = reinterpret_cast<uint64_t>(xclbinId);
  ctx.uuid_size = sizeof (uuid_t) * sizeof (char);
  ctx.cu_index = ipIndex;
  ctx.op = ZOCL_CTX_OP_FREE_CTX;

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  return ret ? -errno : ret;
}

int
shim::
xclRegRW(bool rd, uint32_t ipIndex, uint32_t offset, uint32_t *datap)
{
  std::lock_guard<std::mutex> l(mCuMapLock);

  if (ipIndex >= mCuMaps.size()) {
    xclLog(XRT_ERROR, "%s: invalid CU index: %d", __func__, ipIndex);
    return -EINVAL;
  }
  if ((offset & (sizeof(uint32_t) - 1)) != 0) {
    xclLog(XRT_ERROR, "%s: invalid CU offset: %d", __func__, offset);
    return -EINVAL;
  }

  if (mCuMaps[ipIndex].first == nullptr) {
    drm_zocl_info_cu info = {0, -1, (int)ipIndex, 0};
    int result = ioctl(mKernelFD, DRM_IOCTL_ZOCL_INFO_CU, &info);
    void *p = mmap(0, info.cu_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   mKernelFD, info.apt_idx * getpagesize());
    if (p != MAP_FAILED)
      mCuMaps[ipIndex].first = (uint32_t *)p;
      mCuMaps[ipIndex].second = info.cu_size;
  }

  uint32_t *cumap = mCuMaps[ipIndex].first;
  if (cumap == nullptr) {
    xclLog(XRT_ERROR, "%s: can't map CU: %d", __func__, ipIndex);
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
  for (auto& stat : xrt_core::device_query<xrt_core::query::kds_cu_info>(mCoreDevice))
    if (stat.name == name)
      return stat.index;

  xclLog(XRT_ERROR, "%s not found", name);
  return -ENOENT;
}

int
shim::
xclIPSetReadRange(uint32_t ipIndex, uint32_t start, uint32_t size)
{
    int ret = 0;
    drm_zocl_set_cu_range range = {ipIndex, start, size};

    ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_SET_CU_READONLY_RANGE, &range);
    return ret ? -errno : ret;
}

int
shim::
xclOpenIPInterruptNotify(uint32_t ipIndex, unsigned int flags)
{
  int ret;

  drm_zocl_ctx ctx = {0};
  ctx.cu_index = ipIndex;
  ctx.flags = flags;
  ctx.op = ZOCL_CTX_OP_OPEN_GCU_FD;

  xclLog(XRT_DEBUG, "%s: IP index %d, flags 0x%x", __func__, ipIndex, flags);
  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  return (ret < 0) ? -errno : ret;
}

int
shim::
xclCloseIPInterruptNotify(int fd)
{
  xclLog(XRT_DEBUG, "%s: fd %d", __func__, fd);
  close(fd);
  return 0;
}

size_t
shim::
xclDebugReadCheckers(xdp::LAPCCounterResults* aCheckerResults)
{
  size_t size = 0;

  uint64_t statusRegisters[] = {
    xdp::IP::LAPC::AXI_LITE::STATUS,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_0,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_1,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_2,
    xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_3,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_0,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_1,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_2,
    xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_3
  };

  uint64_t baseAddress[xdp::MAX_NUM_LAPCS];
  uint32_t numSlots = getIPCountAddrNames(LAPC, baseAddress, nullptr, nullptr, nullptr, nullptr, xdp::MAX_NUM_LAPCS);
  uint32_t temp[xdp::IP::LAPC::NUM_COUNTERS];
  aCheckerResults->NumSlots = numSlots;
  snprintf(aCheckerResults->DevUserName, 256, "%s", " ");
  for (uint32_t s = 0; s < numSlots; ++s) {
    for (int c=0; c < xdp::IP::LAPC::NUM_COUNTERS; c++)
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER, baseAddress[s]+statusRegisters[c], &temp[c], 4);

    aCheckerResults->OverallStatus[s]      = temp[xdp::IP::LAPC::sysfs::STATUS];
    std::copy(temp+xdp::IP::LAPC::sysfs::CUMULATIVE_STATUS_0, temp+xdp::IP::LAPC::sysfs::SNAPSHOT_STATUS_0, aCheckerResults->CumulativeStatus[s]);
    std::copy(temp+xdp::IP::LAPC::sysfs::SNAPSHOT_STATUS_0, temp+xdp::IP::LAPC::NUM_COUNTERS, aCheckerResults->SnapshotStatus[s]);
  }

  return size;
}

size_t
shim::
xclDebugReadCounters(xdp::AIMCounterResults* aCounterResults)
{
  size_t size = 0;

  uint64_t aim_offsets[] = {
    xdp::IP::AIM::AXI_LITE::WRITE_BYTES,
    xdp::IP::AIM::AXI_LITE::WRITE_TRANX,
    xdp::IP::AIM::AXI_LITE::READ_BYTES,
    xdp::IP::AIM::AXI_LITE::READ_TRANX,
    xdp::IP::AIM::AXI_LITE::OUTSTANDING_COUNTS,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_ADDRESS,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_DATA,
    xdp::IP::AIM::AXI_LITE::LAST_READ_ADDRESS,
    xdp::IP::AIM::AXI_LITE::LAST_READ_DATA
  };

  uint64_t aim_upper_offsets[] = {
    xdp::IP::AIM::AXI_LITE::WRITE_BYTES_UPPER,
    xdp::IP::AIM::AXI_LITE::WRITE_TRANX_UPPER,
    xdp::IP::AIM::AXI_LITE::READ_BYTES_UPPER,
    xdp::IP::AIM::AXI_LITE::READ_TRANX_UPPER,
    xdp::IP::AIM::AXI_LITE::OUTSTANDING_COUNTS_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_ADDRESS_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_WRITE_DATA_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_READ_ADDRESS_UPPER,
    xdp::IP::AIM::AXI_LITE::LAST_READ_DATA_UPPER
  };

  // Read all metric counters
  uint64_t baseAddress[xdp::MAX_NUM_AIMS];
  uint8_t mPerfmonProperties[xdp::MAX_NUM_AIMS] = {} ;
  uint32_t numSlots = getIPCountAddrNames(AXI_MM_MONITOR, baseAddress, nullptr, mPerfmonProperties, nullptr, nullptr, xdp::MAX_NUM_AIMS);

  uint32_t temp[xdp::IP::AIM::NUM_COUNTERS_REPORT];

  aCounterResults->NumSlots = numSlots;
  snprintf(aCounterResults->DevUserName, 256, "%s", " ");
  for (uint32_t s=0; s < numSlots; s++) {
    uint32_t sampleInterval;
    // Read sample interval register to latch the sampled metric counters
    size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress[s] + xdp::IP::AIM::AXI_LITE::SAMPLE,
                    &sampleInterval, 4);

    // If applicable, read the upper 32-bits of the 64-bit debug counters
    if (mPerfmonProperties[s] & xdp::IP::AIM::mask::PROPERTY_64BIT) {
      for (int c = 0; c < xdp::IP::AIM::NUM_COUNTERS_REPORT; ++c) {
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                baseAddress[s] + aim_upper_offsets[c],
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

    for (int c=0; c < xdp::IP::AIM::NUM_COUNTERS_REPORT; c++)
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+aim_offsets[c], &temp[c], 4);

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
xclDebugReadAccelMonitorCounters(xdp::AMCounterResults* samResult)
{
  size_t size = 0;

  /*
    Here should read the version number
    and return immediately if version
    is not supported
  */

  uint64_t am_offsets[] = {
    xdp::IP::AM::AXI_LITE::EXECUTION_COUNT,
    xdp::IP::AM::AXI_LITE::EXECUTION_CYCLES,
    xdp::IP::AM::AXI_LITE::STALL_INT,
    xdp::IP::AM::AXI_LITE::STALL_STR,
    xdp::IP::AM::AXI_LITE::STALL_EXT,
    xdp::IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES,
    xdp::IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES,
    xdp::IP::AM::AXI_LITE::TOTAL_CU_START
  };

  uint64_t am_upper_offsets[] = {
    xdp::IP::AM::AXI_LITE::EXECUTION_COUNT_UPPER,
    xdp::IP::AM::AXI_LITE::EXECUTION_CYCLES_UPPER,
    xdp::IP::AM::AXI_LITE::STALL_INT_UPPER,
    xdp::IP::AM::AXI_LITE::STALL_STR_UPPER,
    xdp::IP::AM::AXI_LITE::STALL_EXT_UPPER,
    xdp::IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES_UPPER,
    xdp::IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES_UPPER,
    xdp::IP::AM::AXI_LITE::TOTAL_CU_START_UPPER
  };

  // Read all metric counters
  uint64_t baseAddress[xdp::MAX_NUM_AMS] = {0};
  uint8_t  accelmonProperties[xdp::MAX_NUM_AMS] = {0};
  uint8_t  accelmonMajorVersions[xdp::MAX_NUM_AMS] = {0};
  uint8_t  accelmonMinorVersions[xdp::MAX_NUM_AMS] = {0};

  uint32_t numSlots = getIPCountAddrNames(ACCEL_MONITOR, baseAddress, nullptr, accelmonProperties,
                                          accelmonMajorVersions, accelmonMinorVersions, xdp::MAX_NUM_AMS);

  uint32_t temp[xdp::IP::AM::NUM_COUNTERS_REPORT] = {0};

  samResult->NumSlots = numSlots;
  snprintf(samResult->DevUserName, 256, "%s", " ");
  for (uint32_t s=0; s < numSlots; s++) {
    uint32_t sampleInterval;
    // Read sample interval register to latch the sampled metric counters
    size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress[s] + xdp::IP::AM::AXI_LITE::SAMPLE,
                    &sampleInterval, 4);

    bool hasDataflow = (cmpMonVersions(accelmonMajorVersions[s],accelmonMinorVersions[s],1,1) < 0) ? true : false;

    // If applicable, read the upper 32-bits of the 64-bit debug counters
    if (accelmonProperties[s] & xdp::IP::AM::mask::PROPERTY_64BIT) {
      for (int c = 0; c < xdp::IP::AM::NUM_COUNTERS_REPORT; ++c) {
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                baseAddress[s] + am_upper_offsets[c],
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
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::BUSY_CYCLES_UPPER, &dfTmp[0], 4);
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::MAX_PARALLEL_ITER_UPPER, &dfTmp[1], 4);

        samResult->CuBusyCycles[s]      = dfTmp[0] << 32;
        samResult->CuMaxParallelIter[s] = dfTmp[1] << 32;
      }
    }

    for (int c=0; c < xdp::IP::AM::NUM_COUNTERS_REPORT; c++)
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+am_offsets[c], &temp[c], 4);

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
      xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::BUSY_CYCLES, &dfTmp[0], 4);
      xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::MAX_PARALLEL_ITER, &dfTmp[1], 4);

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
xclDebugReadStreamingCounters(xdp::ASMCounterResults* aCounterResults)
{
  size_t size = 0; // The amount of data read from the hardware

  // Get the base addresses of all the ASM IPs in the debug IP layout
  uint64_t baseAddress[xdp::MAX_NUM_ASMS];
  uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_MONITOR,
                                          baseAddress,
                                          nullptr, nullptr, nullptr, nullptr,
                                          xdp::MAX_NUM_ASMS);

  // Fill up the portions of the return struct that are known by the runtime
  aCounterResults->NumSlots = numSlots ;
  snprintf(aCounterResults->DevUserName, 256, "%s", " ");

  // Fill up the return structure with the values read from the hardware
  uint64_t asm_offsets[] = {
    xdp::IP::ASM::AXI_LITE::NUM_TRANX,
    xdp::IP::ASM::AXI_LITE::DATA_BYTES,
    xdp::IP::ASM::AXI_LITE::BUSY_CYCLES,
    xdp::IP::ASM::AXI_LITE::STALL_CYCLES,
    xdp::IP::ASM::AXI_LITE::STARVE_CYCLES
  };

  for (unsigned int i = 0 ; i < numSlots ; ++i)
    {
      uint32_t sampleInterval ;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress[i] + xdp::IP::ASM::AXI_LITE::SAMPLE,
                      &sampleInterval, sizeof(uint32_t));

      // Then read all the individual 64-bit counters
      unsigned long long int tmp[xdp::IP::ASM::NUM_COUNTERS] ;

      for (unsigned int j = 0 ; j < xdp::IP::ASM::NUM_COUNTERS; ++j)
        {
          size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                          baseAddress[i] + asm_offsets[j],
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
xclDebugReadStreamingCheckers(xdp::SPCCounterResults* aStreamingCheckerResults)
{
  size_t size = 0; // The amount of data read from the hardware

  // Get the base addresses of all the SPC IPs in the debug IP layout
  uint64_t baseAddress[xdp::MAX_NUM_SPCS];
  uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_PROTOCOL_CHECKER,
                                          baseAddress,
                                          nullptr, nullptr, nullptr, nullptr,
                                          xdp::MAX_NUM_SPCS);

  // Fill up the portions of the return struct that are known by the runtime
  aStreamingCheckerResults->NumSlots = numSlots ;
  snprintf(aStreamingCheckerResults->DevUserName, 256, "%s", " ");

  // Fill up the return structure with the values read from the hardware
  for (unsigned int i = 0 ; i < numSlots ; ++i) {
      uint32_t pc_asserted ;
      uint32_t current_pc ;
      uint32_t snapshot_pc ;

      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
                      baseAddress[i] + xdp::IP::SPC::AXI_LITE::PC_ASSERTED,
                      &pc_asserted, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
                      baseAddress[i] + xdp::IP::SPC::AXI_LITE::CURRENT_PC,
                      &current_pc, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
                      baseAddress[i] + xdp::IP::SPC::AXI_LITE::SNAPSHOT_PC,
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
  return ret ? -errno : ret;
}

int
shim::
xclErrorClear()
{
  int ret;
  drm_zocl_error_inject ecmd = {ZOCL_ERROR_OP_CLEAR_ALL};

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_ERROR_INJECT, &ecmd);
  return ret ? -errno : ret;
}

int
shim::
resetDevice(xclResetKind kind)
{
  std::string errmsg{""};

  if (kind == XCL_USER_RESET) {
    mDev->sysfs_put("zocl_reset", errmsg, "1\n");
    if (!errmsg.empty())
      throw std::runtime_error("Failed to reset zocl, err : " + errmsg + "\n");
  }
  else
    throw std::runtime_error("Invalid reset type\n"); // other kinds of reset are not supported

  return 0;
}

#ifdef XRT_ENABLE_AIE

std::shared_ptr<zynqaie::aie_array>
shim::
get_aie_array_shared()
{
  return m_aie_array;
}

zynqaie::aie_array*
shim::
getAieArray()
{
  return m_aie_array.get();
}

zynqaie::aied*
shim::
getAied()
{
  return aied.get();
}

void
shim::
registerAieArray()
{
  if(!m_aie_array)
      m_aie_array = std::make_shared<zynqaie::aie_array>(mCoreDevice);

  aied = std::make_unique<zynqaie::aied>(mCoreDevice.get());
}

void
shim::
reset_aie_array()
{
  m_aie_array.reset();
}

bool
shim::
isAieRegistered()
{
  return (m_aie_array != nullptr);
}

int
shim::
getPartitionFd(drm_zocl_aie_fd &aiefd)
{
  return ioctl(mKernelFD, DRM_IOCTL_ZOCL_AIE_FD, &aiefd) ? -errno : 0;
}

int
shim::
resetAIEArray(drm_zocl_aie_reset &reset)
{
  auto ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_AIE_RESET, &reset) ? -errno : 0;
  reset_aie_array();
  return ret;
}

int
shim::
openGraphContext(const uuid_t xclbinId, unsigned int graphId, xrt::graph::access_mode am)
{
  unsigned int flags;
  int ret;

  switch (am) {

  case xrt::graph::access_mode::exclusive:
    flags = ZOCL_CTX_EXCLUSIVE;
    break;

  case xrt::graph::access_mode::primary:
    flags = ZOCL_CTX_PRIMARY;
    break;

  case xrt::graph::access_mode::shared:
    flags = ZOCL_CTX_SHARED;
    break;

  default:
    return -EINVAL;
  }

  drm_zocl_ctx ctx = {0};
  ctx.uuid_ptr = reinterpret_cast<uint64_t>(xclbinId);
  ctx.uuid_size = sizeof (uuid_t) * sizeof (char);
  ctx.graph_id = graphId;
  ctx.flags = flags;
  ctx.op = ZOCL_CTX_OP_ALLOC_GRAPH_CTX;

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  return ret ? -errno : ret;
}

int
shim::
closeGraphContext(unsigned int graphId)
{
  int ret;

  drm_zocl_ctx ctx = {0};
  ctx.graph_id = graphId;
  ctx.op = ZOCL_CTX_OP_FREE_GRAPH_CTX;

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  return ret ? -errno : ret;
}

void
shim::
open_graph_context(const zynqaie::hwctx_object* hwctx, const uuid_t xclbinId, unsigned int graph_id, xrt::graph::access_mode am)
{
  if (!hw_context_enable){
    // for legacy flow
    if (openGraphContext(xclbinId, graph_id, am))
      throw xrt_core::error("Failed to open graph context");

    return;
  }
  // this is for multi slot case
  auto shared = (hwctx->get_mode() != xrt::hw_context::access_mode::exclusive);
  unsigned int flags = shared ? ZOCL_CTX_SHARED : ZOCL_CTX_EXCLUSIVE;
  drm_zocl_open_graph_ctx graph_ctx = {};
  graph_ctx.hw_context = hwctx->get_slotidx();
  graph_ctx.flags = flags;
  graph_ctx.graph_id = graph_id;
  if (ioctl(mKernelFD, DRM_IOCTL_ZOCL_OPEN_GRAPH_CTX, &graph_ctx))
    throw xrt_core::error("Failed to open graph context");
}

void
shim::
close_graph_context(const zynqaie::hwctx_object* hwctx, unsigned int graph_id)
{
  if (!hw_context_enable) {
    // for legacy flow
    if (closeGraphContext(graph_id))
      throw xrt_core::error("Failed to close graph context");

    return;
  }
  // this is for multi slot case
  drm_zocl_close_graph_ctx graph_ctx = {};
  graph_ctx.hw_context = hwctx->get_slotidx();
  graph_ctx.graph_id = graph_id;
  if (ioctl(mKernelFD, DRM_IOCTL_ZOCL_CLOSE_GRAPH_CTX, &graph_ctx))
    throw xrt_core::error("Failed to close graph context");
}

int
shim::
openAIEContext(xrt::aie::access_mode am)
{
  unsigned int flags;
  int ret;

  switch (am) {

  case xrt::aie::access_mode::exclusive:
    flags = ZOCL_CTX_EXCLUSIVE;
    break;

  case xrt::aie::access_mode::primary:
    flags = ZOCL_CTX_PRIMARY;
    break;

  case xrt::aie::access_mode::shared:
    flags = ZOCL_CTX_SHARED;
    break;

  default:
    return -EINVAL;
  }

  drm_zocl_ctx ctx = {0};
  ctx.flags = flags;
  ctx.op = ZOCL_CTX_OP_ALLOC_AIE_CTX;

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_CTX, &ctx);
  return ret ? -errno : ret;
}

xrt::aie::access_mode
shim::
getAIEAccessMode()
{
  return access_mode;
}

void
shim::
setAIEAccessMode(xrt::aie::access_mode am)
{
  access_mode = am;
}

#endif

} // end namespace ZYNQ

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
  return shim->create_hw_context(handle, xclbin_uuid, cfg_param, mode);
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

// Function for converting raw buffer handle returned by some of the
// shim functions into xrt_core::buffer_handle
std::unique_ptr<xrt_core::buffer_handle>
get_buffer_handle(xclDeviceHandle handle, unsigned int bhdl)
{
  auto shim = get_shim_object(handle);
  return std::make_unique<ZYNQ::shim::buffer_object>(shim, bhdl);
}
} // xrt::shim_int
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// Implementation of user exposed SHIM APIs
// This are C level functions
////////////////////////////////////////////////////////////////
void
xclClose(xclDeviceHandle handle)
{
  xdp::hal::profiling_wrapper("xclClose", [handle] {
  //std::cout << "xclClose called" << std::endl;
  if (ZYNQ::shim::handleCheck(handle)) {
    delete ((ZYNQ::shim *) handle);
  }
  }) ;
}

unsigned int
xclAllocBO(xclDeviceHandle handle, size_t size, int, unsigned flags)
{
  return xdp::hal::profiling_wrapper("xclAllocBO", [handle, size, flags] {
    try {
      auto shim = ZYNQ::shim::handleCheck(handle);
      if (!shim)
        return static_cast<unsigned int>(-EINVAL);

        auto bo = shim->xclAllocBO(size, flags);
        auto ptr = static_cast<ZYNQ::shim::buffer_object*>(bo.get());
        return ptr->detach_handle();
      }
      catch (const xrt_core::error& ex) {
        xrt_core::send_exception_message(ex.what());
        return static_cast<unsigned int>(ex.get_code());
      }
    });
}

unsigned int
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  return xdp::hal::profiling_wrapper("xclAllocUserPtrBO",
  [handle, userptr, size, flags] {
    try {
      auto shim = ZYNQ::shim::handleCheck(handle);
      if (!shim)
        return static_cast<unsigned int>(-EINVAL);

      auto bo = shim->xclAllocUserPtrBO(userptr, size, flags);
      auto ptr = static_cast<ZYNQ::shim::buffer_object*>(bo.get());
      return ptr->detach_handle();
    }
    catch (const xrt_core::error& ex) {
      xrt_core::send_exception_message(ex.what());
      return static_cast<unsigned int>(ex.get_code());
    }
  });
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
  xdp::hal::profiling_wrapper("xclFreeBO", [handle, boHandle] {
  //std::cout << "xclFreeBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return;
  drv->xclFreeBO(boHandle);
  }) ;
}

size_t
xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src,
           size_t size, size_t seek)
{
  return xdp::hal::buffer_transfer_profiling_wrapper("xclWriteBO", size, true,
  [handle, boHandle, src, size, seek] {

  //std::cout << "xclWriteBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclWriteBO(boHandle, src, size, seek);
  }) ;
}

size_t
xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst,
          size_t size, size_t skip)
{
  return xdp::hal::buffer_transfer_profiling_wrapper("xclReadBO", size, false,
  [handle, boHandle, dst, size, skip] {

  //std::cout << "xclReadBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclReadBO(boHandle, dst, size, skip);
  }) ;
}

void *
xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  return xdp::hal::profiling_wrapper("xclMapBO", [handle, boHandle, write] {
  //std::cout << "xclMapBO called" << std::endl;
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return static_cast<void*>(nullptr);
  return drv->xclMapBO(boHandle, write);
  } );
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
  return xdp::hal::buffer_transfer_profiling_wrapper("xclSyncBO", size,
						     (dir == XCL_BO_SYNC_BO_TO_DEVICE),
  [handle, boHandle, dir, size, offset] {

  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclSyncBO(boHandle, dir, size, offset);
  }) ;
}

int
xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle,
          unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
  return xdp::hal::profiling_wrapper("xclCopyBO",
  [handle, dst_boHandle, src_boHandle, size, dst_offset, src_offset] {

  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset);
  }) ;
}

int
xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
  try {
    auto shim = ZYNQ::shim::handleCheck(handle);
    if (!shim)
      return -EINVAL;

    auto shared = shim->xclExportBO(boHandle);
    auto ptr = static_cast<ZYNQ::shim::shared_object*>(shared.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get_code();
  }
}

unsigned int
xclImportBO(xclDeviceHandle handle, int fd, unsigned flags)
{
  try {
    auto shim = ZYNQ::shim::handleCheck(handle);
    if (!shim)
      return static_cast<unsigned int>(-EINVAL); // argh ...

    auto bo = shim->xclImportBO(fd, flags);
    auto ptr = static_cast<ZYNQ::shim::buffer_object*>(bo.get());
    return ptr->detach_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return static_cast<unsigned int>(ex.get_code());
  }
}

static int
xclLoadXclBinImpl(xclDeviceHandle handle, const xclBin *buffer, bool meta)
{
  return xdp::hal::profiling_wrapper("xclLoadXclbin", [handle, buffer, meta] {

  try {
    bool checkDrmFD = xrt_core::config::get_enable_flat() ? false : true;
    ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle, checkDrmFD);

    // Retrieve any profiling information still on this device from any previous
    // configuration before the device is reconfigured with the new xclbin (when
    // profiling is enabled).
    xdp::flush_device(handle);

    int ret;
    if (!meta) {
      ret = drv ? drv->xclLoadXclBin(buffer) : -ENODEV;
      if (ret) {
        printf("Load Xclbin Failed\n");

        return ret;
      }
    }
    auto core_device = xrt_core::get_userpf_device(handle);

    core_device->register_axlf(buffer);

#ifdef XRT_ENABLE_AIE
    auto data = core_device->get_axlf_section(AIE_METADATA);
    if (data.first && data.second)
      drv->registerAieArray();
#endif

    /* If PDI is the only section, return here */
    if (xrt_core::xclbin::is_aie_only(buffer)) {
        // Update the profiling library with the information on this new AIE xclbin
        // configuration on this device as appropriate (when profiling is enabled).
      if (!drv->get_hw_context_enable())
        xdp::update_device(handle, false);

#ifndef __HWEM__
        // Setup the user-accessible HAL API profiling interface so user host
        // code can call functions to directly read counter values on profiling IP
        // (if enabled in the xrt.ini).
        START_DEVICE_PROFILING_CB(handle);
#endif

        return 0;
    }

    // Skipping if only loading xclbin metadata
    if (!meta) {
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
    }

    // Update the profiling library with the information on this new AIE xclbin
    // configuration on this device as appropriate (when profiling is enabled).
    if (!drv->get_hw_context_enable())
      xdp::update_device(handle, false);
#ifndef __HWEM__
    // Setup the user-accessible HAL API profiling interface so user host
    // code can call functions to directly read counter values on profiling IP
    // (if enabled in the xrt.ini).
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

  }) ;
}

int
xclLoadXclBinMeta(xclDeviceHandle handle, const xclBin *buffer)
{
  return xclLoadXclBinImpl(handle, buffer, true);
}

int
xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  return xclLoadXclBinImpl(handle, buffer, false);
}

size_t
xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  return xdp::hal::profiling_wrapper("xclWrite",
  [handle, space, offset, hostBuf, size] {
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return static_cast<size_t>(-EINVAL);
  return drv->xclWrite(space, offset, hostBuf, size);
  }) ;
}

size_t
xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  return xdp::hal::profiling_wrapper("xclRead",
  [handle, space, offset, hostBuf, size] {
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return static_cast<size_t>(-EINVAL);
  return drv->xclRead(space, offset, hostBuf, size);
  }) ;
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
  return xdp::hal::profiling_wrapper("xclGetBOProperties",
  [handle, boHandle, properties] {
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return static_cast<int>(drv->xclGetBOProperties(boHandle, properties));
  }) ;
}

unsigned int
xclVersion ()
{
  return 2;
}

int
xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  return xdp::hal::profiling_wrapper("xclExecBuf", [handle, cmdBO] {
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExecBuf(cmdBO) ;
  }) ;
}

int
xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  return xdp::hal::profiling_wrapper("xclExecWait", [handle, timeoutMilliSec] {

  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclExecWait(timeoutMilliSec);
  }) ;
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

double
xclGetHostReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclGetHostReadMaxBandwidthMBps() : 0.0;
}


double
xclGetHostWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclGetHostWriteMaxBandwidthMBps() : 0.0;
}


double
xclGetKernelReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclGetKernelReadMaxBandwidthMBps() : 0.0;
}


double
xclGetKernelWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclGetKernelWriteMaxBandwidthMBps() : 0.0;
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
xclAIEGetCmd(xclDeviceHandle handle, xclAIECmd *cmd)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAIEGetCmd(cmd);
}

int
xclAIEPutCmd(xclDeviceHandle handle, xclAIECmd *cmd)
{
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  if (!drv)
    return -EINVAL;
  return drv->xclAIEPutCmd(cmd);
}

int
xclSKCreate(xclDeviceHandle handle, int *boHandle, uint32_t cu_idx)
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
  return xdp::hal::profiling_wrapper("xclOpenContext",
  [handle, xclbinId, ipIndex, shared] {
    try {
      ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
      return drv ? drv->xclOpenContext(xclbinId, ipIndex, shared) : -EINVAL;
    }
    catch (const xrt_core::error& ex) {
      xrt_core::send_exception_message(ex.what());
      return ex.get_code();
    }
  }) ;
}

int
xclCloseContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned ipIndex)
{
  return xdp::hal::profiling_wrapper("xclCloseContext",
  [handle, xclbinId, ipIndex] {
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->xclCloseContext(xclbinId, ipIndex) : -EINVAL;
  }) ;
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
    return drv->xclDebugReadCheckers(reinterpret_cast<xdp::LAPCCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_AIM:
    return drv->xclDebugReadCounters(reinterpret_cast<xdp::AIMCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_AM:
    return drv->xclDebugReadAccelMonitorCounters(reinterpret_cast<xdp::AMCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_ASM:
    return drv->xclDebugReadStreamingCounters(reinterpret_cast<xdp::ASMCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_SPC:
    return drv->xclDebugReadStreamingCheckers(reinterpret_cast<xdp::SPCCounterResults*>(debugResults));
  default:
    ;
  }
  return -1 ;
}

int
_xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
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
  return xdp::hal::profiling_wrapper("xclLockDevice", [] {
  return 0;
  }) ;
}

int
xclUnlockDevice(xclDeviceHandle handle)
{
  return xdp::hal::profiling_wrapper("xclUnlockDevice", [] {
  return 0;
  }) ;
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
              size_t count, uint64_t offset)
{
  return xdp::hal::profiling_wrapper("xclUnmgdPread", [] {
  return -ENOSYS;
  }) ;
}

ssize_t
xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf,
               size_t count, uint64_t offset)
{
  return xdp::hal::profiling_wrapper("xclUnmgdPwrite", [] {
  return -ENOSYS;
  }) ;
}

int
xclRegisterInterruptNotify(xclDeviceHandle handle, unsigned int userInterrupt, int fd)
{
  return 0;
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
  try {
    ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
    return (drv) ? drv->xclIPName2Index(name) : -ENODEV;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -ENOENT;
  }
}

int
xclIPSetReadRange(xclDeviceHandle handle, uint32_t ipIndex, uint32_t start, uint32_t size)
{
    ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
    return (drv) ? drv->xclIPSetReadRange(ipIndex, start, size) : -ENODEV;
}

int
xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag,
          const char* format, ...)
{
  static auto verbosity = xrt_core::config::get_verbosity();
  if (level > verbosity)
    return 0;

  va_list args;
  va_start(args, format);
  xrt_core::message::sendv(static_cast<xrt_core::message::severity_level>(level), tag, format, args);
  va_end(args);

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
xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t force)
{
  return -ENOSYS;
}

int
xclUpdateSchedulerStat(xclDeviceHandle handle)
{
  return 1; // -ENOSYS;
}

int
xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  return xclInternalResetDevice(handle, kind);
}

int
xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  // NOTE: until xclResetDevice is made completely internal,
  // this wrapper is being used to limit the pragma use to this file
  ZYNQ::shim *drv = ZYNQ::shim::handleCheck(handle);
  return drv ? drv->resetDevice(kind) : -ENODEV;
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
