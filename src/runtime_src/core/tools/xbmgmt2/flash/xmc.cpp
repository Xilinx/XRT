/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include <algorithm>
#include <cstring>
#include <cassert>
#include <vector>
#include <thread>
#include <iomanip>
#include <locale>

#include "xmc.h"
#include "flasher.h"
#include "core/common/utils.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/tools/common/XBUtilities.h"
#include "core/tools/common/ProgressBar.h"
namespace XBU = XBUtilities;
#include "boost/format.hpp"


//#define XMC_DEBUG
#define BMC_JUMP_ADDR   0x201  /* Hard-coded for now */

#ifdef __GNUC__
# define XMC_UNUSED __attribute__((unused))
#else
# define XMC_UNUSED
#endif

#ifdef _WIN32
# pragma warning( disable : 4189 4100 4996)
#endif

XMC_Flasher::XMC_Flasher(unsigned int device_index)
  : m_device(xrt_core::get_mgmtpf_device(device_index))
{
    uint64_t val = 0;
    mPktBufOffset = 0;
    mPkt = {};

    std::string err;
    bool is_mfg = false;
    is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(m_device);
    if (!is_mfg) {
      val = xrt_core::device_query<xrt_core::query::xmc_status>(m_device);
      if (!(val & 1)) {
        mProbingErrMsg << "Failed to detect XMC, xmc.bin not loaded";
        goto nosup;
      }
    }
    try {
        mRegBase = xrt_core::device_query<xrt_core::query::xmc_reg_base>(m_device);
    } catch (...) {}
    if (mRegBase == 0)
        mRegBase = XMC_REG_BASE;
    mRegBase = XMC_REG_BASE;

    val = readReg(XMC_REG_OFF_MAGIC);
    if (val != XMC_MAGIC_NUM) {
      mProbingErrMsg << "Failed to detect XMC, bad magic number: "
                     << std::hex << val << std::dec;
      goto nosup;
    }

    val = readReg(XMC_REG_OFF_VER);
    if (val < XMC_BASE_VERSION) {
        mProbingErrMsg << "Found unsupported XMC version: " << val;
        goto nosup;
    }

    val = readReg(XMC_REG_OFF_FEATURE);
    if (val & XMC_PKT_SUPPORT_MASK) {
        mProbingErrMsg << "XMC packet buffer is not supported";
        goto nosup;
    }

    mPktBufOffset = readReg(XMC_REG_OFF_PKT_OFFSET);
nosup:
    return;
}

XMC_Flasher::~XMC_Flasher()
{
}

/*
 * xclUpgradeFirmware
 */
int XMC_Flasher::xclUpgradeFirmware(std::istream& tiTxtStream) {
    std::string startAddress;
    ELARecord record;
    bool endRecordFound = false;
    bool errorFound = false;
    int retries = 5;
    int ret = 0;

    if (!isXMCReady())
        return -EINVAL;

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
            record.mStartAddress = BMC_JUMP_ADDR;
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
            std::locale loc;

            if (startAddress.size() == 0) {
                    errorFound = true;
            }

            for (unsigned int i = 0; i < line.size() && !errorFound; i++) {
                if (line[i] == ' ') {
                    spaces++;
                } else if (std::isxdigit(line[i], loc)) {
                    digits++;
                } else {
                    errorFound = true;
                }
            }

            // Each line has at most 16-byte of data represented as hex in ASCII
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

    if (errorFound)
        throw xrt_core::error("Bad firmware file format.");

    // Start of flashing BMC firmware
    std::cout << boost::format("%-8s : %s %s %s\n") % "INFO" % "found" % mRecordList.size() % "sections";
    while(retries != 0) {
        retries--;

        ret = erase();
        XBU::ProgressBar sc_flash("Programming SC", static_cast<unsigned int>(mRecordList.size()), XBU::is_esc_enabled(), std::cout);
        int counter = 0;
        for (auto i = mRecordList.begin(); ret == 0 && i != mRecordList.end(); ++i) {
            ret = program(tiTxtStream, *i);
            sc_flash.update(counter);
            counter++;
        }
        
        if(ret == 0) {
            sc_flash.finish(true, "SC successfully updated");
            break;
        } else {
            sc_flash.finish(false, "WARN: Failed to flash firmware, retrying...");
        }
    }
    // End of flashing BMC firmware

    if (ret != 0)
        return ret;

    // Waiting for BMC to come back online.
    // It should not take more than 10 sec, but wait for 1 min to be safe.
    std::cout << boost::format("%-8s : %s\n") % "INFO" % "Loading new firmware on SC";
    for (int i = 0; i < 60; i++) {
        if (BMC_MODE() == BMC_STATE_READY)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "." << std::flush;
    }
    std::cout << std::endl;

    if (!isBMCReady())
        throw xrt_core::error("Time'd out waiting for SC to come back online");
    return 0;
}

