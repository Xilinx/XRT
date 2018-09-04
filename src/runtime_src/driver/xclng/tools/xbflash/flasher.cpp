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
#include "flasher.h"
#include "scan.h"
#include "mgmt-reg.h"
#include <sys/mman.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define MAGIC_XLNX_STRING "xlnx" // from xclfeatures.h FeatureRomHeader
#define MFG_REV_OFFSET  0x131008

/*
 * constructor
 */
Flasher::Flasher(unsigned int index) :
  mIdx(index), mIsValid(false), mMgmtMap(nullptr), mFd(-1)
{
    if( mapDevice( mIdx ) < 0 )
    {
        std::cout << "ERROR: Failed to map pcie device." << std::endl;
    }
    else
    {
        mIsValid = true;
    }
}

/*
 * destructor
 */
Flasher::~Flasher()
{
    if( mMgmtMap != nullptr )
    {
        munmap( mMgmtMap, mSb.st_size );
    }

    if( mFd >= 0 )
    {
        close( mFd );
    }
}

Flasher::E_FlasherType Flasher::getFlashType(std::string typeStr)
{
    E_FlasherType type = E_FlasherType::UNKNOWN;

    if (typeStr.empty())
        typeStr = mDev.flash_type;

    if (typeStr.empty())
    {
        getProgrammingTypeFromDeviceName(mFRHeader.VBNVName, type);
    }
    else if (typeStr.compare("spi") == 0)
    {
        type = E_FlasherType::SPI;
    }
    else if (typeStr.compare("bpi") == 0)
    {
        type = E_FlasherType::BPI;
    }
    else
    {
        std::cout << "Unknown flash type: " << typeStr << std::endl;
    }

    return type;
}

/*
 * upgradeFirmware
 */
int Flasher::upgradeFirmware(const std::string& flasherType,
    firmwareImage *primary, firmwareImage *secondary)
{
    int retVal = -EINVAL;
    E_FlasherType type = getFlashType(flasherType);

    switch(type)
    {
    case SPI:
    {
        XSPI_Flasher xspi(mIdx, mMgmtMap);
        if(secondary == nullptr)
        {
            retVal = xspi.xclUpgradeFirmwareXSpi(*primary);
        }
        else
        {
            retVal = xspi.xclUpgradeFirmware2(*primary, *secondary);
        }
        break;
    }
    case BPI:
    {
        BPI_Flasher bpi(mIdx, mMgmtMap);
        if(secondary != nullptr)
        {
            std::cout << "ERROR: BPI mode does not support two mcs files." << std::endl;
        }
        else
        {
            retVal = bpi.xclUpgradeFirmware(*primary);
        }
        break;
    }
    default:
        break;
    }
    return retVal;
}

int Flasher::upgradeBMCFirmware(firmwareImage* bmc)
{
    XMC_Flasher flasher(mIdx, mMgmtMap);
    const std::string e = flasher.probingErrMsg();

    if (!e.empty())
    {
        std::cout << "ERROR: " << e << std::endl;
        return -EOPNOTSUPP;
    }

    return flasher.xclUpgradeFirmware(*bmc);
}

std::string charVec2String(std::vector<char>& v)
{
    std::stringstream ss;

    for (unsigned i = 0; i < v.size(); i++)
    {
        ss << v[i];
    }
    return ss.str();
}

int Flasher::getBoardInfo(BoardInfo& board)
{
    std::map<char, std::vector<char>> info;
    XMC_Flasher flasher(mIdx, mMgmtMap);

    if (!flasher.probingErrMsg().empty())
    {
        return -EOPNOTSUPP;
    }

    int ret = flasher.xclGetBoardInfo(info);
    if (ret != 0)
        return ret;

    board.mBMCVer = std::move(charVec2String(info[BDINFO_BMC_VER]));
    board.mConfigMode = info[BDINFO_CONFIG_MODE][0];
    board.mFanPresence = info[BDINFO_FAN_PRESENCE][0];
    board.mMacAddr0 = std::move(charVec2String(info[BDINFO_MAC0]));
    board.mMacAddr1 = std::move(charVec2String(info[BDINFO_MAC1]));
    board.mMacAddr2 = std::move(charVec2String(info[BDINFO_MAC2]));
    board.mMacAddr3 = std::move(charVec2String(info[BDINFO_MAC3]));
    board.mMaxPowerLvl = info[BDINFO_MAX_PWR][0];
    board.mName = std::move(charVec2String(info[BDINFO_NAME]));
    board.mRev = std::move(charVec2String(info[BDINFO_REV]));
    board.mSerialNum = std::move(charVec2String(info[BDINFO_SN]));

    return ret;
}

/*
 * mapDevice
 */
