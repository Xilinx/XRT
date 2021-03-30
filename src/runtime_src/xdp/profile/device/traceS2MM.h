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

#ifndef XDP_PROFILE_DEVICE_TRACE_S2MM_H
#define XDP_PROFILE_DEVICE_TRACE_S2MM_H

#include <stdexcept>
#include "profile_ip_access.h"
#include "xdp/profile/device/device_trace_logger.h"

namespace xdp {

/**
 * TraceS2MM ProfileIP (IP with safe access) for AXI Interface Monitor
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
class TraceS2MM : public ProfileIP {
public:

    /**
     * The constructor takes a device handle and a ip index
     * means that the instance of this class has a one-to-one
     * association with one specific IP on one specific device.
     * During the construction, the exclusive access to this
     * IP will be requested, otherwise exception will be thrown.
     */
    TraceS2MM(Device* handle /** < [in] the xrt or hal device handle */,
              uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data = nullptr);

    /**
     * The exclusive access should be release in the destructor
     * to prevent potential card hang.
     */
    virtual ~TraceS2MM()
    {}

    virtual void init(uint64_t bo_size, int64_t bufaddr, bool circular);
    virtual bool isActive();	// ??
    virtual void reset();
    /** 
     * One word is 64 bit with current implementation
     * IP should support word packing if we want to support 512 bit words
     */
    virtual uint64_t getWordCount();
    uint8_t getMemIndex();
    virtual void showStatus();	// ??
    virtual void showProperties();
    virtual uint32_t getProperties() { return properties; }
    void parseTraceBuf(void* buf, uint64_t size, std::vector<xclTraceResults>& traceVector);

    void setTraceFormat(uint32_t tf) { mTraceFormat = tf; }
    bool supportsCircBuf() { return major_version >= 1 && minor_version > 0;}

private:
    uint8_t properties;
    uint8_t major_version;
    uint8_t minor_version;
    uint32_t mTraceFormat = 0;

    void write32(uint64_t offset, uint32_t val);

protected:
    void parsePacketClockTrain(uint64_t packet);
    void parsePacket(uint64_t packet, uint64_t firstTimestamp, xclTraceResults &result);
    uint64_t seekClockTraining(uint64_t* arr, uint64_t count);

protected:
    uint64_t mPacketFirstTs = 0;
    bool mclockTrainingdone = false;
    uint32_t mModulus = 0;

    // Since clock training packets can be interspersed with other packets,
    //  we need to keep track of what we see until we see all four 
    //  clock training packets
    xclTraceResults partialResult = {};
};

} //  xdp

#endif

