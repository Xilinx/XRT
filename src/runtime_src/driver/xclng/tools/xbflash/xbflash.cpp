/**
 * Copyright (C) 2018 Xilinx, Inc
 * Author: Ryan Radjabi
 * A command line utility to program PCIe devices for board bringup.
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
#include <iostream>
#include <unistd.h>
#include <getopt.h>
#include "flasher.h"
#include "xbsak.h"

const char* HelpMessage = "Help: Try 'xbflash [-d device] -m primary_mcs [-n secondary_mcs] [-o spi|bpi]'.";
const char* SudoMessage = "ERROR: XBFLASH requires root privileges.";
bool sudoCheck() {
    return !( getuid() && geteuid() );
}
struct T_Arguments
{
    unsigned devIdx = 0;
    char *file1 = nullptr;
    char *file2 = nullptr;
    Flasher::E_FlasherType flasherType = Flasher::E_FlasherType::UNSET;
    bool isValid = false;
};
int scanDevices( void );
int startIdx = 0;

/*
 * main
 */
int main( int argc, char *argv[] )
{
    std::cout <<"XBFLASH -- Xilinx Board Flash Utility" << std::endl;

    if( argc <= 1 )
    {
        std::cout << HelpMessage << std::endl;
        return 0;
    }

    // launched from xbsak
    if( std::string( argv[ (startIdx+1) ] ).compare( "flash" ) == 0 ) {
        startIdx++;
    }

    if( argc <= (startIdx+2) ) // not a valid flash program scenario
    {
        if( argc <= (startIdx+1) ) {
            std::cout << HelpMessage << std::endl;
            return -1;
        }
        // argc > (startIdx+1)
        if( std::string( argv[ (startIdx+1) ] ).compare( "scan" ) == 0 ) {
            if( !sudoCheck() ) {
                std::cout << SudoMessage << std::endl;
                return -EACCES;
            }
            return scanDevices();
        } else if ( std::string( argv[ (startIdx+1) ] ).compare( "help" ) == 0 ) {
            std::cout << HelpMessage << std::endl;
            return 0;
        } else {
            std::cout << HelpMessage << std::endl;
            return -EINVAL;
        }
    }

    if( !sudoCheck() ) {
        std::cout << SudoMessage << std::endl;
        return -EACCES;
    }

    T_Arguments args;

    // argc > (startIdx+2)
    int opt;
    while( ( opt = getopt( argc, argv, "d:m:n:o:" ) ) != -1 ) // adjust offset?
    {
        switch( opt )
        {
        case 'd':
            args.devIdx = atoi( optarg );
            std::cout << "    device index: " << args.devIdx << std::endl;
            break;
        case 'm':
            args.file1 = optarg;
            std::cout << "    primary mcs: " << args.file1 << std::endl;
            args.isValid = true;
            break;
        case 'n': // optional
            args.file2 = optarg;
            std::cout << "    secondary mcs: " << args.file2 << std::endl;
            break;
        case 'o': // optional
            std::cout << "CAUTION: Overrideing flash programming mode is not recommended. You may damage your device with this option." << std::endl;
            char *input;
            std::cout << "Are you sure you wish to proceed? [y/n]" << std::endl;
            std::cin >> input;
            if( std::string( input ).compare( "y" ) != 0 && std::string( input ).compare( "yes" ) != 0 ) {
                std::cout << "Aborting." << std::endl;
                return 0;
            }
            if( std::string( optarg ).compare( "spi" ) == 0 ) {
                args.flasherType = Flasher::E_FlasherType::SPI;
            } else if( std::string( optarg ).compare( "bpi" ) == 0 ) {
                args.flasherType = Flasher::E_FlasherType::BPI;
            } else {
                args.isValid = false;
                std::cout << "Invalid programming mode '" << optarg
                          << "', must be either 'spi' or 'bpi'." << std::endl;
                break;
            }
            std::cout << "    flash mode: " << optarg << std::endl;
            break;
        default:
            args.isValid = false;
            break;
        }
    }

    if( !args.isValid )
    {
        std::cout << "ERROR: Invalid arguments" << std::endl;
        std::cout << HelpMessage << std::endl;
        return -EINVAL;
    }

    Flasher flasher( args.devIdx, args.flasherType );
    if( !flasher.isValid() ) {
        std::cout << "XBFLASH failed." << std::endl;
        return -EINVAL;
    }

    if( flasher.upgradeFirmware( args.file1, args.file2 ) != 0 )
    {
        std::cout << "XBFLASH failed." << std::endl;
        return -1;
    }
    else
    {
        std::cout << "XBFLASH completed succesfully. Please cold reboot machine to force load the updated flash image on the FPGA." << std::endl;
        return 0;
    }
}

/*
 * scanDevices
 *
 * Uses pci_device_scanner to enumerate SDx devices and populates device_list.
 * TODO: Add error checking and return nonzero in the event of failure.
 * Returns 0 on success.
 */
int scanDevices( void )
{
    std::cout << "SCAN found the following devices:" << std::endl;
    xcldev::pci_device_scanner scanner;
#if DRIVERLESS
    scanner.scan_without_driver();
    for( unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++ ) {
        Flasher f( i );
        std::cout << "[" << i << "]" << std::endl;
        std::cout << "    DBDF:  " << f.sGetDBDF() << std::endl;
        std::cout << "    DSA:   " << f.sGetDSAName() << std::endl;
        std::cout << "    Flash: " << f.sGetFlashType() << std::endl;
    }
#else
    scanner.scan( false ); // silent scan
    std::vector<std::unique_ptr<xcldev::device>> deviceVec;
    for( unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++ ) {
        deviceVec.emplace_back( new xcldev::device( i, nullptr ) );
    }
    for( unsigned i = 0; i < deviceVec.size(); i++ ) {
        std::cout << '[' << i << "] " << deviceVec[i]->name() << std::endl;
        Flasher f( i );
    }

    if( xcldev::pci_device_scanner::device_list.size() == 0 )
    {
        std::cout << "No available devices found.";
    }
#endif
    return 0;
}