int XMC_Flasher::erase()
{
    int ret = 0;

    mPkt = {0};
    mPkt.hdr.opCode = XPO_MSP432_ERASE_FW;

    if ((ret = sendPkt(true)) != 0)
        return ret;

    // Flush the last packet sent to XMC
    return waitTillIdle();
}

int XMC_Flasher::xclGetBoardInfo(std::map<char, std::vector<char>>& info)
{
    int ret = 0;

    if (!isXMCReady() || !isBMCReady())
        return -EINVAL;

    mPkt = {0};
    mPkt.hdr.opCode = XPO_BOARD_INFO;

    if ((ret = sendPkt(false)) != 0){
        if(ret == XMC_HOST_MSG_BRD_INFO_MISSING_ERR)
        {
            std::cout << "Unable to get card info, need to upgrade firmware"
                << std::endl;
        }
        return ret;
    }

    ret = recvPkt();
    if (ret != 0)
        return ret;

    info.clear();
    char *byte = reinterpret_cast<char *>(mPkt.data);
    for(unsigned int i = 0; i < mPkt.hdr.payloadSize;)
    {
        char key = byte[i++];
        uint8_t len = byte[i++];
        std::vector<char> content(len, 0);
        for (int n = 0; n < len; n++)
            content[n] = byte[i++];
        info[key] = content;
    }

    return 0;
}

int XMC_Flasher::program(std::istream& tiTxtStream, const ELARecord& record) //* prog bar
{
    std::string byteStr;
    int ret = 0;
    unsigned int ndigit = 0;
    int pos;
    char c;
    uint8_t *data;
    const int charPerByte = 2;
    std::locale loc;

#ifdef  XMC_DEBUG
    std::cout << std::hex;
    std::cout << "Address=0x" << record.mStartAddress
        << std::dec << ", Length=" << record.mDataCount;
    std::cout<< std::endl;
#endif
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
        if (!std::isxdigit(c, loc))
            continue;
        ndigit++;

        byteStr.push_back(c);
        if (byteStr.size() < charPerByte)
            continue;

        uint8_t n = static_cast<uint8_t>(std::stoi(byteStr, 0 , 16));
        byteStr.clear();

        data[pos++] = n;
        if (pos < maxDataSize)
            continue;

        // Send out a fully loaded pkt
        mPkt.hdr.payloadSize = pos;
        if ((ret = sendPkt(true)) != 0) //* if prog bar val=true sendPkt false
            return ret;
        // Reset opcode and pos for next data pkt
        mPkt.hdr.opCode = XPO_MSP432_SEC_DATA;
        pos = 0;
    }

    // Send the last partially loaded pkt
    if (pos) {
        mPkt.hdr.payloadSize = pos;
        if ((ret = sendPkt(true)) != 0)
            return ret;
    }

    // Flush the last packet sent to XMC
    return waitTillIdle();
}

void describePkt(struct xmcPkt& pkt, bool send)
{
    int lenInUint32 = (sizeof (pkt.hdr) + pkt.hdr.payloadSize +
        sizeof (uint32_t) - 1) / sizeof (uint32_t);

    auto format = xrt_core::utils::ios_restore(std::cout);

    if (send)
        std::cout << "Sending XMC packet: ";
    else
        std::cout << "Receiving XMC packet: ";
    std::cout << std::dec << lenInUint32 << " DWORDs..." << std::endl;

    uint32_t *h = reinterpret_cast<uint32_t *>(&pkt.hdr);
    std::cout << "opcode=" << static_cast<unsigned>(pkt.hdr.opCode)
              << " payload_size=" << pkt.hdr.payloadSize
              << " (0x" << std::hex << std::uppercase << std::setfill('0')
              << std::setw(8) << *h << std::dec << ")"
              << std::endl;

#ifdef  XMC_DEBUG_VERBOSE
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
#endif
}

