/**
 * Copyright (C) 2018-2019 Xilinx, Inc
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
#include "xqspips.h"
#include "xospiversal.h"
#include "prom.h"
#include "xmc.h"
#include "firmware_image.h"
#include "core/pcie/linux/scan.h"
#include "xclfeatures.h"
#include <sys/stat.h>
#include <vector>
#include <memory>
#include <climits>

struct BoardInfo
{
    std::string mSerialNum;
    std::string mRev;
    std::string mName;
    std::string mMacAddr0;
    std::string mMacAddr1;
    std::string mMacAddr2;
    std::string mMacAddr3;
    std::string mBMCVer;
    std::string mMaxPower;
    unsigned int mConfigMode;
    char mFanPresence;
};

enum BoardInfoKey
{
    BDINFO_SN = 0x21,
    BDINFO_MAC0,
    BDINFO_MAC1,
    BDINFO_MAC2,
    BDINFO_MAC3,
    BDINFO_REV,
    BDINFO_NAME,
    BDINFO_BMC_VER,
    BDINFO_MAX_PWR,
    BDINFO_FAN_PRESENCE,
    BDINFO_CONFIG_MODE
};

class Flasher
{
public:
    Flasher(unsigned int index);
    int upgradeFirmware(const std::string& typeStr, firmwareImage* primary, firmwareImage* secondary);
    int upgradeBMCFirmware(firmwareImage* bmc);
    bool isValid(void) { return mDev != nullptr; }

    std::string sGetDBDF();
    std::string sGetFlashType() { return std::string( getFlasherTypeText( getFlashType() ) ); }
    DSAInfo getOnBoardDSA();
    std::vector<DSAInfo> getInstalledDSA();
    int getBoardInfo(BoardInfo& board);

private:
    enum E_FlasherType {
        UNKNOWN,
        SPI,
        BPI,
        QSPIPS,
        OSPIVERSAL,
    };
    const char *E_FlasherTypeStrings[4] = { "UNKNOWN", "SPI", "BPI", "QSPI_PS" };
    const char *getFlasherTypeText( E_FlasherType val ) { return E_FlasherTypeStrings[ val ]; }
    std::shared_ptr<pcidev::pci_device> mDev;

    int getProgrammingTypeFromDeviceName(unsigned char name[], E_FlasherType &type );

    FeatureRomHeader mFRHeader;
    unsigned int mGoldenVer = UINT_MAX;

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
        std::make_pair( "vega-4000", SPI )
        // No more flash type added here. Add them in devices.h please.
    };
    E_FlasherType getFlashType(std::string typeStr = "");
};

#endif // FLASHER_H
