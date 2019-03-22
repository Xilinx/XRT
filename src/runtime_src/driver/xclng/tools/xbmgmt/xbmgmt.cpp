/**
 * Copyright (C) 2019 Xilinx, Inc
 * Author: Ryan Radjabi, Max Zhen, Chien-Wei Lan
 * A command line utility to manage PCIe devices includes program, flash, reset and clock.
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
#include <unistd.h>
#include <getopt.h>
#include "flasher.h"
#include "scan.h"
#include "xbmgmt.h"
#include "firmware_image.h"

int bdf2index(std::string& bdfStr, unsigned& index)
{
    // Extract bdf from bdfStr.
    int dom = 0, b, d, f;
    char dummy;
    std::stringstream s(bdfStr);
    size_t n = std::count(bdfStr.begin(), bdfStr.end(), ':');
    if (n == 1)
        s >> std::hex >> b >> dummy >> d >> dummy >> f;
    else if (n == 2)
        s >> std::hex >> dom >> dummy >> b >> dummy >> d >> dummy >> f;
    if ((n != 1 && n != 2) || s.fail()) {
        std::cout << "ERROR: failed to extract BDF from " << bdfStr << std::endl;
        return -EINVAL;
    }

    for (unsigned i = 0; i < pcidev::get_dev_total(); i++) {
        auto dev = pcidev::get_dev(i);
        if(dev->mgmt){
            if (dom == dev->mgmt->domain && b == dev->mgmt->bus &&
                d == dev->mgmt->dev && (f == 0 || f == 1)) {
                index = i;
                return 0;
            }
        }
    }

    std::cout << "ERROR: No card found for " << bdfStr << std::endl;
    return -ENOENT;
}

int str2index(const char *arg, unsigned& index)
{
    std::string devStr(arg);

    if (devStr.find(":") == std::string::npos) {
    // The arg contains a board index.
        unsigned long i;
        char *endptr;
        i = std::strtoul(arg, &endptr, 0);
        if (*endptr != '\0' || i >= UINT_MAX) {
            std::cout << "ERROR: " << devStr << " is not a valid card index."
                << std::endl;
            return -EINVAL;
        }
        index = i;
    } else {
    // The arg contains domain:bus:device.function string.
        int ret = bdf2index(devStr, index);
        if (ret != 0)
            return ret;
    }

    return 0;
}

const char* UsageMessages[] = {
    "[-d card] -m primary_mcs [-n secondary_mcs] [-o spi|bpi]'",
    "[-d card] -a <all | dsa> [-t timestamp]",
    "[-d card] -p msp432_firmware",
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

    std::cout << "Probing card[" << idx << "]: ";

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
            std::cout << "no shell installed" << std::endl;
            return candidateDSAIndex;
        }
        else if (installedDSA.size() > 1)
        {
            std::cout << "multiple shell installed" << std::endl;
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
                std::cout << "multiple shell installed" << std::endl;
                return candidateDSAIndex;
            }
            candidateDSAIndex = i;
        }
    }

    if (candidateDSAIndex == UINT_MAX)
    {
        std::cout << "specified shell not applicable" << std::endl;
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
        std::cout << "Shell on FPGA is up-to-date" << std::endl;
        return UINT_MAX;
    }
    std::cout << "Shell on FPGA needs updating" << std::endl;
    return candidateDSAIndex;
}

int updateDSA(unsigned boardIdx, unsigned dsaIdx, bool& reboot)
{
    reboot = false;

    Flasher flasher(boardIdx);
    if(!flasher.isValid())
    {
        std::cout << "card not available" << std::endl;
        return -EINVAL;
    }

    std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();
    DSAInfo& candidate = installedDSA[dsaIdx];

    bool same_dsa = false;
    bool same_bmc = false;
    bool updated_dsa = false;
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
    }

    if (!same_bmc)
    {
        std::cout << "Updating SC firmware on card[" << boardIdx << "]"
            << std::endl;
        int ret = flashBMC(flasher, candidate);
        if (ret != 0)
        {
            std::cout << "WARNING: Failed to update SC firmware on card["
                << boardIdx << "]" << std::endl;
        }
    }

    if (!same_dsa)
    {
        std::cout << "Updating shell on card[" << boardIdx << "]" << std::endl;
        int ret = flashDSA(flasher, candidate);
        if (ret != 0)
        {
            std::cout << "ERROR: Failed to update shell on card["
                << boardIdx << "]" << std::endl;
        } else {
            updated_dsa = true;
        }
    }

    reboot = updated_dsa;

    if (!same_dsa && !updated_dsa)
	    return -EINVAL;

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

void print_pci_info(void)
{
    auto print = [](const std::unique_ptr<pcidev::pci_func>& dev) {
        std::cout << std::hex;
        std::cout << ":[" << std::setw(2) << std::setfill('0') << dev->bus
            << ":" << std::setw(2) << std::setfill('0') << dev->dev
            << "." << dev->func << "]";

        std::cout << std::hex;
        std::cout << ":0x" << std::setw(4) << std::setfill('0') << dev->device_id;
        std::cout << ":0x" << std::setw(4) << std::setfill('0') << dev->subsystem_id;

        std::cout << std::dec;
        std::cout << ":[";
        if(!dev->driver_name.empty()) {
            std::cout << dev->driver_name << ":" << dev->driver_version << ":";
            if(dev->instance == INVALID_ID) {
                std::cout << "???";
            } else {
                std::cout << dev->instance;
            }
        }
        std::cout << "]" << std::endl;;
    };

    if (pcidev::get_dev_total() == 0) {
        std::cout << "No card found!" << std::endl;
        return;
    }

    int i = 0;
    int not_ready = 0;
    for (unsigned j = 0; j < pcidev::get_dev_total(); j++) {
        auto dev = pcidev::get_dev(j);
        auto& mdev = dev->mgmt;
        bool ready = dev->is_ready;

        if (mdev != nullptr) {
            std::cout << "[" << i << "]" << "mgmt";
            print(mdev);
        }

        if (!ready)
            not_ready++;
        ++i;
    }
    if (not_ready != 0) {
        std::cout << "WARNING: " << not_ready
                  << " card(s) marked by '*' are not ready, "
                  << "run xbmgmt flash scan -v to further check the details."
                  << std::endl;
    }
}



int xcldev::flash_helper(int argc, char *argv[])
{
    // launched from xbutil
    if(std::string(argv[0]).rfind("xbutil") != std::string::npos)
    {
        optind = 1;
    }
    else
    {
        std::cout <<"XBFLASH -- Xilinx Card Flash Utility" << std::endl;
    }

    if( argc <= optind )
        usageAndDie();

    // Non-flash use case with subcommands.
    std::string subcmd(argv[optind]);
    if(subcmd.compare("scan") == 0 )
    {
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
                << "You may damage your card with this option." << std::endl;
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
                std::cout << "SC firmware flashed successfully" << std::endl;
        }
        else
        {
            ret = flasher.upgradeFirmware(args.flasherType,
                args.primary.get(), args.secondary.get());
            if (ret == 0)
            {
                std::cout << "Shell image flashed succesfully" << std::endl;
                std::cout << "Cold reboot machine to load the new image on FPGA"
                    << std::endl;
            }
        }

        if (ret != 0)
            std::cout << "Failed to flash card." << std::endl;
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
            std::cout << "Specified shell not installed." << std::endl;
            exit(-ENOENT);
        }
        if (multiDSA)
        {
            std::cout << "Specified shell matched more than one installed shell"
                << std::endl;
            exit (-ENOTUNIQ);
        }
    }

    // Collect all indexes of boards need checking
    unsigned total = pcidev::get_dev_total();
    if (args.devIdx == UINT_MAX)
    {
        for(unsigned i = 0; i < total; i++)
            boardsToCheck.push_back(i);
    }
    else
    {
        if (args.devIdx < total)
            boardsToCheck.push_back(args.devIdx);
    }
    if (boardsToCheck.empty())
    {
        std::cout << "Card not found!" << std::endl;
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
        std::cout << "Shell on below card(s) will be updated:" << std::endl;
        for (auto p : boardsToUpdate)
        {
            std::cout << "Card_ID[" << p.first << "]" << std::endl;
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

    std::cout << success << " Card(s) flashed successfully." << std::endl;
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
 * main
 */
