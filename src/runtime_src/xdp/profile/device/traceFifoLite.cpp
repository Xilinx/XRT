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

#include "traceFifoLite.h"

#define AXI_FIFO_RLR                    0x24
#define AXI_FIFO_RESET_VALUE            0xA5
#define AXI_FIFO_SRR                    0x28
#define AXI_FIFO_RDFR                   0x18


namespace xdp {

TraceFifoLite::TraceFifoLite(Device* handle /** < [in] the xrt or hal device handle */,
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

uint32_t TraceFifoLite::getNumTraceSamples()
{
    uint32_t fifoCount = 0;
    uint32_t numSamples = 0;
    uint32_t numBytes = 0;

    read(AXI_FIFO_RLR, 4, &fifoCount);
    // Read bits 22:0 per AXI-Stream FIFO product guide (PG080, 10/1/14)
    numBytes = fifoCount & 0x7FFFFF;
    numSamples = numBytes / (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH/8);

    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
      (*out_stream) << "  No. of trace samples = " << std::dec << numSamples
                    << " (fifoCount = 0x" << std::hex << fifoCount << ")" << std::dec << std::endl;

    return numSamples;
}

size_t TraceFifoLite::reset()
{
    size_t size = 0;
    uint32_t regValue = AXI_FIFO_RESET_VALUE;

    size += write(AXI_FIFO_SRR  /*core address*/, 4, &regValue);
    size += write(AXI_FIFO_RDFR /*fifo address*/, 4, &regValue);

    return size;
}

void TraceFifoLite::showProperties()
{
    std::ostream* outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceFifoLite " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

