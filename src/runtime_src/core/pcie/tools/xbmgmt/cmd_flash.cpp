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

#include <thread>
#include <string>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <map>
#include <functional>
#include <getopt.h>

#include "flasher.h"
#include "core/pcie/linux/scan.h"
#include "core/common/sensor.h"
#include "xbmgmt.h"

// For backward compatibility
const char *subCmdXbutilFlashDesc = "";
const char *subCmdXbutilFlashUsage =
    "[-d mgmt-bdf] -m primary_mcs [-n secondary_mcs] [-o bpi|spi]\n"
    "[-d mgmt-bdf] -a <all | shell> [-t timestamp]\n"
    "[-d mgmt-bdf] -p msp432_firmware\n"
    "scan [-v]\n";

const char *subCmdFlashDesc = "Update SC firmware or shell on the device";
const char *subCmdFlashUsage =
    "--scan [--verbose|--json]\n"
    "--update [--shell name [--id id]] [--card bdf] [--force]\n"
    "--factory_reset [--card bdf] [--force]\n";
const char *subCmdFlashExpUsage =
    "Experts only:\n"
    "--shell --primary primary_file [--secondary secondary_file] --card bdf [--flash_type flash_type]\n"
    "--sc_firmware --path file --card bdf";

#define fmt_str        "    "
#define DEV_TIMEOUT    60

#define HEX(x) std::hex << std::uppercase << std::setw(2) << std::setfill('0') \
	    << (int)(x & 0xFF)

static std::string getMacAddr(char *macAddrFirst, unsigned idx)
{
    std::ostringstream oss;

    oss << HEX(macAddrFirst[0]) << ":" <<
	   HEX(macAddrFirst[1]) << ":" <<
	   HEX(macAddrFirst[2]) << ":" <<
	   HEX(macAddrFirst[3]) << ":" <<
	   HEX(macAddrFirst[4]) << ":" <<
	   HEX((macAddrFirst[5] + idx));

    return oss.str();
}

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
		if (info.mMacContiguousNum) {
		    for (unsigned idx = 0; idx < info.mMacContiguousNum; idx++) {
		    	sensor_tree::put(card + ".mac" + std::to_string(idx),
			    getMacAddr(info.mMacAddrFirst, idx));
		    }
		} else {
                    sensor_tree::put(card + ".mac0", info.mMacAddr0);
                    sensor_tree::put(card + ".mac1", info.mMacAddr1);
                    sensor_tree::put(card + ".mac2", info.mMacAddr2);
                    sensor_tree::put(card + ".mac3", info.mMacAddr3);
		}
            }
        } else {
            std::cout << "Card [" << f.sGetDBDF() << "]" << std::endl;
            std::cout << fmt_str << "Card type:\t\t" << board.board << std::endl;
            std::cout << fmt_str << "Flash type:\t\t" << f.sGetFlashType() << std::endl;
            std::cout << fmt_str << "Flashable partition running on FPGA:" << std::endl;
            std::cout << fmt_str << fmt_str << board << std::endl;

            if (!board.uuids.empty() && verbose)
            {
                std::cout << fmt_str << fmt_str << fmt_str << "Logic UUID:" << std::endl;
                std::cout << fmt_str << fmt_str << fmt_str << board.uuids[0] << std::endl;
            }
            std::cout << fmt_str << "Flashable partitions installed in system:\t";
            if (!installedDSA.empty()) {
                for (auto& d : installedDSA)
                {
                    std::cout << std::endl << fmt_str << fmt_str << d;
                    if (!d.uuids.empty() && verbose)
                    {
                        std::cout << std::endl;
                        std::cout << fmt_str << fmt_str << fmt_str << "Logic UUID:" << std::endl;
                        std::cout << fmt_str << fmt_str << fmt_str << d.uuids[0];
                    }
                }
            } else {
                std::cout << "(None)";
            }
            std::cout << std::endl;
            if (verbose && getinfo_res == 0) {
                std::cout << fmt_str << "Card name\t\t\t" << info.mName << std::endl;
#if 0   // Do not print out rev until further notice
                std::cout << "\tCard rev\t\t" << info.mRev << std::endl;
#endif
                std::cout << fmt_str << "Card S/N: \t\t\t" << info.mSerialNum << std::endl;
                std::cout << fmt_str << "Config mode: \t\t" << info.mConfigMode << std::endl;
                std::cout << fmt_str << "Fan presence:\t\t" << info.mFanPresence << std::endl;
                std::cout << fmt_str << "Max power level:\t\t" << info.mMaxPower << std::endl;
		if (info.mMacContiguousNum) {
		    for (unsigned idx = 0; idx < info.mMacContiguousNum; idx++) {
		        std::cout << fmt_str << "MAC address" << idx << ":\t\t" <<
			    getMacAddr(info.mMacAddrFirst, idx) << std::endl;
		    }
		} else {
		    std::cout << fmt_str << "MAC address0:\t\t" << info.mMacAddr0 << std::endl;
		    std::cout << fmt_str << "MAC address1:\t\t" << info.mMacAddr1 << std::endl;
		    std::cout << fmt_str << "MAC address2:\t\t" << info.mMacAddr2 << std::endl;
		    std::cout << fmt_str << "MAC address3:\t\t" << info.mMacAddr3 << std::endl;
		}
            }
            std::cout << std::endl;
        }
    }

    if (json)
        sensor_tree::json_dump( std::cout );

    return 0;
}