int main( int argc, char *argv[])
{

    unsigned index = 0xffffffff;
    unsigned regionIndex = 0xffffffff;
    unsigned short targetFreq[2] = {0, 0};
    std::string outMemReadFile = "memread.out";
    std::string flashType = ""; // unset and empty by default
    std::string mcsFile1, mcsFile2;
    std::string xclbin;
    bool hot = false;
    int c;

    dd::ddArgs_t ddArgs;
    const char* exe = argv[ 0 ];
    if (argc == 1) {
        xcldev::printHelp(exe);
        return 1;
    }

    argv++;
    const auto v = xcldev::commandTable.find(*argv);
    if (v == xcldev::commandTable.end()) {
        std::cout << "ERROR: Unknown comand \'" << *argv << "\'\n";
        xcldev::printHelp(exe);
        return 1;
    }

    const xcldev::command cmd = v->second;
    std::string cmdname = v->first;
    xcldev::subcommand subcmd = xcldev::MEM_READ;
    unsigned int ipmask = static_cast<unsigned int>(xcldev::STATUS_NONE_MASK);
    argc--;

    if (cmd == xcldev::HELP) {
        xcldev::printHelp(exe);
        return 1;
    }

    if (cmd == xcldev::FLASH) {
        return xcldev::flash_helper(argc, argv);
    }

    argv[0] = const_cast<char *>(exe);
    static struct option long_options[] = {
        {"spm", no_argument, 0, xcldev::STATUS_SPM},
        {"lapc", no_argument, 0, xcldev::STATUS_LAPC},
        {"sspm", no_argument, 0, xcldev::STATUS_SSPM},
        {"tracefunnel", no_argument, 0, xcldev::STATUS_UNSUPPORTED},
        {"monitorfifolite", no_argument, 0, xcldev::STATUS_UNSUPPORTED},
        {"monitorfifofull", no_argument, 0, xcldev::STATUS_UNSUPPORTED},
        {"accelmonitor", no_argument, 0, xcldev::STATUS_UNSUPPORTED},
        {"query-ecc", no_argument, 0, xcldev::MEM_QUERY_ECC},
        {"reset-ecc", no_argument, 0, xcldev::MEM_RESET_ECC},
        {0, 0, 0, 0}
    };

    int long_index;
    const char* short_options = "a:b:c:d:e:f:g:hi:m:n:o:p:r:s"; //don't add numbers
    while ((c = getopt_long(argc, argv, short_options, long_options, &long_index)) != -1)
    {
        if (cmd == xcldev::LIST) {
            std::cout << "ERROR: 'list' command does not accept any options\n";
            return -1;
        }
        switch (c)
        {
        //Deal with long options. Long options return the value set in option::val
        case xcldev::STATUS_LAPC : {
            //--lapc
            if (cmd != xcldev::STATUS) {
                std::cout << "ERROR: Option '" << long_options[long_index].name << "' cannot be used with command " << cmdname << "\n";
                return -1;
            }
            ipmask |= static_cast<unsigned int>(xcldev::STATUS_LAPC_MASK);
            break;
        }
        case xcldev::STATUS_SPM : {
            //--spm
            if (cmd != xcldev::STATUS) {
                std::cout << "ERROR: Option '" << long_options[long_index].name << "' cannot be used with command " << cmdname << "\n";
                return -1;
            }
            ipmask |= static_cast<unsigned int>(xcldev::STATUS_SPM_MASK);
            break;
        }
        case xcldev::STATUS_SSPM : {
            if (cmd != xcldev::STATUS) {
                std::cout << "ERROR: Option '" << long_options[long_index].name << "' cannot be used with command " << cmdname << "\n" ;
                return -1 ;
            }
            ipmask |= static_cast<unsigned int>(xcldev::STATUS_SSPM_MASK);
            break ;
        }
        case xcldev::STATUS_UNSUPPORTED : {
            //Don't give ERROR for as yet unsupported IPs
            std::cout << "INFO: No Status information available for IP: " << long_options[long_index].name << "\n";
            return 0;
        }
        case xcldev::MEM_QUERY_ECC : {
            //--query-ecc
            if (cmd != xcldev::MEM) {
                std::cout << "ERROR: Option '" << long_options[long_index].name << "' cannot be used with command " << cmdname << "\n";
                return -1;
            }
            subcmd = xcldev::MEM_QUERY_ECC;
            break;
        }
        case xcldev::MEM_RESET_ECC : {
            //--reset-ecc
            if (cmd != xcldev::MEM) {
                std::cout << "ERROR: Option '" << long_options[long_index].name << "' cannot be used with command " << cmdname << "\n";
                return -1;
            }
            subcmd = xcldev::MEM_RESET_ECC;
            break;
        }
        //short options are dealt here
        case 'r':
            if (cmd == xcldev::BOOT) {
                std::cout << "ERROR: '-r' not applicable for this command\n";
                return -1;
            }
            regionIndex = std::atoi(optarg);
            break;
        case 'p':
            if (cmd != xcldev::PROGRAM) {
                std::cout << "ERROR: '-p' only allowed with 'program' command\n";
                return -1;
            }
            xclbin = optarg;
            break;
        case 'f':
            if (cmd != xcldev::CLOCK) {
                std::cout << "ERROR: '-f' only allowed with 'clock' command\n";
                return -1;
            }
            targetFreq[0] = std::atoi(optarg);
            break;
        case 'g':
            if (cmd != xcldev::CLOCK) {
                std::cout << "ERROR: '-g' only allowed with 'clock' command\n";
                return -1;
            }
            targetFreq[1] = std::atoi(optarg);
            break;
        case 'h':
        {
            if (cmd != xcldev::RESET) {
                std::cout << "ERROR: '-h' only allowed with 'reset' command\n";
                return -1;
            }
            hot = true;
            break;
        }
        case 'd': {
            int ret = str2index(optarg, index);
            if (ret != 0)
                return ret;
            break;
        }
        default:
            xcldev::printHelp(exe);
            return 1;
        }
    }

    if (optind != argc) {
        std::cout << "ERROR: Illegal command \'" << argv[optind++] << "\'\n";
        return -1;
    }

    if (index == 0xffffffff) index = 0;

    if (regionIndex == 0xffffffff) regionIndex = 0;

    switch (cmd) {
        case xcldev::BOOT:
        case xcldev::QUERY:
        case xcldev::SCAN:
            break;
        case xcldev::PROGRAM:
        {
            if (xclbin.size() == 0) {
                std::cout << "ERROR: Please specify xclbin file with '-p' switch\n";
                return -1;
            }
            break;
        }
        case xcldev::CLOCK:
        {
            if (!targetFreq[0] && !targetFreq[1]) {
                std::cout << "ERROR: Please specify frequency(ies) with '-f' and or '-g' switch(es)\n";
                return -1;
            }
            break;
        }
        default:
            break;
    }

   std::vector<std::unique_ptr<xcldev::device>> deviceVec;

    unsigned int total = pcidev::get_dev_total();
    if (total == 0) {
        std::cout << "ERROR: No card found\n";
        return 1;
    }
    if (cmd != xcldev::DUMP)
        std::cout << "INFO: Found total " << total << " card(s) "
                  << std::endl;

    if (cmd == xcldev::SCAN) {
        print_pci_info();
        return 0;
    }

    for (unsigned i = 0; i < total; i++) {
        try {
            deviceVec.emplace_back(new xcldev::device(i, nullptr));
        } catch (const std::exception& ex) {
            std::cout << ex.what() << std::endl;
        }
    }

    if (cmd == xcldev::LIST) {
        for (unsigned i = 0; i < deviceVec.size(); i++) {
            std::cout << '[' << i << "] " << std::hex
                << std::setw(2) << std::setfill('0') << deviceVec[i]->bus() << ":"
                << std::setw(2) << std::setfill('0') << deviceVec[i]->dev() << "."
                << deviceVec[i]->mgmtFunc() << " "
                << deviceVec[i]->name() << std::endl;
        }
        return 0;
    }

    if (index >= deviceVec.size()) {
        if (index >= total)
            std::cout << "ERROR: Card index " << index << " is out of range";
        else
            std::cout << "ERROR: Card_ID[" << index << "] is not ready";
        std::cout << std::endl;
        return 1;
    }

    if(pcidev::get_dev(index)->mgmt == NULL){
        std::cout << "ERROR: Card index " << index << " is not usable\n";
        return 1;
    }

    int result = 0;

    switch (cmd)
    {
    case xcldev::BOOT:
        result = deviceVec[index]->boot();
        break;
    case xcldev::CLOCK:
        result = deviceVec[index]->reclock2(regionIndex, targetFreq);
        break;
    case xcldev::PROGRAM:
        result = deviceVec[index]->program(xclbin, regionIndex);
        break;
    case xcldev::QUERY:
        try
        {
            result = deviceVec[index]->dump(std::cout);
        }
        catch (...) {
            std::cout << "ERROR: query failed" << std::endl;
        }
        break;
    case xcldev::DUMP:
        result = deviceVec[index]->dumpJson(std::cout);
        break;
    case xcldev::RESET:
        if (hot) regionIndex = 0xffffffff;
        result = deviceVec[index]->reset(regionIndex);
        break;
    case xcldev::MEM:
        if(subcmd == xcldev::MEM_QUERY_ECC) {
            result = deviceVec[index]->printEccInfo(std::cout);
        } else if(subcmd == xcldev::MEM_RESET_ECC) {
            result = deviceVec[index]->resetEccInfo();
        } else {
            result = -1;
        }
        break;
    default:
        std::cout << "ERROR: Not implemented\n";
        result = -1;
    }

    if (result != 0)
        std::cout << "ERROR: xbmgmt " << v->first  << " failed." << std::endl;
    else if (cmd != xcldev::DUMP)
        std::cout << "INFO: xbmgmt " << v->first << " succeeded." << std::endl;

    return result;
}

