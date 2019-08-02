/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include <string>
#include <cstring>
#include <iostream>
#include <map>
#include <functional>
#include <getopt.h>

#include "flasher.h"
#include "core/pcie/linux/scan.h"
#include "core/common/sensor.h"
#include "xbmgmt.h"

const char *subCmdFlashDesc = "Update SC firmware or shell on the device";
const char *subCmdFlashUsage =
    "--scan [--verbose|--json]\n"
    "--update [--shell name [--id id]] [--card bdf] [--force]\n"
    "--shell --path file [--card bdf] [--type flash_type]\n"
    "--sc_firmware --path file [--card bdf]\n"
    "--reset [--card bdf]";

static int scanDevices(bool verbose, bool json)
{
    unsigned total = pcidev::get_dev_total(false);

    if (total == 0) {
        std::cout << "No card is found!" << std::endl;
        return 0;
    }

    for(unsigned i = 0; i < total; i++) {
        Flasher f(i);
        if (!f.isValid())
            continue;

        DSAInfo board = f.getOnBoardDSA();
        std::vector<DSAInfo> installedDSA = f.getInstalledDSA();
        BoardInfo info;
        const auto getinfo_res = f.getBoardInfo(info);
        if (json) {
            const std::string card = "card" + std::to_string(i);
            if (!installedDSA.empty()) {
                std::stringstream shellpackage;
                for (auto& d : installedDSA)
                    shellpackage << d << "; ";
                sensor_tree::put(card + ".shellpackage", shellpackage.str());
            }
            if (getinfo_res == 0) {
                sensor_tree::put(card + ".name", info.mName);
                sensor_tree::put(card + ".serial", info.mSerialNum);
                sensor_tree::put(card + ".config_mode", info.mConfigMode);
                sensor_tree::put(card + ".fan_presence", info.mFanPresence);
                sensor_tree::put(card + ".max_power", info.mMaxPower);
                sensor_tree::put(card + ".mac0", info.mMacAddr0);
                sensor_tree::put(card + ".mac1", info.mMacAddr1);
                sensor_tree::put(card + ".mac2", info.mMacAddr2);
                sensor_tree::put(card + ".mac3", info.mMacAddr3);
            }
            sensor_tree::json_dump( std::cout );
        } else {
            std::cout << "Card [" << f.sGetDBDF() << "]" << std::endl;
            std::cout << "\tCard type:\t\t" << board.board << std::endl;
            std::cout << "\tFlash type:\t\t" << f.sGetFlashType() << std::endl;
            std::cout << "\tShell running on FPGA:" << std::endl;
            std::cout << "\t\t" << board << std::endl;

            std::cout << "\tShell package installed in system:\t";
            if (!installedDSA.empty()) {
                for (auto& d : installedDSA)
                    std::cout << std::endl << "\t\t" << d;
            } else {
                std::cout << "(None)";
            }
            std::cout << std::endl;
            if (verbose && getinfo_res == 0) {
                std::cout << "\tCard name\t\t" << info.mName << std::endl;
#if 0   // Do not print out rev until further notice
                std::cout << "\tCard rev\t\t" << info.mRev << std::endl;
#endif
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
    }

    return 0;
}

static bool match_id(DSAInfo& dsa, std::string& id)
{
    if (dsa.uuid.empty())
    {
        uint64_t ts = strtoull(id.c_str(), nullptr, 16);
        if (ts == dsa.timestamp)
            return true;
    } else {
        std::string uuid(id.length(), 0);
        std::transform(id.begin(), id.end(), uuid.begin(), ::tolower);
        std::string::size_type i = uuid.find("0x");
        if (i == 0)
            uuid.erase(0, 2);
        if (!strncmp(dsa.uuid.c_str(), uuid.c_str(), uuid.length()))
            return true;
    }
    return false;
}

static bool match_id(DSAInfo& dsa1, DSAInfo& dsa2)
{
    if (dsa1.uuid.empty() != dsa2.uuid.empty())
        return false;
    else if (dsa1.uuid.empty())
    {
        if (dsa1.timestamp == dsa2.timestamp)
            return true;
    } else {
        if (!strcmp(dsa1.uuid.c_str(), dsa2.uuid.c_str()))
            return true;
    }
    return false;
}

// Update SC firmware on the board.
static int updateSC(unsigned index, const char *file)
{
    Flasher flasher(index);
    if(!flasher.isValid())
        return -EINVAL;

    std::shared_ptr<firmwareImage> bmc =
        std::make_shared<firmwareImage>(file, BMC_FIRMWARE);
    if (bmc->fail())
        return -EINVAL;

    return flasher.upgradeBMCFirmware(bmc.get());
}

// Update shell on the board.
static int updateShell(unsigned index, std::string flashType,
    const char *primary, const char *secondary)
{
    std::shared_ptr<firmwareImage> pri;
    std::shared_ptr<firmwareImage> sec;

    if (!flashType.empty()) {
        std::cout << "CAUTION: Overriding flash mode is not recommended. " <<
            "You may damage your card with this option." << std::endl;
        if(!canProceed())
            return -ECANCELED;
    }

    Flasher flasher(index);
    if(!flasher.isValid())
        return -EINVAL;

    if (primary == nullptr)
        return -EINVAL;

    pri = std::make_shared<firmwareImage>(primary, MCS_FIRMWARE_PRIMARY);
    if (pri->fail())
        return -EINVAL;

    if (secondary != nullptr) {
        sec = std::make_shared<firmwareImage>(secondary,
            MCS_FIRMWARE_SECONDARY);
        if (sec->fail())
            sec = nullptr;
    }

    return flasher.upgradeFirmware(flashType, pri.get(), sec.get());
}

// Reset shell to factory mode.
static int resetShell(unsigned index)
{
    Flasher flasher(index);
    if(!flasher.isValid())
        return -EINVAL;

    std::cout << "CAUTION: Resetting Card [" << flasher.sGetDBDF() <<
        "] back to factory mode." << std::endl;
    if(!canProceed())
        return -ECANCELED;

    return flasher.upgradeFirmware("", nullptr, nullptr);
}

static int updateShellAndSC(unsigned boardIdx, DSAInfo& candidate, bool& reboot)
{
    reboot = false;

    Flasher flasher(boardIdx);
    if(!flasher.isValid()) {
        std::cout << "card not available" << std::endl;
        return -EINVAL;
    }

    bool same_dsa = false;
    bool same_bmc = false;
    DSAInfo current = flasher.getOnBoardDSA();
    if (!current.name.empty()) {
        same_dsa = (candidate.name == current.name &&
            match_id(candidate, current));
        same_bmc = (current.bmcVer.empty() ||
            candidate.bmcVer == current.bmcVer);
    }
    if (same_dsa && same_bmc)
        std::cout << "update not needed" << std::endl;

    if (!same_bmc) {
        std::cout << "Updating SC firmware on card[" << flasher.sGetDBDF() <<
            "]" << std::endl;
        int ret = updateSC(boardIdx, candidate.file.c_str());
        if (ret != 0) {
            std::cout << "WARNING: Failed to update SC firmware on card ["
                << flasher.sGetDBDF() << "]" << std::endl;
        }
    }

    if (!same_dsa) {
        std::cout << "Updating shell on card[" << flasher.sGetDBDF() <<
            "]" << std::endl;
        int ret = updateShell(boardIdx, "", candidate.file.c_str(),
            candidate.file.c_str());
        if (ret != 0) {
            std::cout << "ERROR: Failed to update shell on card["
                << flasher.sGetDBDF() << "]" << std::endl;
        } else {
            reboot = true;
        }
    }

    if (!same_dsa && !reboot)
        return -EINVAL;

    return 0;
}

static DSAInfo selectShell(unsigned idx, std::string& dsa, std::string& id)
{
    unsigned candidateDSAIndex = UINT_MAX;

    Flasher flasher(idx);
    if(!flasher.isValid())
        return DSAInfo("");

    std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();

    // Find candidate DSA from installed DSA list.
    if (dsa.empty()) {
        std::cout << "Probing card [" << flasher.sGetDBDF() << "]: ";
        if (installedDSA.empty()) {
            std::cout << "no shell is installed" << std::endl;
            return DSAInfo("");
        }
        if (installedDSA.size() > 1) {
            std::cout << "multiple shells are installed" << std::endl;
            return DSAInfo("");
        }
        candidateDSAIndex = 0;
    } else {
        for (unsigned int i = 0; i < installedDSA.size(); i++) {
            DSAInfo& idsa = installedDSA[i];
            if (dsa != idsa.name)
                continue;
            if (!id.empty() && !match_id(idsa, id))
                continue;
            if (candidateDSAIndex != UINT_MAX) {
                std::cout << "multiple shells are installed" << std::endl;
                return DSAInfo("");
            }
            candidateDSAIndex = i;
        }
    }

    if (candidateDSAIndex == UINT_MAX) {
        std::cout << "WARNING: Failed to flash Card["
                  << flasher.sGetDBDF() << "]: Specified shell is not applicable" << std::endl;
        return DSAInfo("");
    }

    DSAInfo& candidate = installedDSA[candidateDSAIndex];

    bool same_dsa = false;
    bool same_bmc = false;
    DSAInfo currentDSA = flasher.getOnBoardDSA();
    if (!currentDSA.name.empty()) {
        same_dsa = (candidate.name == currentDSA.name &&
            match_id(candidate, currentDSA));
        same_bmc = (currentDSA.bmcVer.empty() ||
            candidate.bmcVer == currentDSA.bmcVer);
    }
    if (same_dsa && same_bmc) {
        std::cout << "shell is up-to-date" << std::endl;
        return DSAInfo("");
    }
    std::cout << "shell needs updating" << std::endl;
    return candidate;
}

static int autoFlash(unsigned index, std::string& shell,
    std::string& id, bool force)
{
    std::vector<unsigned int> boardsToCheck;
    std::vector<std::pair<unsigned, DSAInfo>> boardsToUpdate;

    // Sanity check input dsa and timestamp.
    if (!shell.empty()) {
        bool foundDSA = false;
        bool multiDSA = false;
        auto installedDSAs = firmwareImage::getIntalledDSAs();
        for (DSAInfo& dsa : installedDSAs) {
            if (shell == dsa.name &&
                (id.empty() || match_id(dsa, id))) {
                if (!foundDSA)
                    foundDSA = true;
                else
                    multiDSA = true;
            }
        }
        if (!foundDSA) {
            std::cout << "Specified shell not installed." << std::endl;
            return -ENOENT;
        }
        if (multiDSA) {
            std::cout << "Specified shell matched multiple installed shells" <<
                std::endl;
            return -ENOTUNIQ;
        }
    }

    // Collect all indexes of boards need checking
    unsigned total = pcidev::get_dev_total(false);
    if (index == UINT_MAX) {
        for(unsigned i = 0; i < total; i++)
            boardsToCheck.push_back(i);
    } else {
        if (index < total)
            boardsToCheck.push_back(index);
    }
    if (boardsToCheck.empty()) {
        std::cout << "Card not found!" << std::endl;
        return -ENOENT;
    }

    // Collect all indexes of boards need updating
    for (unsigned i : boardsToCheck) {
        DSAInfo dsa = selectShell(i, shell, id);
        if (dsa.DSAValid)
            boardsToUpdate.push_back(std::make_pair(i, dsa));
    }

    // Continue to flash whatever we have collected in boardsToUpdate.
    unsigned success = 0;
    bool needreboot = false;
    if (!boardsToUpdate.empty()) {
        std::cout << "Below card(s) will be updated:" << std::endl;
        for (auto p : boardsToUpdate)
            std::cout << pcidev::get_dev(p.first, false) << std::endl;

        // Prompt user about what boards will be updated and ask for permission.
        if(!force && !canProceed())
            return -ECANCELED;

        // Perform DSA and BMC updating
        for (auto p : boardsToUpdate) {
            bool reboot;
            std::cout << std::endl;
            if (updateShellAndSC(p.first, p.second, reboot) == 0) {
                std::cout << "Successfully flashed Card[" << success << "]"<< std::endl;
                success++;
            }
            needreboot |= reboot;
        }
    }

    std::cout << std::endl;

    if (success!=0) {
        std::cout << success << " Card(s) flashed successfully." << std::endl; 
    } else {
        std::cout << "No cards were flashed." << std::endl; 
    }

    if (needreboot) {
        std::cout << "Cold reboot machine to load the new image on card(s)."
            << std::endl;
    }

    if (success != boardsToUpdate.size()) {
        std::cout << "WARNING:" << boardsToUpdate.size()-success << " Card(s) not flashed. " << std::endl;
        exit(-EINVAL);
    }

    return 0;
}

// For backward compatibility, will be removed later
static int flashCompatibleMode(int argc, char *argv[])
{
    if (argc < 2)
        return -EINVAL;

    if (strcmp(argv[1], "scan") == 0) {
        bool verbose = false;

        if (argc > 3)
            return -EINVAL;

        if (argc == 3) {
            if (strcmp(argv[2], "-v") != 0)
                return -EINVAL;
            verbose = true;
        }

        return scanDevices(verbose, false);
    }

    unsigned devIdx = UINT_MAX;
    char *primary = nullptr;
    char *secondary = nullptr;
    char *bmc = nullptr;
    std::string flashType;
    std::string dsa;
    std::string id;
    bool force = false;
    bool reset = false;

    int opt;
    while ((opt = getopt(argc, argv, "a:d:fm:n:o:p:rt:")) != -1) {
        switch (opt) {
        case 'a':
            dsa = optarg;
            break;
        case 'd':
            if (std::string(optarg).find(":") == std::string::npos)
                devIdx = atoi(optarg);
            else
                devIdx = bdf2index(optarg);
            if (devIdx == UINT_MAX)
                return -EINVAL;
            break;
        case 'f':
            force = true;
            break;
        case 'm':
            primary = optarg;
            break;
        case 'n':
            secondary = optarg;
            break;
        case 'o':
            flashType = optarg;
            break;
        case 'p':
            bmc = optarg;
            break;
        case 't':
	    id = optarg;
            break;
        case 'r':
            reset = true;
            break;
        default:
            return -EINVAL;
        }
    }

    if (reset) {
        int ret = resetShell(devIdx == UINT_MAX ? 0 : devIdx);
        if (ret)
            return ret;
        std::cout << "Shell is reset succesfully" << std::endl;
        std::cout << "Cold reboot machine to load new shell on card" <<
            std::endl;
        return 0;
    }

    if (bmc)
        return updateSC(devIdx == UINT_MAX ? 0 : devIdx, bmc);

    if (primary) {
        int ret = updateShell(devIdx == UINT_MAX ? 0 : devIdx, flashType,
            primary, secondary);
        if (ret)
            return ret;
        std::cout << "Shell is updated succesfully" << std::endl;
        std::cout << "Cold reboot machine to load new shell on card" <<
            std::endl;
        return 0;
    }

    if (!dsa.empty()) {
        if (dsa.compare("all") == 0)
            dsa.clear();
        return autoFlash(devIdx, dsa, id, force);
    }

    return -EINVAL;
}

static int scan(int argc, char *argv[])
{
    bool verbose = false;
    bool json = false;
    const option opts[] = {
        { "verbose", no_argument, nullptr, '0' },
        { "json", no_argument, nullptr, '1' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            verbose = true;
            break;
        case '1':
            json = true;
            break;
        default:
            return -EINVAL;
        }
    }
    if (verbose && json)
        return -EINVAL;
    return scanDevices(verbose, json);
}

static int update(int argc, char *argv[])
{
    bool force = false;
    unsigned index = UINT_MAX;
    std::string shell;
    std::string id;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "shell", required_argument, nullptr, '1' },
        { "id", required_argument, nullptr, '2' },
        { "force", no_argument, nullptr, '3' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '1':
            shell = std::string(optarg);
            break;
        case '2':
	    id = std::string(optarg);
            break;
        case '3':
            force = true;
            break;
        default:
            return -EINVAL;
        }
    }

    if (shell.empty() && !id.empty())
        return -EINVAL;

    return autoFlash(index, shell, id, force);
}