static void testCaseProgressReporter(bool *quit)
{    int i = 0;
    while (!*quit) {
        if (i != 0 && (i % 5 == 0))
            std::cout << "." << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i++;
    }
}

inline const char* getenv_or_empty(const char* path)
{
    return getenv(path) ? getenv(path) : "";
}

static void set_shell_path_env(const std::string& var_name,
    const std::string& trailing_path)
{
    std::string xrt_path(getenv_or_empty("XILINX_XRT"));
    std::string new_path(getenv_or_empty(var_name.c_str()));
    xrt_path += trailing_path + ":";
    new_path = xrt_path + new_path;
    setenv(var_name.c_str(), new_path.c_str(), 1);
}

int runShellCmd(const std::string& cmd, std::string& output)
{
    int ret = 0;
    bool quit = false;

    // Fix environment variables before running test case
    setenv("XILINX_XRT", "/opt/xilinx/xrt", 0);
    set_shell_path_env("PYTHONPATH", "/python");
    set_shell_path_env("LD_LIBRARY_PATH", "/lib");
    set_shell_path_env("PATH", "/bin");
    unsetenv("XCL_EMULATION_MODE");

    int stderr_fds[2];
    if (pipe(stderr_fds)== -1) {
        perror("ERROR: Unable to create pipe");
        return -errno;
    }

    // Save stderr
    int stderr_save = dup(STDERR_FILENO);
    if (stderr_save == -1) {
        perror("ERROR: Unable to duplicate stderr");
        return -errno;
    }

    // Kick off progress reporter
    std::thread t(testCaseProgressReporter, &quit);

    // Close existing stderr and set it to be the write end of the pipe.
    // After fork below, our child process's stderr will point to the same fd.
    dup2(stderr_fds[1], STDERR_FILENO);
    close(stderr_fds[1]);
    std::shared_ptr<FILE> stderr_child(fdopen(stderr_fds[0], "r"), fclose);
    std::shared_ptr<FILE> stdout_child(popen(cmd.c_str(), "r"), pclose);
    // Restore our normal stderr
    dup2(stderr_save, STDERR_FILENO);
    close(stderr_save);

    if (stdout_child == nullptr) {
        std::cout << "ERROR: Failed to run " << cmd << std::endl;
        ret = -EINVAL;
    }

    // Read child's stdout and stderr without parsing the content
    char buf[1024];
    while (ret == 0 && !feof(stdout_child.get())) {
        if (fgets(buf, sizeof (buf), stdout_child.get()) != nullptr) {
            output += buf;
        }
    }
    while (ret == 0 && stderr_child && !feof(stderr_child.get())) {
        if (fgets(buf, sizeof (buf), stderr_child.get()) != nullptr) {
            output += buf;
        }
    }

    // Stop progress reporter
    quit = true;
    t.join();

    return ret;
}

