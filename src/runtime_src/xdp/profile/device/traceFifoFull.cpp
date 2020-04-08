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

#include "xdp/profile/core/rt_util.h"

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

uint32_t TraceFifoFull::readTrace(xclTraceResultsVector& traceVector, uint32_t nSamples)
{
    if(out_stream)
      (*out_stream) << " TraceFifoFull::readTrace " << std::endl;
   
    if(!nSamples) {
      return 0;
    }

    // Limit to max number of samples so we don't overrun trace buffer on host
    uint32_t maxNumSamples = getMaxNumTraceSamples();
    uint32_t numSamples    = (nSamples > maxNumSamples) ? maxNumSamples : nSamples;
    
    uint32_t traceBufSz = 0;
    uint32_t traceSamples = 0; 

    /* Get the trace buffer size and actual number of samples for the specific device
     * On Zynq, we store 2 samples per packet in the FIFO. So, actual number of samples
     * will be different from the already calculated "numSamples".
     */
    getDevice()->getTraceBufferInfo(numSamples, traceSamples /*actual no. of samples for specific device*/, traceBufSz);
    traceVector.mLength = traceSamples;

    uint32_t *traceBuf = new uint32_t[traceBufSz];
    uint32_t wordsPerSample = 1;
    getDevice()->readTraceData(traceBuf, traceBufSz, numSamples/* use numSamples */, getBaseAddress(), wordsPerSample);

    processTraceData(traceVector, numSamples, traceBuf, wordsPerSample); 

    delete [] traceBuf;
    return 0;
}

void TraceFifoFull::processTraceData(xclTraceResultsVector& traceVector,uint32_t numSamples, void* data, uint32_t /*wordsPerSample*/)
{
    xclTraceResults results = {};
    int mod = 0;
    unsigned int clockWordIndex = 7;
    for (uint32_t i = 0; i < numSamples; i++) {

      // Old method has issues with emulation trace
      //uint32_t index = wordsPerSample * i;
      //uint32_t* dataUInt32Ptr = (uint32_t*)data;
      //uint64_t currentSample = *(dataUInt32Ptr + index) | (uint64_t)*(dataUInt32Ptr + index + 1) << 32;
      // Works with HW and HW Emu

      uint64_t* dataUInt64Ptr = (uint64_t*)data;
      uint64_t currentSample = dataUInt64Ptr[i];

      if (!currentSample)
        continue;

      bool isClockTrain = false;
      if (mTraceFormat == 1) {
        isClockTrain = ((currentSample >> 63) & 0x1);
      } else {
        isClockTrain = (i <= clockWordIndex && !mclockTrainingdone);
      }

      // Poor Man's reset
      if (i == 0 && !mclockTrainingdone)
        mfirstTimestamp = currentSample & 0x1FFFFFFFFFFF;

      // This section assumes that we write 8 timestamp packets in startTrace
      if (isClockTrain) {
        if (mod == 0) {
          uint64_t currentTimestamp = currentSample & 0x1FFFFFFFFFFF;
          if (currentTimestamp >= mfirstTimestamp)
            results.Timestamp = currentTimestamp - mfirstTimestamp;
          else
            results.Timestamp = currentTimestamp + (0x1FFFFFFFFFFF - mfirstTimestamp);
        }
        uint64_t partial = (((currentSample >> 45) & 0xFFFF) << (16 * mod));
        results.HostTimestamp = results.HostTimestamp | partial;

        if(out_stream)
            (*out_stream) << "Updated partial host timestamp : " << std::hex << partial << std::endl;

        if (mod == 3) {
          if(out_stream) {
            (*out_stream) << "  Trace sample " << std::dec << i << ": "
                          << " Timestamp : " << results.Timestamp << "   "
                          << " Host Timestamp : " << std::hex << results.HostTimestamp << std::endl;
          }
          results.isClockTrain = 1 ;
          traceVector.mArray[static_cast<int>(i/4)] = results;    // save result
          memset(&results, 0, sizeof(xclTraceResults));
        }
        mod = (mod == 3) ? 0 : mod + 1;
        continue;
      }

      // Trace Packet Format
      results.Timestamp = (currentSample & 0x1FFFFFFFFFFF) - mfirstTimestamp;
      results.EventType = ((currentSample >> 45) & 0xF) ? XCL_PERF_MON_END_EVENT :
          XCL_PERF_MON_START_EVENT;
      results.TraceID = (currentSample >> 49) & 0xFFF;
      results.Reserved = (currentSample >> 61) & 0x1;
      results.Overflow = (currentSample >> 62) & 0x1;
      results.Error = (currentSample >> 63) & 0x1;
      results.EventID = XCL_PERF_MON_HW_EVENT;
      results.EventFlags = ((currentSample >> 45) & 0xF) | ((currentSample >> 57) & 0x10) ;
      results.isClockTrain = 0 ;

      int idx = mclockTrainingdone ? i : i - clockWordIndex + 1;
      if (idx < 0)
        continue;
      traceVector.mArray[idx] = results;   // save result

      if(out_stream) {
        uint64_t previousTimestamp = 0;
        auto packet_dec = std::bitset<64>(currentSample).to_string();
        (*out_stream) << "  Trace sample " << std::dec << std::setw(5) << i << ": "
                      <<  packet_dec.substr(0,19) << " : " << packet_dec.substr(19)
                      << std::endl
                      << " Timestamp : " << results.Timestamp << "   "
                      << "Event Type : " << results.EventType << "   "
                      << "slotID : " << results.TraceID << "   "
                      << "Start, Stop : " << static_cast<int>(results.Reserved) << "   "
                      << "Overflow : " << static_cast<int>(results.Overflow) << "   "
                      << "Error : " << static_cast<int>(results.Error) << "   "
                      << "EventFlags : " << static_cast<int>(results.EventFlags) << "   "
                      << "Interval : " << results.Timestamp - previousTimestamp << "   "
                      << std::endl;
        previousTimestamp = results.Timestamp;
      }
      memset(&results, 0, sizeof(xclTraceResults));
    }
    mclockTrainingdone = true;
}

void TraceFifoFull::showProperties()
{
    std::ostream* outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceFifoFull " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

