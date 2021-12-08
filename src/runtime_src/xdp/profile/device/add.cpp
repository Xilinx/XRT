/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "add.h"

#define SYSTEM_DEADLOCK_OFFSET  0x0

namespace xdp {

DeadlockDetector::DeadlockDetector(Device* handle /** < [in] the xrt or hal device handle */
                            , uint64_t index /** < [in] the index of the IP in debug_ip_layout */
                            , debug_ip_data* data)
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

uint32_t DeadlockDetector::getDeadlockStatus()
{
    uint32_t reg = 0;
    read(SYSTEM_DEADLOCK_OFFSET, sizeof(uint32_t), &reg);
    return reg;
}

size_t DeadlockDetector::reset()
{
    return 0;
}

void DeadlockDetector::showProperties()
{
    std::ostream* outputStream = (out_stream /*  && out_stream->is_open() && out_stream->is_open() out_stream->is_open()*/) ? out_stream : (&(std::cout));
    (*outputStream) << " DeadlockDetector " << std::endl;
    ProfileIP::showProperties();
}


}   // namespace xdp

