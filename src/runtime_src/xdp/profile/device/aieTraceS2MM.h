/*
 * Copyright (C) 2019 Xilinx Inc - All rights reserved
 * Xilinx Debug & Profile (XDP) APIs
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

#ifndef XDP_PROFILE_DEVICE_AIE_TRACE_S2MM_H
#define XDP_PROFILE_DEVICE_AIE_TRACE_S2MM_H

#include <stdexcept>
#include "traceS2MM.h"

namespace xdp {

/**
 * AIE TraceS2MM ProfileIP (IP with safe access) for AIE PLIO trace data mover
 *
 * Description:
 *
 * This class represents the high level exclusive and OS protected
 * access to a profiling IP on the device.
 *
 * Note:
 *
 * This class only aims at providing interface for easy and
 * safe access to a single profiling IP. Managing the
 * association between IPs and devices should be done in a
 * different data structure that is built on top of this class.
 */
class AIETraceS2MM : public TraceS2MM {
public:

    /**
     * The constructor takes a device handle and a ip index
     * means that the instance of this class has a one-to-one
     * association with one specific IP on one specific device.
     * During the construction, the exclusive access to this
     * IP will be requested, otherwise exception will be thrown.
     */
    AIETraceS2MM(Device* handle /** < [in] the xrt or hal device handle */,
              uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data = nullptr)
        : TraceS2MM(handle, index, data)
    {}

    /**
     * The exclusive access should be release in the destructor
     * to prevent potential card hang.
     */
    virtual ~AIETraceS2MM()
    {}

    /** 
     * One word is 64 bit with current implementation
     * IP should support word packing if we want to support 512 bit words
     */
    virtual void parseTraceBuf(void* /*buf*/, uint64_t /*size*/, xclTraceResultsVector& /*traceVector*/) {}
};

} //  xdp

#endif

