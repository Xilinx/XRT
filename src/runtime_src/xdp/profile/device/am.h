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

#ifndef XDP_PROFILE_DEVICE_AM_H
#define XDP_PROFILE_DEVICE_AM_H

#include <stdexcept>
#include "profile_ip_access.h"

namespace xdp {

/**
 * AM ProfileIP (IP with safe access) for Accelerator Monitor
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
class AM : public ProfileIP {
public:

    /**
     * The constructor takes a device handle and a ip index
     * means that the instance of this class has a one-to-one
     * association with one specific IP on one specific device.
     * During the construction, the exclusive access to this
     * IP will be requested, otherwise exception will be thrown.
     */
    AM(Device* handle /** < [in] the xrt or hal device handle */, 
       uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data = nullptr);

    /**
     * The exclusive access should be release in the destructor
     * to prevent potential card hang.
     */
    virtual ~AM()
    {}

    size_t startCounter();
    size_t stopCounter();
    size_t readCounter(xclCounterResults& counterResult, uint32_t index);

    // Used to explicitly disable the monitor when kernel channels are used
    void disable();
    void configureDataflow(bool cuHasApCtrlChain);

    bool has64bit() const;
    bool hasDataflow() const;
    bool hasStall() const;
    bool hasTraceID (uint32_t index) const;

    signed compareVersion(unsigned major2, unsigned minor2) const;

    size_t triggerTrace(uint32_t traceOption /*startTrigger*/);

    virtual void showProperties();
    virtual uint32_t getProperties() { return properties; }

private:
    uint8_t properties;
    uint8_t major_version;
    uint8_t minor_version;
    bool m_enabled = true;
};

} //  xdp

#endif

