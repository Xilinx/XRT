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
#include "xdp/profile/device/aie_trace_offload.h"
#include "xdp/profile/device/aie_trace_logger.h"

// Default dma chunk size
#define CHUNK_SZ (MAX_TRACE_NUMBER_SAMPLES * TRACE_PACKET_SIZE)

namespace xdp {


AIETraceOffload::AIETraceOffload(DeviceIntf* dInt,
                                 AIETraceLogger* logger,
                                 bool isPlio,
                                 uint64_t totalSize,
                                 uint64_t numStrm)
               : deviceIntf(dInt),
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
  for(auto b : buffers) {
    b.boHandle = deviceIntf->allocTraceBuf(bufAllocSz, 1 /*deviceIntf->getTS2MmMemIndex*/);
    if(!b.boHandle) {
      return false;
    }
    b.isFull = false;
    // Data Mover will write input stream to this address
    uint64_t bufAddr = deviceIntf->getDeviceAddr(b.boHandle);
    if(isPLIO) {
      deviceIntf->initAIETS2MM(bufAllocSz, bufAddr, i);
    } else {
		// XAIEDma_ShimSetBDAddr
    }
    ++i;
  }
  return true;	// no need for m_initialized flag ?
}

void AIETraceOffload::endReadTrace()
{
  // reset
  uint64_t i = 0;
  for(auto b : buffers) {
    if(!b.boHandle) {
      continue; // or break; ??
    }
    if(isPLIO) {
      deviceIntf->resetAIETS2MM(i);
    } else {
      // ?
    }
    deviceIntf->freeTraceBuf(b.boHandle);
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

//  auto  start = std::chrono::steady_clock::now();
  void* hostBuf = deviceIntf->syncTraceBuf(buffers[i].usedSz, buffers[i].offset, nBytes);
//  auto  end = std::chrono::steady_clock::now();

#if 0
  debug_stream
    << "Elapsed time in microseconds for sync in readAiePartial: "
    << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
    << " µs" << std::endl;
#endif
  if(hostBuf) {
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
}


}

