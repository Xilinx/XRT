/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include <list>
#include "pcidev.h"

#define INVALID_OFFSET	((size_t)-1)

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
    XSPI_Flasher(pcidev::pci_device *dev, bool dualQSPI);
    ~XSPI_Flasher();
    int xclUpgradeFirmware1(std::istream& mcsStream1);
    int xclUpgradeFirmware2(std::istream& mcsStream1, std::istream& mcsStream2);
    int revertToMFG(void);

private:
    pcidev::pci_device *mDev;
    std::FILE *mFlashDev = nullptr;
    bool mDualQSPI;

    int parseMCS(std::istream& mcsStream);

    size_t flash_base;
    int xclTestXSpi(int device_index);
    unsigned readReg(unsigned offset);
    int writeReg(unsigned regOffset, unsigned value);
    bool waitTxEmpty();
    bool isFlashReady();
    bool sectorErase(unsigned Addr, unsigned erase_cmd);
    bool writeBitstreamGuard(unsigned Addr);
    bool clearBitstreamGuard(unsigned Addr);
    bool bulkErase();
    bool writeEnable();
    bool getFlashId();
    bool finalTransfer(uint8_t *sendBufPtr, uint8_t *recvBufPtr, int byteCount);
    bool writePage(unsigned addr, uint8_t writeCmd = 0xff);
    bool readPage(unsigned addr, uint8_t readCmd = 0xff);
    bool prepareXSpi(uint8_t slave_sel);
    int programRecord(std::istream& mcsStream, const ELARecord& record);
    int programXSpi(std::istream& mcsStream, uint32_t bitstream_shift_addr);
    bool readRegister(unsigned commandCode, unsigned bytes);
    bool writeRegister(unsigned commandCode, unsigned value, unsigned bytes);
    bool setSector(unsigned address);
    unsigned getSector(unsigned address);

    // Upgrade firmware via driver.
    int upgradeFirmware1Drv(std::istream& mcsStream1);
    int upgradeFirmware2Drv(std::istream& mcsStream1, std::istream& mcsStream2);
};

#endif
