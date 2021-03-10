/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/device/aie_trace/aie_trace_logger.h"

#include <iostream>
#ifdef XRT_ENABLE_AIE
#include <sys/mman.h>
#include "core/include/xrt.h"
#include "core/edge/user/aie/aie.h"
#include "core/edge/user/shim.h"
#endif

// Default dma chunk size
#define CHUNK_SZ (MAX_TRACE_NUMBER_SAMPLES * TRACE_PACKET_SIZE)

namespace xdp {


AIETraceOffload::AIETraceOffload(void* handle, uint64_t id,
                                 DeviceIntf* dInt,
                                 AIETraceLogger* logger,
                                 bool isPlio,
                                 uint64_t totalSize,
                                 uint64_t numStrm)
               : deviceHandle(handle),
                 deviceId(id),
                 deviceIntf(dInt),
                 traceLogger(logger),
                 isPLIO(isPlio),
                 totalSz(totalSize),
                 numStream(numStrm)
{
  bufAllocSz = (totalSz / numStream) & 0xfffffffffffff000;
}

AIETraceOffload::~AIETraceOffload()
{
}

bool AIETraceOffload::initReadTrace()
{
  buffers.clear();
  buffers.resize(numStream);

  uint8_t  memIndex = 0;
  if(isPLIO) {
    memIndex = deviceIntf->getAIETs2mmMemIndex(0); // all the AIE Ts2mm s will have same memory index selected
  } else {
    memIndex = 0;  // for now
  }
  for(uint64_t i = 0; i < numStream ; ++i) {
    buffers[i].boHandle = deviceIntf->allocTraceBuf(bufAllocSz, memIndex);
    if(!buffers[i].boHandle) {
      return false;
    }
    buffers[i].isFull = false;
    // Data Mover will write input stream to this address
    uint64_t bufAddr = deviceIntf->getDeviceAddr(buffers[i].boHandle);
    if(isPLIO) {
      deviceIntf->initAIETs2mm(bufAllocSz, bufAddr, i);
    } else {
#ifdef XRT_ENABLE_AIE
      VPDatabase* db = VPDatabase::Instance();
      TraceGMIO*  traceGMIO = (db->getStaticInfo()).getTraceGMIO(deviceId, i);

      ZYNQ::shim *drv = ZYNQ::shim::handleCheck(deviceHandle);
      if(!drv) {
        return false;
      }
      zynqaie::Aie* aieObj = drv->getAieArray();

      zynqaie::ShimDMA* shimDmaObj = &(aieObj->shim_dma[traceGMIO->shimColumn]);

      XAie_DevInst* devInst = aieObj->getDevInst();

      XAie_LocType shimTile = XAie_TileLoc(traceGMIO->shimColumn, 0);
      XAie_DmaDescInit(devInst, &(shimDmaObj->desc), shimTile);

      // channelNumber: (0-S2MM0,1-S2MM1,2-MM2S0,3-MM2S1)
      // Enable shim DMA channel, need to start first so the status is correct
      uint16_t channelNumber = (traceGMIO->channelNumber > 1) ? (traceGMIO->channelNumber - 2) : traceGMIO->channelNumber;
      XAie_DmaDirection dir = (traceGMIO->channelNumber > 1) ? DMA_MM2S : DMA_S2MM;

      XAie_DmaChannelEnable(devInst, XAie_TileLoc(traceGMIO->shimColumn, 0), channelNumber, dir);

      // Set AXI burst length
      XAie_DmaSetAxi(&(shimDmaObj->desc), 0, traceGMIO->burstLength, 0, 0, 0);

      XAie_MemInst memInst;
	  XAie_MemCacheProp prop = XAIE_MEM_CACHEABLE;
      xclBufferExportHandle boExportHandle = xclExportBO(deviceHandle, buffers[i].boHandle);
      if(XRT_NULL_BO_EXPORT == boExportHandle) {
        throw std::runtime_error("Unable to export BO while attaching to AIE Driver");
      }
      XAie_MemAttach(devInst,  &memInst, 0, 0, 0, prop, boExportHandle);

      char* vaddr = reinterpret_cast<char *>(mmap(NULL, bufAllocSz, PROT_READ | PROT_WRITE, MAP_SHARED, boExportHandle, 0));
      XAie_DmaSetAddrLen(&(shimDmaObj->desc), (uint64_t)vaddr, bufAllocSz);

      XAie_DmaEnableBd(&(shimDmaObj->desc));

      // For trace, use bd# 0 for S2MM0, use bd# 4 for S2MM1
      int bdNum = channelNumber * 4;
      // Write to shim DMA BD AxiMM registers
      XAie_DmaWriteBd(devInst, &(shimDmaObj->desc), XAie_TileLoc(traceGMIO->shimColumn, 0), bdNum);

      // Enqueue BD
      XAie_DmaChannelPushBdToQueue(devInst, XAie_TileLoc(traceGMIO->shimColumn, 0), channelNumber, dir, bdNum);

#endif
    }
  }
  return true;
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

      zynqaie::ShimDMA* shimDmaObj = &(aieObj->shim_dma[traceGMIO->shimColumn]);

      XAie_DevInst* devInst = aieObj->getDevInst();

      XAie_LocType shimTile = XAie_TileLoc(traceGMIO->shimColumn, 0);

      // channelNumber: (0-S2MM0,1-S2MM1,2-MM2S0,3-MM2S1)
      // Enable shim DMA channel, need to start first so the status is correct
      uint16_t channelNumber = (traceGMIO->channelNumber > 1) ? (traceGMIO->channelNumber - 2) : traceGMIO->channelNumber;
      XAie_DmaDirection dir = (traceGMIO->channelNumber > 1) ? DMA_MM2S : DMA_S2MM;

      XAie_DmaChannelReset(devInst, XAie_TileLoc(traceGMIO->shimColumn, 0), channelNumber, dir, DMA_CHANNEL_RESET);
#endif
      
    }
    deviceIntf->freeTraceBuf(buffers[i].boHandle);
    buffers[i].boHandle = 0;
  }
  buffers.clear();
}

void AIETraceOffload::readTrace()
{
  for(uint64_t i = 0; i < numStream; ++i) {
    if(isPLIO) {
      configAIETs2mm(i);
    } else { 
      buffers[i].usedSz = bufAllocSz;
    }
    while (1) {
      auto bytes = readPartialTrace(i);

      if (buffers[i].usedSz == bufAllocSz) {
        buffers[i].isFull = true;
        break;
      }

      if (bytes != CHUNK_SZ)
        break;
    }
  }
}

uint64_t AIETraceOffload::readPartialTrace(uint64_t i)
{
  if(buffers[i].offset >= buffers[i].usedSz) {
    return 0;
  }

  uint64_t nBytes = CHUNK_SZ;

  if((buffers[i].offset + CHUNK_SZ) > buffers[i].usedSz)
    nBytes = buffers[i].usedSz - buffers[i].offset;

  void* hostBuf = deviceIntf->syncTraceBuf(buffers[i].boHandle, buffers[i].offset, nBytes);

  if(hostBuf) {
    traceLogger->addAIETraceData(i, hostBuf, nBytes);
    buffers[i].offset += nBytes;
    return nBytes;
  }
  return 0;
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

void AIETraceOffload::configAIETs2mm(uint64_t i /*index*/)
{
  uint64_t wordCount = deviceIntf->getWordCountAIETs2mm(i);
  uint64_t usedSize  = wordCount * TRACE_PACKET_SIZE;
  if(usedSize <= bufAllocSz) {
    buffers[i].usedSz = usedSize;
  } else {
    buffers[i].usedSz = bufAllocSz;
  }
}


}

