/**
 * Copyright (C) 2019-2021 Xilinx, Inc
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

#define XDP_SOURCE

#include <iostream>

#include "core/common/message.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_logger.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/device/device_intf.h"

#ifdef XRT_ENABLE_AIE
#include <sys/mman.h>
#include "core/include/xrt.h"
#include "core/edge/user/shim.h"
#endif


// Default dma chunk size
#define CHUNK_SZ (MAX_TRACE_NUMBER_SAMPLES * TRACE_PACKET_SIZE)

namespace xdp {


AIETraceOffload::AIETraceOffload
  ( void* handle, uint64_t id
  , DeviceIntf* dInt
  , AIETraceLogger* logger
  , bool isPlio
  , uint64_t totalSize
  , uint64_t numStrm
  )
  : deviceHandle(handle)
  , deviceId(id)
  , deviceIntf(dInt)
  , traceLogger(logger)
  , isPLIO(isPlio)
  , totalSz(totalSize)
  , numStream(numStrm)
  , traceContinuous(false)
  , offloadIntervalms(0)
  , bufferInitialized(false)
  , offloadStatus(AIEOffloadThreadStatus::IDLE)
  , mEnCircularBuf(false)
{
  bufAllocSz = deviceIntf->getAlignedTraceBufferSize(totalSz, static_cast<unsigned int>(numStream));
}

AIETraceOffload::~AIETraceOffload()
{
  stopOffload();
  if (offloadThread.joinable()) {
    offloadThread.join();
  }
}

bool AIETraceOffload::initReadTrace()
{
  buffers.clear();
  buffers.resize(numStream);

  uint8_t  memIndex = 0;
  if (isPLIO) {
    memIndex = deviceIntf->getAIETs2mmMemIndex(0); // all the AIE Ts2mm s will have same memory index selected
  } else {
    memIndex = 0;  // for now
    gmioDMAInsts.clear();
    gmioDMAInsts.resize(numStream);
  }
  for(uint64_t i = 0; i < numStream ; ++i) {
    buffers[i].boHandle = deviceIntf->allocTraceBuf(bufAllocSz, memIndex);
    if(!buffers[i].boHandle) {
      bufferInitialized = false;
      return bufferInitialized;
    }

    // Data Mover will write input stream to this address
    uint64_t bufAddr = deviceIntf->getDeviceAddr(buffers[i].boHandle);

    std::string msg = "Allocating trace buffer of size " + std::to_string(bufAllocSz) + " for AIE Stream " + std::to_string(i);
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.c_str());

    if (isPLIO) {
      deviceIntf->initAIETs2mm(bufAllocSz, bufAddr, i, mEnCircularBuf);
    } else {
#ifdef XRT_ENABLE_AIE
      VPDatabase* db = VPDatabase::Instance();
      TraceGMIO*  traceGMIO = (db->getStaticInfo()).getTraceGMIO(deviceId, i);

      ZYNQ::shim *drv = ZYNQ::shim::handleCheck(deviceHandle);
      if(!drv) {
        bufferInitialized = false;
        return bufferInitialized;
      }
      zynqaie::Aie* aieObj = drv->getAieArray();

      XAie_DevInst* devInst = aieObj->getDevInst();

      gmioDMAInsts[i].gmioTileLoc = XAie_TileLoc(traceGMIO->shimColumn, 0);

      int driverStatus = XAIE_OK;
      driverStatus = XAie_DmaDescInit(devInst, &(gmioDMAInsts[i].shimDmaInst), gmioDMAInsts[i].gmioTileLoc);
      if(XAIE_OK != driverStatus) {
        throw std::runtime_error("Initialization of DMA Descriptor failed while setting up SHIM DMA channel for GMIO Trace offload");
      }

      // channelNumber: (0-S2MM0,1-S2MM1,2-MM2S0,3-MM2S1)
      // Enable shim DMA channel, need to start first so the status is correct
      uint16_t channelNumber = (traceGMIO->channelNumber > 1) ? (traceGMIO->channelNumber - 2) : traceGMIO->channelNumber;
      XAie_DmaDirection dir = (traceGMIO->channelNumber > 1) ? DMA_MM2S : DMA_S2MM;

      XAie_DmaChannelEnable(devInst, gmioDMAInsts[i].gmioTileLoc, channelNumber, dir);

      // Set AXI burst length
      XAie_DmaSetAxi(&(gmioDMAInsts[i].shimDmaInst), 0, traceGMIO->burstLength, 0, 0, 0);

      XAie_MemInst memInst;
      XAie_MemCacheProp prop = XAIE_MEM_CACHEABLE;
      xclBufferExportHandle boExportHandle = xclExportBO(deviceHandle, buffers[i].boHandle);
      if(XRT_NULL_BO_EXPORT == boExportHandle) {
        throw std::runtime_error("Unable to export BO while attaching to AIE Driver");
      }
      XAie_MemAttach(devInst,  &memInst, 0, 0, 0, prop, boExportHandle);

      char* vaddr = reinterpret_cast<char *>(mmap(NULL, bufAllocSz, PROT_READ | PROT_WRITE, MAP_SHARED, boExportHandle, 0));
      XAie_DmaSetAddrLen(&(gmioDMAInsts[i].shimDmaInst), (uint64_t)vaddr, bufAllocSz);

      XAie_DmaEnableBd(&(gmioDMAInsts[i].shimDmaInst));

      // For trace, use bd# 0 for S2MM0, use bd# 4 for S2MM1
      int bdNum = channelNumber * 4;
      // Write to shim DMA BD AxiMM registers
      XAie_DmaWriteBd(devInst, &(gmioDMAInsts[i].shimDmaInst), gmioDMAInsts[i].gmioTileLoc, bdNum);

      // Enqueue BD
      XAie_DmaChannelPushBdToQueue(devInst, gmioDMAInsts[i].gmioTileLoc, channelNumber, dir, bdNum);

#endif
    }
  }
  bufferInitialized = true;
  return bufferInitialized;
}

void AIETraceOffload::endReadTrace()
{
  // reset
  for(uint64_t i = 0; i < numStream ; ++i) {
    if(!buffers[i].boHandle) {
      continue;
    }
    if(isPLIO) {
      deviceIntf->resetAIETs2mm(i);
//      deviceIntf->freeTraceBuf(b.boHandle);
    } else {
#ifdef XRT_ENABLE_AIE
      VPDatabase* db = VPDatabase::Instance();
      TraceGMIO*  traceGMIO = (db->getStaticInfo()).getTraceGMIO(deviceId, i);

      ZYNQ::shim *drv = ZYNQ::shim::handleCheck(deviceHandle);
      if(!drv) {
        return ;
      }
      zynqaie::Aie* aieObj = drv->getAieArray();
      XAie_DevInst* devInst = aieObj->getDevInst();

      // channelNumber: (0-S2MM0,1-S2MM1,2-MM2S0,3-MM2S1)
      // Enable shim DMA channel, need to start first so the status is correct
      uint16_t channelNumber = (traceGMIO->channelNumber > 1) ? (traceGMIO->channelNumber - 2) : traceGMIO->channelNumber;
      XAie_DmaDirection dir = (traceGMIO->channelNumber > 1) ? DMA_MM2S : DMA_S2MM;

      XAie_DmaChannelDisable(devInst, gmioDMAInsts[i].gmioTileLoc, channelNumber, dir);
#endif
      
    }
    deviceIntf->freeTraceBuf(buffers[i].boHandle);
    buffers[i].boHandle = 0;
  }
  buffers.clear();
  bufferInitialized = false;
}

void AIETraceOffload::readTrace(bool final)
{
  for (uint64_t i = 0; i < numStream; ++i) {
    configAIETs2mm(i, final);

    if (buffers[i].offset >= buffers[i].usedSz)
      continue;

    // Amount of newly written trace
    uint64_t nBytes = buffers[i].usedSz - buffers[i].offset;
    // Optimization : Skip small trace reads
    if (nBytes < TS2MM_MIN_READ_SIZE && !final)
      continue;

    // Sync to host
    auto start = std::chrono::steady_clock::now();
    void* hostBuf = deviceIntf->syncTraceBuf(buffers[i].boHandle, buffers[i].offset, nBytes);
    auto end = std::chrono::steady_clock::now();

    if (!hostBuf)
      continue;

    // Log trace buffer
    traceLogger->addAIETraceData(i, hostBuf, nBytes);

    debug_stream
    << "ts2mm_" << i << " : bytes : " << nBytes << " "
    << "sync: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "µs "
    << std::hex << "from 0x" << buffers[i].offset << " to 0x" << buffers[i].usedSz
    << std::dec << std::endl;

    //Update offset and check for full buffer
    buffers[i].offset += nBytes;
    if (nBytes >= bufAllocSz)
      buffers[i].isFull = true;
  }
}

bool AIETraceOffload::isTraceBufferFull()
{
  // Detect if any trace buffer got full
  if (isPLIO) {
    for (auto& buf: buffers) {
      if (buf.isFull) {
        return true;
      }
    }
  }
  return false;
}

void AIETraceOffload::configAIETs2mm(uint64_t index, bool final)
{
  auto& bd = buffers[index];

  if (bd.offloadDone)
    return;

  // read complete trace buffer in gmio
  if (!isPLIO) {
    bd.usedSz = bufAllocSz;
    bd.offloadDone = true;
    return;
  }

  uint64_t wordCount = deviceIntf->getWordCountAIETs2mm(index, final);
  // AIE Trace packets are 4 words of 64 bit
  wordCount -= wordCount % 4;

  uint64_t bytes_written  = wordCount * TRACE_PACKET_SIZE;
  uint64_t bytes_read = bd.usedSz + bd.rollover_count * bufAllocSz;

  // Offload cannot keep up with the DMA
  // There is a slight chance that overwrite could occur
  // during this check. In that case trace could be corrupt
  if (bytes_written > bytes_read + bufAllocSz) {
    // Don't read any more data
    bd.offset = bd.usedSz;
    bd.offloadDone = true;
    //stop_offload();
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_CIRC_BUF_OVERWRITE);
    return;
  }

  // Start Offload from previous offset
  bd.offset = bd.usedSz;
  if (bd.offset == bufAllocSz) {
    if (!mEnCircularBuf) {
      bd.offloadDone = true;
      //stop_offload();
      return;
    }
    bd.rollover_count++;
    bd.offset = 0;
  }

  // End Offload at this offset
  bd.usedSz = bytes_written - bd.rollover_count * bufAllocSz;
  if (bd.usedSz > bufAllocSz)
    bd.usedSz = bufAllocSz;

  debug_stream
    << "AIETraceOffload::config_s2mm_" << index << " "
    << "Reading from 0x"
    << std::hex << bd.offset << " to 0x" << bd.usedSz << std::dec
    << " Bytes Read : " << bytes_read
    << " Bytes Written : " << bytes_written
    << " Rollovers : " << bd.rollover_count
    << std::endl;
}

void AIETraceOffload::checkCircularBufferSupport()
{
  if (!isPLIO)
    return;
  if (deviceIntf->supportsCircBufAIE()) {
      if (offloadIntervalms != 0) {
        circ_buf_cur_rate_plio = bufAllocSz * (1000 / offloadIntervalms);
        if (circ_buf_cur_rate_plio >= circ_buf_min_rate_plio)
          mEnCircularBuf = true;
      } else {
        mEnCircularBuf = true;
      }
    }
    debug_stream
      << "Circular buffer support : " << mEnCircularBuf
      << std::endl;
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
  checkCircularBufferSupport();

  while (keepOffloading()) {
    readTrace(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(offloadIntervalms));
  }

  // Note: This will call flush and reset on datamover
  readTrace(true);
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

}

