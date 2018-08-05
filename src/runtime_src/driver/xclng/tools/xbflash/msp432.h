/**
 * Copyright (C) 2015-2018 Xilinx, Inc
 * Author(s): Max Zhen
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
#ifndef _MSP432_H_
#define _MSP432_H_

#include <list>
#include <sys/stat.h>
#include <iostream>

// Register offset in mgmt pf BAR 0
#define XMC_REG_BASE                0x120000

// Register offset in register map of XMC
#define XMC_REG_OFF_MAGIC             0x0
#define XMC_REG_OFF_VER           0x4
#define XMC_REG_OFF_ERR             0xc
#define XMC_REG_OFF_FEATURE         0x10
#define XMC_REG_OFF_CTL             0x18
#define XMC_REG_OFF_PKT_OFFSET      0x300
#define XMC_REG_OFF_PKT_STATUS      0x304

#define XMC_MAGIC_NUM               0x74736574
#define XMC_VERSION                 2018201

#define XMC_PKT_SUPPORT_MASK        (1 << 3)
#define XMC_PKT_OWNER_MASK          (1 << 5)
#define XMC_PKT_ERR_MASK            (1 << 26)

enum xmc_packet_op {
    XPO_UNKNOWN = 0,
    XPO_MSP432_SEC_START,
    XPO_MSP432_SEC_DATA,
    XPO_MSP432_IMAGE_END
};

const size_t xmcPktSize = (1024 / sizeof (uint32_t)) * 4; // In uint32_t
struct xmcPkt {
	// Make sure hdr is uint32_t aligned
	struct xmcPktHdr {
		unsigned payloadSize : 12;
		unsigned reserved : 12;
		unsigned opCode : 8;
	} hdr;
	uint32_t data[xmcPktSize - sizeof (hdr) / sizeof (uint32_t)];
};

class MSP432_Flasher
{
    struct ELARecord
    {
        unsigned mStartAddress;
        unsigned mEndAddress;
        unsigned mDataCount;
        std::streampos mDataPos;
        ELARecord() : mStartAddress(0), mEndAddress(0), mDataCount(0), mDataPos(0) {}
    };

    typedef std::list<ELARecord> ELARecordList;
    ELARecordList mRecordList;

public:
    MSP432_Flasher( unsigned int device_index, char *inMap );
    ~MSP432_Flasher();
    int xclUpgradeFirmware(const char *fileName);

private:
    char *mMgmtMap;
    unsigned mPktBufOffset;
    struct xmcPkt mPkt;
    int program(std::ifstream& tiTxtStream, const ELARecord& record);
    int sendPkt();
    int waitTillIdle();
    unsigned readReg(unsigned RegOffset);
    int writeReg(unsigned RegOffset, unsigned value);
};

#endif
