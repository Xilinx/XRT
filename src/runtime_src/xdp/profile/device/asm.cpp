/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE
#include "core/include/xdp/asm.h"
#include "xdp/profile/device/asm.h"
#include "xdp/profile/device/utility.h"

namespace xdp {

ASM::ASM(Device* handle /** < [in] the xrt or hal device handle */,
         uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data)
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
    if(out_stream)
        (*out_stream) << " ASM::startCounter " << std::endl;

    size_t size = 0;
    uint32_t regValue = 0;
    uint32_t origRegValue = 0;

    size += read(IP::ASM::AXI_LITE::CONTROL, 4, &origRegValue);

    // Reset
    regValue = origRegValue | IP::ASM::mask::COUNTER_RESET;
    size += write(IP::ASM::AXI_LITE::CONTROL, 4, &regValue);

    // Write original value after reset
    size += write(IP::ASM::AXI_LITE::CONTROL, 4, &origRegValue);

    return size;
}

size_t ASM::stopCounter()
{
    if(out_stream)
        (*out_stream) << " ASM::stopCounter " << std::endl;
    return 0;
}

size_t ASM::readCounter(xdp::CounterResults& counterResults)
{
    if(out_stream)
        (*out_stream) << " ASM::readCounter " << std::endl;

    size_t size = 0;
    uint32_t sampleInterval = 0;

    uint64_t s = util::getASMSlotId(getMIndex());

    if(out_stream) {
        (*out_stream) << "Reading AXI Stream Monitors.." << std::endl;
    }

    size += read(IP::ASM::AXI_LITE::SAMPLE, 4, &sampleInterval);

    size += read(IP::ASM::AXI_LITE::NUM_TRANX, 8, &counterResults.StrNumTranx[s]);
    size += read(IP::ASM::AXI_LITE::DATA_BYTES, 8, &counterResults.StrDataBytes[s]);
    size += read(IP::ASM::AXI_LITE::BUSY_CYCLES, 8, &counterResults.StrBusyCycles[s]);
    size += read(IP::ASM::AXI_LITE::STALL_CYCLES, 8, &counterResults.StrStallCycles[s]);
    size += read(IP::ASM::AXI_LITE::STARVE_CYCLES, 8, &counterResults.StrStarveCycles[s]);

    // AXIS without TLAST is assumed to be one long transfer
    if (counterResults.StrNumTranx[s] == 0 && counterResults.StrDataBytes[s] > 0) {
      counterResults.StrNumTranx[s] = 1;
    }

    if(out_stream) {
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
    // Tun on/off trace as requested
    uint32_t regValue = 0;
    read(IP::ASM::AXI_LITE::CONTROL, 4, &regValue);
    if (traceOption & IP::ASM::mask::TRACE_CTRL)
      regValue |= IP::ASM::mask::TRACE_ENABLE;
    else
      regValue &= (~IP::ASM::mask::TRACE_ENABLE);
    write(IP::ASM::AXI_LITE::CONTROL, 4, &regValue);
    return 0;
}


void ASM::showProperties()
{
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << " ASM " << std::endl;
    ProfileIP::showProperties();
}

}   // namespace xdp
