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
#include "scan.h"

const char* UsageMessages[] = {
    "xbflash [-d device] -m primary_mcs [-n secondary_mcs] [-o spi|bpi]'",
    "xbflash [-d device] -p msp432_firmware",
};
const char* SudoMessage = "ERROR: XBFLASH requires root privileges.";
void usage() {
    std::cout << "Usage:" << std::endl;
    for (unsigned i = 0; i < (sizeof(UsageMessages) / sizeof(UsageMessages[0])); i++)
        std::cout << "\t" << UsageMessages[i] << std::endl;
}
void usageAndDie() { usage(); exit(-EINVAL); }
void sudoOrDie() {
    if ( (getuid() == 0) || (geteuid() == 0) ) {
        return;
    }
    std::cout << SudoMessage << std::endl;
    exit(-EPERM);
}
void notSeenOrDie(bool& seen_opt) {
    if (seen_opt) { usageAndDie(); }
    seen_opt = true;
}
struct T_Arguments
{
    unsigned devIdx = 0;
    char *file1 = nullptr;
    char *file2 = nullptr;
    std::string flasherType;
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
        usage();
        return 0;
    }

    // launched from xbutil
    if( std::string( argv[ (startIdx+1) ] ).compare( "flash" ) == 0 ) {
        startIdx++;
    }

    if( argc <= (startIdx+2) ) // not a valid flash program scenario
    {
        if( argc <= (startIdx+1) ) {
            usageAndDie();
        }

        // argc > (startIdx+1)
        if( std::string( argv[ (startIdx+1) ] ).compare( "scan" ) == 0 ) {
            sudoOrDie();
            return scanDevices();
        } else if ( std::string( argv[ (startIdx+1) ] ).compare( "help" ) == 0 ) {
            usage();
            return 0;
        } else {
            usageAndDie();
        }
    }

    sudoOrDie();

    bool seen_d = false;
    bool seen_m = false;
    bool seen_n = false;
    bool seen_o = false;
    bool seen_p = false;
    T_Arguments args;

    // argc > (startIdx+2)
    int opt;
    while( ( opt = getopt( argc, argv, "d:m:n:o:p:" ) ) != -1 ) // adjust offset?
    {
        switch( opt )
        {
        case 'd':
            notSeenOrDie(seen_d);
            args.devIdx = atoi( optarg );
            std::cout << "    device index: " << args.devIdx << std::endl;
            break;
        case 'm':
            notSeenOrDie(seen_m);
            args.file1 = optarg;
            std::cout << "    primary mcs: " << args.file1 << std::endl;
            args.isValid = true;
            break;
        case 'n': // optional
            notSeenOrDie(seen_n);
            args.file2 = optarg;
            std::cout << "    secondary mcs: " << args.file2 << std::endl;
            break;
        case 'o': // optional
          {
            notSeenOrDie(seen_o);
            std::cout << "CAUTION: Overrideing flash programming mode is not recommended. You may damage your device with this option." << std::endl;
            std::string input;
            std::cout << "Are you sure you wish to proceed? [y/n]" << std::endl;
            std::cin >> input;
            if( input.compare( "y" ) != 0 && input.compare( "yes" ) != 0 ) {
                std::cout << "Aborting." << std::endl;
                return 0;
            }
            args.flasherType = std::string( optarg );
            break;
          }
        case 'p':
            notSeenOrDie(seen_p);
            args.file1 = optarg;
            args.flasherType = Flasher::E_FlasherType::MSP432;
            std::cout << "    MSP432 firmware image: " << args.file1 << std::endl;
            args.isValid = true;
            break;
        default:
            args.isValid = false;
            break;
        }
    }

    if( !args.isValid || (seen_p && (seen_m || seen_n || seen_o)) )
    {
        usageAndDie();
    }

    Flasher flasher( args.devIdx, args.flasherType );
    if( !flasher.isValid() ) {
        std::cout << "XBFLASH failed." << std::endl;
        return -EINVAL;
    }

    if( flasher.upgradeFirmware( args.file1, args.file2 ) != 0 )
    {
        std::cout << "XBFLASH failed." << std::endl;
        return -EINVAL;
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