static int writeSCImage(Flasher &flasher, const char *file)
{
    int ret = 0;
    std::shared_ptr<firmwareImage> bmc =
       std::make_shared<firmwareImage>(file, BMC_FIRMWARE);
    if (bmc->fail()) {
        ret = -EINVAL;
    } else {
        ret = flasher.upgradeBMCFirmware(bmc.get());
    }
    return ret;
}

// Update SC firmware on the board.
static int updateSC(unsigned index, const char *file,
    bool cardlevel = true, bool force = false)
{
    int ret = 0;
    Flasher flasher(index);
    if(!flasher.isValid())
        return -EINVAL;

    bool is_mfg = false;
    std::string errmsg;
    auto mgmt_dev = pcidev::get_dev(index, false);
    mgmt_dev->sysfs_get<bool>("", "mfg", errmsg, is_mfg, false);
    if (is_mfg || force) {
        return writeSCImage(flasher, file);
    }

    {// check fixed sc before trying to shutdown device
        XMC_Flasher xflasher(mgmt_dev);
        if (xflasher.fixedSC()) {
            std::cerr << "Flashing fixed SC not allowed" << std::endl;
            return -ENOTSUP;
        }
    }
    std::string vbnv;
    mgmt_dev->sysfs_get( "rom", "VBNV", errmsg, vbnv );
    if (!errmsg.empty()) {
        std::cerr << errmsg << std::endl;
        return -EINVAL;
    }
    //don't trigger reset for u30. let python helper handle everything
    if (vbnv.find("_u30_") != std::string::npos) {
        if (!cardlevel)
            return writeSCImage(flasher, file);

        std::string output;
        std::stringstream dbdf;
        const std::string scFlashPath = "/opt/xilinx/xrt/bin/unwrapped/_scflash.py";
        dbdf << std::setfill('0') << std::hex
            << std::setw(4) << mgmt_dev->domain << ":"
            << std::setw(2) << mgmt_dev->bus << ":"
            << std::setw(2) << mgmt_dev->dev << "."
            << std::setw(1) << mgmt_dev->func;
        const auto cmd = "/usr/bin/python3 " + scFlashPath + " -y -d " +
           dbdf.str() + " -p " + file;
        return runShellCmd(cmd, output);
    }

    auto dev = mgmt_dev->lookup_peer_dev();
    ret = pcidev::shutdown(mgmt_dev);
    if (ret) {
        std::cout << "Only proceed with SC update if all user applications for the target card(s) are stopped." << std::endl;
        return ret;
    }
    ret = writeSCImage(flasher, file);
    dev->sysfs_put("", "shutdown", errmsg, "0\n");
    if (!errmsg.empty()) {
        std::cout << "ERROR: online userpf failed. Please warm reboot." << std::endl;
        return ret;
    }

    int wait = 0;
    do {
        auto hdl =dev->open("", O_RDWR);
        if (hdl != -1) {
            dev->close(hdl);
            break;
        }
        sleep(1);
    } while (++wait < DEV_TIMEOUT);
    if (wait == DEV_TIMEOUT) {
        std::cout << "ERROR: user function does not back online. Please warm reboot." << std::endl;
    }

    return ret;
}

// Update shell on the board.
static int updateShell(unsigned index, std::string flashType,
    const char *primary, const char *secondary)
{
    std::shared_ptr<firmwareImage> pri;
    std::shared_ptr<firmwareImage> sec;
    std::shared_ptr<firmwareImage> stripped;

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

    stripped = std::make_shared<firmwareImage>(primary, STRIPPED_FIRMWARE);
    if (stripped->fail())
        stripped = nullptr;

    if (secondary != nullptr) {
        sec = std::make_shared<firmwareImage>(secondary,
            MCS_FIRMWARE_SECONDARY);
        if (sec->fail())
            sec = nullptr;
    }

    return flasher.upgradeFirmware(flashType, pri.get(), sec.get(),
        stripped.get());
}

