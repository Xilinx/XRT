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

#ifndef _ZYNQ_SHIM_H_
#define _ZYNQ_SHIM_H_

#include "core/edge/include/xclhal2_mpsoc.h"
#include "core/edge/include/zynq_ioctl.h"
#include <cstdint>
#include <fstream>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include "core/common/bo_cache.h"

namespace ZYNQ {

// Forward declaration
class ZYNQShimProfiling ;

class ZYNQShim {

  static const int BUFFER_ALIGNMENT = 0x80; // TODO: UKP
public:
  ~ZYNQShim();
  ZYNQShim(unsigned index, const char *logfileName,
           xclVerbosityLevel verbosity);

  // The entry of profiling functions
  std::unique_ptr<ZYNQShimProfiling> profiling;

  int mapKernelControl(const std::vector<std::pair<uint64_t, size_t>>& offsets);
  void *getVirtAddressOfApture(xclAddressSpace space, const uint64_t phy_addr, uint64_t& offset);

  // Raw read/write
  size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf,
                  size_t size);
  size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf,
                 size_t size);
  // Restricted read/write on IP register space
  int xclRegWrite(uint32_t cu_index, uint32_t offset, uint32_t data);
  int xclRegRead(uint32_t cu_index, uint32_t offset, uint32_t *datap);

  unsigned int xclAllocBO(size_t size, int unused, unsigned flags);
  unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
  unsigned int xclGetHostBO(uint64_t paddr, size_t size);
  void xclFreeBO(unsigned int boHandle);
  int xclGetBOInfo(uint64_t handle);
  int xclWriteBO(unsigned int boHandle, const void *src, size_t size,
                 size_t seek);
  int xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
  void *xclMapBO(unsigned int boHandle, bool write);
  int xclExportBO(unsigned int boHandle);
  unsigned int xclImportBO(int fd, unsigned flags);
  unsigned int xclGetBOProperties(unsigned int boHandle,
                                  xclBOProperties *properties);
  int xclExecBuf(unsigned int cmdBO);
  int xclExecWait(int timeoutMilliSec);
  int xclSKGetCmd(xclSKCmd *cmd);
  int xclSKCreate(unsigned int boHandle, uint32_t cu_idx);
  int xclSKReport(uint32_t cu_idx, xrt_scu_state state);

  uint xclGetNumLiveProcesses();

  int xclGetSysfsPath(const char *subdev, const char *entry, char *sysfPath,
                      size_t size);

  // Bitstream/bin download
  int xclLoadXclBin(const xclBin *buffer);
  int xclLoadAxlf(const axlf *buffer);

  int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size,
                size_t offset);
  int xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                size_t dst_offset, size_t src_offset);

  int xclGetDeviceInfo2(xclDeviceInfo2 *info);

  void xclWriteHostEvent(xclPerfMonEventType type, xclPerfMonEventID id);

  bool isGood() const;
  static ZYNQShim *handleCheck(void *handle);

private:
  const int mBoardNumber = -1;
  std::ofstream mLogStream;
  std::ifstream mVBNV;
  xclVerbosityLevel mVerbosity;
  int mKernelFD;
  std::map<uint64_t, uint32_t *> mKernelControl;
  std::unique_ptr<xrt_core::bo_cache> mCmdBOCache;

  /*
   * Mapped CU register space for xclRegRead/Write(). We support at most
   * 128 CUs and each map is of 64k bytes. Does not support debug IP access.
   */
  std::vector<uint32_t*> mCuMaps;
  const size_t mCuMapSize = 64 * 1024;
  std::mutex mCuMapLock;
  int xclRegRW(bool rd, uint32_t cu_index, uint32_t offset, uint32_t *datap);
};

} // namespace ZYNQ

#endif
