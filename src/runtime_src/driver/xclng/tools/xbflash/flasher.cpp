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

#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define MAGIC_XLNX_STRING "xlnx" // from xclfeatures.h FeatureRomHeader

/*
 * constructor
 */
Flasher::Flasher(unsigned int index, std::string flasherType) : 
  mIdx(index)
{
    mMgmtMap = nullptr;
    mXspi = nullptr;
    mBpi  = nullptr;
    mFd = 0;
    mIsValid = false;
    mType = E_FlasherType::UNSET;
    memset( &mFRHeader, 0, sizeof(mFRHeader) ); // initialize before access

    if( mapDevice( mIdx ) < 0 )
    {
        std::cout << "ERROR: Failed to map pcie device." << std::endl;
        return;
    }

    if( flasherType.size() == 0) {
        flasherType = std::string(mDev.flash_type);
    }

    if (flasherType.size() == 0) {
        // not specified in both command line or device profile
        getProgrammingTypeFromDeviceName( mFRHeader.VBNVName, mType );
    } else if( std::string( flasherType ).compare( "spi" ) == 0 ) {
        mType = Flasher::E_FlasherType::SPI;
    } else if( std::string( flasherType ).compare( "bpi" ) == 0 ) {
        mType = Flasher::E_FlasherType::BPI;
    } else {
        // user input invalid flash type string
        std::cout << "Invalid programming mode '" << flasherType
                  << "', must be either 'spi' or 'bpi'." << std::endl;
        return;
    }

    switch( mType )
    {
    case SPI:
        mXspi = new XSPI_Flasher( mIdx, mMgmtMap );
        mIsValid = true;
        break;
    case BPI:
        mBpi = new BPI_Flasher( mIdx, mMgmtMap );
        mIsValid = true;
        break;
    default:
        std::cout << " Unknown flash type " << std::endl;
        return;
    }
}

/*
 * destructor
 */
Flasher::~Flasher()
{
    delete mXspi;
    mXspi = nullptr;

    delete mBpi;
    mBpi = nullptr;

    if( mMgmtMap != nullptr )
    {
        munmap( mMgmtMap, mSb.st_size );
    }

    if( mFd > 0 )
    {
        close( mFd );
    }
}

/*
 * upgradeFirmware
 */
int Flasher::upgradeFirmware(firmwareImage *primary, firmwareImage *secondary)
{
    int retVal = -EINVAL;
    switch( mType )
    {
    case SPI:
        if(secondary == nullptr)
        {
            retVal = mXspi->xclUpgradeFirmwareXSpi(*primary);
        }
        else
        {
            retVal = mXspi->xclUpgradeFirmware2(*primary, *secondary);
        }
        break;
    case BPI:
        if(secondary != nullptr)
        {
            std::cout << "ERROR: BPI mode does not support two mcs files." << std::endl;
        }
        else
        {
            retVal = mBpi->xclUpgradeFirmware(*primary);
        }
        break;
    default:
        std::cout << "ERROR: Invalid programming type." << std::endl;
        break;
    }
    return retVal;
}

int Flasher::upgradeBMCFirmware(firmwareImage* bmc)
{
    MSP432_Flasher flasher(mIdx, mMgmtMap);
    return flasher.xclUpgradeFirmware(*bmc);
}

/*
 * mapDevice
 */
int Flasher::mapDevice(unsigned int devIdx)
{
    xcldev::pci_device_scanner scanner;
    scanner.scan_without_driver();

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
    if (!scanner.get_feature_rom_bar_offset(mIdx, feature_rom_base))
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
        std::cout << "ERROR: failed to determine flash type." << std::endl;
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
    std::string vbnv = std::string(reinterpret_cast<char *>(mFRHeader.VBNVName));

    if (vbnv.empty())
        std::cout << "No Feature ROM Loaded" << std::endl;

    return DSAInfo(vbnv, mFRHeader.TimeSinceEpoch);
}
