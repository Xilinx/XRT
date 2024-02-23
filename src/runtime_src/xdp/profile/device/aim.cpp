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

#include "core/include/xdp/aim.h"
#include "xdp/profile/device/aim.h"
#include "xdp/profile/device/utility.h"
#include <bitset>

namespace xdp {

AIM::AIM(Device* handle /** < [in] the xrt or hal device handle */,
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

size_t AIM::startCounter()
{
    if(out_stream)
        (*out_stream) << " AIM::startCounter " << std::endl;

    size_t size = 0;
    uint32_t regValue = 0;
    
    // 1. Reset AXI - MM monitor metric counters
    size += read(IP::AIM::AXI_LITE::CONTROL, 4, &regValue);

    regValue = regValue | IP::AIM::mask::CR_COUNTER_RESET;
    size += write(IP::AIM::AXI_LITE::CONTROL, 4, &regValue);

    regValue = regValue & ~(IP::AIM::mask::CR_COUNTER_RESET);
    size += write(IP::AIM::AXI_LITE::CONTROL, 4, &regValue);

    // 2. Start AXI-MM monitor metric counters
    regValue = regValue | IP::AIM::mask::CR_COUNTER_ENABLE;
    size += write(IP::AIM::AXI_LITE::CONTROL, 4, &regValue);

    // 3. Read from sample register to ensure total time is read again at end
    size += read(IP::AIM::AXI_LITE::SAMPLE, 4, &regValue);

    return size;
}

size_t AIM::stopCounter()
{
    if(out_stream)
        (*out_stream) << " AIM::stopCounter " << std::endl;

    size_t size = 0;
    uint32_t regValue = 0;

    // 1. Stop AIM metric counters
    size += read(IP::AIM::AXI_LITE::CONTROL, 4, &regValue);

    regValue = regValue & ~(IP::AIM::mask::CR_COUNTER_ENABLE);
    size += write(IP::AIM::AXI_LITE::CONTROL, 4, &regValue);

    return size;
}

size_t AIM::readCounter(xdp::CounterResults& counterResults)
{
    if(out_stream)
        (*out_stream) << " AIM::readCounter " << std::endl;

    size_t size = 0;
    uint32_t sampleInterval = 0;

    uint64_t s = util::getAIMSlotId(getMIndex());
    
    // Read sample interval register
    // NOTE: this also latches the sampled metric counters
    size += read(IP::AIM::AXI_LITE::SAMPLE, 4, &sampleInterval);

    // The sample interval in the counter results struct is never used,
    //  so don't set it

    size += read(IP::AIM::AXI_LITE::WRITE_BYTES, 4, &counterResults.WriteBytes[s]);
    size += read(IP::AIM::AXI_LITE::WRITE_TRANX, 4, &counterResults.WriteTranx[s]);
    size += read(IP::AIM::AXI_LITE::WRITE_LATENCY, 4, &counterResults.WriteLatency[s]);
    size += read(IP::AIM::AXI_LITE::READ_BYTES, 4, &counterResults.ReadBytes[s]);
    size += read(IP::AIM::AXI_LITE::READ_TRANX, 4, &counterResults.ReadTranx[s]);
    size += read(IP::AIM::AXI_LITE::READ_LATENCY, 4, &counterResults.ReadLatency[s]);
    size += read(IP::AIM::AXI_LITE::READ_BUSY_CYCLES, 4, &counterResults.ReadBusyCycles[s]);
    size += read(IP::AIM::AXI_LITE::WRITE_BUSY_CYCLES, 4, &counterResults.WriteBusyCycles[s]);

    // Read upper 32 bits (if available)
    if(has64bit()) {
        uint64_t upper[8] = {};
        size += read(IP::AIM::AXI_LITE::WRITE_BYTES_UPPER, 4, &upper[0]);
        size += read(IP::AIM::AXI_LITE::WRITE_TRANX_UPPER, 4, &upper[1]);
        size += read(IP::AIM::AXI_LITE::WRITE_LATENCY_UPPER, 4, &upper[2]);
        size += read(IP::AIM::AXI_LITE::READ_BYTES_UPPER, 4, &upper[3]);
        size += read(IP::AIM::AXI_LITE::READ_TRANX_UPPER, 4, &upper[4]);
        size += read(IP::AIM::AXI_LITE::READ_LATENCY_UPPER, 4, &upper[5]);
        size += read(IP::AIM::AXI_LITE::READ_BUSY_CYCLES_UPPER, 4, &upper[6]);
        size += read(IP::AIM::AXI_LITE::WRITE_BUSY_CYCLES_UPPER, 4, &upper[7]);

        counterResults.WriteBytes[s]      += (upper[0] << 32);
        counterResults.WriteTranx[s]      += (upper[1] << 32);
        counterResults.WriteLatency[s]    += (upper[2] << 32);
        counterResults.ReadBytes[s]       += (upper[3] << 32);
        counterResults.ReadTranx[s]       += (upper[4] << 32);
        counterResults.ReadLatency[s]     += (upper[5] << 32);
        counterResults.ReadBusyCycles[s]  += (upper[6] << 32);
        counterResults.WriteBusyCycles[s] += (upper[7] << 32);

#if 0
        if(out_stream) {
          (*out_stream) << "AXI Interface Monitor Upper 32, slot " << s << std::endl
                        << "  WriteBytes : " << upper[0] << std::endl
                        << "  WriteTranx : " << upper[1] << std::endl
                        << "  WriteLatency : " << upper[2] << std::endl
                        << "  ReadBytes : " << upper[3] << std::endl
                        << "  ReadTranx : " << upper[4] << std::endl
                        << "  ReadLatency : " << upper[5] << std::endl
                        << "  ReadBusyCycles : " << upper[6] << std::endl
                        << "  WriteBusyCycles : " << upper[7] << std::endl;
        }
#endif
    }


    if(out_stream) {
        (*out_stream) << "Reading AXI Interface Monitor... SlotNum : " << s << std::endl
                      << "Reading AXI Interface Monitor... WriteBytes : " << counterResults.WriteBytes[s] << std::endl
                      << "Reading AXI Interface Monitor... WriteTranx : " << counterResults.WriteTranx[s] << std::endl
                      << "Reading AXI Interface Monitor... WriteLatency : " << counterResults.WriteLatency[s] << std::endl
                      << "Reading AXI Interface Monitor... ReadBytes : " << counterResults.ReadBytes[s] << std::endl
                      << "Reading AXI Interface Monitor... ReadTranx : " << counterResults.ReadTranx[s] << std::endl
                      << "Reading AXI Interface Monitor... ReadLatency : " << counterResults.ReadLatency[s] << std::endl
                      << "Reading AXI Interface Monitor... ReadBusyCycles : " << counterResults.ReadBusyCycles[s] << std::endl
                      << "Reading AXI Interface Monitor... WriteBusyCycles : " << counterResults.WriteBusyCycles[s] << std::endl;
    }

    return size;
}

size_t AIM::triggerTrace(uint32_t traceOption /* starttrigger*/)
{
    size_t size = 0;
    uint32_t regValue = 0;
    // Set AIM trace control register bits
    regValue = traceOption & IP::AIM::mask::TRACE_CTRL;
    size += write(IP::AIM::AXI_LITE::TRACE_CTRL, 4, &regValue);

    return size;
}

bool AIM::isHostMonitor() const
{
  return ((properties & IP::AIM::mask::PROPERTY_HOST) ? true : false);
}

bool AIM::isShellMonitor() 
{
    if(isHostMonitor() && (getName().find("HOST" /*IP_LAYOUT_HOST_NAME*/) == std::string::npos)) {
       return true;
    }
    return false;
}

bool AIM::has64bit() const
{
  return ((properties & IP::AIM::mask::PROPERTY_64BIT) ? true : false);
}

void AIM::showProperties()
{
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << " AIM " << std::endl;
    ProfileIP::showProperties();
}

bool AIM::hasCoarseMode() const
{
    return ((properties & IP::AIM::mask::PROPERTY_COARSE_MODE_OFF) ? false : true);
}

}   // namespace xdp

