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
#include <iomanip>
#include "flasher.h"
#include "scan.h"
#include "firmware_image.h"

const char* UsageMessages[] = {
    "[-d device] -m primary_mcs [-n secondary_mcs] [-o spi|bpi]'",
    "[-d device] -a <all | dsa> [-t timestamp]",
    "[-d device] -p msp432_firmware",
    "scan [-v]",
};
const char* SudoMessage = "ERROR: root privileges required.";
int scanDevices(int argc, char *argv[]);

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
    std::string flasherType;
    std::string dsa;
    uint64_t timestamp = 0;
    bool force = false;
};

int flashDSA(Flasher& f, DSAInfo& dsa)
{
    std::shared_ptr<firmwareImage> primary;
    std::shared_ptr<firmwareImage> secondary;

    if (dsa.file.rfind(DSABIN_FILE_SUFFIX) != std::string::npos)
    {
        primary = std::make_shared<firmwareImage>(dsa.file.c_str(), MCS_FIRMWARE_PRIMARY);
        secondary = std::make_shared<firmwareImage>(dsa.file.c_str(), MCS_FIRMWARE_SECONDARY);
        if (secondary->fail())
            secondary = nullptr;
    }
    else
    {
        primary = std::make_shared<firmwareImage>(dsa.file.c_str(), MCS_FIRMWARE_PRIMARY);
        size_t pos = dsa.file.rfind("primary");
        if (pos != std::string::npos)
        {
            std::string sec = dsa.file.substr(0, pos);
            sec += "secondary" "." DSA_FILE_SUFFIX;
            secondary = std::make_shared<firmwareImage>(sec.c_str(), MCS_FIRMWARE_SECONDARY);
        }
    }

    if (primary->fail() || (secondary != nullptr && secondary->fail()))
        return -EINVAL;

    return f.upgradeFirmware(primary.get(), secondary.get());
}

int updateDSA(unsigned idx, std::string& dsa, uint64_t ts, bool dryrun)
{
    Flasher flasher(idx);
    if(!flasher.isValid())
        return -EINVAL;

    std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();
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
            if (ts != NULL_TIMESTAMP && ts != idsa.timestamp)
                continue;
            if (candidateDSAIndex != UINT_MAX)
                return -ENOTUNIQ;
            candidateDSAIndex = i;
        }
    }

    if (candidateDSAIndex == UINT_MAX)
        return -ENOENT;
    DSAInfo& candidate = installedDSA[candidateDSAIndex];

    DSAInfo currentDSA = flasher.getOnBoardDSA();
    if (!currentDSA.name.empty())
    {
        if (candidate.name == currentDSA.name &&
            candidate.timestamp == currentDSA.timestamp)
            return -EALREADY;
    }

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
        sudoOrDie();
        return scanDevices(argc, argv);
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
    bool seen_f = false;
    bool seen_m = false;
    bool seen_n = false;
    bool seen_o = false;
    bool seen_p = false;
    bool seen_t = false;
    T_Arguments args;

    int opt;
    while( ( opt = getopt( argc, argv, "a:d:fm:n:o:p:t:" ) ) != -1 )
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
        case 'f':
            notSeenOrDie(seen_f);
            args.force = true;
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
            args.flasherType = std::string(optarg);
            break;
        case 'p':
            notSeenOrDie(seen_p);
            args.bmc = std::make_shared<firmwareImage>(optarg, BMC_FIRMWARE);
            if (args.bmc->fail())
                exit(-EINVAL);
            break;
        case 't':
            notSeenOrDie(seen_t);
            args.timestamp = strtoull(optarg, nullptr, 0);
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
        // By default, only flash the first board.
        if (args.devIdx == UINT_MAX)
            args.devIdx = 0;

        Flasher flasher(args.devIdx, args.flasherType);

        if(!flasher.isValid())
            ret = -EINVAL;
        else if (args.bmc != nullptr)
            ret = flasher.upgradeBMCFirmware(args.bmc.get());
        else
            ret = flasher.upgradeFirmware(args.primary.get(), args.secondary.get());
    }
    // Automatically choose DSA/BMC files.
    else
    {
        bool foundDSA = false;
        bool multiDSA = false;
        std::vector<unsigned int> boardsToCheck;
        std::vector<unsigned int> boardsToUpdate;

        // Sanity check input dsa and timestamp.
        if (args.dsa.compare("all") != 0)
        {
            auto installedDSAs = firmwareImage::getIntalledDSAs();
            for (DSAInfo& dsa : installedDSAs)
            {
                if (args.dsa == dsa.name &&
                    (args.timestamp == NULL_TIMESTAMP || args.timestamp == dsa.timestamp))
                {
                    if (!foundDSA)
                        foundDSA = true;
                    else
                        multiDSA = true;
                }
            }
        }
        else
        {
            foundDSA = true;
        }

        // Collect all indexes of boards need checking
        if (foundDSA && !multiDSA)
        {
            xcldev::pci_device_scanner scanner;
            scanner.scan_without_driver();
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
                std::cout << "Board not found!" << std::endl;
                exit(-ENOENT);
            }
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
            if (boardsToCheck.empty())
            {
                std::cout << "Can't find DSA: " << args.dsa;
                if (args.timestamp != NULL_TIMESTAMP)
                {
                    std::cout << ", SN=0x" << std::hex <<
                        std::setw(16) << std::setfill('0') << args.timestamp;
                }
                std::cout << std::endl;
            }
            else
            {
                std::cout << "Can't find board matching DSA: " << args.dsa << std::endl;
            }
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
            std::cout << "No DSA on board needs updating." << std::endl;
            exit (0);
        }
        else
        {
            std::cout << "DSA on below boards will be updated:" << std::endl;
            for (unsigned int i : boardsToUpdate)
            {
                std::cout << "Board [" << i << "]" << std::endl;
            }
            if(!args.force && !canProceed())
            {
                exit(-ECANCELED);
            }
        }

        // Perform DSA and BMC updating
        for (unsigned int i : boardsToUpdate)
        {
            std::cout << "Flashing DSA on board [" << i << "]" << std::endl;
            ret = updateDSA(i, args.dsa, args.timestamp, false);
            if (ret)
                break;
        }
    }

    if (ret == 0)
    {
        if (args.bmc != nullptr)
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
int scanDevices(int argc, char *argv[])
{
    bool verbose = false;
    int opt;

    while((opt = getopt(argc, argv, "v")) != -1)
    {
        switch(opt)
        {
        case 'v':
            verbose = true;
            break;
        default:
            usageAndDie();
            break;
        }
    }

    xcldev::pci_device_scanner scanner;
    scanner.scan_without_driver();

    if ( xcldev::pci_device_scanner::device_list.size() == 0 )
        std::cout << "No device is found!" << std::endl;

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
        if (verbose)
            std::cout << "\tDSA on board:\t" << board << std::endl;
        else
            std::cout << "\tDSA on board:\t" << board.name << std::endl;

        std::vector<DSAInfo> installedDSA = f.getInstalledDSA();
        std::cout << "\tDSA installed:\t";
        if (!installedDSA.empty())
        {
            for (DSAInfo& d : installedDSA)
            {
                if (verbose)
                    std::cout << d << std::endl;
                else
                    std::cout << d.name << std::endl;
                std::cout << "\t\t\t";
            }
        }
        else
        {
            std::cout << "(None)" << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}
