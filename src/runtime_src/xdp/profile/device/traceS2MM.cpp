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

#include "traceS2MM.h"

namespace xdp {

TraceS2MM::TraceS2MM(void* handle /** < [in] the xrt hal device handle */,
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

void TraceS2MM::initiateOffload()
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::initiateOffload " << std::endl;

}

void TraceS2MM::readBuffer()
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::readBuffer " << std::endl;

}

void TraceS2MM::endOffload()
{
    if(out_stream)
        (*out_stream) << " TraceS2MM::endOffload " << std::endl;

}

void TraceS2MM::showProperties()
{
    std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << " TraceS2MM " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