void xcldev::printHelp(const std::string& exe)
{
    std::cout << "Running xbmgmt\n\n";
    std::cout << "Usage: " << exe << " <command> [options]\n\n";
    std::cout << "Command and option summary:\n";
    std::cout << "  clock   [-d card] [-r region] [-f clock1_freq_MHz] [-g clock2_freq_MHz]\n";
    std::cout << "  dump\n";
    std::cout << "  help\n";
    std::cout << "  list\n";
    std::cout << "  mem --query-ecc [-d card]\n";
    std::cout << "  program [-d card] [-r region] -p xclbin\n";
    std::cout << "  query   [-d card [-r region]]\n";
    std::cout << "  reset   [-d card] [-h | -r region]\n";
    std::cout << "  scan\n";
    std::cout << " Requires root privileges:\n";
    std::cout << "  mem --reset-ecc [-d card]\n";
    std::cout << "  flash   [-d card] -m primary_mcs [-n secondary_mcs] [-o bpi|spi]\n";
    std::cout << "  flash   [-d card] -a <all | dsa> [-t timestamp]\n";
    std::cout << "  flash   [-d card] -p msp432_firmware\n";
    std::cout << "  flash   scan [-v]\n";
    std::cout << "\nExamples:\n";
    std::cout << "Print JSON file to stdout\n";
    std::cout << "  " << exe << " dump\n";
    std::cout << "List all cards\n";
    std::cout << "  " << exe << " list\n";
    std::cout << "Scan for Xilinx PCIe card(s) & associated drivers (if any) and relevant system information\n";
    std::cout << "  " << exe << " scan\n";
    std::cout << "Change the clock frequency of region 0 in card 0 to 100 MHz\n";
    std::cout << "  " << exe << " clock -f 100\n";
    std::cout << "For card 0 which supports multiple clocks, change the clock 1 to 200MHz and clock 2 to 250MHz\n";
    std::cout << "  " << exe << " clock -f 200 -g 250\n";
    std::cout << "Download the accelerator program for card 2\n";
    std::cout << "  " << exe << " program -d 2 -p a.xclbin\n";
    std::cout << "Flash all installed DSA for all cards, if not done\n";
    std::cout << "  sudo " << exe << " flash -a all\n";
    std::cout << "Show DSA related information for all cards in the system\n";
    std::cout << "  sudo " << exe << " flash scan\n";
}