static int shell(int argc, char *argv[])
{
    unsigned index = UINT_MAX;
    std::string file;
    std::string type;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "path", required_argument, nullptr, '1' },
        { "flash_type", required_argument, nullptr, '2' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '1':
            file = std::string(optarg);
            break;
        case '2':
            type = std::string(optarg);
            break;
        default:
            return -EINVAL;
        }
    }

    if (file.empty())
        return -EINVAL;

    int ret = updateShell(index == UINT_MAX ? 0 : index, type, file.c_str(),
        nullptr);
    if (ret)
        return ret;

    std::cout << "Shell is updated succesfully" << std::endl;
    std::cout << "Cold reboot machine to load new shell on card" << std::endl;
    return 0;
}

static int sc(int argc, char *argv[])
{
    unsigned index = UINT_MAX;
    std::string file;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "path", required_argument, nullptr, '1' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '1':
            file = std::string(optarg);
            break;
        default:
            return -EINVAL;
        }
    }

    if (file.empty())
        return -EINVAL;

    int ret = updateSC(index == UINT_MAX ? 0 : index, file.c_str());
    if (ret)
        return ret;

    std::cout << "SC firmware is updated succesfully" << std::endl;
    return 0;
}

static int reset(int argc, char *argv[])
{
    unsigned index = UINT_MAX;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        default:
            return -EINVAL;
        }
    }

    int ret = resetShell(index == UINT_MAX ? 0 : index);
    if (ret)
        return ret;

    std::cout << "Shell is reset succesfully" << std::endl;
    std::cout << "Cold reboot machine to load new shell on card" << std::endl;
    return 0;
}

static const std::map<std::string, std::function<int(int, char **)>> optList = {
    { "--scan", scan },
    { "--update", update },
    { "--shell", shell },
    { "--sc_firmware", sc },
    { "--reset", reset },
};

int flashHandler(int argc, char *argv[])
{
    if (argc < 2)
        return -EINVAL;

    sudoOrDie();

    std::string subcmd(argv[1]);

    // Backward compatible, no long option used.
    if (subcmd.find("--") != 0)
        return flashCompatibleMode(argc,argv);

    auto cmd = optList.find(subcmd);
    if (cmd == optList.end())
        return -EINVAL;

    argc--;
    argv++;
    return cmd->second(argc, argv);
}
