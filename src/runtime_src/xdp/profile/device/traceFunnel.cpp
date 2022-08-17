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

#include "traceFunnel.h"
#include <chrono>
#include <thread>

constexpr uint32_t SW_TRACE_OFFSET = 0x0;
constexpr uint32_t SW_RESET_OFFSET = 0xc;
constexpr uint32_t HOST_TIMESTAMP_MASK = 0xFFFF;

constexpr uint32_t SHIFT_TIMESTAMP_2 = 16;
constexpr uint32_t SHIFT_TIMESTAMP_3 = 32;
constexpr uint32_t SHIFT_TIMESTAMP_4 = 48;

constexpr unsigned int US_BETWEEN_WRITES = 10;

namespace xdp {

TraceFunnel::TraceFunnel(Device* handle /** < [in] the xrt or hal device handle */,
                         uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data)
    : ProfileIP(handle, index, data),
      properties(0),
      major_version(0),
      minor_version(0)
{
    if (data) {
        properties = data->m_properties;
        major_version = data->m_major;
        minor_version = data->m_minor;
    }
}

size_t TraceFunnel::initiateClockTraining()
{
    size_t size = 0;
    uint32_t regValue = 0;

    for(int i = 0; i < 2 ; i++) {
      uint64_t hostTimeStamp = getDevice()->getTraceTime();
      regValue = static_cast <uint32_t> (hostTimeStamp & HOST_TIMESTAMP_MASK);
      size += write(SW_TRACE_OFFSET, 4, &regValue);
      regValue = static_cast <uint32_t> (hostTimeStamp >> SHIFT_TIMESTAMP_2 & HOST_TIMESTAMP_MASK);
      size += write(SW_TRACE_OFFSET, 4, &regValue);
      regValue = static_cast <uint32_t> (hostTimeStamp >> SHIFT_TIMESTAMP_3 & HOST_TIMESTAMP_MASK);
      size += write(SW_TRACE_OFFSET, 4, &regValue);
      regValue = static_cast <uint32_t> (hostTimeStamp >> SHIFT_TIMESTAMP_4 & HOST_TIMESTAMP_MASK);
      size += write(SW_TRACE_OFFSET, 4, &regValue);
      std::this_thread::sleep_for(std::chrono::microseconds(US_BETWEEN_WRITES));
   }
    return size;
}

void TraceFunnel::reset()
{
    uint32_t regValue = 0x1;
    write(SW_RESET_OFFSET, 4, &regValue);
}

void TraceFunnel::showProperties()
{
    std::ostream* outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceFunnel " << std::endl;
    ProfileIP::showProperties();
}

/*
 * Returns  1 if Version2 > Current Version1
 * Returns  0 if Version2 = Current Version1
 * Returns -1 if Version2 < Current Version1
 */
signed int TraceFunnel::compareVersion(unsigned major2, unsigned minor2) const
{
    if (major2 > major_version)
      return 1;
    else if (major2 < major_version)
      return -1;
    else if (minor2 > minor_version)
      return 1;
    else if (minor2 < minor_version)
      return -1;
    else return 0;
}

}   // namespace xdp

