/**
 * Copyright (C) 2020 Xilinx, Inc
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

//
// This utility is implemented by porting flash code from xbmgmt. Since that
// it's used only by non-XRT users, we do not expect to maintain and enhance
// this code a lot in the future. Hence, no cleanup effort is ever attempted
// while porting the code from xbmgmt at this point.
// If it works, don't change it.
//

#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <libgen.h>
#include <string.h>

#include "pcidev.h"
#include "xspi.h"
#include "xqspips.h"
#include "firmware_image.h"

const option flash_opts[] = {
    // Key option to identify flash operation, must be 0
    { "primary", required_argument, nullptr, '0' },

    { "force", no_argument, nullptr, '1' },
    { "card", required_argument, nullptr, '2' },
    { "secondary", required_argument, nullptr, '3' },
    { "bar", required_argument, nullptr, '4' },
    { "bar-offset", required_argument, nullptr, '5' },
    { nullptr, 0, nullptr, 0 },
};

const option reset_opts[] = {
    // Key option to identify reset operation, must be 0
    { "factory-reset", no_argument, nullptr, '0' },

    { "force", no_argument, nullptr, '1' },
    { "card", required_argument, nullptr, '2' },
    { "dual-flash", no_argument, nullptr, '3' },
    { "bar", required_argument, nullptr, '4' },
    { "bar-offset", required_argument, nullptr, '5' },
    { nullptr, 0, nullptr, 0 },
};

const option qspips_erase_opts[] = {
    // Key option to identify flash operation, must be 0
    { "qspips-erase", no_argument, nullptr, '0' },

    { "card", required_argument, nullptr, '1' },
    { "offset", required_argument, nullptr, '2' },
    { "length", required_argument, nullptr, '3' },
    { "flash-type", required_argument, nullptr, '4' },
    { "bar", required_argument, nullptr, '5' },
    { "bar-offset", required_argument, nullptr, '6' },
    { "force", no_argument, nullptr, '7' },
    { nullptr, 0, nullptr, 0 },
};

const option qspips_flash_opts[] = {
    // Key option to identify flash operation, must be 0
    { "qspips-flash", no_argument, nullptr, '0' },

    { "card", required_argument, nullptr, '1' },
    { "input", required_argument, nullptr, '2' },
    { "offset", required_argument, nullptr, '3' },
    { "flash-type", required_argument, nullptr, '4' },
    { "bar", required_argument, nullptr, '5' },
    { "bar-offset", required_argument, nullptr, '6' },
    { "force", no_argument, nullptr, '7' },
    { nullptr, 0, nullptr, 0 },
};

const option qspips_readback_opts[] = {
    // Key option to identify flash operation, must be 0
    { "qspips-read", no_argument, nullptr, '0' },

    { "card", required_argument, nullptr, '1' },
    { "output", required_argument, nullptr, '2' },
    { "offset", required_argument, nullptr, '3' },
    { "length", required_argument, nullptr, '4' },
    { "flash-type", required_argument, nullptr, '5' },
    { "bar", required_argument, nullptr, '6' },
    { "bar-offset", required_argument, nullptr, '7' },
    { nullptr, 0, nullptr, 0 },
};

static const char *option_key(const option *opts)
{
    while (opts->name != nullptr) {
        if (opts->val == '0')
            return opts->name;
        opts++;
    }
    return nullptr;
}

static bool is_op(const option *opts, int argc, char *argv[])
{
    const char *key = option_key(opts);
    std::string optkey = "--";

    if (key == nullptr)
        return false;

    optkey += key;
    for (int i = 0; i < argc; i++) {
        if (optkey.compare(argv[i]) == 0)
            return true;
    }
    return false;
}

static void sudoOrDie()
{
    const char* SudoMessage = "ERROR: root privileges required.";
    if ((getuid() == 0) || (geteuid() == 0))
        return;
    std::cout << SudoMessage << std::endl;
    exit(-EPERM);
}

static bool canProceed()
{
    std::string input;
    bool answered = false;
    bool proceed = false;

    while (!answered) {
        std::cout << "Are you sure you wish to proceed? [y/n]: ";
        std::cin >> input;
        answered = (input.compare("y") == 0 || input.compare("n") == 0);
    }

    proceed = (input.compare("y") == 0);
    if (!proceed)
        std::cout << "Action canceled." << std::endl;
    return proceed;
}

static void printHelp(const char *fname)
{
    char *tmp = strdup(fname);

    std::cout << "Usage: " << std::endl;

    std::cout << basename(tmp) << " [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;

    std::cout << "\n\"SPI flash\"\n";
    std::cout << "  --primary, <MCS-path>, must be 1st option\n";
    std::cout << "  [--secondary <MCS-path>], default is empty\n";
    std::cout << "  --card <BDF>\n";
    std::cout << "  [--force, yes for prompt]\n";
    std::cout << "  [--bar <BAR-index-for-QSPI>], default is 0\n";
    std::cout << "  [--bar-offset <BAR-offset-for-QSPI>], default is 0x40000\n";

    std::cout << "\n\"SPI factory-reset\"\n";
    std::cout << "  --factory-reset, must be 1st option\n";
    std::cout << "  [--dual-flash]\n";
    std::cout << "  --card <BDF>\n";
    std::cout << "  [--force, yes for prompt]\n";
    std::cout << "  [--bar <BAR-index-for-QSPI>], default is 0\n";
    std::cout << "  [--bar-offset <BAR-offset-for-QSPI>], default is 0x40000\n";

    std::cout << "\n\"QSPIPS erase\"\n";
    std::cout << "  --qspips-erase, must be 1st option\n";
    std::cout << "  --card <BDF>\n";
    std::cout << "  [--offset <offset-on-flash-to-start-with>], default is 0\n";
    std::cout << "  [--length <length-to-read>], default is 96MB\n";
    std::cout << "  [--flash-type <qspips-flash-type>], default is qspi_ps_x2_single\n";
    std::cout << "  [--bar <BAR-index-for-QSPIPS>], default is 0\n";
    std::cout << "  [--bar-offset <BAR-offset-for-QSPIPS>], default is 0x40000\n";
    std::cout << "  [--force, yes for prompt]\n";

    std::cout << "\n\"QSPIPS flash\"\n";
    std::cout << "  --qspips-flash, must be 1st option\n";
    std::cout << "  --input <path-to-qspips-BOOT-BIN-file>\n";
    std::cout << "  --card <BDF>\n";
    std::cout << "  [--offset <offset-on-flash-to-start-with>], default is 0\n";
    std::cout << "  [--flash-type <qspips-flash-type>], default is qspi_ps_x2_single\n";
    std::cout << "  [--bar <BAR-index-for-QSPIPS>], default is 0\n";
    std::cout << "  [--bar-offset <BAR-offset-for-QSPIPS>], default is 0x40000\n";
    std::cout << "  [--force, yes for prompt]\n";

    std::cout << "\n\"QSPIPS read back\"\n";
    std::cout << "  --qspips-read, must be 1st option\n";
    std::cout << "  --output <output-file-to-save-read-contents>\n";
    std::cout << "  --card <BDF>\n";
    std::cout << "  [--offset <offset-on-flash-to-start-with>], default is 0\n";
    std::cout << "  [--length <length-to-read>], default is 128MB\n";
    std::cout << "  [--flash-type <qspips-flash-type>], default is qspi_ps_x2_single\n";
    std::cout << "  [--bar <BAR-index-for-QSPIPS>], default is 0\n";
    std::cout << "  [--bar-offset <BAR-offset-for-QSPIPS>], default is 0x40000\n";
}

int reset(int argc, char *argv[])
{
    bool force = false;
    std::string bdf;
    const char *fname = argv[0];
    int bar = 0;
    size_t baroff = INVALID_OFFSET;
    bool dualflash = false;

    sudoOrDie();

    while (true) {
        const auto opt = getopt_long(argc, argv, "", reset_opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '2':
            bdf = std::string(optarg);
            break;
        case '1':
            force = true;
            break;
        case '0':
            break;
        case '3':
            dualflash = true;
            break;
        case '4':
            bar = std::stoi(optarg, nullptr, 0);
            break;
        case '5':
            baroff = std::stoi(optarg, nullptr, 0);
            break;
        default:
            printHelp(fname);
            return -EINVAL;
        }
    }
    if (bdf.empty()) {
            printHelp(fname);
            return -EINVAL;
    }

    std::cout
        << "About to revert to golden image for card " << bdf << std::endl;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff);
    XSPI_Flasher xspi(&dev, dualflash);
    return xspi.revertToMFG();
}

int flash(int argc, char *argv[])
{
    int ret = 0;
    bool force = false;
    std::string primary_file;
    std::string secondary_file;
    std::string bdf;
    const char *fname = argv[0];
    int bar = 0;
    size_t baroff = INVALID_OFFSET;

    sudoOrDie();

    while (true) {
        const auto opt = getopt_long(argc, argv, "", flash_opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '2':
            bdf = std::string(optarg);
            break;
        case '1':
            force = true;
            break;
        case '0':
            primary_file = std::string(optarg);
            break;
        case '3':
            secondary_file = std::string(optarg);
            break;
        case '4':
            bar = std::stoi(optarg);
            break;
        case '5':
            baroff = std::stoi(optarg, nullptr, 0);
            break;
        default:
            printHelp(fname);
            return -EINVAL;
        }
    }
    if (bdf.empty() || primary_file.empty()) {
            printHelp(fname);
            return -EINVAL;
    }

    std::cout
        << "About to flash below MCS bitstream onto card "
        << bdf << ":" << std::endl;
    std::cout << primary_file << std::endl;
    if (!secondary_file.empty())
        std::cout << secondary_file << std::endl;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff);
    XSPI_Flasher xspi(&dev, !secondary_file.empty());

    if (secondary_file.empty()) {
        firmwareImage pri(primary_file.c_str());
        if (pri.fail())
            return -EINVAL;
        ret = xspi.xclUpgradeFirmware1(pri);
    } else {
        firmwareImage pri(primary_file.c_str());
        firmwareImage sec(secondary_file.c_str());
        if (pri.fail() || sec.fail())
            return -EINVAL;
        ret = xspi.xclUpgradeFirmware2(pri, sec);
    }

    return ret;
}

int qspips_erase(int argc, char *argv[])
{
    std::string bdf;
    const char *fname = argv[0];
    int bar = 0;
    size_t baroff = INVALID_OFFSET;
    std::string flash_type;
    size_t offset = 0, len = GOLDEN_BASE;
    bool force = false;

    sudoOrDie();

    while (true) {
        const auto opt = getopt_long(argc, argv, "", qspips_erase_opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            break;
        case '1':
            bdf = std::string(optarg);
            break;
        case '2':
            offset = std::stoi(optarg, nullptr, 0);
            break;
        case '3':
            len = std::stoi(optarg, nullptr, 0);
            break;
        case '4':
            flash_type = std::string(optarg);
            break;
        case '5':
            bar = std::stoi(optarg);
            break;
        case '6':
            baroff = std::stoi(optarg, nullptr, 0);
            break;
        case '7':
            force = true;
            break;
        default:
            printHelp(fname);
            return -EINVAL;
        }
    }
    if (bdf.empty()) {
            printHelp(fname);
            return -EINVAL;
    }

    std::cout << "About to erase flash"
        << " [0x" << std::hex << offset << ",0x" << offset+len 
        << "] on card " << bdf << std::endl;

    if (offset + len > GOLDEN_BASE)
        std::cout << "\nThis might erase golden image if there is !!\n" << std::endl;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclErase(offset, len);
}

int qspips_flash(int argc, char *argv[])
{
    std::string bdf;
    const char *fname = argv[0];
    int bar = 0;
    size_t baroff = INVALID_OFFSET;
    size_t offset = 0;
    std::string flash_type;
    std::string bin_file;
    bool force = false;

    sudoOrDie();

    while (true) {
        const auto opt = getopt_long(argc, argv, "", qspips_flash_opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            break;
        case '1':
            bdf = std::string(optarg);
            break;
        case '2':    
            bin_file = std::string(optarg);
            break;
        case '3':
            offset = std::stoi(optarg, nullptr, 0);
            break;
        case '4':
            flash_type = std::string(optarg);
            break;
        case '5':
            bar = std::stoi(optarg);
            break;
        case '6':
            baroff = std::stoi(optarg, nullptr, 0);
            break;
        case '7':
            force = true;
            break;
        default:
            printHelp(fname);
            return -EINVAL;
        }
    }
    if (bdf.empty() || bin_file.empty()) {
            printHelp(fname);
            return -EINVAL;
    }

    firmwareImage bin(bin_file.c_str());
    if (bin.fail())
            return -EINVAL;
    std::cout
        << "About to program flash on card " << bdf << " at offset 0x" << std::hex << offset << std::dec << std::endl;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclUpgradeFirmware(bin, offset);
}

int qspips_readback(int argc, char *argv[])
{
    std::string bdf;
    const char *fname = argv[0];
    int bar = 0;
    size_t baroff = INVALID_OFFSET;
    size_t offset = 0, len = FLASH_SIZE;
    std::string flash_type;
    std::string output;

    sudoOrDie();

    while (true) {
        const auto opt = getopt_long(argc, argv, "", qspips_readback_opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            break;
        case '1':
            bdf = std::string(optarg);
            break;
        case '2':
            output = std::string(optarg);
            break;
        case '3':
            offset = std::stoi(optarg, nullptr, 0);
            break;
        case '4':
            len = std::stoi(optarg, nullptr, 0);
            break;
        case '5':
            flash_type = std::string(optarg);
            break;
        case '6':
            bar = std::stoi(optarg);
            break;
        case '7':
            baroff = std::stoi(optarg, nullptr, 0);
            break;
        default:
            printHelp(fname);
            return -EINVAL;
        }
    }
    if (bdf.empty() || output.empty()) {
            printHelp(fname);
            return -EINVAL;
    }

    std::cout << "Read out flash"
        << " [0x" << std::hex << offset << ",0x" << offset+len 
        << "] on card " << bdf << " to " << output << std::dec << std::endl;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclReadBack(output, offset, len);
}

int main(int argc, char *argv[])
{
    try {
        if (is_op(reset_opts, argc, argv))
            return reset(argc, argv);

        if (is_op(flash_opts, argc, argv))
            return flash(argc, argv);

        if (is_op(qspips_erase_opts, argc, argv))
            return qspips_erase(argc, argv);

        if (is_op(qspips_flash_opts, argc, argv))
            return qspips_flash(argc, argv);

        if (is_op(qspips_readback_opts, argc, argv))
            return qspips_readback(argc, argv);

    } catch (const std::exception& ex) {
        std::cout << "Failed to flash: " << ex.what() << std::endl;
        return -EINVAL;
    }

    printHelp(argv[0]);
    return -EINVAL;
}
