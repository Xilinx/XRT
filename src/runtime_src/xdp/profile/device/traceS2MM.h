/*
 * Copyright (C) 2019 Xilinx Inc - All rights reserved
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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
#include <vector>

#include "core/include/xdp/trace.h"
#include "profile_ip_access.h"

namespace xdp {

// Todo : Read this from debug IP Layout?
constexpr uint64_t TS2MM_V2_BURST_LEN = 32;

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
    virtual uint64_t getWordCount(bool final = false);
    virtual uint8_t getMemIndex();
    virtual void showStatus();	// ??
    virtual void showProperties();
    virtual uint32_t getProperties() { return properties; }
    void parseTraceBuf(void* buf, uint64_t size, std::vector<xdp::TraceEvent>& traceVector);

    void setTraceFormat(uint32_t tf) { mTraceFormat = tf; }

    /**
     * All datamovers after 1.0 support circular buffer
     * Remove code that depends on this check in future
     */
    bool supportsCircBuf() { return true; }

    /**
     *  Version 2 Features and Requirements :
     * Data written in multiple of burst size
     * Trace buffer size needs to be multiple of burst size (needed for circulat buffer to work)
     * Reset process involves flushing of leftover data to memory
     * Datamover count could be in terms of 128/64 bits
     */
    bool isVersion2() {return mIsVersion2;}

protected:
    uint8_t properties;

private:
    uint8_t major_version;
    uint8_t minor_version;
    uint32_t mTraceFormat = 0;

protected:
    void write32(uint64_t offset, uint32_t val);

protected:
    void parsePacketClockTrain(uint64_t packet);
    void parsePacket(uint64_t packet, uint64_t firstTimestamp, xdp::TraceEvent &result);
    uint64_t seekClockTraining(uint64_t* arr, uint64_t count);

protected:
    uint64_t mPacketFirstTs = 0;
    bool mclockTrainingdone = false;
    uint32_t mModulus = 0;

    // Since clock training packets can be interspersed with other packets,
    //  we need to keep track of what we see until we see all four 
    //  clock training packets
    xdp::TraceEvent partialResult = {};

    // Members specific to version 2 datamover
    bool mIsVersion2 = false;
    uint32_t mBurstLen = 1;
};

} //  xdp

#endif

