// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
#ifndef PCIE_LINUX_SHIM_H_
#define PCIE_LINUX_SHIM_H_

#include "pcidev.h"
#include "xclhal2.h"

#include "core/common/device.h"
#include "core/common/system.h"
#include "core/common/xrt_profiling.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/hwqueue_handle.h"
#include "core/pcie/driver/linux/include/qdma_ioctl.h"
#include "core/pcie/driver/linux/include/xocl_ioctl.h"

#include "core/include/xdp/app_debug.h"
#include "core/include/xstream.h" /* for stream_opt_type */

#include <linux/aio_abi.h>
#include <libdrm/drm.h>

#include <cassert>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

// Forward declaration
namespace xrt_core {
    class bo_cache;
}

namespace xocl {

const uint64_t mNullAddr = 0xffffffffffffffffull;
const uint64_t mNullBO = 0xffffffff;

class shim
{
public:
  ~shim();
  shim(unsigned index);
  void init(unsigned index);

  // Raw unmanaged read/write on the entire PCIE user BAR
  size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
  size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
  // Restricted read/write on IP register space
  int xclRegWrite(uint32_t ipIndex, uint32_t offset, uint32_t data);
  int xclRegRead(uint32_t ipIndex, uint32_t offset, uint32_t *datap);

  std::unique_ptr<xrt_core::buffer_handle>
  xclAllocBO(size_t size, unsigned flags);

  std::unique_ptr<xrt_core::buffer_handle>
  xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);

  void xclFreeBO(unsigned int boHandle);
  int xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek);
  int xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
  void *xclMapBO(unsigned int boHandle, bool write);
  int xclUnmapBO(unsigned int boHandle, void* addr);
  int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset);
  int xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                size_t dst_offset, size_t src_offset);

  int xclUpdateSchedulerStat();

  std::unique_ptr<xrt_core::shared_handle>
  xclExportBO(unsigned int boHandle);

  std::unique_ptr<xrt_core::buffer_handle>
  xclImportBO(int fd, unsigned flags);

  int xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties);

  // Bitstream/bin download
  int xclLoadXclBin(const xclBin *buffer);
  int xclGetErrorStatus(xclErrorStatus *info);
  int xclGetDeviceInfo2(xclDeviceInfo2 *info);
  bool isGood() const;
  static shim *handleCheck(void * handle);
  int resetDevice(xclResetKind kind);
  int p2pEnable(bool enable, bool force);
  int cmaEnable(bool enable, uint64_t size);
  bool xclLockDevice();
  bool xclUnlockDevice();
  int xclReClock2(unsigned short region, const unsigned short *targetFreqMHz);
  int xclGetUsageInfo(xclDeviceUsage *info);

  int xclTestXSpi(int device_index);
  int xclBootFPGA();
  int xclRemoveAndScanFPGA();

  ssize_t xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset);
  ssize_t xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset);

  int xclGetSectionInfo(void *section_info, size_t *section_size, enum axlf_section_kind, int index);

  double xclGetDeviceClockFreqMHz();
  double xclGetHostReadMaxBandwidthMBps();
  double xclGetHostWriteMaxBandwidthMBps();
  double xclGetKernelReadMaxBandwidthMBps();
  double xclGetKernelWriteMaxBandwidthMBps();
  //debug related
  uint32_t getCheckerNumberSlots(int type);
  uint32_t getIPCountAddrNames(int type, uint64_t *baseAddress, std::string * portNames,
                               uint8_t *properties, uint8_t *majorVersions, uint8_t *minorVersions,
                               size_t size);
  size_t xclDebugReadCounters(xdp::AIMCounterResults* debugResult);
  size_t xclDebugReadCheckers(xdp::LAPCCounterResults* checkerResult);
  size_t xclDebugReadStreamingCounters(xdp::ASMCounterResults* streamingResult);
  size_t xclDebugReadStreamingCheckers(xdp::SPCCounterResults* streamingCheckerResult);
  size_t xclDebugReadAccelMonitorCounters(xdp::AMCounterResults* samResult);

  // APIs using sysfs information
  uint32_t xclGetNumLiveProcesses();
  int xclGetSysfsPath(const char* subdev, const char* entry, char* sysfsPath, size_t size);

  /* Enable/disable CMA chunk with specific size
   * e.g. enable = true, sz = 0x100000 (2M): add 2M CMA chunk
   *      enable = false: remove CMA chunk
   */
  int xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t total_size);

  int xclGetDebugIPlayoutPath(char* layoutPath, size_t size);
  int xclGetSubdevPath(const char* subdev, uint32_t idx, char* path, size_t size);
  int xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
  int xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);

  // Execute and interrupt abstraction
  int xclExecBuf(unsigned int cmdBO);
  int xclExecBuf(unsigned int cmdBO, xrt_core::hwctx_handle* ctxhdl);
  int xclExecBuf(unsigned int cmdBO,size_t numdeps, unsigned int* bo_wait_list);
  int xclRegisterEventNotify(unsigned int userInterrupt, int fd);
  int xclExecWait(int timeoutMilliSec);
  int xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared) const;
  int xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex);
  int xclIPSetReadRange(uint32_t ipIndex, uint32_t start, uint32_t size);

  int getBoardNumber( void ) { return mBoardNumber; }

  int xclIPName2Index(const char *name);

  int xclOpenIPInterruptNotify(uint32_t ipIndex, unsigned int flags);
  int xclCloseIPInterruptNotify(int fd);

  xrt_core::cuidx_type
  open_cu_context(const xrt_core::hwctx_handle* hwctx_hdl, const std::string& cuname);

  void
  close_cu_context(const xrt_core::hwctx_handle* hwctx_hdl, xrt_core::cuidx_type cuidx);

  std::unique_ptr<xrt_core::hwctx_handle>
  create_hw_context(const xrt::uuid&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode);

  void
  destroy_hw_context(xrt_core::hwctx_handle::slot_id slotidx);

  // Registers an xclbin, but does not load it.
  void
  register_xclbin(const xrt::xclbin&);

  // Exec Buf with hw ctx handle.
  void
  exec_buf(xclBufferHandle boh, xrt_core::hwctx_handle* ctxhdl);
