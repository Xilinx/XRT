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
#include <iostream>
#include <iomanip>
#include <string>
#include <list>
#include <fstream>
#include <cassert>
#include "msp432.h"
#include "flasher.h"

MSP432_Flasher::MSP432_Flasher(unsigned int device_index, char *inMap)
{
    unsigned val;
    mMgmtMap = inMap;
    mPktBufOffset = 0;
    mPkt = {};

    val = readReg(XMC_REG_OFF_MAGIC);
    if (val != XMC_MAGIC_NUM) {
        std::cout << "ERROR: Failed to detect XMC, bad magic number: " << std::hex << val
                  << std::dec << std::endl;
        goto nosup;
    }

    val = readReg(XMC_REG_OFF_VER);
    if (val != XMC_VERSION) {
        std::cout << "ERROR: Found unsupported XMC version: " << val << std::endl;
        goto nosup;
    }

    val = readReg(XMC_REG_OFF_FEATURE);
    if (val & XMC_PKT_SUPPORT_MASK) {
        std::cout << "ERROR: XMC packet buffer is not supported" << std::endl;
        goto nosup;
    }

    mPktBufOffset = readReg(XMC_REG_OFF_PKT_OFFSET);
    std::cout << "INFO: XMC packet buffer offset is " << mPktBufOffset << std::endl;

    return;

nosup:
       mMgmtMap = nullptr;
       return;
}

MSP432_Flasher::~MSP432_Flasher()
{
}

/*
 * xclUpgradeFirmware
 */
int MSP432_Flasher::xclUpgradeFirmware(std::istream& tiTxtStream) {
    std::string startAddress;
    ELARecord record;
    bool endRecordFound = false;
    bool errorFound = false;
    int retries = 5;
    int ret = 0;

    if (mMgmtMap == nullptr) {
        std::cout << "ERROR: Invalid XMC device register layout, device can't be supported" << std::endl;
        return -EOPNOTSUPP;
    }

    while (!tiTxtStream.eof() && !endRecordFound && !errorFound) {
        std::string line;
        std::getline(tiTxtStream, line);
        if (line.size() == 0) {
            continue;
        }

        switch (line[0]) {
        case 'q':
        case 'Q':
        {
            if (startAddress.size()) {
                // Finish the last record
                mRecordList.push_back(record);
                startAddress.clear();
            }
            // Create and append the end-of-image record (mDataCount must be 0).
            record.mStartAddress = 0x201; /* Hard-coded for now */
            record.mDataPos = tiTxtStream.tellg();
            record.mEndAddress = record.mStartAddress;
            record.mDataCount = 0;
            mRecordList.push_back(record);
            endRecordFound = true;
            break;
        }
        case '@':
        {
            std::string newAddress = line.substr(1);
            if (startAddress.size()) {
                // Finish the last record
                mRecordList.push_back(record);
                startAddress.clear();
            }
            // Start a new record
            record.mStartAddress = std::stoi(newAddress, 0 , 16);
            record.mDataPos = tiTxtStream.tellg();
            record.mEndAddress = record.mStartAddress;
            record.mDataCount = 0;
            startAddress = newAddress;
            break;
        }
        default:
        {
            int spaces = 0;
            int digits = 0;

            if (startAddress.size() == 0) {
                    errorFound = true;
            }

            for (unsigned i = 0; i < line.size() && !errorFound; i++) {
                if (line[i] == ' ') {
                    spaces++;
                } else if (std::isxdigit(line[i])) {
                    digits++;
                } else {
                    errorFound = true;
                }
            }

            // Each line has at most 16 bytes of data represented as hex in ASCII
            if (((digits % 2) != 0) || digits > 16 * 2) {
                errorFound = true;
            }

            if (!errorFound) {
                int bytes = digits / 2;

                record.mDataCount += bytes;
                record.mEndAddress += bytes;
                if (bytes < 16) {
                    // Finish the last record
                    mRecordList.push_back(record);
                    startAddress.clear();
                }
            }
        }
        }
    }

    tiTxtStream.seekg(0);

    if (errorFound) {
        std::cout << "ERROR: Bad firmware file format." << std::endl;
        return -EINVAL;
    }

    std::cout << "INFO: Found " << mRecordList.size() << " Sections" << std::endl;

    while(retries!=0){
        retries--;
        std::cout << "Erase FW..." << std::endl;
        erase();
        for (ELARecordList::iterator i = mRecordList.begin(); i != mRecordList.end(); ++i) {
            ret = program(tiTxtStream, *i);

            if (ret != 0)
                break;
        }
        if(ret == 0)
            return ret;
    }
    return ret;
}

