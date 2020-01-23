/**
 * Copyright (C) 2018 Xilinx, Inc
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
#include <string>
#include <cassert>
#include <vector>
#include <thread>
#include <iomanip>

#include "xmc.h"
#include "flasher.h"
#include "core/common/utils.h"

//#define XMC_DEBUG
#define BMC_JUMP_ADDR   0x201  /* Hard-coded for now */

XMC_Flasher::XMC_Flasher(std::shared_ptr<pcidev::pci_device> dev)
{
    unsigned val = 0;
    mDev = dev;
    mPktBufOffset = 0;
    mPkt = {};

    std::string err;
    bool is_mfg = false;
    mDev->sysfs_get<bool>("", "mfg", err, is_mfg, false);
    if (!is_mfg) {
        if (mDev->get_sysfs_path("xmc", "").empty())
            goto nosup;

        mDev->sysfs_get<unsigned>("xmc", "status", err, val, 0);
	if (!err.empty() || !(val & 1)) {
            mProbingErrMsg << "Failed to detect XMC, xmc.bin not loaded";
            goto nosup;
        }
    }

    mDev->sysfs_get<unsigned long long>("xmc", "reg_base", err, mRegBase, -1);
    if (!err.empty())
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

    mXmcDev = nullptr;
    if (std::getenv("FLASH_VIA_DRIVER")) {
        int fd = mDev->open("xmc", O_RDWR);
        if (fd >= 0)
            mXmcDev = fdopen(fd, "r+");
        if (mXmcDev == nullptr)
            std::cout << "Failed to open XMC device on card" << std::endl;
    }

nosup:
    return;
}

XMC_Flasher::~XMC_Flasher()
{
    if (mXmcDev)
        std::fclose(mXmcDev);
}

/*
 * xclUpgradeFirmware
 */
