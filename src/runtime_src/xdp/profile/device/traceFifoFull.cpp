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

#include "traceFifoFull.h"

#define AXI_FIFO_RDFD_AXI_FULL          0x1000

#define TRACE_WORD_WIDTH            64
#define TRACE_NUMBER_SAMPLES        8192

#include<iomanip>
#include<cstring>
#include <bitset>

namespace xdp {

TraceFifoFull::TraceFifoFull(Device* handle /** < [in] the xrt or hal device handle */,
                             uint64_t index  /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data)
    : ProfileIP(handle, index, data),
      properties(0),
      major_version(0),
      minor_version(0)
{
    if(data) {
        properties = data->m_properties;
        major_version = data->m_major;
        minor_version = data->m_minor;
    }
}

uint32_t TraceFifoFull::getNumTraceSamples()
{
    return 0;
}

size_t TraceFifoFull::reset()
{
    // Reset logic
    mclockTrainingdone = false;
    mfirstTimestamp = 0;
    return 0;
}

uint32_t TraceFifoFull::getMaxNumTraceSamples()
{
    return TRACE_NUMBER_SAMPLES;
}

// This function allocates a buffer and returns the pointer in traceData.
//  The caller is responsible for reclaiming this memory.
uint32_t TraceFifoFull::readTrace(uint32_t*& traceData, uint32_t nSamples)
{
  if (nSamples == 0) {
    return 0 ;
  }

  // Limit to max number of samples so we don't overrun trace buffer on host
  uint32_t maxNumSamples = getMaxNumTraceSamples();
  uint32_t numSamples = (nSamples > maxNumSamples) ? maxNumSamples : nSamples;
    
  uint32_t traceBufSz = 0;
  uint32_t traceSamples = 0;

  // Get the trace buffer size and actual number of samples 
  //  for the specific device.  On Zynq, we store 2 samples per packet
  //  in the FIFO.  So the actual number of samples will be different
  //  from the already calculated "numSamples"

  getDevice()->getTraceBufferInfo(numSamples, traceSamples /*actual no. of samples for specific device*/, traceBufSz);

  traceData = new uint32_t[traceBufSz];
  uint32_t wordsPerSample = 1;
  getDevice()->readTraceData(traceData, traceBufSz, numSamples/* use numSamples */, getBaseAddress(), wordsPerSample);

  return traceSamples * sizeof(uint64_t) ; // Actual number of bytes used
}

void TraceFifoFull::showProperties()
{
    std::ostream* outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceFifoFull " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

