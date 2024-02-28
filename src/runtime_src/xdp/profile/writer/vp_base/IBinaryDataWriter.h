/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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
#ifndef BINARY_WRITER_XRT_IBINARYDATAWRITER_H
#define BINARY_WRITER_XRT_IBINARYDATAWRITER_H

#include "xdp/config.h"
#include <string>
#include "IBinaryDataEvent.h"

namespace xdp::AIEBinaryData
{
class IBinaryDataWriter
{
public:
  XDP_CORE_EXPORT IBinaryDataWriter();
  XDP_CORE_EXPORT virtual ~IBinaryDataWriter();

public:
  XDP_CORE_EXPORT virtual void writeField(const char* data, uint32_t size)  =0;
  XDP_CORE_EXPORT virtual void writeField(const std::string& str)           =0;
  XDP_CORE_EXPORT virtual void writeEvent(IBinaryDataEvent::Time current_time, const IBinaryDataEvent& event) =0;
};

} // xdp::AIEBinaryData

#endif //BINARY_WRITER_XRT_IBINARYDATAWRITER_H