int Flasher::mapDevice(unsigned int devIdx)
{
    xcldev::pci_device_scanner scanner;
    scanner.scan_without_driver();

    memset( &mFRHeader, 0, sizeof(mFRHeader) ); // initialize before access

    if( devIdx >= scanner.device_list.size() ) {
        std::cout << "ERROR: Invalid device index." << std::endl;
        return -EINVAL;
    }

    char cDBDF[128]; // size of dbdf string
    mDev = scanner.device_list.at( devIdx );
    sprintf( cDBDF, "%.4x:%.2x:%.2x.%.1x", mDev.domain, mDev.bus, mDev.device, mDev.mgmt_func );
    mDBDF = std::string( cDBDF );
    std::string devPath = "/sys/bus/pci/devices/" + mDBDF;

    char bar[5];
    snprintf(bar, sizeof (bar) - 1, "%d", mDev.user_bar);
    std::string resourcePath = devPath + "/resource" + bar;

    void *p;
    void *addr = (caddr_t)0;
    mFd = open( resourcePath.c_str(), O_RDWR );
    if( mFd <= 0 )
    {
        std::cout << "ERROR: open sysfs failed\n";
        return -EINVAL;
    }
    if( fstat( mFd, &mSb ) == -1 )
    {
        std::cout << "ERROR: unable to fstat sysfs\n";
        return -EINVAL;
    }
    p = mmap( addr, mSb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, mFd, 0 );
    if( p == MAP_FAILED )
    {
        std::cout << "mmap failed: " << errno << std::endl;
        perror( "mmap" );
        close( mFd );
        return errno;
    }
    mMgmtMap = (char *)p;
    close( mFd );

    unsigned long long feature_rom_base;
    if (scanner.get_feature_rom_bar_offset(mIdx, feature_rom_base) == 0)
    {
        pcieBarRead( 0, (unsigned long long)mMgmtMap + feature_rom_base, &mFRHeader, sizeof(struct FeatureRomHeader) );
        // Something funny going on here. There must be a strange line ending character. Using "<" will check for a match
        // that EntryPointString starts with magic char sequence "xlnx".
        if( std::string( reinterpret_cast<const char*>( mFRHeader.EntryPointString ) ).compare( MAGIC_XLNX_STRING ) < 0 )
        {
            std::cout << "ERROR: Failed to detect feature ROM." << std::endl;
            return -EINVAL;
        }
    }
    else if (mDev.is_mfg)
    {
        pcieBarRead( 0, (unsigned long long)mMgmtMap + MFG_REV_OFFSET, &mGoldenVer, sizeof(mGoldenVer) );
    }
    else
    {
        std::cout << "ERROR: Device not supported." << std::endl;
        return -EINVAL;
    }

    return 0;
}

/*
 * pcieBarRead
 */
int Flasher::pcieBarRead(unsigned int pf_bar, unsigned long long offset, void* buffer, unsigned long long length)
{
    wordcopy(buffer, (void*)offset, length);
    return 0;
}

/*
 * pcieBarWrite
 */
int Flasher::pcieBarWrite(unsigned int pf_bar, unsigned long long offset, const void* buffer, unsigned long long length)
{
    wordcopy((void*)offset, buffer, length);
    return 0;
}

int Flasher::flashRead(unsigned int pf_bar, unsigned long long offset, void* buffer, unsigned long long length)
{
    return pcieBarRead( pf_bar, ( offset + FLASH_BASE_ADDRESS ), buffer, length );
}

int Flasher::flashWrite(unsigned int pf_bar, unsigned long long offset, const void* buffer, unsigned long long length)
{
    return pcieBarWrite( pf_bar, (offset + FLASH_BASE_ADDRESS ), buffer, length );
}

/*
 * wordcopy
 *
 * Copy bytes word (32bit) by word.
 * Neither memcpy, nor std::copy work as they become byte copying on some platforms.
 */
void* Flasher::wordcopy(void *dst, const void* src, size_t bytes)
{
    // assert dest is 4 byte aligned
    assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

    using word = uint32_t;
    auto d = reinterpret_cast<word*>(dst);
    auto s = reinterpret_cast<const word*>(src);
    auto w = bytes/sizeof(word);

    for (size_t i=0; i<w; ++i)
    {
        d[i] = s[i];
    }

    return dst;
}

/*
 * getProgrammingTypeFromDeviceName
 */
int Flasher::getProgrammingTypeFromDeviceName(unsigned char name[], E_FlasherType &type)
{
    std::string strDsaName( reinterpret_cast<const char*>(name) );
    bool typeFound = false;
    for( unsigned i = 0; i < flashPairs.size(); i++ )
    {
        auto pair = flashPairs.at( i );
        if( strDsaName.find( pair.first ) != std::string::npos )
        {
            type = pair.second;
            typeFound = true;
            break;
        }
    }
    if( !typeFound )
    {
        return -EINVAL;
    }
    return 0;
}

/*
 * Obtain all DSA installed on the system for this board
 */
std::vector<DSAInfo> Flasher::getInstalledDSA()
{
    std::vector<DSAInfo> DSAs;

    // Obtain board info.
    DSAInfo onBoard = getOnBoardDSA();
    if (onBoard.vendor.empty() || onBoard.board.empty())
    {
        std::cout << "Onboard DSA is unknown" << std::endl;
        return DSAs;
    }

    // Obtain installed DSA info.
    auto installedDSAs = firmwareImage::getIntalledDSAs();
    for (DSAInfo& dsa : installedDSAs)
    {
        if (onBoard.vendor != dsa.vendor ||
            onBoard.board != dsa.board ||
            dsa.timestamp == NULL_TIMESTAMP)
            continue;
        DSAs.push_back(dsa);
    }

    return DSAs;
}

DSAInfo Flasher::getOnBoardDSA()
{
    std::string vbnv;
    std::string bmc;
    uint64_t ts = NULL_TIMESTAMP;

    if (mDev.is_mfg)
    {
        std::stringstream ss;

        ss << "xilinx_" << mDev.board_name << "_GOLDEN_" << mGoldenVer;
        vbnv = ss.str();
    }
    else if (mFRHeader.VBNVName[0] != '\0')
    {
        vbnv = std::string(reinterpret_cast<char *>(mFRHeader.VBNVName));
        ts = mFRHeader.TimeSinceEpoch;
    }
    else
    {
        std::cout << "ERROR: No Feature ROM found" << std::endl;
    }

    BoardInfo info;
    if (getBoardInfo(info) == 0)
        bmc = info.mBMCVer;

    return DSAInfo(vbnv, ts, bmc);
}
