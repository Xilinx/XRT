// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _ZYNQ_SHIM_H_
#define _ZYNQ_SHIM_H_

#include "zynq_dev.h"
#include "hwctx_object.h"

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
#include "core/common/shim/graph_handle.h"
#include "core/common/error.h"

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

  void
  register_xclbin(const xrt::xclbin&);

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
    zynqaie::hwctx_object* m_hwctx_obj{nullptr};

#ifdef XRT_ENABLE_AIE
    std::shared_ptr<zynqaie::aie_array> m_aie_array;
#endif

  public:
    buffer_object(shim* shim, xclBufferHandle hdl, xrt_core::hwctx_handle* hwctx_hdl = nullptr)
      : m_shim{shim}
      , m_hdl{hdl}
    {
#ifdef XRT_ENABLE_AIE
      if (nullptr != hwctx_hdl) { // hwctx specific
        m_hwctx_obj = dynamic_cast<zynqaie::hwctx_object*>(hwctx_hdl);

        if (nullptr != m_hwctx_obj) {
          m_aie_array = m_hwctx_obj->get_aie_array_shared();
        }
      }
      else {
        auto device = xrt_core::get_userpf_device(m_shim);
        auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());
        if (drv->isAieRegistered())
          m_aie_array = drv->get_aie_array_shared();
      }
#endif
    }

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

    void
    sync_aie_bo(xrt::bo& bo, const char *port_name, bo_direction dir, size_t size, size_t offset) override
    {
#ifdef XRT_ENABLE_AIE
      if (!m_aie_array->is_context_set()) {
        auto device = xrt_core::get_userpf_device(m_shim);
        m_aie_array->open_context(device.get(), m_hwctx_obj, xrt::aie::access_mode::primary);
      }

      auto bosize = bo.size();

      if (offset + size > bosize)
        throw xrt_core::error(-EINVAL, "Sync AIE BO fails: exceed BO boundary.");

      std::vector<xrt::bo> bos {bo};
      m_aie_array->sync_bo(bos, port_name, dir, size, offset);
#endif
    }

    void
    sync_aie_bo_nb(xrt::bo& bo, const char *port_name, bo_direction dir, size_t size, size_t offset) override
    {
#ifdef XRT_ENABLE_AIE
      if (!m_aie_array->is_context_set()) {
        auto device = xrt_core::get_userpf_device(m_shim);
        m_aie_array->open_context(device.get(), m_hwctx_obj, xrt::aie::access_mode::primary);
      }

      auto bosize = bo.size();

      if (offset + size > bosize)
        throw xrt_core::error(-EINVAL, "Sync AIE NBO fails: exceed BO boundary.");

      std::vector<xrt::bo> bos {bo};
      m_aie_array->sync_bo_nb(bos, port_name, dir, size, offset);
#endif
    }

  }; // buffer_object

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
  xclAllocBO(size_t size, unsigned flags, xrt_core::hwctx_handle* hwctx_hdl = nullptr);

  std::unique_ptr<xrt_core::buffer_handle>
  xclAllocUserPtrBO(void *userptr, size_t size, unsigned int flags, xrt_core::hwctx_handle* hwctx_hdl = nullptr);

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
  create_hw_context(xclDeviceHandle handle, const xrt::uuid&, const xrt::hw_context::cfg_param_type&, xrt::hw_context::access_mode);

  void
  destroy_hw_context(xrt_core::hwctx_handle::slot_id slotidx);

  void
  hwctx_exec_buf(const xrt_core::hwctx_handle* hwctx_hdl, xclBufferHandle boh);
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
  int prepare_hw_axlf(const axlf *buffer, struct drm_zocl_axlf *axlf_obj);
  int load_hw_axlf(xclDeviceHandle handle, const xclBin *buffer, drm_zocl_create_hw_ctx *hw_ctx);

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
  zynqaie::aie_array* getAieArray();
  std::shared_ptr<zynqaie::aie_array> get_aie_array_shared();
  zynqaie::aied* getAied();
  int getBOInfo(drm_zocl_info_bo &info);
  void registerAieArray();
  void reset_aie_array();
  bool isAieRegistered();
  int getPartitionFd(drm_zocl_aie_fd &aiefd);
  int resetAIEArray(drm_zocl_aie_reset &reset);
  int openGraphContext(const uuid_t xclbinId, unsigned int graphId, xrt::graph::access_mode am);
  int closeGraphContext(unsigned int graphId);
  void open_graph_context(const zynqaie::hwctx_object* hwctx, const uuid_t xclbinId, unsigned int graph_id, xrt::graph::access_mode am);
  void close_graph_context(const zynqaie::hwctx_object* hwctx, unsigned int graph_id);
  int openAIEContext(xrt::aie::access_mode am);
  xrt::aie::access_mode getAIEAccessMode();
  void setAIEAccessMode(xrt::aie::access_mode am);
#endif

public:
  inline bool get_hw_context_enable() { return hw_context_enable; }

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
  bool hw_context_enable = false;

  /*
   * Mapped CU register space for xclRegRead/Write(). We support at most
   * 128 CUs and each map is of 64k bytes. Does not support debug IP access.
   */
  std::vector<std::pair<uint32_t*, uint32_t>> mCuMaps;
  const size_t mCuMapSize = 64 * 1024;
  std::mutex mCuMapLock;
  int xclRegRW(bool rd, uint32_t cu_index, uint32_t offset, uint32_t *datap);

#ifdef XRT_ENABLE_AIE
  std::shared_ptr<zynqaie::aie_array> m_aie_array;
  std::unique_ptr<zynqaie::aied> aied;
  xrt::aie::access_mode access_mode = xrt::aie::access_mode::none;
#endif
};

} // namespace ZYNQ

#endif
