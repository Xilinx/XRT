// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _ZYNQ_SHIM_H_
#define _ZYNQ_SHIM_H_

#include "zynq_dev.h"

#include "core/edge/include/xclhal2_mpsoc.h"
#include "core/edge/include/zynq_ioctl.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/bo_cache.h"
#include "core/common/xrt_profiling.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/shared_handle.h"
#include "core/include/xdp/app_debug.h"

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

  // Shim handle for shared objects, like buffer and sync objects
  class shared_object : public xrt_core::shared_handle
  {
    shim* m_shim;
    xclBufferExportHandle m_ehdl;
  public:
    shared_object(shim* shim, xclBufferExportHandle ehdl)
      : m_shim(shim)
      , m_ehdl(ehdl)
    {}

    ~shared_object()
    {
      close(m_ehdl);
    }

    // Detach and return export handle for legacy xclAPI use
    xclBufferExportHandle
    detach_handle()
    {
      return std::exchange(m_ehdl, XRT_NULL_BO_EXPORT);
    }

    export_handle
    get_export_handle() const override
    {
      return static_cast<export_handle>(m_ehdl);
    }
  }; // shared_object

  // Shim handle for buffer object
  class buffer_object : public xrt_core::buffer_handle
  {
    shim* m_shim;
    xclBufferHandle m_hdl;

  public:
    buffer_object(shim* shim, xclBufferHandle hdl)
      : m_shim(shim)
      , m_hdl(hdl)
    {}

    ~buffer_object()
    {
      m_shim->xclFreeBO(m_hdl);
    }

    xclBufferHandle
    get_handle() const
    {
      return m_hdl;
    }

    static xclBufferHandle
    get_handle(const xrt_core::buffer_handle* bhdl)
    {
      return static_cast<const buffer_object*>(bhdl)->get_handle();
    }

    // Detach and return export handle for legacy xclAPI use
    xclBufferHandle
    detach_handle()
    {
      return std::exchange(m_hdl, XRT_NULL_BO);
    }

    // Export buffer for use with another process or device
    // An exported buffer can be imported by another device
    // or hardware context.
    std::unique_ptr<xrt_core::shared_handle>
    share() const override
    {
      return m_shim->xclExportBO(m_hdl);
    }

    void*
    map(map_type mt) override
    {
      return m_shim->xclMapBO(m_hdl, (mt == xrt_core::buffer_handle::map_type::write));
    }

    void
    unmap(void* addr) override
    {
      m_shim->xclUnmapBO(m_hdl, addr);
    }

    void
    sync(direction dir, size_t size, size_t offset) override
    {
      m_shim->xclSyncBO(m_hdl, static_cast<xclBOSyncDirection>(dir), size, offset);
    }

    void
    copy(const buffer_handle* src, size_t size, size_t dst_offset, size_t src_offset) override
    {
      auto bo_src = static_cast<const buffer_object*>(src);
      m_shim->xclCopyBO(m_hdl, bo_src->get_handle(), size, dst_offset, src_offset);
    }

    properties
    get_properties() const override
    {
      xclBOProperties xprop;
      m_shim->xclGetBOProperties(m_hdl, &xprop);
      return {xprop.flags, xprop.size, xprop.paddr};
    }

    xclBufferHandle
    get_xcl_handle() const override
    {
      return m_hdl;
    }
  }; // buffer_object


  // Shim handle for hardware context Even as hw_emu does not
  // support hardware context, it still must implement a shim
  // hardware context handle representing the default slot
  class hwcontext : public xrt_core::hwctx_handle
  {
    shim* m_shim;
    xrt::uuid m_uuid;
    slot_id m_slotidx;
    xrt::hw_context::access_mode m_mode;

  public:
    hwcontext(shim* shim, slot_id slotidx, xrt::uuid uuid, xrt::hw_context::access_mode mode)
      : m_shim(shim)
      , m_uuid(std::move(uuid))
      , m_slotidx(slotidx)
      , m_mode(mode)
    {}

    void
    update_access_mode(access_mode mode) override
    {
      m_mode = mode;
    }

    slot_id
    get_slotidx() const override
    {
      return m_slotidx;
    }

    xrt::hw_context::access_mode
    get_mode() const
    {
      return m_mode;
    }

    xrt::uuid
    get_xclbin_uuid() const
    {
      return m_uuid;
    }

    std::unique_ptr<xrt_core::hwqueue_handle>
    create_hw_queue() override
    {
      return nullptr;
    }

    std::unique_ptr<xrt_core::buffer_handle>
    alloc_bo(void* userptr, size_t size, unsigned int flags) override
    {
      // The hwctx is embedded in the flags, use regular shim path
      return m_shim->xclAllocUserPtrBO(userptr, size, flags);
    }

    std::unique_ptr<xrt_core::buffer_handle>
    alloc_bo(size_t size, unsigned int flags) override
    {
      // The hwctx is embedded in the flags, use regular shim path
      return m_shim->xclAllocBO(size, flags);
    }

    xrt_core::cuidx_type
    open_cu_context(const std::string& cuname) override
    {
      return m_shim->open_cu_context(this, cuname);
    }

    void
    close_cu_context(xrt_core::cuidx_type cuidx) override
    {
      m_shim->close_cu_context(this, cuidx);
    }

    void
    exec_buf(xrt_core::buffer_handle* cmd) override
    {
      m_shim->xclExecBuf(cmd->get_xcl_handle());
    }
  }; // class hwcontext

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

  std::unique_ptr<xrt_core::buffer_handle>
  xclAllocBO(size_t size, unsigned flags);

  std::unique_ptr<xrt_core::buffer_handle>
  xclAllocUserPtrBO(void *userptr, size_t size, unsigned int flags);

  std::unique_ptr<xrt_core::shared_handle>
  xclExportBO(unsigned int boHandle);

  std::unique_ptr<xrt_core::buffer_handle>
  xclImportBO(int fd, unsigned int flags);

  unsigned int xclGetHostBO(uint64_t paddr, size_t size);
  void xclFreeBO(unsigned int boHandle);
  int xclWriteBO(unsigned int boHandle, const void *src, size_t size,
                 size_t seek);
  int xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
  void *xclMapBO(unsigned int boHandle, bool write);
  int xclUnmapBO(unsigned int boHandle, void* addr);
  unsigned int xclGetBOProperties(unsigned int boHandle,
                                  xclBOProperties *properties);
  int xclExecBuf(unsigned int cmdBO);
  int xclExecWait(int timeoutMilliSec);
  int resetDevice(xclResetKind kind);

  ////////////////////////////////////////////////////////////////
  // Context handling
  ////////////////////////////////////////////////////////////////
  xrt_core::cuidx_type
  open_cu_context(const xrt_core::hwctx_handle* hwctx_hdl, const std::string& cuname);

  void
  close_cu_context(const xrt_core::hwctx_handle* hwctx_hdl, xrt_core::cuidx_type cuidx);

  std::unique_ptr<xrt_core::hwctx_handle>
  create_hw_context(const xrt::uuid&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode);
////////////////////////////////////////////////////////////////

  int xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared);
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
  int xclIPSetReadRange(uint32_t ipIndex, uint32_t start, uint32_t size);

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