private:
  std::shared_ptr<xrt_core::device> mCoreDevice;
  std::shared_ptr<xrt_core::pci::dev> mDev;
  std::ofstream mLogStream;
  int mUserHandle;
  int mStreamHandle;
  int mBoardNumber;
  bool hw_context_enable;
  uint64_t mOffsets[XCL_ADDR_SPACE_MAX];
  xclDeviceInfo2 mDeviceInfo;
  uint32_t mMemoryProfilingNumberSlots;
  uint32_t mAccelProfilingNumberSlots;
  uint32_t mStallProfilingNumberSlots;
  uint32_t mStreamProfilingNumberSlots;
  std::string mDevUserName;
  std::unique_ptr<xrt_core::bo_cache> mCmdBOCache;

  /*
   * Mapped CU register space for xclRegRead/Write(). We support at most
   * 128 CUs and each CU map is a pair <address, size>.
   */
  struct CuData {
      uint32_t* addr;
      uint32_t  size;
      uint32_t  start;
      uint32_t  end;
  };
  std::vector<CuData> mCuMaps;
  std::mutex mCuMapLock;

  bool zeroOutDDR();
  bool isXPR() const {
    return ((mDeviceInfo.mSubsystemId >> 12) == 4);
  }

  int dev_init();
  void dev_fini();

  int xclLoadAxlf(const axlf *buffer);
  int xclLoadHwAxlf(const axlf *buffer, drm_xocl_create_hw_ctx *hw_ctx);
  int xclPrepareAxlf(const axlf *buffer, struct drm_xocl_axlf *axlf_obj);
  int getAxlfObjSize(const axlf *buffer);
  void xclSysfsGetDeviceInfo(xclDeviceInfo2 *info);
  void xclSysfsGetUsageInfo(drm_xocl_usage_stat& stat);
  void xclSysfsGetErrorStatus(xclErrorStatus& stat);

  int freezeAXIGate();
  int freeAXIGate();

  int xclRegRW(bool rd, uint32_t ipIndex, uint32_t offset, uint32_t *datap);

  bool readPage(unsigned addr, uint8_t readCmd = 0xff);
  bool writePage(unsigned addr, uint8_t writeCmd = 0xff);
  unsigned readReg(unsigned offset);
  int writeReg(unsigned regOffset, unsigned value);
  bool finalTransfer(uint8_t *sendBufPtr, uint8_t *recvBufPtr, int byteCount);
  bool getFlashId();
  //All remaining read /write register commands can be issued through this function.
  bool readRegister(unsigned commandCode, unsigned bytes);
  bool writeRegister(unsigned commandCode, unsigned value, unsigned bytes);
  bool select4ByteAddressMode();
  bool deSelect4ByteAddressMode();

  // Performance monitoring helper functions
  signed cmpMonVersions(unsigned major1, unsigned minor1, unsigned major2, unsigned minor2);

  // QDMA AIO
  aio_context_t mAioContext;
  bool mAioEnabled;

  /* CopyBO helpers */
  int execbufCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                    size_t dst_offset, size_t src_offset);
  int m2mCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                size_t dst_offset, size_t src_offset);
}; /* shim */

} /* xocl */

#endif
