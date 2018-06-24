/**
 * Copyright (C) 2015-2018 Xilinx, Inc
 * In-System Programming of BPI PROM using PCIe
 * Based on XAPP518 (v1.3) April 23, 2014
 * Author(s): Sonal Santan
 *            Ryan Radjabi
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

// Register offset in mgmt pf BAR 0
#define	XMC_REG_BASE 			0x120000

// Register offset in register map of XMC
#define	XMC_REG_OFF_VER 		0x0
#define	XMC_REG_OFF_MAGIC 		0x4
#define	XMC_REG_OFF_ERR			0xc
#define	XMC_REG_OFF_FEATURE		0x10
#define	XMC_REG_OFF_CTL			0x18
#define	XMC_REG_OFF_PKT_OFFSET 	0x300
#define	XMC_REG_OFF_PKT_STATUS 	0x304

#define	XMC_MAGIC_NUM			0x74736574
#define	XMC_VERSION				2017403

#define	XMC_PKT_SUPPORT_MASK	(1 << 3)
#define	XMC_PKT_OWNER_MASK		(1 << 5)
#define	XMC_PKT_ERR_MASK		(1 << 26)

enum xmc_packet_op {
	XPO_UNKNOWN = 0,
	XPO_MSP432_SEC_START,
	XPO_MSP432_SEC_DATA
};
MSP432_Flasher::MSP432_Flasher(unsigned int device_index, char *inMap)
{
	unsigned val;
    mMgmtMap = inMap;

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
int MSP432_Flasher::xclUpgradeFirmware(const char *tiTxtFileName) {
    std::ifstream tiTxtStream(tiTxtFileName);
    std::string startAddress;
    ELARecord record;
    bool endRecordFound = false;
    bool errorFound = false;

    if (mMgmtMap == nullptr) {
        std::cout << "ERROR: Invalid XMC device register layout, device can't be supported" << std::endl;
        return -EOPNOTSUPP;
    }

    if(!tiTxtStream.is_open()) {
        std::cout << "ERROR: Cannot open " << tiTxtFileName
        		  <<". Check that it exists and is readable." << std::endl;
        return -ENOENT;
    }

    std::cout << "INFO: Parsing file " << tiTxtFileName << std::endl;
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

	for (ELARecordList::iterator i = mRecordList.begin(); i != mRecordList.end(); ++i) {
		int ret = program(tiTxtStream, *i);

		if (ret != 0)
			return ret;
	}

	return 0;
}

int MSP432_Flasher::program(std::ifstream& tiTxtStream, const ELARecord& record)
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

	tiTxtStream.seekg(record.mDataPos, std::ifstream::beg);
	byteStr.clear();
	mPkt.hdr.opCode = XPO_MSP432_SEC_START;
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
#if 0
	return waitTillIdle();
#else
	return 0;
#endif
}

void describePkt(struct xmcPkt& pkt)
{
	uint32_t *h = reinterpret_cast<uint32_t *>(&pkt.hdr);
	uint8_t *data = reinterpret_cast<uint8_t *>(&pkt.data[0]);
	std::cout << "opcode=" << static_cast<unsigned>(pkt.hdr.opCode)
			  << " payload_size=" << pkt.hdr.payloadSize
			  << " (0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << *h << std::dec << ")"
			  << std::endl;
#if 0
	std::cout << std::hex;
	int nbytes = 0;
	for (unsigned i = 0; i < pkt.hdr.payloadSize; i++) {
		std::cout << std::uppercase << std::setfill('0') << std::setw(2)
				  << static_cast<unsigned>(data[i]) << " ";
		nbytes++;
		if ((nbytes % 16) == 0)
			std::cout << std::endl;
	}
	std::cout << std::endl << std::dec;
#endif
}

int MSP432_Flasher::sendPkt()
{
	int ret;
	int lenInUint32 = (sizeof (mPkt.hdr) + mPkt.hdr.payloadSize + sizeof (uint32_t) - 1) / sizeof (uint32_t);
	uint32_t *pkt = reinterpret_cast<uint32_t *>(&mPkt);

	std::cout << "Sending XMC packet of " << lenInUint32 << " DWORDs..." << std::endl;
	describePkt(mPkt);
#if 0
	ret = waitTillIdle();
	if (ret != 0)
		return ret;

	for (int i = 0; i < lenInUint32; i++) {
		writeReg(mPktBufOffset + i * sizeof (uint32_t), pkt[i]);
	}

	// Flip pkt buffer ownership bit
	writeReg(XMC_REG_OFF_PKT_STATUS, readReg(XMC_REG_OFF_CTL) & ~XMC_PKT_OWNER_MASK);
#endif
	return 0;
}

int MSP432_Flasher::waitTillIdle()
{
	// In total, wait for 50 * 10ms
    const timespec req = {0, 10 * 1000 * 1000}; // 10ms
    int retry = 50;
    unsigned err = 0;

    while ((retry-- > 0) && !(readReg(XMC_REG_OFF_CTL) & XMC_PKT_OWNER_MASK))
    	(void) nanosleep(&req, nullptr);

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

