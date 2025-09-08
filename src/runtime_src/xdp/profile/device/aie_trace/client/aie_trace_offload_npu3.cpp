// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_PLUGIN_SOURCE

#include "aie_trace_offload_npu3.h"
#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_logger.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include <iostream>

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  AIETraceOffload::AIETraceOffload(void* handle, uint64_t id, PLDeviceIntf* dInt,
                                   AIETraceLogger* logger, bool isPlio,
                                   uint64_t totalSize, uint64_t numStrm,
                                   xrt::hw_context context,
                                   std::shared_ptr<AieTraceMetadata>(metadata))
    : deviceHandle(handle), deviceId(id), plDeviceIntf(dInt), traceLogger(logger),
      isPLIO(isPlio), totalSz(totalSize), numStream(numStrm),
      traceContinuous(false), offloadIntervalUs(0), bufferInitialized(false),
      offloadStatus(AIEOffloadThreadStatus::IDLE), mEnCircularBuf(false),
      mCircularBufOverwrite(false), context(context), metadata(metadata)
  {
    bufAllocSz = getAlignedTraceBufSize(totalSz, static_cast<unsigned int>(numStream));
    mReadTrace = std::bind(&AIETraceOffload::readTraceGMIO, this, std::placeholders::_1);
  }

  AIETraceOffload::~AIETraceOffload() 
  {
    stopOffload();
    if (offloadThread.joinable())
      offloadThread.join();
  }

  bool AIETraceOffload::initReadTrace()
  {
    // The code below is hanging, so for now don't run
    //return;

    xrt_core::message::send(severity_level::info, "XRT",
      "Starting configuration for NPU3.");
    
    buffers.clear();
    buffers.resize(numStream);

    // TODO: get board-specific values
    constexpr std::uint64_t DDR_AIE_ADDR_OFFSET = std::uint64_t{0x80000000};

    xdp::aie::driver_config meta_config = metadata->getAIEConfigMetadata();

    XAie_Config cfg{
      meta_config.hw_gen,
      meta_config.base_address,
      meta_config.column_shift,
      meta_config.row_shift,
      meta_config.num_rows,
      meta_config.num_columns,
      meta_config.shim_row,
      meta_config.mem_row_start,
      meta_config.mem_num_rows,
      meta_config.aie_tile_row_start,
      meta_config.aie_tile_num_rows,
      {0} // PartProp
    };

    auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);

    if (RC != XAIE_OK) {
      xrt_core::message::send(severity_level::warning, "XRT",
                              "AIE Driver Initialization Failed.");
      return false;
    }

    tranxHandler = std::make_unique<aie::NPU3Transaction>();
    if (!tranxHandler->initializeTransaction(&aieDevInst, "AieTraceOffload"))
      return false;

    for (uint64_t i = 0; i < numStream; ++i) {
      VPDatabase* db = VPDatabase::Instance();
      TraceGMIO* traceGMIO = (db->getStaticInfo()).getTraceGMIO(deviceId, i);

      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
        "Allocating trace buffer of size " + std::to_string(bufAllocSz) + " for AIE Stream " 
        + std::to_string(i));
      xrt_bos.emplace_back(xrt::bo(context.get_device(), bufAllocSz,
                           XRT_BO_FLAGS_HOST_ONLY, tranxHandler->getGroupID(0, context)));
      
      buffers[i].bufId = xrt_bos.size();
      if (!buffers[i].bufId) {
        bufferInitialized = false;
        return bufferInitialized;
      }

      if (!xrt_bos.empty()) {
        auto bo_map = xrt_bos.back().map<uint8_t*>();
        memset(bo_map, 0, bufAllocSz);
      }

      XAie_LocType loc;
      XAie_DmaDesc dmaDesc;
      loc = XAie_TileLoc(traceGMIO->shimColumn, 0);

      auto dmaType = traceGMIO->type;
      XAie_DmaDirection dmaDir = (dmaType == io_type::TRACE_DMA) ? DMA_S2MM_TRACE : DMA_S2MM;
      uint8_t  s2mm_ch_id = (dmaType >= S2MM_TRACE) ? 0 : traceGMIO->channelNumber;
      uint16_t s2mm_bd_id = 0; /* always use first bd in private pool */

      // S2MM BD
      RC = XAie_DmaDescInit(&aieDevInst, &dmaDesc, loc);
      RC = XAie_DmaSetAddrLen(&dmaDesc, xrt_bos[i].address() + DDR_AIE_ADDR_OFFSET,
                              static_cast<uint32_t>(bufAllocSz));
      RC = XAie_DmaSetAxi(&dmaDesc, 0U, 8U, 0U, 0U, 0U);
      //RC = XAie_DmaWriteBd(&aieDevInst, &dmaDesc, loc, s2mm_bd_id);
      RC = XAie_DmaWriteBdPvtBuffPool(&aieDevInst, &dmaDesc, loc, s2mm_ch_id, dmaDir, s2mm_bd_id);
      RC = XAie_DmaChannelPushBdToQueue(&aieDevInst, loc, s2mm_ch_id, dmaDir, s2mm_bd_id);
      RC = XAie_DmaChannelEnable(&aieDevInst, loc, s2mm_ch_id, dmaDir);
   
      if (!tranxHandler->submitTransaction(&aieDevInst, context))
        return false;
      
      xrt_core::message::send(severity_level::info, "XRT",
        "Successfully scheduled AIE Trace Offloading NPU3.");
    }

    bufferInitialized = true;
    return bufferInitialized;
  }

  void AIETraceOffload::readTraceGMIO(bool final)
  {
    // Keep it low to save bandwidth
    constexpr uint64_t chunk_512k = 0x80000;

    for (uint64_t index = 0; index < numStream; ++index) {
      auto& bd = buffers[index];
      if (bd.offloadDone)
        continue;

      // read one chunk or till the end of buffer
      auto chunkEnd = bd.offset + chunk_512k;
      if (final || chunkEnd > bufAllocSz)
        chunkEnd = bufAllocSz;
      bd.usedSz = chunkEnd;

      bd.offset += syncAndLog(index);
    }
  }

  uint64_t AIETraceOffload::syncAndLog(uint64_t index)
  {
    auto& bd = buffers[index];

    if (bd.offset >= bd.usedSz)
      return 0;

    // Amount of newly written trace
    uint64_t nBytes = bd.usedSz - bd.offset;

    xrt_bos[index].sync(XCL_BO_SYNC_BO_FROM_DEVICE, nBytes, bd.offset);
    auto in_bo_map = xrt_bos[index].map<uint32_t*>() + bd.offset;

    if (!in_bo_map)
      return 0;

    nBytes = searchWrittenBytes((void*)in_bo_map, bufAllocSz);

    // check for full buffer
    if (bd.offset + nBytes >= bufAllocSz) {
      bd.isFull = true;
      bd.offloadDone = true;
    }

    // Log nBytes of trace
    traceLogger->addAIETraceData(index, (void*)in_bo_map, nBytes, true);
    return nBytes;
  }

  void AIETraceOffload::startOffload() 
  {
    if (offloadStatus == AIEOffloadThreadStatus::RUNNING)
      return;

    std::lock_guard<std::mutex> lock(statusLock);
    offloadStatus = AIEOffloadThreadStatus::RUNNING;

    offloadThread = std::thread(&AIETraceOffload::continuousOffload, this);
  }

  void AIETraceOffload::continuousOffload()
  {
    if (!bufferInitialized && !initReadTrace()) {
      offloadFinished();
      return;
    }

    while (keepOffloading()) {
      mReadTrace(false);
      std::this_thread::sleep_for(std::chrono::microseconds(offloadIntervalUs));
    }

    // Note: This will call flush and reset on datamover
    mReadTrace(true);
    endReadTrace();
    offloadFinished();
  }

  bool AIETraceOffload::keepOffloading() 
  { 
    std::lock_guard<std::mutex> lock(statusLock);
    return (AIEOffloadThreadStatus::RUNNING == offloadStatus); 
  }

  void AIETraceOffload::stopOffload() 
  {
    std::lock_guard<std::mutex> lock(statusLock);
    if (AIEOffloadThreadStatus::STOPPED == offloadStatus)
      return;
    offloadStatus = AIEOffloadThreadStatus::STOPPING;
  }

  void AIETraceOffload::offloadFinished() 
  {
    std::lock_guard<std::mutex> lock(statusLock);
    if (AIEOffloadThreadStatus::STOPPED == offloadStatus)
      return;
    offloadStatus = AIEOffloadThreadStatus::STOPPED;
  }

  void AIETraceOffload::endReadTrace() 
  {
    for (uint64_t i = 0; i < numStream ; ++i) {
      if (!buffers[i].bufId)
        continue;

      buffers[i].bufId = 0;
    }
    bufferInitialized = false;
  }

  uint64_t AIETraceOffload::searchWrittenBytes(void* buf, uint64_t bytes)
  {
    /*
     * Look For trace boundary using binary search.
     * Use Dword to be safe
     */
    auto words = static_cast<uint64_t*>(buf);
    uint64_t wordcount = bytes / TRACE_PACKET_SIZE;

    // indices
    int64_t low = 0;
    int64_t high = static_cast<int64_t>(wordcount) - 1;

    // Boundary at which trace ends and 0s begin
    uint64_t boundary = wordcount;

    while (low <= high) {
      int64_t mid = low + (high - low) / 2;

      if (!words[mid]) {
        boundary = mid;
        high = mid - 1;
      }
      else {
        low = mid + 1;
      }
    }

    uint64_t written = boundary * TRACE_PACKET_SIZE;

    debug_stream << "Found Boundary at 0x" << std::hex << written << std::dec
                 << std::endl;

    return written;
  }

} // namespace xdp