int XMC_Flasher::xclUpgradeFirmware(std::istream& tiTxtStream) {
    if (mXmcDev)
        return xclUpgradeFirmwareDrv(tiTxtStream);

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

    if (errorFound) {
        std::cout << "ERROR: Bad firmware file format." << std::endl;
        return -EINVAL;
    }

    // Start of flashing BMC firmware
    std::cout << "INFO: found " << mRecordList.size() << " sections" << std::endl;
    while(retries != 0) {
        retries--;

        ret = erase();
        for (auto i = mRecordList.begin(); ret == 0 && i != mRecordList.end(); ++i) {
            ret = program(tiTxtStream, *i);
        }
        if(ret == 0)
            break;
        std::cout << "WARN: Failed to flash firmware, retrying..." << std::endl;
    }
    std::cout << std::endl;
    // End of flashing BMC firmware

    if (ret != 0)
        return ret;

    // Waiting for BMC to come back online.
    // It should not take more than 10 sec, but wait for 1 min to be safe.
    std::cout << "INFO: Loading new firmware on SC" << std::endl;
    for (int i = 0; i < 60; i++) {
        if (BMC_MODE() == BMC_STATE_READY)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "." << std::flush;
    }
    std::cout << std::endl;

    if (!isBMCReady()) {
        std::cout << "ERROR: Time'd out waiting for SC to come back online"
            << std::endl;
        return -ETIMEDOUT;
    }

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
    for(uint i = 0; i < mPkt.hdr.payloadSize;)
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

int XMC_Flasher::program(std::istream& tiTxtStream, const ELARecord& record)
{
    std::string byteStr;
    int ret = 0;
    unsigned ndigit = 0;
    int pos;
    char c;
    uint8_t *data;
    const int charPerByte = 2;

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
        if ((ret = sendPkt(true)) != 0)
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

    xrt_core::ios_flags_restore format(std::cout);

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
    unsigned lenInUint32 =
        (mPkt.hdr.payloadSize + sizeof (uint32_t) - 1) / sizeof (uint32_t);

    if (lenInUint32 <= 0 || lenInUint32 > xmcMaxPayload) {
        std::cout << "ERROR: Received bad XMC packet" << std::endl;
        return -EINVAL;
    }

    for (unsigned i = 0; i < lenInUint32; i++)
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
    if (print_dot)
        std::cout << "." << std::flush;
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
    const timespec req = {0, 10 * 1000 * 1000}; // 10ms
    int retry = 500;
    unsigned err = 0;

#ifdef  XMC_DEBUG
    std::cout << "INFO: Waiting until idle" << std::endl;
#endif
    while ((retry-- > 0) && (readReg(XMC_REG_OFF_CTL) & XMC_PKT_OWNER_MASK)){
        (void) nanosleep(&req, nullptr);
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

unsigned XMC_Flasher::readReg(unsigned RegOffset) {
    unsigned value;
    if( mDev->pcieBarRead(mRegBase + RegOffset, &value, 4) != 0 ) {
        assert(0);
        std::cout << "read reg ERROR" << std::endl;
    }
    return value;
}

int XMC_Flasher::writeReg(unsigned RegOffset, unsigned value) {
    int status = mDev->pcieBarWrite(mRegBase + RegOffset, &value, 4);
    if(status != 0) {
        assert(0);
        std::cout << "write reg ERROR " << std::endl;
        return -EINVAL;
    }
    return 0;
}

bool XMC_Flasher::isXMCReady()
{
    bool xmcReady = (XMC_MODE() == XMC_READY);

    if (!xmcReady) {
        xrt_core::ios_flags_restore format(std::cout);
        if (!mDev->get_sysfs_path("xmc", "").empty()) {
            std::cout << "ERROR: XMC is not ready: 0x" << std::hex
                << XMC_MODE() << std::endl;
        }
    }
    return xmcReady;
}

bool XMC_Flasher::isBMCReady()
{
    unsigned int val;
    std::string errmsg;

    mDev->sysfs_get<unsigned>("xmc", "sc_presence", errmsg, val, 0);
    if (!errmsg.empty()) {
        std::cout << "can't read sc_presence node from " << mDev->sysfs_name <<
            " : " << errmsg << std::endl;
        return false;
    }

    if (val) {
        bool bmcReady = (BMC_MODE() == 0x1);
        if (!bmcReady) {
            xrt_core::ios_flags_restore format(std::cout);
            std::cout << "ERROR: SC is not ready: 0x" << std::hex
                << BMC_MODE() << std::endl;
        }
        return bmcReady;
    }

    return true;
}

static void tiTxtStreamToBin(std::istream& tiTxtStream,
    unsigned int& currentAddr, std::vector<unsigned char>& buf)
{
    // offset to write to XMC device to set SC jump address.
    const unsigned int jumpOffset = 0xffffffff;
    // SC jump address is hard-coded.
    const unsigned int jumpAddr = 0x201;
    bool sectionEnd = false;

    buf.clear();

    while (!sectionEnd) {
        std::string line;
        std::string sectionEndChar("@qQ"); // any char will mark end of section

        // Check if we're done with current section.
        int nextChar = tiTxtStream.peek();
        sectionEnd = (
            (sectionEndChar.find(nextChar) != std::string::npos && !buf.empty())
            || (nextChar == EOF));
        if (sectionEnd)
            break;

        // Skip empty lines.
        std::getline(tiTxtStream, line);
        if (line.size() == 0)
            continue;

        switch (line[0]) {
        case '@':
            // Address line
            currentAddr = std::stoi(line.substr(1), NULL , 16);
            break;
        case 'q':
        case 'Q':
        {
            // End of image, return jump section.
            currentAddr = jumpOffset;
            auto *tmp = reinterpret_cast<const unsigned char *>(&jumpAddr);
            for (unsigned int i = 0; i < sizeof(jumpAddr); i++)
                buf.push_back(tmp[i]);
            sectionEnd = true;
            break;
        }
        default:
            // Data line
            std::stringstream ss(line);
            std::string token;
            while (std::getline(ss, token, ' '))
                buf.push_back(std::stoi(token, NULL, 16));
            break;
        }
    }
}

static int writeImage(std::FILE *xmcDev,
    unsigned int addr, std::vector<unsigned char>& buf)
{
    int ret = 0;
    size_t len = 0;
    const size_t max_write = 4000; // Max size per write

    ret = std::fseek(xmcDev, addr, SEEK_SET);
    if (ret)
        return ret;

    // Write SC image to xmc and print '.' for each write as progress indicator
    for (size_t i = 0; ret == 0 && i < buf.size(); i += len) {
        len = std::min(max_write, buf.size() - i);

        std::cout << "." << std::flush;

        std::size_t s = std::fwrite(buf.data() + i, 1, len, xmcDev);
        if (s != len)
            ret = -ferror(xmcDev);
    }
    return ret;
}

int XMC_Flasher::xclUpgradeFirmwareDrv(std::istream& tiTxtStream)
{
    int ret = 0;

    // Parse Ti-TXT data and write each contiguous chunk to XMC.
    std::vector<unsigned char> buf;
    unsigned int curAddr = UINT_MAX;
    while (ret == 0) {
        tiTxtStreamToBin(tiTxtStream, curAddr, buf);
        if (buf.empty())
            break;
#ifdef  XMC_DEBUG
        std::cout << "Extracted " << buf.size() << "B firmware image @0x"
            << std::hex << curAddr << std::dec << std::endl;
#endif
        ret = writeImage(mXmcDev, curAddr, buf);
    }
    std::cout << std::endl;
    if (ret) {
        std::cout << "ERROR: Failed to update SC firmware, err=" << ret
            << std::endl;
        std::cout << "ERROR: Please refer to dmesg for more details"
            << std::endl;
    }

    return ret;
}
