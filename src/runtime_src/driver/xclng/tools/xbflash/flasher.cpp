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
#include <dirent.h>
#include <cassert>
#include <cstring>
#include <vector>

#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define MAGIC_XLNX_STRING "xlnx" // from xclfeatures.h FeatureRomHeader

/*
 * constructor
 */
Flasher::Flasher(unsigned int index, E_FlasherType flasherType) : 
  mType(flasherType), mIdx(index)
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
        std::cout << "ERROR: Failed to detect feature ROM." << std::endl;
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
int Flasher::upgradeFirmware(const char *f1, const char *f2)
{
    int retVal = -EINVAL;
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
        }
        else
        {
            retVal = mBpi->xclUpgradeFirmware( f1 );
        }
        break;
    default:
        std::cout << "ERROR: Invalid programming type." << std::endl;
        break;
    }
    return retVal;
}

int Flasher::upgradeBMCFirmware(const char *f1)
{
    int ret;

    MSP432_Flasher *flasher = new MSP432_Flasher(mIdx, mMgmtMap);
    ret = flasher->xclUpgradeFirmware(f1);
    delete flasher;
    return ret;
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
    xcldev::pci_device_scanner::device_info dev = scanner.device_list.at( devIdx );
    sprintf( cDBDF, "%.4x:%.2x:%.2x.%.1x", dev.domain, dev.bus, dev.device, dev.mgmt_func );
    mDBDF = std::string( cDBDF );
    std::string devPath = "/sys/bus/pci/devices/" + mDBDF;

    char bar[5];
    snprintf(bar, sizeof (bar) - 1, "%d", dev.user_bar);
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
        return -EINVAL;
    }
    return 0;
}

/*
 * Helper to parse DSA name string and retrieve all tokens
 * The DSA name string is passed in by value since it'll be modified inside.
 */
std::vector<std::string> DSANameParser(std::string name)
{
    std::vector<std::string> tokens;
    std::string delimiter = "_";

    size_t pos = 0;
    std::string token;
    while ((pos = name.find(delimiter)) != std::string::npos)
    {
        token = name.substr(0, pos);
        tokens.push_back(token);
        name.erase(0, pos + delimiter.length());
    }
    tokens.push_back(name);
    return tokens;
}

/*
 * Obtain all DSA installed on the system for this board
 */
std::vector<std::string> Flasher::sGetInstalledDSA()
{
    std::vector<std::string> DSAs;
    std::vector<std::string> tokens = DSANameParser(sGetDSAName());
    struct dirent *entry;
    DIR *dp;
    std::string nm;

    // At least, we need vendor.board.dsa
    if (tokens.size() < 3)
        return DSAs;

    dp = opendir(FIRMWARE_DIR);
    if (dp)
    {
        while ((entry = readdir(dp)))
        {
            std::string e(entry->d_name);

            // Looking for ".mcs" file
            size_t pos = e.find_last_of(".");
            if (pos == 0 || pos == std::string::npos ||
                e.substr(pos + 1).compare(DSA_FILE_SUFFIX) != 0)
                continue;

            // Matching vendor.board token in the file name
            std::string dsa = e.substr(0, pos);
            std::vector<std::string> t = DSANameParser(dsa);
            if (t.back().compare("secondary") == 0 || t[0] != tokens[0] || t[1] != tokens[1])
                continue;

            // Take it
            if (t.back().compare("primary") == 0)
                dsa.erase(dsa.rfind("primary") - 1);
            DSAs.push_back(dsa);
        }
        closedir(dp);
    }

    return DSAs;
}