// Reset shell to factory mode.
static int resetShell(unsigned index, bool force)
{
    Flasher flasher(index);
    if(!flasher.isValid())
        return -EINVAL;

    // Hack: u30 doesn't support factory reset yet
    // To be removed after in
    auto mgmt_dev = pcidev::get_dev(index, false);
    std::string errmsg, vbnv;
    mgmt_dev->sysfs_get( "rom", "VBNV", errmsg, vbnv );
    if (!errmsg.empty()) {
        std::cerr << errmsg << std::endl;
        return -EINVAL;
    }
    
    if (vbnv.find("_u30_") != std::string::npos) {
        std::cout << "Factory reset is not currently supported on U30.\n" << std::endl;
        return -ECANCELED;
    }

    std::cout << "CAUTION: Resetting Card [" << flasher.sGetDBDF() <<
        "] back to factory mode." << std::endl;
    if(!force && !canProceed())
        return -ECANCELED;

    return flasher.upgradeFirmware("", nullptr, nullptr, nullptr);
}

/*
 * bmcVer (shown as [SC=version]) can be 3 status:
 *   1) regular SC version;
 *        example: [SC=4.1.7]
 *   2) INACTIVE;
 *        exmaple: [SC=INACTIVE], this means no xmc subdev, we should not
 *        attemp to flash the SC;
 *   3) UNKNOWN;
 *        example: [SC=UNKNOWN], this means xmc subdev is online, but status in
 *        not normal, we still allow flashing SC.
 *   4) FIXED SC version;
 *        example: [SC=4.1.7(FIXED)], this means SC is running on slave mgmt pf
 *        and cannot be updated throught this pf, SC version cannot be changed.
 */
static void isSameShellOrSC(DSAInfo& candidate, DSAInfo& current,
    bool *same_dsa, bool *same_bmc)
{
    if (!current.name.empty()) {
        *same_dsa = ((candidate.name == current.name) &&
            candidate.matchId(current));
        *same_bmc = (current.bmcVerIsFixed() ||
            (current.bmcVer.compare(DSAInfo::INACTIVE) == 0) ||
            (candidate.bmcVer == current.bmcVer));
    }
}