int MSP432_Flasher::erase()
{
    int ret = 0;

    mPkt = {0};
    mPkt.hdr.opCode = XPO_MSP432_ERASE_FW;
    mPkt.hdr.reserved = 0;

    if ((ret = sendPkt()) != 0)
        return ret;
    // Flush the last packet sent to XMC
    return waitTillIdle();
}


int MSP432_Flasher::xclGetBoardInfo(uint32_t *msp_packet) {
    std::string startAddress;
    ELARecord record;

    std::cout << "MSP432_Flasher::xclGetBoardInfo" << std::endl;
    
    if (mMgmtMap == nullptr) {
        std::cout << "ERROR: Invalid XMC device register layout, device can't be supported" << std::endl;
        return -EOPNOTSUPP;
    }

    return getBoardInfo(msp_packet);
}

int MSP432_Flasher::getBoardInfo(uint32_t *msp_packet)
{
    int ret = 0;
    mPkt.hdr.opCode = XPO_MSP432_INFO_RESP;
    mPkt.hdr.reserved = 0;

    if ((ret = sendPkt()) != 0){
        if(ret==XMC_HOST_MSG_BRD_INFO_MISSING_ERR)
            std::cout << "Unable to get board info, need to upgrade firmware" << std::endl;
        return ret;
    }

    recvPkt();

    for(uint i=0;i<(mPkt.hdr.payloadSize/sizeof(uint32_t));++i)
        *(msp_packet+i) = mPkt.data[i];

    return waitTillIdle();
}

int MSP432_Flasher::program(std::istream& tiTxtStream, const ELARecord& record)
{
    std::string byteStr;
    int ret = 0;
    unsigned ndigit = 0;
    int pos;
    char c;
    uint8_t *data;
    const int charPerByte = 2;

    std::cout << std::hex;
    std::cout << "\tAddress=0x" << record.mStartAddress << std::dec << "\tLength=" << record.mDataCount;
    std::cout<< std::endl;

    if (record.mDataCount == 0) {
        std::cout << "ERROR: Found zero length record, ignore" << std::endl;
        return 0;
    }

    tiTxtStream.seekg(record.mDataPos, std::ios_base::beg);

    byteStr.clear();
    mPkt.hdr.opCode =
        record.mDataCount ? XPO_MSP432_SEC_START : XPO_MSP432_IMAGE_END;
    mPkt.hdr.reserved = 0;

    const int maxDataSize = sizeof (mPkt.data);
    data = reinterpret_cast<uint8_t *>(&mPkt.data[0]);
    // First uint32_t in payload is always the address
    mPkt.data[0] = record.mStartAddress;
    mPkt.data[1] = record.mDataCount;
    pos = sizeof (uint32_t) * 2;

    while (ndigit < record.mDataCount * charPerByte) {
        if (!tiTxtStream.get(c)) {
            std::cout << "Cannot read data from firmware file" << std::endl;
            return -EIO;
        }
        if (!std::isxdigit(c))
            continue;
        ndigit++;

        byteStr.push_back(c);
        if (byteStr.size() < charPerByte)
            continue;

        int n = std::stoi(byteStr, 0 , 16);
        byteStr.clear();

        data[pos++] = n;
        if (pos < maxDataSize)
            continue;

        // Send out a fully loaded pkt
        mPkt.hdr.payloadSize = pos;
        if ((ret = sendPkt()) != 0)
            return ret;
        // Reset opcode and pos for next data pkt
        mPkt.hdr.opCode = XPO_MSP432_SEC_DATA;
        pos = 0;
    }

    // Send the last partially loaded pkt
    if (pos) {
        mPkt.hdr.payloadSize = pos;
        if ((ret = sendPkt()) != 0)
            return ret;
    }

    // Flush the last packet sent to XMC
    return waitTillIdle();
}

