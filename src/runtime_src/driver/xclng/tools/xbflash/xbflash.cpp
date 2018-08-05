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
#include <climits>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <getopt.h>
#include <memory>
#include "flasher.h"
#include "scan.h"
#include "firmware_image.h"

const char* UsageMessages[] = {
    "[-d device] -m primary_mcs [-n secondary_mcs] [-o spi|bpi]'",
    "[-d device] -p msp432_firmware",
    "[-d device] -a dsa",
};
const char* SudoMessage = "ERROR: root privileges required.";
int scanDevices( void );

void usage() {
    std::cout << "Available options:" << std::endl;
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
    unsigned devIdx = UINT_MAX;
    std::shared_ptr<firmwareImage> primary;
    std::shared_ptr<firmwareImage> secondary;
    std::shared_ptr<firmwareImage> bmc;
    Flasher::E_FlasherType flasherType = Flasher::E_FlasherType::UNSET;
    std::string dsa;
    uint64_t timestamp = ULLONG_MAX;
};

int flashDSA(Flasher& f, DSAInfo& dsa)
{
    std::shared_ptr<firmwareImage> primary;
    std::shared_ptr<firmwareImage> secondary;

    primary = std::make_shared<firmwareImage>(dsa.file.c_str(), MCS_FIRMWARE_PRIMARY);

    size_t pos = dsa.file.rfind("primary");
    if (pos != std::string::npos)
    {
        std::string sec = dsa.file.substr(0, pos);
        sec += "secondary" "." DSA_FILE_SUFFIX;
        secondary = std::make_shared<firmwareImage>(sec.c_str(), MCS_FIRMWARE_SECONDARY);
    }

    if (primary->fail() || (secondary != nullptr && secondary->fail()))
        return -EINVAL;

    return f.upgradeFirmware(primary, secondary);
}

int updateDSA(unsigned idx, std::string& dsa, uint64_t ts, bool dryrun)
{
    Flasher flasher(idx);
    if(!flasher.isValid())
        return -EINVAL;

    std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();
    DSAInfo currentDSA = flasher.getOnBoardDSA();
    unsigned int candidateDSAIndex = UINT_MAX;

    // Find candidate DSA from installed DSA list.
    if (dsa.compare("all") == 0)
    {
        if (installedDSA.size() > 1)
            return -ENOTUNIQ;
        else
            candidateDSAIndex = 0;
    }
    else
    {
        for (unsigned int i = 0; i < installedDSA.size(); i++)
        {
            DSAInfo& idsa = installedDSA[i];
            if (dsa != idsa.name)
                continue;
            if (ts != ULLONG_MAX && ts != idsa.timestamp)
                continue;
            if (candidateDSAIndex != UINT_MAX)
                return -ENOTUNIQ;
            candidateDSAIndex = i;
        }
    }

    if (candidateDSAIndex == UINT_MAX)
        return -ENOENT;
    DSAInfo& candidate = installedDSA[candidateDSAIndex];

    if (candidate.name == currentDSA.name &&
        candidate.timestamp == currentDSA.timestamp)
        return -EALREADY;

    if (!dryrun)
        return flashDSA(flasher, candidate);

    return 0;
}

bool canProceed()
{
    std::string input;
    bool answered = false;
    bool proceed = false;

    while (!answered)
    {
        std::cout << "Are you sure you wish to proceed? [y/n]" << std::endl;
        std::cin >> input;

        if(input.compare("y") == 0 || input.compare("n") == 0)
            answered = true;
    }

    proceed = (input.compare("y") == 0);
    if (!proceed)
        std::cout << "Action canceled." << std::endl;
    return proceed;
}

/*
 * main
 */