std::unique_ptr<xcldev::device> xcldev::xclGetDevice(unsigned index)
{
    try {
        unsigned int count = pcidev::get_dev_total();
        if (count == 0) {
            std::cout << "ERROR: No card found" << std::endl;
        } else if (index >= count) {
            std::cout << "ERROR: Card index " << index << " out of range";
            std::cout << std::endl;
        } else {
            return std::make_unique<xcldev::device>(index,nullptr);
        }
    }
    catch (const std::exception& ex) {
        std::cout << "ERROR: " << ex.what() << std::endl;
    }

    return nullptr;
}

const std::string dsaPath("/opt/xilinx/dsa/");

void testCaseProgressReporter(bool *quit)
{    int i = 0;
    while (!*quit) {
        if (i != 0 && (i % 5 == 0))
            std::cout << "." << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i++;
    }
}

int runShellCmd(const std::string& cmd, std::string& output)
{
    int ret = 0;
    bool quit = false;

    // Kick off progress reporter
    std::thread t(testCaseProgressReporter, &quit);

    // Run test case
    setenv("XILINX_XRT", "/opt/xilinx/xrt", 0);
    setenv("LD_LIBRARY_PATH", "/opt/xilinx/xrt/lib", 1);
    unsetenv("XCL_EMULATION_MODE");
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (pipe == nullptr) {
        std::cout << "ERROR: Failed to run " << cmd << std::endl;
        ret = -EINVAL;
    }

    // Read all output
    char buf[256];
    while (ret == 0 && !feof(pipe.get())) {
        if (fgets(buf, sizeof (buf), pipe.get()) != nullptr) {
            output += buf;
        }
    }

    // Stop progress reporter
    quit = true;
    t.join();

    return ret;
}

