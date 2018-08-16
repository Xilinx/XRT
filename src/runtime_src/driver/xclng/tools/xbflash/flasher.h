/**
 * Copyright (C) 2018 Xilinx, Inc
 * Author: Ryan Radjabi
 *
 * This is a wrapper class that does the prep work required to program a flash
 * device. Flasher will create a specific flash object determined by the program
 * mode read from FeatureROM. Common functions between XSPI_Flasher and BPI_Flasher
 * are implemented here.
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
#ifndef FLASHER_H
#define FLASHER_H

#include "xspi.h"
#include "prom.h"
#include "msp432.h"
#include "xclfeatures.h"
#include "firmware_image.h"
#include "scan.h"
#include <sys/stat.h>
#include <vector>
#include <memory>

class Flasher
{
public:
    enum E_FlasherType {
        UNSET,
        SPI,
        BPI,
    };
    const char *E_FlasherTypeStrings[3] = { "UNSET", "SPI", "BPI" };
    const char *getFlasherTypeText( E_FlasherType val ) { return E_FlasherTypeStrings[ val ]; }

    Flasher(unsigned int index, std::string flasherType="");
    ~Flasher();
    int upgradeFirmware(firmwareImage* primary, firmwareImage* secondary);
    int upgradeBMCFirmware(firmwareImage* bmc);
    bool isValid( void ) { return mIsValid; }

    static void* wordcopy(void *dst, const void* src, size_t bytes);
    static int flashRead(unsigned int pf_bar, unsigned long long offset, void *buffer, unsigned long long length);
    static int flashWrite(unsigned int pf_bar, unsigned long long offset, const void *buffer, unsigned long long length);
    static int pcieBarRead(unsigned int pf_bar, unsigned long long offset, void* buffer, unsigned long long length);
    static int pcieBarWrite(unsigned int pf_bar, unsigned long long offset, const void* buffer, unsigned long long length);

    std::string sGetDBDF() { return mDBDF; }
    std::string sGetFlashType() { return std::string( getFlasherTypeText( mType ) ); }
    DSAInfo getOnBoardDSA();
    std::vector<DSAInfo> getInstalledDSA();

private:
    E_FlasherType mType;
    unsigned int mIdx;
    XSPI_Flasher *mXspi;
    BPI_Flasher  *mBpi;
    bool mIsValid;
    xcldev::pci_device_scanner::device_info mDev;

    int mapDevice(unsigned int devIdx);
    int getProgrammingTypeFromDeviceName(unsigned char name[], E_FlasherType &type );

    FeatureRomHeader mFRHeader;
    char *mMgmtMap;
    int mFd;
    struct stat mSb;
    std::string mDBDF;

    const std::vector<std::pair<std::string, E_FlasherType>> flashPairs = {
        std::make_pair( "7v3", BPI ),
        std::make_pair( "8k5", BPI ),
        std::make_pair( "ku3", BPI ),
        std::make_pair( "vu9p",      SPI ),
        std::make_pair( "ku115",     SPI ),
        std::make_pair( "kcu1500",   SPI ),
        std::make_pair( "vcu1525",   SPI ),
        std::make_pair( "vcu1526",   SPI ),
        std::make_pair( "vcu1550",   SPI ),
        std::make_pair( "vcu1551",   SPI ),
        std::make_pair( "vega-4000", SPI ),
        std::make_pair( "u200",    SPI ),
        std::make_pair( "u250",    SPI )
    };
};

#endif // FLASHER_H
