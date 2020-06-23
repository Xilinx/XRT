/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef _ZYNQ_SHIM_H_
#define _ZYNQ_SHIM_H_

#include "zynq_dev.h"
#include "core/edge/include/xclhal2_mpsoc.h"
#include "core/edge/include/zynq_ioctl.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/bo_cache.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xcl_app_debug.h"
#include <cstdint>
#include <fstream>
#include <map>
#include <vector>
#include <mutex>
#include <memory>

#ifdef XRT_ENABLE_AIE
#include "core/edge/user/aie/aie.h"
#endif

namespace ZYNQ {

class shim {

  static const int BUFFER_ALIGNMENT = 0x80; // TODO: UKP
public:
  ~shim();
  shim(unsigned index, const char *logfileName,
           xclVerbosityLevel verbosity);

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
  int xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex);

  int xclSKGetCmd(xclSKCmd *cmd);
  int xclSKCreate(unsigned int boHandle, uint32_t cu_idx);
  int xclSKReport(uint32_t cu_idx, xrt_scu_state state);

  double xclGetDeviceClockFreqMHz();

  uint xclGetNumLiveProcesses();

  std::string xclGetSysfsPath(const std::string& entry);

  int xclGetDebugIPlayoutPath(char* layoutPath, size_t size);
  int xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
  int xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);

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
  static shim *handleCheck(void *handle);
  int xclIPName2Index(const char *name);
  static int xclLogMsg(xrtLogMsgLevel level, const char* tag,
		       const char* format, va_list args);

  // Application debug path functionality for xbutil
  size_t xclDebugReadCheckers(xclDebugCheckersResults* aCheckerResults);
  size_t xclDebugReadCounters(xclDebugCountersResults* aCounterResults);
  size_t xclDebugReadAccelMonitorCounters(xclAccelMonitorCounterResults* samResult);
  size_t xclDebugReadStreamingCounters(xclStreamingDebugCountersResults* aCounterResults);
  size_t xclDebugReadStreamingCheckers(xclDebugStreamingCheckersResults* aStreamingCheckerResults);
  uint32_t getIPCountAddrNames(int type, uint64_t* baseAddress,
                              std::string* portNames,
                              uint8_t* properties, uint8_t* majorVersions,
                              uint8_t* minorVersions, size_t size) ;
  int cmpMonVersions(unsigned int major1, unsigned int minor1,
		     unsigned int major2, unsigned int minor2);

#ifdef XRT_ENABLE_AIE
  zynqaie::Aie *getAieArray();
  void setAieArray(zynqaie::Aie *aie);
  int getBOInfo(unsigned bo, drm_zocl_info_bo &info);
#endif

private:
  std::shared_ptr<xrt_core::device> mCoreDevice;
  const int mBoardNumber = -1;
  std::ofstream mLogStream;
  std::ifstream mVBNV;
  xclVerbosityLevel mVerbosity;
  int mKernelFD;
  std::map<uint64_t, uint32_t *> mKernelControl;
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
  int xclLog(xrtLogMsgLevel level, const char* tag, const char* format, ...);

#ifdef XRT_ENABLE_AIE
  zynqaie::Aie *aieArray;
#endif
};

} // namespace ZYNQ

#endif
