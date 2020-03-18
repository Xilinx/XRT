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

/************************ AXI Interface Monitor (AIM, earlier SPM) ***********************/

/* Address offsets in core */
#define XAIM_CONTROL_OFFSET                          0x08
#define XAIM_TRACE_CTRL_OFFSET                       0x10
#define XAIM_SAMPLE_OFFSET                           0x20
#define XAIM_SAMPLE_WRITE_BYTES_OFFSET               0x80
#define XAIM_SAMPLE_WRITE_TRANX_OFFSET               0x84
#define XAIM_SAMPLE_WRITE_LATENCY_OFFSET             0x88
#define XAIM_SAMPLE_READ_BYTES_OFFSET                0x8C
#define XAIM_SAMPLE_READ_TRANX_OFFSET                0x90
#define XAIM_SAMPLE_READ_LATENCY_OFFSET              0x94
// The following two registers are still in the hardware,
//  but are unused
//#define XAIM_SAMPLE_MIN_MAX_WRITE_LATENCY_OFFSET   0x98
//#define XAIM_SAMPLE_MIN_MAX_READ_LATENCY_OFFSET    0x9C
#define XAIM_SAMPLE_OUTSTANDING_COUNTS_OFFSET        0xA0
#define XAIM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET        0xA4
#define XAIM_SAMPLE_LAST_WRITE_DATA_OFFSET           0xA8
#define XAIM_SAMPLE_LAST_READ_ADDRESS_OFFSET         0xAC
#define XAIM_SAMPLE_LAST_READ_DATA_OFFSET            0xB0
#define XAIM_SAMPLE_READ_BUSY_CYCLES_OFFSET          0xB4
#define XAIM_SAMPLE_WRITE_BUSY_CYCLES_OFFSET         0xB8
#define XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET         0xC0
#define XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET         0xC4
#define XAIM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET       0xC8
#define XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET          0xCC
#define XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET          0xD0
#define XAIM_SAMPLE_READ_LATENCY_UPPER_OFFSET        0xD4
// Reserved for high 32-bits of MIN_MAX_WRITE_LATENCY - 0xD8
// Reserved for high 32-bits of MIN_MAX_READ_LATENCY  - 0xDC
#define XAIM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET  0xE0
#define XAIM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET  0xE4
#define XAIM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET     0xE8
#define XAIM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET   0xEC
#define XAIM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET      0xF0
#define XAIM_SAMPLE_READ_BUSY_CYCLES_UPPER_OFFSET    0xF4
#define XAIM_SAMPLE_WRITE_BUSY_CYCLES_UPPER_OFFSET   0xF8

/* SPM Control Register masks */
#define XAIM_CR_COUNTER_RESET_MASK               0x00000002
#define XAIM_CR_COUNTER_ENABLE_MASK              0x00000001
#define XAIM_TRACE_CTRL_MASK                     0x00000003        

/* Debug IP layout properties mask bits */
#define XAIM_HOST_PROPERTY_MASK                  0x4
#define XAIM_64BIT_PROPERTY_MASK                 0x8

#define XDP_SOURCE

#include "aim.h"
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
    size += read(XAIM_CONTROL_OFFSET, 4, &regValue);

    regValue = regValue | XAIM_CR_COUNTER_RESET_MASK;
    size += write(XAIM_CONTROL_OFFSET, 4, &regValue);

    regValue = regValue & ~(XAIM_CR_COUNTER_RESET_MASK);
    size += write(XAIM_CONTROL_OFFSET, 4, &regValue);

    // 2. Start AXI-MM monitor metric counters
    regValue = regValue | XAIM_CR_COUNTER_ENABLE_MASK;
    size += write(XAIM_CONTROL_OFFSET, 4, &regValue);

    // 3. Read from sample register to ensure total time is read again at end
    size += read(XAIM_SAMPLE_OFFSET, 4, &regValue);

    return size;
}

size_t AIM::stopCounter()
{
    if(out_stream)
        (*out_stream) << " AIM::stopCounter " << std::endl;

    size_t size = 0;
    uint32_t regValue = 0;

    // 1. Stop AIM metric counters
    size += read(XAIM_CONTROL_OFFSET, 4, &regValue);

    regValue = regValue & ~(XAIM_CR_COUNTER_ENABLE_MASK);
    size += write(XAIM_CONTROL_OFFSET, 4, &regValue);

    return size;
}

size_t AIM::readCounter(xclCounterResults& counterResults, uint32_t s /*index*/)
{
    if(out_stream)
        (*out_stream) << " AIM::readCounter " << std::endl;

    size_t size = 0;
    uint32_t sampleInterval = 0;
    
    // Read sample interval register
    // NOTE: this also latches the sampled metric counters
    size += read(XAIM_SAMPLE_OFFSET, 4, &sampleInterval);

    // Samples are taken almost immediately and it is assumed that the intervals are close to each other.
    // So, only one sample interval reading is okay.
    if (s==0 && getDevice()) {
       counterResults.SampleIntervalUsec = static_cast<float>(sampleInterval / (getDevice()->getDeviceClock()));
    }

    size += read(XAIM_SAMPLE_WRITE_BYTES_OFFSET, 4, &counterResults.WriteBytes[s]);
    size += read(XAIM_SAMPLE_WRITE_TRANX_OFFSET, 4, &counterResults.WriteTranx[s]);
    size += read(XAIM_SAMPLE_WRITE_LATENCY_OFFSET, 4, &counterResults.WriteLatency[s]);
    size += read(XAIM_SAMPLE_READ_BYTES_OFFSET, 4, &counterResults.ReadBytes[s]);
    size += read(XAIM_SAMPLE_READ_TRANX_OFFSET, 4, &counterResults.ReadTranx[s]);
    size += read(XAIM_SAMPLE_READ_LATENCY_OFFSET, 4, &counterResults.ReadLatency[s]);
    size += read(XAIM_SAMPLE_READ_BUSY_CYCLES_OFFSET, 4, &counterResults.ReadBusyCycles[s]);
    size += read(XAIM_SAMPLE_WRITE_BUSY_CYCLES_OFFSET, 4, &counterResults.WriteBusyCycles[s]);

    // Read upper 32 bits (if available)
    if(has64bit()) {
        uint64_t upper[8] = {};
        size += read(XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET, 4, &upper[0]);
        size += read(XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET, 4, &upper[1]);
        size += read(XAIM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET, 4, &upper[2]);
        size += read(XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET, 4, &upper[3]);
        size += read(XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET, 4, &upper[4]);
        size += read(XAIM_SAMPLE_READ_LATENCY_UPPER_OFFSET, 4, &upper[5]);
        size += read(XAIM_SAMPLE_READ_BUSY_CYCLES_UPPER_OFFSET, 4, &upper[6]);
        size += read(XAIM_SAMPLE_WRITE_BUSY_CYCLES_UPPER_OFFSET, 4, &upper[7]);

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
    regValue = traceOption & XAIM_TRACE_CTRL_MASK;
    size += write(XAIM_TRACE_CTRL_OFFSET, 4, &regValue);

    return size;
}

bool AIM::isHostMonitor() const
{
    return ((properties & XAIM_HOST_PROPERTY_MASK) ? true : false);
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
    return ((properties & XAIM_64BIT_PROPERTY_MASK) ? true : false);
}

void AIM::showProperties()
{
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << " AIM " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

