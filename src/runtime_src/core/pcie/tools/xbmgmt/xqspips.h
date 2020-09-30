/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author(s) : Min Ma
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

#ifndef _XQSPIPS_H_
#define _XQSPIPS_H_

#include <sys/stat.h>
#include <list>
#include <iostream>
#include "core/pcie/linux/scan.h"

#define PAGE_SIZE 256
#define PAGE_8K   8192

class XQSPIPS_Flasher
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
    XQSPIPS_Flasher(std::shared_ptr<pcidev::pci_device> dev);
    ~XQSPIPS_Flasher();
    int revertToMFG();
    int xclUpgradeFirmware(std::istream& binStream);

private:
    typedef struct {
        uint8_t     *bufPtr;
        uint32_t    byteCount;
        uint32_t    busWidth;
        uint32_t    flags;
    } xqspips_msg_t;

    std::shared_ptr<pcidev::pci_device> mDev;
    uint8_t mWriteBuffer[PAGE_8K];
    uint8_t mReadBuffer[PAGE_8K];
    uint32_t mTxBytes;
    uint32_t mRxBytes;
    uint64_t flash_base;

    /* QSPI configure */
    uint32_t mConnectMode; //Single, Stacked and Parallel mode
    uint32_t mBusWidth; // x1, x2, x4

    void clearReadBuffer(unsigned size);
    void clearWriteBuffer(unsigned size);
    void clearBuffers(unsigned size);

    /* Flash functions */
    int xclTestXQSpiPS(int device_index);
    bool getFlashID();
    bool isFlashReady();
    bool readFlashReg(unsigned commandCode, unsigned bytes);
    bool writeFlashReg(unsigned commandCode, unsigned value, unsigned bytes);
    bool eraseSector(unsigned addr, uint32_t byteCount, uint8_t eraseCmd = 0xff);
    bool eraseBulk();
    bool readFlash(unsigned addr, uint32_t byteCount, uint8_t readCmd = 0xff);
    bool writeFlash(unsigned addr, uint32_t byteCount, uint8_t writeCmd = 0xff);

    /* PS QSPI functions */
    void initQSpiPS();
    void abortQSpiPS();
    void resetQSpiPS();
    bool waitGenFifoEmpty();
    bool waitTxEmpty();
    int  writeReg(unsigned regOffset, unsigned value);
    bool enterOrExitFourBytesMode(uint32_t enable);
    uint32_t readReg(unsigned RegOffset);
    uint32_t selectSpiMode(uint8_t SpiMode);
    bool setWriteEnable();
    void readRxFifo(xqspips_msg_t *msg, int32_t Size);
    void fillTxFifo(xqspips_msg_t *msg, int32_t Size);
    void setupTXRX(xqspips_msg_t *msg, uint32_t *GenFifoEntry);
    void sendGenFifoEntryCSAssert();
    void sendGenFifoEntryData(xqspips_msg_t *msg);
    void sendGenFifoEntryCSDeAssert();
    bool finalTransfer(xqspips_msg_t *msg, uint32_t numMsg);
};

#endif