int main( int argc, char *argv[] )
{
    if( argc <= 1 )
    {
        usage();
        return 0;
    }

    // launched from xbutil
    if(std::string(argv[0]).rfind("xbutil") != std::string::npos)
    {
        optind = 2;
    }
    else
    {
        std::cout <<"XBFLASH -- Xilinx Board Flash Utility" << std::endl;
    }

    if( argc <= optind )
        usageAndDie();

    // Non-flash use case with subcommands.
    std::string subcmd(argv[optind]);
    if(subcmd.compare("scan") == 0 )
    {
        if (argc != optind + 1)
            usageAndDie();

        sudoOrDie();
        return scanDevices();
    } else if (subcmd.compare("help") == 0)
    {
        if (argc != optind + 1)
            usageAndDie();

        usage();
        return 0;
    }

    // Flash use case without subcommands.

    sudoOrDie();

    bool seen_a = false;
    bool seen_d = false;
    bool seen_m = false;
    bool seen_n = false;
    bool seen_o = false;
    bool seen_p = false;
    bool seen_t = false;
    T_Arguments args;

    int opt;
    while( ( opt = getopt( argc, argv, "a:d:m:n:o:p:t:" ) ) != -1 )
    {
        switch( opt )
        {
        case 'a':
            notSeenOrDie(seen_a);
            args.dsa = optarg;
            break;
        case 'd':
            notSeenOrDie(seen_d);
            args.devIdx = atoi( optarg );
            break;
        case 'm':
            notSeenOrDie(seen_m);
            args.primary = std::make_shared<firmwareImage>(optarg, MCS_FIRMWARE_PRIMARY);
            if (args.primary->fail())
                exit(-EINVAL);
            break;
        case 'n': // optional
            notSeenOrDie(seen_n);
            args.secondary = std::make_shared<firmwareImage>(optarg, MCS_FIRMWARE_SECONDARY);
            if (args.secondary->fail())
                exit(-EINVAL);
            break;
        case 'o': // optional
            notSeenOrDie(seen_o);
            std::cout << "CAUTION: Overrideing flash programming mode is not recommended. "
                << "You may damage your device with this option." << std::endl;
            if(!canProceed())
                exit(-ECANCELED);

            if( std::string( optarg ).compare( "spi" ) == 0 ) {
                args.flasherType = Flasher::E_FlasherType::SPI;
            } else if( std::string( optarg ).compare( "bpi" ) == 0 ) {
                args.flasherType = Flasher::E_FlasherType::BPI;
            } else {
                std::cout << "Invalid programming mode '" << optarg << "', "
                    << "must be either 'spi' or 'bpi'." << std::endl;
                usageAndDie();
                break;
            }
            break;
        case 'p':
            notSeenOrDie(seen_p);
            args.bmc = std::make_shared<firmwareImage>(optarg, BMC_FIRMWARE);
            if (args.bmc->fail())
                exit(-EINVAL);
            break;
        case 't':
            notSeenOrDie(seen_t);
            args.timestamp = strtoull(optarg, nullptr, 16);
            break;
        default:
            usageAndDie();
            break;
        }
    }

    if(argc > optind || // More options than expected.
        // Combination of options not expected.
        (seen_p && (seen_m || seen_n || seen_o))    ||
        (seen_a && (seen_m || seen_n || seen_o))    ||
        (seen_t && (!seen_a || args.dsa.compare("all") == 0)))
    {
        usageAndDie();
    }

    int ret = 0;

    // Manually specify DSA/BMC files.
    if (args.dsa.empty())
    {
        if (args.devIdx == UINT_MAX)
            args.devIdx = 0;

        Flasher flasher(args.devIdx, args.flasherType);

        if(!flasher.isValid())
            ret = -EINVAL;
        else if (args.bmc != nullptr)
            ret = flasher.upgradeBMCFirmware(args.bmc);
        else
            ret = flasher.upgradeFirmware(args.primary, args.secondary);
    }
    // Automatically choose DSA/BMC files.
    else
    {
        bool foundDSA = false;
        bool multiDSA = false;
        std::vector<unsigned int> boardsToCheck;
        std::vector<unsigned int> boardsToUpdate;
        xcldev::pci_device_scanner scanner;
        scanner.scan_without_driver();

        // Collect all indexes of boards need checking
        if (args.devIdx == UINT_MAX)
        {
            for(unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++)
                boardsToCheck.push_back(i);
        }
        else
        {
            if (args.devIdx < xcldev::pci_device_scanner::device_list.size())
                boardsToCheck.push_back(args.devIdx);
        }
        if (boardsToCheck.empty())
        {
            std::cout << "Board(s) not found!" << std::endl;
            exit(-ENOENT);
        }

        // Collect all indexes of boards need updating
        for (unsigned int i : boardsToCheck)
        {
            ret = updateDSA(i, args.dsa, args.timestamp, true);
            switch (ret)
            {
            case 0:
                foundDSA = true;
                boardsToUpdate.push_back(i);
                break;
            case -ENOTUNIQ:
                foundDSA = true;
                multiDSA = true;
                break;
            case -EALREADY:
                foundDSA = true;
                break;
            default:
                break;
            }
        }

        // Check and quit on fatal errors.
        if (!foundDSA)
        {
            std::cout << "DSA not found: " << args.dsa << std::endl;
            exit (-ENOENT);
        }
        if (multiDSA)
        {
            std::cout << "Multiple applicable installed DSA found." << std::endl;
            std::cout << "Run xbutil flash scan to obtain detailed info." << std::endl;
            std::cout << "Use xbutil flash -a <dsa-name> [-t <timestamp>] "
                << "to pick a specific one to flash" << std::endl;
            exit (-ENOTUNIQ);
        }

        // Continue to flash whatever we have collected in boardsToUpdate.

        // Update user about what boards will be updated and get permission.
        if (boardsToUpdate.empty())
        {
            std::cout << "No board needs updating." << std::endl;
            exit (0);
        }
        else
        {
            std::cout << "DSA on below boards will be updated:" << std::endl;
            for (unsigned int i : boardsToUpdate)
            {
                std::cout << "Board [" << boardsToUpdate[i] << "]" << std::endl;
            }
            if(!canProceed())
            {
                std::cout << "Firmware updating is canceled." << std::endl;
                exit(-ECANCELED);
            }
        }

        // Perform DSA and BMC updating
        for (unsigned int i : boardsToUpdate)
        {
            ret = updateDSA(i, args.dsa, args.timestamp, false);
            if (ret)
                break;
        }
    }

    if (ret == 0)
    {
        if (xcldev::pci_device_scanner::device_list.size() == 0)
        {
            std::cout << "No devices found" << std::endl;
        }
        else if (args.bmc != nullptr)
        {
            std::cout << "BMC firmware updated successfully" << std::endl;
        }
        else
        {
            std::cout << "Board(s) flashed succesfully. "
                    << "Please cold reboot machine to load the new flash image on the FPGA."
                    << std::endl;
        }
    }
    else
    {
            std::cout << "Failed to flash board(s)." << std::endl;
    }
    return ret;
}

