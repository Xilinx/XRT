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
#include<chrono>
#include <unistd.h>

namespace xdp {

TraceFunnel::TraceFunnel(Device* handle /** < [in] the xrt or hal device handle */,
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

size_t TraceFunnel::initiateClockTraining()
{
    size_t size = 0;
    uint32_t regValue = 0;    


    for(int i = 0; i < 2 ; i++) {
 // Get host timestamp to write to APM
 // IMPORTANT NOTE: this *must* be compatible with the method of generating
 // timestamps as defined in RTProfile::getTraceTime()
// uint64_t shim::getHostTraceTimeNsec() {
   using namespace std::chrono;
   typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
   duration_ns time_span =
       duration_cast<duration_ns>(high_resolution_clock::now().time_since_epoch());
   uint64_t hostTimeStamp = time_span.count();
// }

   regValue = static_cast <uint32_t> (hostTimeStamp & 0xFFFF);
   size += write(0, 4, &regValue);
   regValue = static_cast <uint32_t> (hostTimeStamp >> 16 & 0xFFFF);
   size += write(0, 4, &regValue);
   regValue = static_cast <uint32_t> (hostTimeStamp >> 32 & 0xFFFF);
   size += write(0, 4, &regValue);
   regValue = static_cast <uint32_t> (hostTimeStamp >> 48 & 0xFFFF);
   size += write(0, 4, &regValue);
   usleep(10);  // Linux specific
    }

    return size;


}

void TraceFunnel::showProperties()
{
    std::ostream* outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceFunnel " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