void describePkt(struct xmcPkt& pkt)
{
    std::ios::fmtflags f(std::cout.flags());
    uint32_t *h = reinterpret_cast<uint32_t *>(&pkt.hdr);
    std::cout << "opcode=" << static_cast<unsigned>(pkt.hdr.opCode)
              << " payload_size=" << pkt.hdr.payloadSize
              << " (0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << *h << std::dec << ")"
              << std::endl;
    uint8_t *data = reinterpret_cast<uint8_t *>(&pkt.data[0]);
    std::cout << std::hex;
    int nbytes = 0;
    for (unsigned i = 0; i < pkt.hdr.payloadSize; i++) {
        std::cout << std::uppercase << std::setfill('0') << std::setw(2)
                  << static_cast<unsigned>(data[i]) << " ";
        nbytes++;
        if ((nbytes % 16) == 0)
            std::cout << std::endl;
    }
    std::cout << std::endl;
    std::cout.flags(f);
}

int MSP432_Flasher::recvPkt()
{
    uint32_t header;
    uint16_t payload_size;
    int lenInUint32;
    describePkt(mPkt);
    uint32_t *pkt;

    int ret = waitTillIdle();
    if (ret != 0)
        return ret;

    header = readReg(mPktBufOffset);
    payload_size = (header) & 0xfff;


    mPkt.hdr.payloadSize = payload_size;

    lenInUint32= (sizeof (mPkt.hdr)+ payload_size + sizeof (uint32_t) - 1) / sizeof (uint32_t);
    std::cout << std::dec << "Receiving XMC packet of " << lenInUint32 << " DWORDs..." << std::endl;
    pkt = reinterpret_cast<uint32_t *>(&mPkt.data[0]);

    for (int i = 0; i < lenInUint32; i++) {
        pkt[i] = readReg(mPktBufOffset+ i * sizeof (uint32_t));
    }

    // Flip pkt buffer ownership bit
    writeReg(XMC_REG_OFF_CTL, readReg(XMC_REG_OFF_CTL) | XMC_PKT_OWNER_MASK);
    return 0;
}

int MSP432_Flasher::sendPkt()
{
    int lenInUint32 = (sizeof (mPkt.hdr) + mPkt.hdr.payloadSize + sizeof (uint32_t) - 1) / sizeof (uint32_t);

    std::cout << "Sending XMC packet of " << lenInUint32 << " DWORDs..." << std::endl;
    describePkt(mPkt);
    uint32_t *pkt = reinterpret_cast<uint32_t *>(&mPkt);
    int ret = waitTillIdle();
    if (ret != 0)
        return ret;

    for (int i = 0; i < lenInUint32; i++) {
        writeReg(mPktBufOffset + i * sizeof (uint32_t), pkt[i]);
    }

    // Flip pkt buffer ownership bit
    writeReg(XMC_REG_OFF_CTL, readReg(XMC_REG_OFF_CTL) | XMC_PKT_OWNER_MASK);
    return 0;
}

int MSP432_Flasher::waitTillIdle()
{
    // In total, wait for 500 * 10ms
    const timespec req = {0, 10 * 1000 * 1000}; // 10ms
    int retry = 500;
    unsigned err = 0;

    std::cout << "INFO: Waiting until idle" << std::endl;
    while ((retry-- > 0) && (readReg(XMC_REG_OFF_CTL) & XMC_PKT_OWNER_MASK)){
        (void) nanosleep(&req, nullptr);
    }

    if (retry == 0) {
        std::cout << "ERROR: Time'd out while waiting for XMC packet to be idle" << std::endl;
        return -ETIMEDOUT;
    }

    if (readReg(XMC_REG_OFF_ERR) & XMC_PKT_ERR_MASK)
        err = readReg(XMC_REG_OFF_PKT_STATUS);

    if (err) {
        std::cout << "ERROR: XMC packet error: " << err << std::endl;
        return -EINVAL;
    }

    return 0;
}

unsigned MSP432_Flasher::readReg(unsigned RegOffset) {
    unsigned value;
    if( Flasher::pcieBarRead(0, (unsigned long long)mMgmtMap + XMC_REG_BASE + RegOffset, &value, 4 ) != 0 ) {
        assert(0);
        std::cout << "read reg ERROR" << std::endl;
    }
    return value;
}

int MSP432_Flasher::writeReg(unsigned RegOffset, unsigned value) {
    int status = Flasher::pcieBarWrite(0, (unsigned long long)mMgmtMap + XMC_REG_BASE + RegOffset, &value, 4);
    if(status != 0) {
        assert(0);
        std::cout << "write reg ERROR " << std::endl;
        return -EINVAL;
    }
    return 0;
}