int XMC_Flasher::recvPkt()
{
    uint32_t *pkt = reinterpret_cast<uint32_t *>(&mPkt);
    *pkt = readReg(mPktBufOffset);
    unsigned int lenInUint32 =
        (mPkt.hdr.payloadSize + sizeof (uint32_t) - 1) / sizeof (uint32_t);

    if (lenInUint32 <= 0 || lenInUint32 > xmcMaxPayload) {
        std::cout << "ERROR: Received bad XMC packet" << std::endl;
        return -EINVAL;
    }

    for (unsigned int i = 0; i < lenInUint32; i++)
        mPkt.data[i] = readReg(mPktBufOffset + (i + 1) * sizeof (uint32_t));

#ifdef  XMC_DEBUG
    describePkt(mPkt, false);
#endif
    return waitTillIdle();
}

int XMC_Flasher::sendPkt(bool print_dot)
{
    int lenInUint32 = (sizeof (mPkt.hdr) + mPkt.hdr.payloadSize +
        sizeof (uint32_t) - 1) / sizeof (uint32_t);

#ifdef  XMC_DEBUG
    describePkt(mPkt, true);
#else
    // if (print_dot)
    //     std::cout << "." << std::flush;
#endif

    uint32_t *pkt = reinterpret_cast<uint32_t *>(&mPkt);

    for (int i = 0; i < lenInUint32; i++) {
        writeReg(mPktBufOffset + i * sizeof (uint32_t), pkt[i]);
    }

    // Flip pkt buffer ownership bit
    writeReg(XMC_REG_OFF_CTL, readReg(XMC_REG_OFF_CTL) | XMC_PKT_OWNER_MASK);
    return waitTillIdle();
}

int XMC_Flasher::waitTillIdle()
{
    // In total, wait for 500 * 10ms
    int retry = 500;
    unsigned int err = 0;

#if  XMC_DEBUG
    std::cout << "INFO: Waiting until idle" << std::endl;
#endif
    while ((retry-- > 0) && (readReg(XMC_REG_OFF_CTL) & XMC_PKT_OWNER_MASK)){
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
    }

    if (retry == 0) {
        std::cout << "ERROR: Time'd out while waiting for XMC packet to be idle"
            << std::endl;
        return -ETIMEDOUT;
    }

    if (readReg(XMC_REG_OFF_ERR) & XMC_PKT_ERR_MASK)
        err = readReg(XMC_REG_OFF_PKT_STATUS);

    if (err) {
        std::cout << "ERROR: XMC packet error: " << err << std::endl;
        writeReg(XMC_REG_OFF_CTL, readReg(XMC_REG_OFF_CTL) | XMC_CTRL_ERR_CLR);
        return -EINVAL;
    }

    return 0;
}

unsigned int XMC_Flasher::readReg(unsigned int RegOffset) {
    unsigned int value = 0;
    RegOffset = RegOffset;
    m_device->read(mRegBase + RegOffset, &value, 4);
    return value;
}

int XMC_Flasher::writeReg(unsigned int RegOffset, unsigned int value) {
    value = value; 
    m_device->write(mRegBase + RegOffset, &value, 4);
    return 0;
}

bool XMC_Flasher::isXMCReady()
{
    bool xmcReady = (XMC_MODE() == XMC_READY);

    if (!xmcReady) {
        auto format = xrt_core::utils::ios_restore(std::cout);
        std::cout << "ERROR: XMC is not ready: 0x" << std::hex
                  << XMC_MODE() << std::endl;
    }
    return xmcReady;
}

bool XMC_Flasher::isBMCReady()
{
    bool bmcReady = (BMC_MODE() == 0x1);

    if (!bmcReady) {
        auto format = xrt_core::utils::ios_restore(std::cout);
        std::cout << "ERROR: SC is not ready: 0x" << std::hex
                  << BMC_MODE() << std::endl;
    }
    return bmcReady;
}