/*
 * scanDevices
 *
 * Uses pci_device_scanner to enumerate SDx devices and populates device_list.
 */
int scanDevices( void )
{
    xcldev::pci_device_scanner scanner;
    scanner.scan_without_driver();

    if ( xcldev::pci_device_scanner::device_list.size() == 0 )
        std::cout << "No device is found!" << std::endl;
    else
        std::cout << "Found the following devices:" << std::endl;

    for( unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++ )
    {
        std::cout << "Board [" << i << "]" << std::endl;

        Flasher f( i );
        if (!f.isValid())
            continue;

        DSAInfo board = f.getOnBoardDSA();
        std::cout << "\tDevice type:\t" << board.board << std::endl;
        std::cout << "\tDevice BDF:\t" << f.sGetDBDF() << std::endl;
        std::cout << "\tFlash type:\t" << f.sGetFlashType() << std::endl;
        std::cout << "\tDSA on board:\t" << board << std::endl;

        std::vector<DSAInfo> installedDSA = f.getInstalledDSA();
        std::cout << "\tDSA installed:\t";
        if (!installedDSA.empty())
        {
            std::cout << installedDSA[0] << std::endl;
            for (size_t d = 1; d < installedDSA.size(); d++)
            {
                std::cout << "\t\t\t" << installedDSA[d] << std::endl;
            }
        }
        else
        {
            std::cout << "(None)" << std::endl;
        }
    }

    return 0;
}
