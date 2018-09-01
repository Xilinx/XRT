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
#ifndef _XMC_H_
#define _XMC_H_

#include <list>
#include <sys/stat.h>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>

// Register offset in mgmt pf BAR 0
#define XMC_REG_BASE                0x120000
#define XMC_GPIO_RESET              0x131000

// Register offset in register map of XMC
#define XMC_REG_OFF_MAGIC                   0x0
#define XMC_REG_OFF_VER                     0x4
#define XMC_REG_OFF_STATUS                  0x8
#define XMC_REG_OFF_ERR                     0xc
#define XMC_REG_OFF_FEATURE                 0x10
#define XMC_REG_OFF_CTL                     0x18
#define XMC_REG_OFF_PKT_OFFSET              0x300
#define XMC_REG_OFF_PKT_STATUS              0x304

#define XMC_MAGIC_NUM                       0x74736574
#define XMC_BASE_VERSION                    2018201

#define XMC_PKT_SUPPORT_MASK                (1 << 3)
#define XMC_PKT_OWNER_MASK                  (1 << 5)
#define XMC_PKT_ERR_MASK                    (1 << 26)

#define XMC_HOST_MSG_NO_ERR                 0x00
#define XMC_HOST_MSG_BAD_OPCODE_ERR         0x01
#define XMC_HOST_MSG_UNKNOWN_ERR            0x02
#define XMC_HOST_MSG_MSP432_MODE_ERR        0x03
#define XMC_HOST_MSG_MSP432_FW_LENGTH_ERR   0x04
#define XMC_HOST_MSG_BRD_INFO_MISSING_ERR   0x05

#define BMC_MODE()    (readReg(XMC_REG_OFF_STATUS) >> 28)

enum xmc_packet_op {
    XPO_UNKNOWN = 0,
    XPO_MSP432_SEC_START,
    XPO_MSP432_SEC_DATA,
    XPO_MSP432_IMAGE_END,
    XPO_BOARD_INFO,
    XPO_MSP432_ERASE_FW
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
const size_t xmcMaxPayload =
    xmcPktSize -sizeof (xmcPkt::xmcPktHdr) / sizeof (uint32_t); // In uint32_t

class XMC_Flasher
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
    XMC_Flasher( unsigned int device_index, char *inMap );
    ~XMC_Flasher();
    int xclUpgradeFirmware(std::istream& tiTxtStream);
    int xclGetBoardInfo(std::map<char, std::vector<char>>& info);
    const std::string probingErrMsg() { return mProbingErrMsg.str(); }

private:
    char *mMgmtMap;
    unsigned mPktBufOffset;
    struct xmcPkt mPkt;
    std::stringstream mProbingErrMsg;
    int program(std::istream& tiTxtStream, const ELARecord& record);
    int erase();
    int sendPkt(bool print_dot);
    int recvPkt();
    int waitTillIdle();
    unsigned readReg(unsigned RegOffset);
    int writeReg(unsigned RegOffset, unsigned value);
};

#endif
