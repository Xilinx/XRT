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

#ifdef XRT_ENABLE_AIE
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
  if(numStream == 1)
    return;

  bufAllocSz = (totalSz / numStream) & 0xffffffffffffff00;
}

AIETraceOffload::~AIETraceOffload()
{
}

bool AIETraceOffload::initReadTrace()
{
  buffers.clear();
  buffers.resize(numStream);
  uint64_t i = 0;
  uint8_t  memIndex = 0;
  if(isPLIO) {
    memIndex = deviceIntf->getAIETs2mmMemIndex(0); // all the AIE Ts2mm s will have same memory index selected
  } else {
    memIndex = 0;  // for now
  }
  for(auto b : buffers) {
    b.boHandle = deviceIntf->allocTraceBuf(bufAllocSz, memIndex);
std::cout << " In AIETraceOffload::initReadTrace : after allocTraceBuf : i " << i << " isPLIO " << isPLIO << " numStream " << numStream << " bo Handle " << b.boHandle << " memIndex " << memIndex << std::endl;
    if(!b.boHandle) {
      return false;
    }
    b.isFull = false;
    // Data Mover will write input stream to this address
    uint64_t bufAddr = deviceIntf->getDeviceAddr(b.boHandle);
    if(isPLIO) {
std::cout << " In AIETraceOffload::initReadTrace : just before initAIETs2mm : i " << i << std::endl;
      deviceIntf->initAIETs2mm(bufAllocSz, bufAddr, i);
std::cout << " In AIETraceOffload::initReadTrace : just after initAIETs2mm : i " << i << " bufAllocSz " << bufAllocSz << " bufAddr " << bufAddr << std::endl;
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
      XAie_DmaSetAddrLen(&(shimDmaObj->desc), bufAddr, bufAllocSz);
#endif
    }
    ++i;
  }
  return true;
}

void AIETraceOffload::endReadTrace()
{
  // reset
  uint64_t i = 0;
  for(auto b : buffers) {
    if(!b.boHandle) {
      continue;
    }
    if(isPLIO) {
      deviceIntf->resetAIETs2mm(i);
    } else {
      // no reset required
    }
    deviceIntf->freeTraceBuf(b.boHandle);
std::cout << " In AIETraceOffload::ENDreadTrace : after freeTraceBuf i " << i << std::endl;
    b.boHandle = 0;
    ++i;
  }
  buffers.clear();
}

void AIETraceOffload::readTrace()
{
  for(uint64_t i = 0; i < numStream; ++i) {
    if(isPLIO) {
      configAIETs2mm(i);
    } 
    while (1) {
      auto bytes = readPartialTrace(i);

      if (buffers[i].usedSz == bufAllocSz)
        buffers[i].isFull = true;

std::cout << " In AIETraceOffload::readTrace : i " << i << " usedSz " << buffers[i].usedSz << " isFull " << buffers[i].isFull << std::endl;
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

  void* hostBuf = deviceIntf->syncTraceBuf(buffers[i].usedSz, buffers[i].offset, nBytes);
std::cout << " In AIETraceOffload::readPartialTrace : i " << i << " hostBuf " << hostBuf << " buffers[i].usedSz " << buffers[i].usedSz << " buffers[i].offset " << buffers[i].offset << " nBytes " << nBytes << std::endl;

  if(hostBuf) {
std::cout << " In AIETraceOffload::readPartialTrace : RIGHT BEFORE addAIETraceData i " << i << " hostBuf " << hostBuf << " nBytes " << nBytes << std::endl;
    traceLogger->addAIETraceData(i, hostBuf, nBytes);
    buffers[i].offset += nBytes;
    return nBytes;
  }
  return 0;
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
std::cout << " In AIETraceOffload::configAIETs2mm : i " << i << " usedSz " << buffers[i].usedSz << " usedSize " << usedSize << std::endl;
}


}

