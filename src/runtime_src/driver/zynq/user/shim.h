/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#include "driver/zynq/include/xclhal2_mpsoc.h"
#include "driver/zynq/include/zynq_ioctl.h"
//#include "driver/include/xclperf.h"
//#include "driver/zynq/include/zynq_perfmon_params.h"
#include <cstdint>
#include <fstream>

namespace ZYNQ {

class ZYNQShim {

  static const int BUFFER_ALIGNMENT = 0x80; // TODO: UKP
public:
  ~ZYNQShim();
  ZYNQShim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);
  // Raw read/write
  size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
  size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
  unsigned int xclAllocBO(size_t size, xclBOKind domain, unsigned flags);
  unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
  unsigned int xclGetHostBO(uint64_t paddr, size_t size);
  void xclFreeBO(unsigned int boHandle);
  int xclGetBOInfo(uint64_t handle);
  int xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek);
  int xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
  void *xclMapBO(unsigned int boHandle, bool write);
  int xclExportBO(unsigned int boHandle);
  unsigned int xclImportBO(int fd, unsigned flags);
  unsigned int xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties);
  int xclExecBuf(unsigned int cmdBO);
  int xclExecWait(int timeoutMilliSec);

  int xclGetSysfsPath(const char* subdev, const char* entry, char* sysfPath, size_t size);

  // Bitstream/bin download
  int xclLoadXclBin(const xclBin *buffer);
	int xclLoadAxlf(const axlf *buffer);

  int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size,
                size_t offset);

  int xclGetDeviceInfo2(xclDeviceInfo2 *info);

  void xclWriteHostEvent(xclPerfMonEventType type, xclPerfMonEventID id);

  bool isGood() const;
  static ZYNQShim *handleCheck(void * handle);

private:
  const int mBoardNumber = -1;
  std::ofstream mLogStream;
  std::ifstream mVBNV;
  xclVerbosityLevel mVerbosity;
  int mKernelFD;
  uint32_t* mKernelControlPtr;
};

}
;

#endif
