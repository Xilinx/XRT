// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _ZYNQ_SHIM_H_
#define _ZYNQ_SHIM_H_

#include "zynq_dev.h"

#include "core/edge/include/xclhal2_mpsoc.h"
#include "core/edge/include/zynq_ioctl.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/bo_cache.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xdp/app_debug.h"
#include "core/include/experimental/xrt_hw_context.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <vector>
#include <mutex>
#include <memory>

#ifdef XRT_ENABLE_AIE
#include "core/edge/user/aie/aie.h"
#include "core/edge/user/aie/aied.h"
#endif

namespace ZYNQ {

class shim {

  static const int BUFFER_ALIGNMENT = 0x80; // TODO: UKP
public:
  ~shim();
  shim(unsigned index);

  int mapKernelControl(const std::vector<std::pair<uint64_t, size_t>>& offsets);
  void *getVirtAddressOfApture(xclAddressSpace space, const uint64_t phy_addr, uint64_t& offset);

  // Raw read/write
  size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf,
                  size_t size);
  size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf,
                 size_t size);
  // Restricted read/write on IP register space
  int xclRegWrite(uint32_t ipIndex, uint32_t offset, uint32_t data);
  int xclRegRead(uint32_t ipIndex, uint32_t offset, uint32_t *datap);

  unsigned int xclAllocBO(size_t size, int unused, unsigned flags);
  unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
  unsigned int xclGetHostBO(uint64_t paddr, size_t size);
  void xclFreeBO(unsigned int boHandle);
  int xclWriteBO(unsigned int boHandle, const void *src, size_t size,
                 size_t seek);
  int xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
  void *xclMapBO(unsigned int boHandle, bool write);
  int xclUnmapBO(unsigned int boHandle, void* addr);
  int xclExportBO(unsigned int boHandle);
  unsigned int xclImportBO(int fd, unsigned flags);
  unsigned int xclGetBOProperties(unsigned int boHandle,
                                  xclBOProperties *properties);
  int xclExecBuf(unsigned int cmdBO);
  int xclExecWait(int timeoutMilliSec);

  int xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared);
  // aka xclOpenContextByName()
  xrt_core::cuidx_type open_cu_context(const xrt::hw_context& hwctx, const std::string& cuname);
  int xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex);

  int xclSKGetCmd(xclSKCmd *cmd);
  int xclSKCreate(int *boHandle, uint32_t cu_idx);
  int xclSKReport(uint32_t cu_idx, xrt_scu_state state);

  int xclAIEGetCmd(xclAIECmd *cmd);
  int xclAIEPutCmd(xclAIECmd *cmd);

  double xclGetDeviceClockFreqMHz();

  uint xclGetNumLiveProcesses();

  std::string xclGetSysfsPath(const std::string& entry);

  int xclGetDebugIPlayoutPath(char* layoutPath, size_t size);
  int xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
  int xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);

  double xclGetHostReadMaxBandwidthMBps();
  double xclGetHostWriteMaxBandwidthMBps();
  double xclGetKernelReadMaxBandwidthMBps();
  double xclGetKernelWriteMaxBandwidthMBps();

  // Bitstream/bin download
  int xclLoadXclBin(const xclBin *buffer);
  int xclLoadAxlf(const axlf *buffer);

  int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size,
                size_t offset);
  int xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                size_t dst_offset, size_t src_offset);

  int xclGetDeviceInfo2(xclDeviceInfo2 *info);

  int xclOpenIPInterruptNotify(uint32_t ipIndex, unsigned int flags);
  int xclCloseIPInterruptNotify(int fd);

  bool isGood() const;
  static shim *handleCheck(void *handle, bool checkDrmFd = true);
  int xclIPName2Index(const char *name);

  // Application debug path functionality for xbutil
  size_t xclDebugReadCheckers(xdp::LAPCCounterResults* aCheckerResults);
  size_t xclDebugReadCounters(xdp::AIMCounterResults* aCounterResults);
  size_t xclDebugReadAccelMonitorCounters(xdp::AMCounterResults* samResult);
  size_t xclDebugReadStreamingCounters(xdp::ASMCounterResults* aCounterResults);
  size_t xclDebugReadStreamingCheckers(xdp::SPCCounterResults* aStreamingCheckerResults);
  uint32_t getIPCountAddrNames(int type, uint64_t* baseAddress,
                              std::string* portNames,
                              uint8_t* properties, uint8_t* majorVersions,
                              uint8_t* minorVersions, size_t size) ;
  int cmpMonVersions(unsigned int major1, unsigned int minor1,
		     unsigned int major2, unsigned int minor2);

  int xclErrorInject(uint16_t num, uint16_t driver, uint16_t  severity, uint16_t module, uint16_t eclass);
  int xclErrorClear();
  int secondXclbinLoadCheck(std::shared_ptr<xrt_core::device> core_dev, const axlf *top);

#ifdef XRT_ENABLE_AIE
  zynqaie::Aie* getAieArray();
  zynqaie::Aied* getAied();
  int getBOInfo(drm_zocl_info_bo &info);
  void registerAieArray();
  bool isAieRegistered();
  int getPartitionFd(drm_zocl_aie_fd &aiefd);
  int resetAIEArray(drm_zocl_aie_reset &reset);
  int openGraphContext(const uuid_t xclbinId, unsigned int graphId, xrt::graph::access_mode am);
  int closeGraphContext(unsigned int graphId);
  int openAIEContext(xrt::aie::access_mode am);
  xrt::aie::access_mode getAIEAccessMode();
  void setAIEAccessMode(xrt::aie::access_mode am);
#endif

private:
  std::shared_ptr<xrt_core::device> mCoreDevice;
  const int mBoardNumber = -1;
  std::ofstream mLogStream;
  std::ifstream mVBNV;
  int mKernelFD;
  static std::map<uint64_t, uint32_t *> mKernelControl;
  std::unique_ptr<xrt_core::bo_cache> mCmdBOCache;
  zynq_device *mDev = nullptr;
  size_t mKernelClockFreq;

  /*
   * Mapped CU register space for xclRegRead/Write(). We support at most
   * 128 CUs and each map is of 64k bytes. Does not support debug IP access.
   */
  std::vector<uint32_t*> mCuMaps;
  const size_t mCuMapSize = 64 * 1024;
  std::mutex mCuMapLock;
  int xclRegRW(bool rd, uint32_t cu_index, uint32_t offset, uint32_t *datap);

#ifdef XRT_ENABLE_AIE
  std::unique_ptr<zynqaie::Aie> aieArray;
  std::unique_ptr<zynqaie::Aied> aied;
  xrt::aie::access_mode access_mode = xrt::aie::access_mode::none;
#endif
};

} // namespace ZYNQ

#endif
