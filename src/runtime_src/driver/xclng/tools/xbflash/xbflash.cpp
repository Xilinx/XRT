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
#include "flasher.h"
#include "scan.h"

const char* UsageMessages[] = {
    "xbflash [-d device] -m primary_mcs [-n secondary_mcs] [-o spi|bpi]'",
    "xbflash [-d device] -p msp432_firmware",
    "xbflash [-d device] -a dsa",
};
const char* SudoMessage = "ERROR: root privileges required.";
int scanDevices( void );
int startIdx = 0;

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
    unsigned devIdx = UINT_MAX;
    char *file1 = nullptr;
    char *file2 = nullptr;
    char *bmcfile = nullptr;
    Flasher::E_FlasherType flasherType = Flasher::E_FlasherType::UNSET;
    bool isValid = true;
    std::string dsa;
};

void addDsaFile(std::vector<std::string>& dsafiles, std::string dsafile)
{
    // Make sure file exists before add to the list.
    std::ifstream f(dsafile.c_str());
    if (f.good())
        dsafiles.push_back(dsafile);
}

std::vector<std::string> dsa2files(std::string& dsa)
{
    std::vector<std::string> files;
    std::string file;

    file += FIRMWARE_DIR "/";
    file += dsa;

    // kcu1500 has two dsa files in below order.
    size_t pos = dsa.find("kcu1500");
    if (pos != std::string::npos)
    {
        addDsaFile(files, file + "_primary." DSA_FILE_SUFFIX);
        addDsaFile(files, file + "_secondary." DSA_FILE_SUFFIX);
    }
    else
    {
        addDsaFile(files, file + "." DSA_FILE_SUFFIX);
    }

    return files;
}

int flashDSA(Flasher& f, std::string& dsa)
{
    std::vector<std::string> files = dsa2files(dsa);

    if (files.empty())
        return -ENOENT;
    if (files.size() == 1)
        return f.upgradeFirmware(files[0].c_str(), nullptr);
    if (files.size() == 2)
        return f.upgradeFirmware(files[0].c_str(), files[1].c_str());
    return -EINVAL;
}

