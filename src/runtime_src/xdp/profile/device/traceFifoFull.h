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

#ifndef XDP_PROFILE_DEVICE_TRACE_FIFO_FULL_H
#define XDP_PROFILE_DEVICE_TRACE_FIFO_FULL_H

#include <stdexcept>
#include <fstream>
#include <iostream>
#include "profile_ip_access.h"
#include "core/include/xclperf.h"

namespace xdp {

/**
 * TRACE FIFO ProfileIP (IP with safe access)
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
class TraceFifoFull : public ProfileIP {
public:

    /**
     * The constructor takes a device handle and a ip index
     * means that the instance of this class has a one-to-one
     * association with one specific IP on one specific device.
     * During the construction, the exclusive access to this
     * IP will be requested, otherwise exception will be thrown.
     */
    TraceFifoFull(Device* handle /** < [in] the xrt or hal device handle */, 
                  uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data = nullptr);

    /**
     * The exclusive access should be release in the destructor
     * to prevent potential card hang.
     */
    virtual ~TraceFifoFull()
    {}

    uint32_t getNumTraceSamples(/* need type */);
    uint32_t getMaxNumTraceSamples();
    uint32_t readTrace(xclTraceResultsVector& traceVector, uint32_t nSamples);

    size_t reset();
    void setTraceFormat(uint32_t tf) { mTraceFormat = tf; }

    virtual void showProperties();
    virtual uint32_t getProperties() { return properties; }

private:
    uint8_t properties;
    uint8_t major_version;
    uint8_t minor_version;
#if 0
    uint32_t readTraceForPCIEDevice(xclTraceResultsVector& traceVector, uint32_t nSamples);
    uint32_t readTraceForEdgeDevice(xclTraceResultsVector& traceVector, uint32_t nSamples);
#endif
    void processTraceData(xclTraceResultsVector& traceVector, uint32_t numSamples, void* data, uint32_t wordsPerSample);

    bool mclockTrainingdone = false;
    uint64_t mfirstTimestamp = 0;
    uint32_t mTraceFormat = 0;
};

} //  xdp

#endif

