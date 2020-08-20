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

#ifndef XDP_PROFILE_DEVICE_AIE_COUNTER_H
#define XDP_PROFILE_DEVICE_AIE_COUNTER_H

#include <stdexcept>
#include "profile_ip_access.h"

namespace xdp {

/**
 * AI Engine Counter ProfileIP (IP with safe access)
 *
 * Description:
 *
 * This class represents the high level exclusive and OS protected
 * access to a single AIE counter on the device.
 *
 * Note:
 *
 * This class only aims at providing interface for easy and
 * safe access to a single profiling IP. Managing the
 * association between IPs and devices should be done in a
 * different data structure that is built on top of this class.
 */
class AIECounter : public ProfileIP {
public:
    /**
     * The constructor takes a device handle and an ip index
     * means that the instance of this class has a one-to-one
     * association with one specific IP on one specific device.
     * During the construction, the exclusive access to this
     * IP will be requested, otherwise exception will be thrown.
     */
    AIECounter(Device* handle /** < [in] the xrt or hal device handle */,
        uint64_t index /** < [in] the index of the IP in debug_ip_layout */, debug_ip_data* data = nullptr);

    /**
     * The exclusive access should be release in the destructor
     * to prevent potential card hang.
     */
    virtual ~AIECounter()
    {}

    virtual void init();
    virtual void showStatus();
    virtual void showProperties();

    virtual uint32_t getID()            {return mID;}
    virtual uint32_t getColumn()        {return mColumn;}
    virtual uint32_t getRow()           {return mRow;}
    virtual uint8_t  getCounterNumber() {return mCounterNumber;}
    virtual uint8_t  getStartEvent()    {return mStartEvent;}
    virtual uint8_t  getEndEvent()      {return mEndEvent;}
    virtual uint8_t  getResetEvent()    {return mResetEvent;}
    virtual double getClockFreqMhz()    {return mClockFreqMhz;}
    virtual std::string getModule()     {return mModule;}
    virtual std::string getName()       {return mName;}
   
    //virtual uint64_t readCounter();

private:
    uint8_t mMajorVersion;
    uint8_t mMinorVersion;
    uint32_t mID;
    uint32_t mColumn;
    uint32_t mRow;
    uint8_t mCounterNumber;
    uint8_t mStartEvent;
    uint8_t mEndEvent;
    uint8_t mResetEvent;
    double mClockFreqMhz;
    std::string mModule;
    std::string mName;
};

} //  xdp

#endif

