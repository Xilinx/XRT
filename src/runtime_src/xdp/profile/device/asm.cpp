/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#ifndef _PERFMON_PARAMETERS_H_
#define _PERFMON_PARAMETERS_H_

/************************ AXI Stream FIFOs ************************************/

/* Address offsets in core */
#define AXI_FIFO_RDFR                   0x18
#define AXI_FIFO_RDFO                   0x1c
#define AXI_FIFO_RDFD                   0x20
#define AXI_FIFO_RDFD_AXI_FULL          0x1000
#define AXI_FIFO_TDFD                   0x10
#define AXI_FIFO_RLR                    0x24
#define AXI_FIFO_SRR                    0x28
#define AXI_FIFO_RESET_VALUE            0xA5

/************************** AXI Stream Monitor (ASM, earlier SSPM) *********************/

#define XSSPM_CONTROL_OFFSET           0x0
#define XSSPM_SAMPLE_OFFSET            0x20
#define XSSPM_NUM_TRANX_OFFSET         0x80
#define XSSPM_DATA_BYTES_OFFSET        0x88
#define XSSPM_BUSY_CYCLES_OFFSET       0x90
#define XSSPM_STALL_CYCLES_OFFSET      0x98
#define XSSPM_STARVE_CYCLES_OFFSET     0xA0

/* SSPM Control Mask */
#define XSSPM_COUNTER_RESET_MASK       0x00000001

/********************* AXI Stream Protocol Checker (SPC) *********************/

#define XSPC_PC_ASSERTED_OFFSET 0x0
#define XSPC_CURRENT_PC_OFFSET  0x100
#define XSPC_SNAPSHOT_PC_OFFSET 0x200

#endif /* _PERFMON_PARAMETERS_H_ */



#include "asm.h"


namespace xdp {

ASM::ASM(void* handle /** < [in] the xrt hal device handle */,
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

size_t ASM::startCounter()
{
    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
        (*out_stream) << " ASM::startCounter " << std::endl;

    size_t size = 0;
    uint32_t regValue = 0;
    uint32_t origRegValue = 0;

    size += read(XSSPM_CONTROL_OFFSET, 4, &origRegValue);

    // Reset
    regValue = origRegValue | XSSPM_COUNTER_RESET_MASK;
    size += write(XSSPM_CONTROL_OFFSET, 4, &regValue);

    // Write original value after reset
    size += write(XSSPM_CONTROL_OFFSET, 4, &origRegValue);

    return size;
}

size_t ASM::stopCounter()
{
    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
        (*out_stream) << " ASM::stopCounter " << std::endl;
    return 0;
}

size_t ASM::readCounter(xclCounterResults& counterResults, uint32_t s /*index*/)
{
    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/)
        (*out_stream) << " ASM::readCounter " << std::endl;

    size_t size = 0;
    uint32_t sampleInterval = 0;

    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) {
        (*out_stream) << "Reading AXI Stream Monitors.." << std::endl;
    }

    size += read(XSSPM_SAMPLE_OFFSET, 4, &sampleInterval);

    size += read(XSSPM_NUM_TRANX_OFFSET, 8, &counterResults.StrNumTranx[s]);
    size += read(XSSPM_DATA_BYTES_OFFSET, 8, &counterResults.StrDataBytes[s]);
    size += read(XSSPM_BUSY_CYCLES_OFFSET, 8, &counterResults.StrBusyCycles[s]);
    size += read(XSSPM_STALL_CYCLES_OFFSET, 8, &counterResults.StrStallCycles[s]);
    size += read(XSSPM_STARVE_CYCLES_OFFSET, 8, &counterResults.StrStarveCycles[s]);

    // AXIS without TLAST is assumed to be one long transfer
    if (counterResults.StrNumTranx[s] == 0 && counterResults.StrDataBytes[s] > 0) {
      counterResults.StrNumTranx[s] = 1;
    }

    if(out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) {
        (*out_stream) << "Reading AXI Stream Monitor... SlotNum : " << s << std::endl
                      << "Reading AXI Stream Monitor... NumTranx : " << counterResults.StrNumTranx[s] << std::endl
                      << "Reading AXI Stream Monitor... DataBytes : " << counterResults.StrDataBytes[s] << std::endl
                      << "Reading AXI Stream Monitor... BusyCycles : " << counterResults.StrBusyCycles[s] << std::endl
                      << "Reading AXI Stream Monitor... StallCycles : " << counterResults.StrStallCycles[s] << std::endl
                      << "Reading AXI Stream Monitor... StarveCycles : " << counterResults.StrStarveCycles[s] << std::endl;
    }
    return size;
}

size_t ASM::triggerTrace(uint32_t traceOption /* starttrigger*/)
{
    return 0;
}


void ASM::showProperties()
{
    std::ostream *outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " ASM " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

