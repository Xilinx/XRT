/**
 * Copyright (C) 2018 Xilinx, Inc
 * Author: Ryan Radjabi, Max Zhen
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
    for (unsigned i = 0; i < (sizeof(UsageMessages) / sizeof(UsageMessages[0]));
        i++)
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

// Flashing DSA on the board.
int flashDSA(Flasher& f, DSAInfo& dsa)
{
    std::shared_ptr<firmwareImage> primary;
    std::shared_ptr<firmwareImage> secondary;

    if (dsa.file.rfind(DSABIN_FILE_SUFFIX) != std::string::npos)
    {
        primary = std::make_shared<firmwareImage>(dsa.file.c_str(),
            MCS_FIRMWARE_PRIMARY);
        if (primary->fail())
            primary = nullptr;
        secondary = std::make_shared<firmwareImage>(dsa.file.c_str(),
            MCS_FIRMWARE_SECONDARY);
        if (secondary->fail())
            secondary = nullptr;
    }
    else
    {
        primary = std::make_shared<firmwareImage>(dsa.file.c_str(),
            MCS_FIRMWARE_PRIMARY);
        if (primary->fail())
            primary = nullptr;
        size_t pos = dsa.file.rfind("primary");
        if (pos != std::string::npos)
        {
            std::string sec = dsa.file.substr(0, pos);
            sec += "secondary" "." DSA_FILE_SUFFIX;
            secondary = std::make_shared<firmwareImage>(sec.c_str(),
                MCS_FIRMWARE_SECONDARY);
            if (secondary->fail())
                secondary = nullptr;
        }
    }

    if (primary == nullptr)
        return -EINVAL;

    return f.upgradeFirmware("", primary.get(), secondary.get());
}

// Flashing BMC on the board.
int flashBMC(Flasher& f, DSAInfo& dsa)
{
    std::shared_ptr<firmwareImage> bmc;

    if (dsa.file.rfind(DSABIN_FILE_SUFFIX) != std::string::npos)
    {
        bmc = std::make_shared<firmwareImage>(dsa.file.c_str(), BMC_FIRMWARE);
        if (bmc->fail())
            bmc = nullptr;
    }

    if (bmc == nullptr)
        return -EINVAL;

    return f.upgradeBMCFirmware(bmc.get());
}

unsigned selectDSA(unsigned idx, std::string& dsa, uint64_t ts)
{
    unsigned candidateDSAIndex = UINT_MAX;

    std::cout << "Probing board[" << idx << "]: ";

    Flasher flasher(idx);
    if(!flasher.isValid())
    {
        return candidateDSAIndex;
    }

    std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();

    // Find candidate DSA from installed DSA list.
    if (dsa.compare("all") == 0)
    {
        if (installedDSA.empty())
        {
            std::cout << "no DSA installed" << std::endl;
            return candidateDSAIndex;
        }
        else if (installedDSA.size() > 1)
        {
            std::cout << "multiple DSA installed" << std::endl;
            return candidateDSAIndex;
        }
        else
        {
            candidateDSAIndex = 0;
        }
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
            {
                std::cout << "multiple DSA installed" << std::endl;
                return candidateDSAIndex;
            }
            candidateDSAIndex = i;
        }
    }

    if (candidateDSAIndex == UINT_MAX)
    {
        std::cout << "specified DSA not applicable" << std::endl;
        return candidateDSAIndex;
    }

    DSAInfo& candidate = installedDSA[candidateDSAIndex];

    bool same_dsa = false;
    bool same_bmc = false;
    DSAInfo currentDSA = flasher.getOnBoardDSA();
    if (!currentDSA.name.empty())
    {
        same_dsa = (candidate.name == currentDSA.name &&
            candidate.timestamp == currentDSA.timestamp);
        same_bmc = (currentDSA.bmcVer.empty() ||
            candidate.bmcVer == currentDSA.bmcVer);
    }
    if (same_dsa && same_bmc)
    {
        std::cout << "DSA on FPGA is up-to-date" << std::endl;
        return UINT_MAX;
    }

    std::cout << "DSA on FPGA needs updating" << std::endl;
    return candidateDSAIndex;
}

int updateDSA(unsigned boardIdx, unsigned dsaIdx, bool& reboot)
{
    reboot = false;

    Flasher flasher(boardIdx);
    if(!flasher.isValid())
    {
        std::cout << "device not available" << std::endl;
        return -EINVAL;
    }

    std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();
    DSAInfo& candidate = installedDSA[dsaIdx];

    bool same_dsa = false;
    bool same_bmc = false;
    DSAInfo current = flasher.getOnBoardDSA();
    if (!current.name.empty())
    {
        same_dsa = (candidate.name == current.name &&
            candidate.timestamp == current.timestamp);
        same_bmc = (current.bmcVer.empty() ||
            candidate.bmcVer == current.bmcVer);
    }
    if (same_dsa && same_bmc)
    {
        std::cout << "update not needed" << std::endl;
        return -EINVAL;
    }

    if (!same_bmc)
    {
        std::cout << "Updating BMC firmware on board[" << boardIdx << "]"
            << std::endl;
        int ret = flashBMC(flasher, candidate);
        if (ret != 0)
        {
            std::cout << "Failed to update BMC firmware on board["
                << boardIdx << "]" << std::endl;
            return ret;
        }
    }

    if (!same_dsa)
    {
        std::cout << "Updating DSA on board[" << boardIdx << "]" << std::endl;
        int ret = flashDSA(flasher, candidate);
        if (ret != 0)
        {
            std::cout << "Failed to update DSA on board[" << boardIdx << "]"
                << std::endl;
            return ret;
        }
        reboot = true;
    }

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
        optind++;
        return scanDevices(argc, argv);
    }
    if (subcmd.compare("help") == 0)
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
            args.primary = std::make_shared<firmwareImage>(optarg,
                MCS_FIRMWARE_PRIMARY);
            if (args.primary->fail())
                exit(-EINVAL);
            break;
        case 'n': // optional
            notSeenOrDie(seen_n);
            args.secondary = std::make_shared<firmwareImage>(optarg,
                MCS_FIRMWARE_SECONDARY);
            if (args.secondary->fail())
                exit(-EINVAL);
            break;
        case 'o': // optional
            notSeenOrDie(seen_o);
            std::cout <<"CAUTION: Overriding flash mode is not recommended. "
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

        Flasher flasher(args.devIdx);

        if(!flasher.isValid())
        {
            ret = -EINVAL;
        }
        else if (args.bmc != nullptr)
        {
            ret = flasher.upgradeBMCFirmware(args.bmc.get());
            if (ret == 0)
                std::cout << "BMC firmware flashed successfully" << std::endl;
        }
        else
        {
            ret = flasher.upgradeFirmware(args.flasherType,
                args.primary.get(), args.secondary.get());
            if (ret == 0)
            {
                std::cout << "DSA image flashed succesfully" << std::endl;
                std::cout << "Cold reboot machine to load the new image on FPGA"
                    << std::endl;
            }
        }

        if (ret != 0)
            std::cout << "Failed to flash board." << std::endl;
        return ret;
    }

    // Automatically choose DSA/BMC files.
    std::vector<unsigned int> boardsToCheck;
    std::vector<std::pair<unsigned, unsigned>> boardsToUpdate;

    // Sanity check input dsa and timestamp.
    if (args.dsa.compare("all") != 0)
    {
        bool foundDSA = false;
        bool multiDSA = false;
        auto installedDSAs = firmwareImage::getIntalledDSAs();
        for (DSAInfo& dsa : installedDSAs)
        {
            if (args.dsa == dsa.name &&
                (args.timestamp == NULL_TIMESTAMP ||
                args.timestamp == dsa.timestamp))
            {
                if (!foundDSA)
                    foundDSA = true;
                else
                    multiDSA = true;
            }
        }
        if (!foundDSA)
        {
            std::cout << "Specified DSA not installed." << std::endl;
            exit(-ENOENT);
        }
        if (multiDSA)
        {
            std::cout << "Specified DSA matched more than one installed DSA"
                << std::endl;
            exit (-ENOTUNIQ);
        }
    }

    // Collect all indexes of boards need checking
    xcldev::pci_device_scanner scanner;
    scanner.scan_without_driver();
    if (args.devIdx == UINT_MAX)
    {
        for(unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size();
            i++)
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

    // Collect all indexes of boards need updating
    for (unsigned int i : boardsToCheck)
    {
        unsigned dsaidx = selectDSA(i, args.dsa, args.timestamp);
        if (dsaidx != UINT_MAX)
            boardsToUpdate.push_back(std::make_pair(i, dsaidx));
    }

    // Continue to flash whatever we have collected in boardsToUpdate.
    unsigned success = 0;
    bool needreboot = false;
    if (!boardsToUpdate.empty())
    {
        std::cout << "DSA on below boards will be updated:" << std::endl;
        for (auto p : boardsToUpdate)
        {
            std::cout << "Board [" << p.first << "]" << std::endl;
        }

        // Prompt user about what boards will be updated and ask for permission.
        if(!args.force && !canProceed())
        {
            exit(-ECANCELED);
        }

        // Perform DSA and BMC updating
        for (auto p : boardsToUpdate)
        {
            bool reboot;
            ret = updateDSA(p.first, p.second, reboot);
            needreboot |= reboot;
            if (ret == 0)
                success++;
        }
    }

    std::cout << success << " boards flashed successfully." << std::endl;
    if (needreboot)
    {
        std::cout << "Cold reboot machine to load the new image on FPGA."
            << std::endl;
    }

    if (success != boardsToUpdate.size())
        exit(-EINVAL);

    return 0;
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
    if (argc != optind)
        usageAndDie();

    xcldev::pci_device_scanner scanner;
    scanner.scan_without_driver();

    if ( xcldev::pci_device_scanner::device_list.size() == 0 )
        std::cout << "No device is found!" << std::endl;

    for(unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++)
    {
        std::cout << "Board [" << i << "]" << std::endl;

        Flasher f(i);
        if (!f.isValid())
            continue;

        DSAInfo board = f.getOnBoardDSA();
        std::cout << "\tDevice BDF:\t\t" << f.sGetDBDF() << std::endl;
        std::cout << "\tDevice type:\t\t" << board.board << std::endl;
        std::cout << "\tFlash type:\t\t" << f.sGetFlashType() << std::endl;
        std::cout << "\tDSA running on FPGA:" << std::endl;
        std::cout << "\t\t" << board << std::endl;

        std::vector<DSAInfo> installedDSA = f.getInstalledDSA();
        std::cout << "\tDSA package installed in system:\t";
        if (!installedDSA.empty())
        {
            for (auto& d : installedDSA)
            {
                std::cout << std::endl << "\t\t" << d;
            }
        }
        else
        {
            std::cout << "(None)";
        }
        std::cout << std::endl;

        BoardInfo info;
        if (verbose && f.getBoardInfo(info) == 0)
        {
            std::cout << "\tBoard name\t\t" << info.mName << std::endl;
            std::cout << "\tBoard rev\t\t" << info.mRev << std::endl;
            std::cout << "\tBoard S/N: \t\t" << info.mSerialNum << std::endl;
            std::cout << "\tConfig mode: \t\t" << info.mConfigMode << std::endl;
            std::cout << "\tFan presence:\t\t" << info.mFanPresence << std::endl;
            std::cout << "\tMax power level:\t" << info.mMaxPowerLvl << std::endl;
            std::cout << "\tMAC address0:\t\t" << info.mMacAddr0 << std::endl;
            std::cout << "\tMAC address1:\t\t" << info.mMacAddr1 << std::endl;
            std::cout << "\tMAC address2:\t\t" << info.mMacAddr2 << std::endl;
            std::cout << "\tMAC address3:\t\t" << info.mMacAddr3 << std::endl;
        }

        std::cout << std::endl;
    }

    return 0;
}