int xcldev::device::runTestCase(const std::string& exe,
    const std::string& xclbin, std::string& output)
{
    std::string testCasePath = dsaPath +
        std::string(m_devinfo.mName) + "/test/";
    std::string exePath = testCasePath + exe;
    std::string xclbinPath = testCasePath + xclbin;
    std::string idxOption;
    struct stat st;

    output.clear();

    if (stat(exePath.c_str(), &st) != 0 || stat(xclbinPath.c_str(), &st) != 0) {
        output += "ERROR: Failed to find ";
        output += exe;
        output += " or ";
        output += xclbin;
        output += ", Shell package not installed properly.";
        return -ENOENT;
    }

    // Program xclbin first.
    int ret = program(xclbinPath, 0);
    if (ret != 0) {
        output += "ERROR: Failed to download xclbin: ";
        output += xclbin;
        return -EINVAL;
    }

    if (m_idx != 0)
        idxOption = "-d " + std::to_string(m_idx);

    std::string cmd = exePath + " " + xclbinPath + " " + idxOption;
    return runShellCmd(cmd, output);
}

static int getEccMemTags(const pcidev::pci_device *dev,
    std::vector<std::string>& tags)
{
    std::string errmsg;
    std::vector<char> buf;

    if(!dev->mgmt)
        return 0;

    dev->mgmt->sysfs_get("icap", "mem_topology", errmsg, buf);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }
    const mem_topology *map = (mem_topology *)buf.data();

    if(buf.empty() || map->m_count == 0) {
        std::cout << "WARNING: 'mem_topology' not found, "
            << "unable to query ECC info. Has the xclbin been loaded? "
            << "See 'xbmgmt program'." << std::endl;
        return -EINVAL;
    }

    // Only support DDR4 mem controller for ECC status
    for(int32_t i = 0; i < map->m_count; i++) {
        if(!map->m_mem_data[i].m_used)
            continue;
        tags.emplace_back(
            reinterpret_cast<const char *>(map->m_mem_data[i].m_tag));
    }

    if (tags.empty()) {
        std::cout << "No supported ECC controller detected!" << std::endl;
        return -ENOENT;
    }
    return 0;
}

