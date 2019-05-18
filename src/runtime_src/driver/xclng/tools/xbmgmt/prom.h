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
#ifndef _PROM_H_
#define _PROM_H_

#include <list>
#include <sys/stat.h>
#include <iostream>
#include "scan.h"

class BPI_Flasher
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
    BPI_Flasher(std::shared_ptr<pcidev::pci_device> dev);
    ~BPI_Flasher();
    int xclUpgradeFirmware(std::istream& mcsStream);

private:
    std::shared_ptr<pcidev::pci_device> mDev;

    int freezeAXIGate();
    int freeAXIGate();
    int prepare_microblaze(unsigned startAddress, unsigned endAddress);
    int prepare(unsigned startAddress, unsigned endAddress);
    int program_microblaze(std::istream& mcsStream, const ELARecord& record);
    int program(std::istream& mcsStream, const ELARecord& record);
    int program(std::istream& mcsStream);
    int waitForReady_microblaze(unsigned code, bool verbose = true);
    int waitForReady(unsigned code, bool verbose = true);
    int waitAndFinish_microblaze(unsigned code, unsigned data, bool verbose = true);
    int waitAndFinish(unsigned code, unsigned data, bool verbose = true);
};

#endif