static int updateShellAndSC(unsigned boardIdx, DSAInfo& candidate, bool& reboot)
{
    reboot = false;
    int ret = 0;

    Flasher flasher(boardIdx);
    if(!flasher.isValid()) {
        std::cout << "card not available" << std::endl;
        return -EINVAL;
    }

    bool same_dsa = false;
    bool same_bmc = false;
    DSAInfo current = flasher.getOnBoardDSA();
    isSameShellOrSC(candidate, current, &same_dsa, &same_bmc);

    // Always update Arista devices.
    if (candidate.vendor_id == ARISTA_ID)
        same_dsa = false;

    if (same_dsa && same_bmc)
        std::cout << "update not needed" << std::endl;

    if (!same_bmc) {
        std::cout << "Updating SC firmware on card[" << flasher.sGetDBDF() <<
            "]" << std::endl;
        ret = updateSC(boardIdx, candidate.file.c_str());
        if (ret != 0) {
            std::cout << "WARNING: Failed to update SC firmware on card ["
                << flasher.sGetDBDF() << "]" << std::endl;
        }
    }

    if (!same_dsa) {
        std::cout << "Updating shell on card[" << flasher.sGetDBDF() <<
            "]" << std::endl;
        ret = updateShell(boardIdx, "", candidate.file.c_str(),
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

    return ret;
}

static DSAInfo selectShell(unsigned idx, std::string& dsa, std::string& id, bool& multi_shell)
{
    unsigned candidateDSAIndex = UINT_MAX;

    Flasher flasher(idx);
    if(!flasher.isValid())
        return DSAInfo("");

    std::vector<DSAInfo> installedDSA = flasher.getInstalledDSA();

    // Find candidate DSA from installed DSA list.
    if (dsa.empty()) {
        std::cout << "Card [" << flasher.sGetDBDF() << "]: " << std::endl;
        if (installedDSA.empty()) {
            std::cout << "\t Status: no shell is installed" << std::endl;
            return DSAInfo("");
        }
        if (installedDSA.size() > 1) {
            std::cout << "\t Status: multiple shells are installed" << std::endl;
            multi_shell = true;
            return DSAInfo("");
        }
        candidateDSAIndex = 0;
    } else {
        for (unsigned int i = 0; i < installedDSA.size(); i++) {
            DSAInfo& idsa = installedDSA[i];
            if (dsa != idsa.name)
                continue;
            if (!id.empty() && !idsa.matchId(id))
                continue;
            if (candidateDSAIndex != UINT_MAX) {
                std::cout << "\t Status: multiple shells are installed" << std::endl;
                multi_shell = true;
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
    isSameShellOrSC(candidate, currentDSA, &same_dsa, &same_bmc);

    // Always update Arista devices.
    if (candidate.vendor_id == ARISTA_ID)
        same_dsa = false;

    if (same_dsa && same_bmc) {
        std::cout << "\t Status: shell is up-to-date" << std::endl;
        return DSAInfo("");
    }

    if (!same_bmc) {
        std::cout << "\t Status: SC needs updating" << std::endl;
        std::cout << "\t Current SC: " << currentDSA.bmcVer<< std::endl;
        std::cout << "\t SC to be flashed: " << candidate.bmcVer << std::endl;
    }
    if (!same_dsa) {
        std::cout << "\t Status: shell needs updating" << std::endl;
        std::cout << "\t Current shell: " << currentDSA.name << std::endl;
        std::cout << "\t Shell to be flashed: " << candidate.name << std::endl;
    }
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
                (id.empty() || dsa.matchId(id))) {
                if (!foundDSA)
                    foundDSA = true;
                else
                    multiDSA = true;
            }
        }
        if (!foundDSA) {
            std::cout << "Specified shell not found." << std::endl;
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

    bool has_multiple_shells = false;
    // Collect all indexes of boards need updating
    for (unsigned i : boardsToCheck) {
        DSAInfo dsa = selectShell(i, shell, id, has_multiple_shells);
        if (dsa.hasFlashImage)
            boardsToUpdate.push_back(std::make_pair(i, dsa));
    }

    // Continue to flash whatever we have collected in boardsToUpdate.
    unsigned success = 0;
    bool needreboot = false;
    if (!boardsToUpdate.empty()) {

        // Prompt user about what boards will be updated and ask for permission.
        if(!force && !canProceed())
            return -ECANCELED;

        // Perform DSA and BMC updating
        for (auto p : boardsToUpdate) {
            bool reboot;
            std::cout << std::endl;
            if (updateShellAndSC(p.first, p.second, reboot) == 0) {
                std::cout << "Successfully flashed Card[" << getBDF(p.first) << "]"<< std::endl;
                success++;
            }
            needreboot |= reboot;
        }
    }

    std::cout << std::endl;

    if (has_multiple_shells) {
        std::cout << "Card(s) can not be auto updated. \nPlease make sure only one shell is installed." <<std::endl;
        return 0;
    }

    if (boardsToUpdate.size() == 0) {
        std::cout << "Card(s) up-to-date and do not need to be flashed." << std::endl;
        return 0;
    }

    if (success != 0) {
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
int flashXbutilFlashHandler(int argc, char *argv[])
{
    if (argc < 2)
        return -EINVAL;

    sudoOrDie();

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
            if (std::string(optarg).find(":") == std::string::npos) {
                std::cout <<
                    "Please use -d <mgmt-BDF> to specify the device to flash"
                    << std::endl;
                std::cout << "Run xbmgmt scan to find mgmt BDF"
                    << std::endl;
            } else {
                devIdx = bdf2index(optarg);
            }
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
        int ret = resetShell(devIdx == UINT_MAX ? 0 : devIdx, force);
        if (ret)
            return ret;
        std::cout << "Shell is reset successfully" << std::endl;
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
        std::cout << "Shell is updated successfully" << std::endl;
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
        { nullptr, 0, nullptr, 0 },
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
        { nullptr, 0, nullptr, 0 },
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
    std::string type;
    std::string primary_file;
    std::string secondary_file;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "path", required_argument, nullptr, '1' },
        { "primary", required_argument, nullptr, '2' },
        { "secondary", required_argument, nullptr, '3' },
        { "flash_type", required_argument, nullptr, '4' },
        { nullptr, 0, nullptr, 0 },
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
        case '2':
            primary_file = std::string(optarg);
            break;
        case '3':
            secondary_file = std::string(optarg);
            break;
        case '4':
            type = std::string(optarg);
            break;
        default:
            return -EINVAL;
        }
    }

    // one of the --primary/--path switch has to be provided.
    // Throw an error if no switch is provided
    if ( primary_file.empty() ) {
        std::cout << "--primary/--path switch is not provided." << std::endl;
        return -EINVAL;
    }

    // Throw an error if index is not provided
    if (index == UINT_MAX)
    {
        std::cout << "--card switch is not provided." << std::endl;
        return -EINVAL;
    }

    const char* secondary = secondary_file.empty() ? nullptr : secondary_file.c_str() ;

    int ret = updateShell(index, type, primary_file.c_str(), secondary);
    if (ret)
        return ret;

    std::cout << "Shell is updated successfully" << std::endl;
    std::cout << "Cold reboot machine to load new shell on card" << std::endl;
    return 0;
}

static int sc(int argc, char *argv[])
{
    unsigned index = UINT_MAX;
    std::string file;
    bool cardlevel = true;
    bool force = false;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "path", required_argument, nullptr, '1' },
        { "no_cardlevel", no_argument, nullptr, '2' },
        { "force", no_argument, nullptr, '3' },
        { nullptr, 0, nullptr, 0 },
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
            cardlevel = false;
            break;
        case '3':
            force = true;
            break;
        default:
            return -EINVAL;
        }
    }

    if (file.empty() || index == UINT_MAX)
        return -EINVAL;

    int ret = updateSC(index, file.c_str(), cardlevel, force);
    if (ret)
        return ret;

    std::cout << "SC firmware is updated successfully" << std::endl;
    return 0;
}