static int eccStatus2String(unsigned int status, std::string& str)
{
    const int ce_mask = (0x1 << 1);
    const int ue_mask = (0x1 << 0);

    str.clear();

    // If unknown status bits, can't support.
    if (status & ~(ce_mask | ue_mask)) {
        std::cout << "Bad ECC status detected!" << std::endl;
        return -EINVAL;
    }

    if (status == 0) {
        str = "(None)";
        return 0;
    }

    if (status & ue_mask)
        str += "UE ";
    if (status & ce_mask)
        str += "CE ";
    // Remove the trailing space.
    str.pop_back();
    return 0;
}

int xcldev::device::printEccInfo(std::ostream& ostr) const
{
    std::string errmsg;
    std::vector<std::string> tags;
    auto dev = pcidev::get_dev(m_idx);

    if(!dev->mgmt)
        return 0;

    int err = getEccMemTags(dev, tags);
    if (err)
        return err;

    // Report ECC status
    ostr << std::endl;
    ostr << std::left << std::setw(16) << "Tag" << std::setw(12) << "Errors"
        << std::setw(12) << "CE Count" << std::setw(20) << "CE FFA"
        << std::setw(20) << "UE FFA" << std::endl;
    for (auto tag : tags) {
        unsigned status = 0;
        std::string st;
        dev->mgmt->sysfs_get(tag, "ecc_status", errmsg, status);
        if (!errmsg.empty())
            continue;
        err = eccStatus2String(status, st);
        if (err)
            return err;

        unsigned ce_cnt = 0;
        dev->mgmt->sysfs_get(tag, "ecc_ce_cnt", errmsg, ce_cnt);
        uint64_t ce_ffa = 0;
        dev->mgmt->sysfs_get(tag, "ecc_ce_ffa", errmsg, ce_ffa);
        uint64_t ue_ffa = 0;
        dev->mgmt->sysfs_get(tag, "ecc_ue_ffa", errmsg, ue_ffa);
        ostr << std::left << std::setw(16) << tag << std::setw(12) << st
            << std::setw(12) << ce_cnt << "0x" << std::setw(18) << std::hex
            << ce_ffa << "0x" << std::setw(18) << ue_ffa << std::endl;
    }
    ostr << std::endl;
    return 0;
}

