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

#define AXI_FIFO_RLR                    0x24
#define AXI_FIFO_RESET_VALUE            0xA5

#define AXI_FIFO_RDFD_AXI_FULL          0x1000

#define MAX_TRACE_NUMBER_SAMPLES                        16384

#define XPAR_AXI_PERF_MON_0_TRACE_NUMBER_FIFO           3
#define XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH            64
#define XPAR_AXI_PERF_MON_0_TRACE_NUMBER_SAMPLES        8192


#define XPAR_AXI_PERF_MON_1_TRACE_NUMBER_FIFO           0
#define XPAR_AXI_PERF_MON_1_TRACE_WORD_WIDTH            0
#define XPAR_AXI_PERF_MON_1_TRACE_NUMBER_SAMPLES        0

#define XPAR_AXI_PERF_MON_2_TRACE_WORD_WIDTH            64
#define XPAR_AXI_PERF_MON_2_TRACE_NUMBER_SAMPLES        8192

#define XPAR_AXI_PERF_MON_2_TRACE_OFFSET_0              0x01000
#define XPAR_AXI_PERF_MON_2_TRACE_OFFSET_1              0x02000
#define XPAR_AXI_PERF_MON_2_TRACE_OFFSET_2              0x03000

#include<iomanip>
#include<cstring>

#include "core/common/memalign.h"

namespace xdp {


  // Memory alignment for DDR and AXI-MM trace access
  template <typename T> class AlignedAllocator {
      void *mBuffer;
      size_t mCount;
  public:
      T *getBuffer() {
          return (T *)mBuffer;
      }

      size_t size() const {
          return mCount * sizeof(T);
      }

      AlignedAllocator(size_t alignment, size_t count) : mBuffer(0), mCount(count) {
        if (xrt_core::posix_memalign(&mBuffer, alignment, count * sizeof(T))) {
              mBuffer = 0;
          }
      }
      ~AlignedAllocator() {
          if (mBuffer)
              free(mBuffer);
      }
  };




// Convert decimal to binary string
// NOTE: length of string is always sizeof(uint32_t) * 8
std::string dec2bin(uint32_t n) {
    char result[(sizeof(uint32_t) * 8) + 1];
    unsigned index = sizeof(uint32_t) * 8;
    result[index] = '\0';

    do {
      result[ --index ] = '0' + (n & 1);
    } while (n >>= 1);

    for (int i=index-1; i >= 0; --i)
      result[i] = '0';

    return std::string( result );
}

// Convert decimal to binary string of length bits
std::string dec2bin(uint32_t n, unsigned bits) {
    char result[bits + 1];
    unsigned index = bits;
    result[index] = '\0';

    do result[ --index ] = '0' + (n & 1);
    while (n >>= 1);

    for (int i=index-1; i >= 0; --i)
      result[i] = '0';

    return std::string( result );
}


TraceFifoFull::TraceFifoFull(void* handle /** < [in] the xrt hal device handle */,
                int index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data)
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

    return 0;
}

uint32_t TraceFifoFull::getMaxNumTraceSamples()
{
    return XPAR_AXI_PERF_MON_0_TRACE_NUMBER_SAMPLES;
    
#if 0
 if (type == XCL_PERF_MON_MEMORY) return XPAR_AXI_PERF_MON_0_TRACE_NUMBER_SAMPLES;
 if (type == XCL_PERF_MON_HOST) return XPAR_AXI_PERF_MON_1_TRACE_NUMBER_SAMPLES;
 // TODO: get number of samples from metadata
 if (type == XCL_PERF_MON_ACCEL) return XPAR_AXI_PERF_MON_2_TRACE_NUMBER_SAMPLES;

#endif
}

uint32_t TraceFifoFull::readTrace(xclTraceResultsVector& traceVector, uint32_t nSamples)
{
    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
        (*out_stream) << " TraceFifoFull::readTrace " << std::endl;
   
    if(!nSamples) {
        return 0;
    }

    size_t size = 0;
    // Limit to max number of samples so we don't overrun trace buffer on host
    uint32_t maxNumSamples = getMaxNumTraceSamples();
    uint32_t numSamples    = (nSamples > maxNumSamples) ? maxNumSamples : nSamples;
    traceVector.mLength = numSamples;

    const uint32_t bytesPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 8);
    const uint32_t wordsPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 32);

    uint32_t numWords = numSamples * wordsPerSample;

    // Create trace buffer on host (requires alignment)
    const int BUFFER_BYTES = MAX_TRACE_NUMBER_SAMPLES * bytesPerSample;
    const int BUFFER_WORDS = MAX_TRACE_NUMBER_SAMPLES * wordsPerSample;

#ifndef _WINDOWS
// TODO: Windows build support
//    alignas is defined in c++11
#if GCC_VERSION >= 40800
    /* Alignment is limited to 16 by PPC64LE : so , should it be 
    alignas(16) uint32_t hostbuf[BUFFER_WORDS];
    */
    alignas(AXI_FIFO_RDFD_AXI_FULL) uint32_t hostbuf[BUFFER_WORDS];