int updateDSAOnBoards(unsigned idx, std::string& dsa, bool dryrun, bool *update)
{
    Flasher flasher(idx);
    if(!flasher.isValid())
    {
        return -EINVAL;
    }

    std::vector<std::string> installedDSA = flasher.sGetInstalledDSA();
    std::string currentDSA = flasher.sGetDSAName();

    // Bring DSA in-sync w/ installed ones
    if (dsa.compare("all") == 0)
    {
        if (installedDSA.size() > 1)
        {
            std::cout << "Multiple types of DSA installed for board." << std::endl;
            std::cout << "Run xbutil flash list to obtain detailed information." << std::endl;
            std::cout << "Run xbutil flash -a <dsa name> to pick one to flash." << std::endl;
            return -EINVAL;
        }
        *update = (installedDSA[0].compare(currentDSA) != 0);

        if (update && !dryrun)
        {
            return flashDSA(flasher, installedDSA[0]);
        }
    }
    // Flash specified DSA
    else
    {
        *update = false;
        if (dsa.compare(currentDSA) != 0)
        {
            for (unsigned d = 0; d < installedDSA.size(); d++)
            {
                if (dsa.compare(installedDSA[d]) == 0)
                {
                    *update = true;
                    break;
                }
            }

            if (update && !dryrun)
            {
                return flashDSA(flasher, dsa);
            }
        }
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
    if( std::string( argv[ (startIdx+1) ] ).compare( "flash" ) == 0 )
    {
        startIdx++;
    }
    else
    {
        std::cout <<"XBFLASH -- Xilinx Board Flash Utility" << std::endl;
    }

    if( argc <= (startIdx+2) ) // not a valid flash program scenario
    {
        if( argc <= (startIdx+1) ) {
            usageAndDie();
        }

        // argc > (startIdx+1)
        if( std::string( argv[ (startIdx+1) ] ).compare( "list" ) == 0 ) {
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

    bool seen_a = false;
    bool seen_d = false;
    bool seen_m = false;
    bool seen_n = false;
    bool seen_o = false;
    bool seen_p = false;
    T_Arguments args;

    // argc > (startIdx+2)
    int opt;
    while( ( opt = getopt( argc, argv, "a:d:m:n:o:p:" ) ) != -1 ) // adjust offset?
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
            std::cout << "    device index: " << args.devIdx << std::endl;
            break;
        case 'm':
            notSeenOrDie(seen_m);
            args.file1 = optarg;
            std::cout << "    primary mcs: " << args.file1 << std::endl;
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
            if(!canProceed())
                return -ECANCELED;

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
          }
        case 'p':
            notSeenOrDie(seen_p);
            args.bmcfile = optarg;
            std::cout << "    MSP432 firmware image: " << args.bmcfile << std::endl;
            break;
        default:
            args.isValid = false;
            break;
        }
    }

    if( !args.isValid ||
        (seen_p && (seen_m || seen_n || seen_o)) ||
        (seen_a && (seen_m || seen_n || seen_o)))
    {
        usageAndDie();
    }

    int ret = 0;

    if (args.dsa.empty())
    {
        if (args.devIdx == UINT_MAX)
            args.devIdx = 0;

        Flasher flasher(args.devIdx, args.flasherType);

        if(!flasher.isValid())
            ret = -EINVAL;
        else if (args.bmcfile)
            ret = flasher.upgradeBMCFirmware(args.bmcfile);
        else
            ret = flasher.upgradeFirmware(args.file1, args.file2);
    }
    else
    {
        bool needed;
        std::vector<unsigned int> boards;

        // Make sure input DSA is valid
        if (args.dsa.compare("all") != 0)
        {
            std::vector<std::string> f = dsa2files(args.dsa);
            if (f.empty())
            {
                std::cout << "DSA " << args.dsa << " is not found" << std::endl;
                return -ENOENT;
            }
        }

        // Collecting all indexes of boards need updating
        if (args.devIdx == UINT_MAX)
        {
            xcldev::pci_device_scanner scanner;
            scanner.scan_without_driver();

            // Find out which boards to flash
            for(unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++)
            {
                ret = updateDSAOnBoards(i, args.dsa, true, &needed);
                if (ret != 0)
                    break;

                if (needed)
                {
                    boards.push_back(i);
                }
            }
        }
        else
        {
            ret = updateDSAOnBoards(args.devIdx, args.dsa, true, &needed);
            if (ret == 0 && needed)
            {
                boards.push_back(args.devIdx);
            }
        }

        // Updating DSA & MSP432 firmware
        if (ret == 0)
        {
            if (boards.empty())
            {
                std::cout << "No board needs updating." << std::endl;
                return 0;
            }
            else
            {
                std::cout << "DSA on below boards will be updated:" << std::endl;
                for (unsigned i = 0; i < boards.size(); i++)
                {
                    std::cout << "Board [" << boards[i] << "]" << std::endl;
                }
                if(canProceed())
                {
                    for (unsigned i = 0; i < boards.size(); i++)
                    {
                        ret = updateDSAOnBoards(i, args.dsa, false, &needed);
                    }
                }
                else
                {
                    ret = -ECANCELED;
                }
            }
        }
    }

    if (ret == 0)
    {
        if (xcldev::pci_device_scanner::device_list.size() == 0)
        {
            std::cout << "No devices found" << std::endl;
        }
        else if (args.bmcfile)
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
 * TODO: Add error checking and return nonzero in the event of failure.
 * Returns 0 on success.
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
        Flasher f( i );
        if (!f.isValid())
            continue;

        std::vector<std::string> installedDSA = f.sGetInstalledDSA();
        std::cout << "Board [" << i << "]" << std::endl;
        std::cout << "\tDevice BDF: " << f.sGetDBDF() << std::endl;
        std::cout << "\tFlash type: " << f.sGetFlashType() << std::endl;
        std::cout << "\tDSA on board:  " << f.sGetDSAName() << std::endl;
        std::cout << "\tDSA installed: ";
        if (!installedDSA.empty())
        {
            std::cout << installedDSA[0] << std::endl;
            for (size_t d = 1; d < installedDSA.size(); d++)
            {
                std::cout << "\t               " << installedDSA[d] << std::endl;
            }
        }
        else
        {
            std::cout << "(None)" << std::endl;
        }
    }

    return 0;
}