static int reset(int argc, char *argv[])
{
    unsigned index = UINT_MAX;
    bool force = false;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "force", no_argument, nullptr, '1' },
        { nullptr, 0, nullptr, 0 },
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
            force = true;
            break;
        default:
            return -EINVAL;
        }
    }

    int ret = resetShell(index == UINT_MAX ? 0 : index, force);
    if (ret)
        return ret;

    std::cout << "Shell is reset successfully" << std::endl;
    std::cout << "Cold reboot machine to load new shell on card" << std::endl;
    return 0;
}

static int file(int argc, char *argv[])
{
    unsigned index = UINT_MAX;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "input", required_argument, nullptr, '1' },
        { "output", required_argument, nullptr, '2' },
        { nullptr, 0, nullptr, 0 },
    };
    char *input_path = nullptr;
    char *output_path = nullptr;

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
            input_path = optarg;
            break;
        case '2':
            output_path = optarg;
            break;
        default:
            return -EINVAL;
        }
    }

    if ((input_path == nullptr) == (output_path == nullptr)) {
        std::cout << "Specify input or output file path" << std::endl;
        return -EINVAL;
    }

    Flasher flasher(index);
    if(!flasher.isValid())
        return -EINVAL;

    if (input_path) {
        std::ifstream ifs(input_path, std::ifstream::binary);
        if (!ifs.good()) {
            std::cout << "invalid input path: " << input_path << std::endl;
            return -EINVAL;
        }
        ifs.seekg(0, ifs.end);
        size_t len = ifs.tellg();
        ifs.seekg(0, ifs.beg);
        std::vector<unsigned char> data(len);
        ifs.read(reinterpret_cast<char *>(data.data()), len);
        return flasher.writeData(data);
    }

    if (output_path) {
        std::ofstream ofs(output_path,
            std::ofstream::binary | std::ofstream::trunc);
        if (!ofs.good()) {
            std::cout << "invalid output path: " << output_path << std::endl;
            return -EINVAL;
        }
        std::vector<unsigned char> data;
        int ret = flasher.readData(data);
        if (ret) {
            std::cout << "failed to read data from flash: " << std::endl;
            return ret;
        }
        ofs.write(reinterpret_cast<char *>(data.data()), data.size());
    }

    return 0;
}

static const std::map<std::string, std::function<int(int, char **)>> optList = {
    { "--scan", scan },
    { "--update", update },
    { "--shell", shell },
    { "--sc_firmware", sc },
    { "--factory_reset", reset },
    { "--file", file },
};

int flashHandler(int argc, char *argv[])
{
    if (argc < 2)
        return -EINVAL;

    sudoOrDie();

    std::string subcmd(argv[1]);

    auto cmd = optList.find(subcmd);
    if (cmd == optList.end())
        return -EINVAL;

    argc--;
    argv++;
    return cmd->second(argc, argv);
}