#else
    AlignedAllocator<uint32_t> alignedBuffer(AXI_FIFO_RDFD_AXI_FULL, BUFFER_WORDS);
    uint32_t* hostbuf = alignedBuffer.getBuffer();
#endif
#else
    uint32_t hostbuf[BUFFER_WORDS];
#endif

    // Now read trace data
    memset((void *)hostbuf, 0, BUFFER_BYTES);
    
    // Iterate over chunks
    // NOTE: AXI limits this to 4K bytes per transfer
    uint32_t chunkSizeWords = 256 * wordsPerSample;
    if (chunkSizeWords > 1024) chunkSizeWords = 1024;
    uint32_t chunkSizeBytes = 4 * chunkSizeWords;
    uint32_t words=0;

    // Read trace a chunk of bytes at a time
    if (numWords > chunkSizeWords) {
      for (; words < (numWords-chunkSizeWords); words += chunkSizeWords) {
          if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
            (*out_stream) << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
                          << std::hex << (getBaseAddress() + AXI_FIFO_RDFD_AXI_FULL) /*fifoReadAddress[0] or AXI_FIFO_RDFD*/ << " and writing it to 0x"
                          << (void *)(hostbuf + words) << std::dec << std::endl;

        unmgdRead(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, AXI_FIFO_RDFD_AXI_FULL /*offset : or AXI_FIFO_RDFD*/);

        size += chunkSizeBytes;
      }
    }    

    // Read remainder of trace not divisible by chunk size
    if (words < numWords) {
      chunkSizeBytes = 4 * (numWords - words);

      if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) {
        (*out_stream) << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
                      << std::hex << (getBaseAddress() + AXI_FIFO_RDFD_AXI_FULL) /*fifoReadAddress[0]*/ << " and writing it to 0x"
                      << (void *)(hostbuf + words) << std::dec << std::endl;
      }

      unmgdRead(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, AXI_FIFO_RDFD_AXI_FULL /*offset : or AXI_FIFO_RDFD*/);

      size += chunkSizeBytes;
    }

    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
        (*out_stream) << __func__ << ": done reading " << size << " bytes " << std::endl;


    // ******************************
    // Read & process all trace FIFOs
    // ******************************
    static unsigned long long firstTimestamp;
    xclTraceResults results = {};
    uint64_t previousTimestamp = 0;
    for (uint32_t wordnum=0; wordnum < numSamples; wordnum++) {
      uint32_t index = wordsPerSample * wordnum;
      uint64_t temp = 0;

      temp = *(hostbuf + index) | (uint64_t)*(hostbuf + index + 1) << 32;
      if (!temp)
        continue;
      // Poor Man's reset
      if (wordnum == 0)
        firstTimestamp = temp & 0x1FFFFFFFFFFF;

      // This section assumes that we write 8 timestamp packets in startTrace
      int mod = (wordnum % 4);
      unsigned int clockWordIndex = 7;
      if (wordnum > clockWordIndex || mod == 0) {
        memset(&results, 0, sizeof(xclTraceResults));
      }
      if (wordnum <= clockWordIndex) {
        if (mod == 0) {
          uint64_t currentTimestamp = temp & 0x1FFFFFFFFFFF;
          if (currentTimestamp >= firstTimestamp)
            results.Timestamp = currentTimestamp - firstTimestamp;
          else
            results.Timestamp = currentTimestamp + (0x1FFFFFFFFFFF - firstTimestamp);
        }
        uint64_t partial = (((temp >> 45) & 0xFFFF) << (16 * mod));
        results.HostTimestamp = results.HostTimestamp | partial;

        if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
            (*out_stream) << "Updated partial host timestamp : " << std::hex << partial << std::endl;

        if (mod == 3) {
          if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
            (*out_stream) << "  Trace sample " << std::dec << wordnum << ": "
                          << " Timestamp : " << results.Timestamp << "   "
                          << " Host Timestamp : " << std::hex << results.HostTimestamp << std::endl;
  
          traceVector.mArray[static_cast<int>(wordnum/4)] = results;    // save result
        }
        continue;
      }

      // Zynq Packet Format
      results.Timestamp = (temp & 0x1FFFFFFFFFFF) - firstTimestamp;
      results.EventType = ((temp >> 45) & 0xF) ? XCL_PERF_MON_END_EVENT :
          XCL_PERF_MON_START_EVENT;
      results.TraceID = (temp >> 49) & 0xFFF;
      results.Reserved = (temp >> 61) & 0x1;
      results.Overflow = (temp >> 62) & 0x1;
      results.Error = (temp >> 63) & 0x1;
      results.EventID = XCL_PERF_MON_HW_EVENT;
      results.EventFlags = ((temp >> 45) & 0xF) | ((temp >> 57) & 0x10) ;

      traceVector.mArray[wordnum - clockWordIndex + 1] = results;   // save result

      if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) {
        (*out_stream) << "  Trace sample " << std::dec << std::setw(5) << wordnum << ": "
                      << dec2bin(uint32_t(temp>>32)) << " " << dec2bin(uint32_t(temp&0xFFFFFFFF))
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
    }

    return size;
}

void TraceFifoFull::showProperties()
{
    std::ostream* outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceFifoFull " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

