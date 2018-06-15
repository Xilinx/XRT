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
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define MAGIC_XLNX_STRING "xlnx" // from xclfeatures.h FeatureRomHeader

/*
 * constructor
 */
Flasher::Flasher(unsigned int index, E_FlasherType flasherType) : mIdx(index), mType(flasherType)
{
    mMgmtMap = nullptr;
    mXspi = nullptr;
    mBpi  = nullptr;
    mFd = 0;
    mIsValid = false;
    memset( &mFRHeader, 0, sizeof(mFRHeader) ); // initialize before access

    if( mapDevice( mIdx ) < 0 )
    {
        std::cout << "ERROR: Failed to map pcie device." << std::endl;
        return;
    }

    pcieBarRead( 0, (unsigned long long)mMgmtMap + FEATURE_ROM_BASE, &mFRHeader, sizeof(struct FeatureRomHeader) );

    // Something funny going on here. There must be a strange line ending character. Using "<" will check for a match
    // that EntryPointString starts with magic char sequence "xlnx".
    if( std::string( reinterpret_cast<const char*>( mFRHeader.EntryPointString ) ).compare( MAGIC_XLNX_STRING ) < 0 )
    {
        std::cout << "ERROR: EntryPointString mismatch:" << mFRHeader.EntryPointString << std::endl;
        return;
    }

    if( mType == E_FlasherType::UNSET )
    {
        getProgrammingTypeFromDeviceName( mFRHeader.VBNVName, mType );
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
        break;
    }
}

/*
 * destructor
 */
Flasher::~Flasher()
{
    if( mXspi != nullptr )
    {
        delete mXspi;
        mXspi = nullptr;
    }

    if( mBpi != nullptr )
    {
        delete mBpi;
        mBpi = nullptr;
    }

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
int Flasher::upgradeFirmware(const char *f1, const char *f2)
{
    int retVal = -1;
    switch( mType )
    {
    case SPI:
        if( f2 == nullptr )
        {
            retVal = mXspi->xclUpgradeFirmwareXSpi( f1 );
        }
        else
        {
            retVal = mXspi->xclUpgradeFirmware2( f1, f2 );
        }
        break;
    case BPI:
        if( f2 != nullptr )
        {
            std::cout << "ERROR: BPI mode does not support two mcs files." << std::endl;
            retVal = -1;
        }
        else
        {
            retVal = mBpi->xclUpgradeFirmware( f1 );
        }
        break;
    default:
        std::cout << "ERROR: Invalid programming type." << std::endl;
        retVal = -1;
        break;
    }
    return retVal;
}

/*
 * mapDevice
 */
int Flasher::mapDevice(unsigned int devIdx)
{
    xcldev::pci_device_scanner scanner;
#if DRIVERLESS
    scanner.scan_without_driver();

    if( devIdx >= scanner.device_list.size() ) {
        std::cout << "ERROR: Invalid device index." << std::endl;
        return -EINVAL;
    }

    char cDBDF[128]; // size of dbdf string
    xcldev::pci_device_scanner::device_info dev = scanner.device_list.at( devIdx );
    sprintf( cDBDF, "%.4x:%.2x:%.2x.%.1x", dev.domain, dev.bus, dev.device, dev.mgmt_func );
    mDBDF = std::string( cDBDF );
    std::string devPath = "/sys/bus/pci/devices/" + mDBDF;
#else
    scanner.scan( false );
    std::string mgmtDeviceName;
    if( !scanner.get_mgmt_device_name( mgmtDeviceName, devIdx ) )
    {
        std::cout << "ERROR: Cannot find mgmt device." << std::endl;
        return -EBUSY;
    }
    std::string devPath = "/sys/bus/pci/devices/" + mgmtDeviceName;
#endif
    std::string resourcePath = devPath + "/resource0";

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
        std::cout << "mmap failed : " << errno << std::endl;
        perror( "mmap" );
        close( mFd );
        return errno;
    }
    mMgmtMap = (char *)p;
    close( mFd );

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
        std::cout << "ERROR: failed to determine DSA type, unable to flash device." << std::endl;
        return -1;
    }
    return 0;
}