int xcldev::device::resetEccInfo()
{
    std::string errmsg;
    std::vector<std::string> tags;
    auto dev = pcidev::get_dev(m_idx);

    if ((getuid() != 0) && (geteuid() != 0)) {
        std::cout << "ERROR: root privileges required." << std::endl;
        return -EPERM;
    }

    int err = getEccMemTags(dev, tags);
    if (err)
        return err;

    std::cout << "Resetting ECC info..." << std::endl;
    for (auto tag : tags)
        dev->mgmt->sysfs_put(tag, "ecc_reset", errmsg, "1");
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

    sudoOrDie();

    unsigned total = pcidev::get_dev_total();
    if (total == 0) {
        std::cout << "No card is found!" << std::endl;
        return 0;
    }

    for(unsigned i = 0; i < total; i++)
    {
        std::cout << "Card_ID[" << i << "]" << std::endl;

        Flasher f(i);
        if (!f.isValid())
            continue;

        DSAInfo board = f.getOnBoardDSA();
        std::cout << "\tCard BDF:\t\t" << f.sGetDBDF() << std::endl;
        std::cout << "\tCard type:\t\t" << board.board << std::endl;
        std::cout << "\tFlash type:\t\t" << f.sGetFlashType() << std::endl;
        std::cout << "\tShell running on FPGA:" << std::endl;
        std::cout << "\t\t" << board << std::endl;

        std::vector<DSAInfo> installedDSA = f.getInstalledDSA();
        std::cout << "\tShell package installed in system:\t";
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
            std::cout << "\tCard name\t\t" << info.mName << std::endl;
            std::cout << "\tCard rev\t\t" << info.mRev << std::endl;
            std::cout << "\tCard S/N: \t\t" << info.mSerialNum << std::endl;
            std::cout << "\tConfig mode: \t\t" << info.mConfigMode << std::endl;
            std::cout << "\tFan presence:\t\t" << info.mFanPresence << std::endl;
            std::cout << "\tMax power level:\t" << info.mMaxPower << std::endl;
            std::cout << "\tMAC address0:\t\t" << info.mMacAddr0 << std::endl;
            std::cout << "\tMAC address1:\t\t" << info.mMacAddr1 << std::endl;
            std::cout << "\tMAC address2:\t\t" << info.mMacAddr2 << std::endl;
            std::cout << "\tMAC address3:\t\t" << info.mMacAddr3 << std::endl;
        }

        std::cout << std::endl;
    }

    return 0;
}