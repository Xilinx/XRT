/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author(s) : Sonal Santan
 *           : Hem Neema
 *           : Ryan Radjabi
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
#ifndef _XSPI_H_
#define _XSPI_H_

#include <sys/stat.h>
#include <list>
#include <iostream>


class XSPI_Flasher
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
    ELARecordList recordList;

public:
    XSPI_Flasher( unsigned int device_index, char *inMap );
    ~XSPI_Flasher();
    int xclUpgradeFirmware2(const char *file1, const char* file2);
    int xclUpgradeFirmwareXSpi(const char *fileName, int device_index=0);
//    std::ofstream mLogStream;

private:
    char *mMgmtMap;

    int xclTestXSpi(int device_index);
    unsigned readReg(unsigned offset);
    int writeReg(unsigned regOffset, unsigned value);
    bool waitTxEmpty();
    bool isFlashReady();
    bool sectorErase(unsigned Addr);
    bool bulkErase();
    bool writeEnable();
    bool getFlashId();
    bool finalTransfer(uint8_t *sendBufPtr, uint8_t *recvBufPtr, int byteCount);
    bool writePage(unsigned addr, uint8_t writeCmd = 0xff);
    bool readPage(unsigned addr, uint8_t readCmd = 0xff);
    bool prepareXSpi();
    int programXSpi(std::ifstream& mcsStream, const ELARecord& record);
    int programXSpi(std::ifstream& mcsStream);
    bool readRegister(unsigned commandCode, unsigned bytes);
    bool writeRegister(unsigned commandCode, unsigned value, unsigned bytes);
};

#endif
