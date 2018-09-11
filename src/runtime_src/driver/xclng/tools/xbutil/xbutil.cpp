/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author: Hem C Neema, Ryan Radjabi
 * Simple command line utility to inetract with SDX PCIe devices
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
#include <chrono>
#include <curses.h>
#include <sstream>
#include <climits>
#include <algorithm>

#include "xbutil.h"
#include "shim.h"

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

    xcldev::pci_device_scanner devScanner;
    try {
        devScanner.scan(false);
    } catch (...) {
        std::cout << "ERROR: failed to scan device" << std::endl;
        return -EINVAL;
    }

    for (unsigned i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++) {
        auto& dev = xcldev::pci_device_scanner::device_list[i];
        if (dom == dev.domain && b == dev.bus && d == dev.device &&
            (f == 0 || f == 1)) {
            index = i;
            return 0;
        }
    }

    std::cout << "ERROR: no device found for " << bdfStr << std::endl;
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
            std::cout << "ERROR: " << devStr << " is not a valid board index."
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

int main(int argc, char *argv[])
{
    unsigned index = 0xffffffff;
    unsigned regionIndex = 0xffffffff;
    unsigned computeIndex = 0xffffffff;
    unsigned short targetFreq[2] = {0, 0};
    unsigned fanSpeed = 0;
    unsigned long long startAddr = 0;
    unsigned int pattern_byte = 'J';//Rather than zero; writing char 'J' by default
    size_t sizeInBytes = 0;
    std::string outMemReadFile = "memread.out";
    std::string flashType = ""; // unset and empty by default
    std::string mcsFile1, mcsFile2;
    std::string xclbin;
    size_t blockSize = 0;
    bool hot = false;
    int c;
    dd::ddArgs_t ddArgs;

    const char* exe = argv[ 0 ];
    if (argc == 1) {
        xcldev::printHelp(exe);
        return 1;
    }

    /*
     * Call xbflash if first argument is "flash". This calls
     * xbflash and never returns. All arguments will be passed
     * down to xbflash.
     */
    if( std::string( argv[ 1 ] ).compare( "flash" ) == 0 ) {
        // get self path, launch xbflash from self path
        char buf[ PATH_MAX ] = {0};
        auto len = readlink( "/proc/self/exe", buf, PATH_MAX );
        if( len == -1 ) {
            perror( "readlink:" );
            return errno;
        }
        buf[ len - 1 ] = 0; // null terminate after successful read

        // remove exe name from this to get the parent path
        size_t found = std::string( buf ).find_last_of( "/\\" ); // finds the last backslash char
        std::string path = std::string( buf ).substr( 0, found );
        // coverity[TAINTED_STRING] argv will be validated inside xbflash
        return execv( std::string( path + "/xbflash" ).c_str(), argv );
    } /* end of call to xbflash */

    if( std::string( argv[ 1 ] ).compare( "top" ) == 0 ) {
        optind++;
        return xcldev::xclTop(argc, argv);
    }
    if( std::string( argv[ 1 ] ).compare( "validate" ) == 0 ) {
        optind++;
        return xcldev::xclValidate(argc, argv);
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

    argv[0] = const_cast<char *>(exe);
    static struct option long_options[] = {
	{"read", no_argument, 0, xcldev::MEM_READ},
	{"write", no_argument, 0, xcldev::MEM_WRITE},
	{"spm", no_argument, 0, xcldev::STATUS_SPM},
	{"lapc", no_argument, 0, xcldev::STATUS_LAPC},
	{"tracefunnel", no_argument, 0, xcldev::STATUS_UNSUPPORTED},
	{"monitorfifolite", no_argument, 0, xcldev::STATUS_UNSUPPORTED},
	{"monitorfifofull", no_argument, 0, xcldev::STATUS_UNSUPPORTED},
	{"accelmonitor", no_argument, 0, xcldev::STATUS_UNSUPPORTED}
    };
    int long_index;
    const char* short_options = "a:b:c:d:e:f:g:hi:m:n:o:p:r:s:"; //don't add numbers
    while ((c = getopt_long(argc, argv, short_options, long_options, &long_index)) != -1)
    {
        if (cmd == xcldev::LIST) {
            std::cout << "ERROR: 'list' command does not accept any options\n";
            return -1;
        }
        switch (c)
        {
        //Deal with long options. Long options return the value set in option::val
        case xcldev::MEM_READ : {
            //--read
            if (cmd != xcldev::MEM) {
                std::cout << "ERROR: Option '" << long_options[long_index].name << "' cannot be used with command " << cmdname << "\n";
                return -1;
            }
            subcmd = xcldev::MEM_READ;
            break;
        }
        case xcldev::MEM_WRITE : {
            //--write
            if (cmd != xcldev::MEM) {
                std::cout << "ERROR: Option '" << long_options[long_index].name << "' cannot be used with command " << cmdname << "\n";
                return -1;
            }
            subcmd = xcldev::MEM_WRITE;
            break;
        }
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
        case xcldev::STATUS_UNSUPPORTED : {
            //Don't give ERROR for as yet unsupported IPs
            std::cout << "INFO: No Status information available for IP: " << long_options[long_index].name << "\n";
            return 0;
        }
            //short options are dealt here
        case 'a':{
            if (cmd != xcldev::MEM) {
                std::cout << "ERROR: '-a' not applicable for this command\n";
                return -1;
            }
            size_t idx = 0;
            try {
                startAddr = std::stoll(optarg, &idx, 0);
            }
            catch (const std::exception& ex) {
                //out of range, invalid argument ex
                std::cout << "ERROR: Value supplied to -" << (char)c << " option is invalid\n";
                return -1;
            }
            if (idx < strlen(optarg)) {
                std::cout << "ERROR: Value supplied to -" << (char)c << " option is invalid\n";
                return -1;
            }
            break;
        }
        case 'o': {
            if (cmd == xcldev::FLASH) {
                flashType = optarg;
                break;
            } else if (cmd != xcldev::MEM || subcmd != xcldev::MEM_READ) {
                std::cout << "ERROR: '-o' not applicable for this command\n";
                return -1;
            }
            outMemReadFile = optarg;
            break;
        }
        case 'e': {
            if (cmd != xcldev::MEM || subcmd != xcldev::MEM_WRITE) {
                std::cout << "ERROR: '-e' not applicable for this command\n";
                return -1;
            }
            size_t idx = 0;
            try {
                pattern_byte = std::stoi(optarg, &idx, 0);
            }
            catch (const std::exception& ex) {
                //out of range, invalid argument ex
                std::cout << "ERROR: Value supplied to -" << (char)c << " option must be a value between 0 and 255\n";
                return -1;
            }
            if (pattern_byte > 0xff || idx < strlen(optarg)) {
                std::cout << "ERROR: Value supplied to -" << (char)c << " option must be a value between 0 and 255\n";
                return -1;
            }
            break;
        }
        case 'i': {
            if (cmd != xcldev::MEM) {
                std::cout << "ERROR: '-i' not applicable for this command\n";
                return -1;
            }
            size_t idx = 0;
            try {
                sizeInBytes = std::stoll(optarg, &idx, 0);
            }
            catch (const std::exception& ex) {
                //out of range, invalid argument ex
                std::cout << "ERROR: Value supplied to -" << (char)c << " option is invalid\n";
                return -1;
            }
            if (idx < strlen(optarg)) {
                std::cout << "ERROR: Value supplied to -" << (char)c << " option is invalid\n";
                return -1;
            }
            break;
        }
        case 'd': {
            int ret = str2index(optarg, index);
            if (ret != 0)
                return ret;
            if (cmd == xcldev::DD) {
                ddArgs = dd::parse_dd_options( argc, argv );
            }
            break;
        }
        case 'r':
            if ((cmd == xcldev::FLASH) || (cmd == xcldev::BOOT) || (cmd == xcldev::DMATEST) ||(cmd == xcldev::STATUS)) {
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
        case 'm':
            if (cmd != xcldev::FLASH) {
                std::cout << "ERROR: '-m' only allowed with 'flash' command\n";
                return -1;
            }
            mcsFile1 = optarg;
            break;
        case 'n':
            if (cmd != xcldev::FLASH) {
                std::cout << "ERROR: '-n' only allowed with 'flash' command\n";
                return -1;
            }
            mcsFile2 = optarg;
            break;
        case 'c':
            if (cmd != xcldev::RUN) {
                std::cout << "ERROR: '-c' only allowed with 'run' command\n";
                return -1;
            }
            computeIndex = std::atoi(optarg);
            break;
        case 's':
            if (cmd != xcldev::FAN) {
                std::cout << "ERROR: '-s' only allowed with 'fan' command\n";
                return -1;
            }
            fanSpeed = std::atoi(optarg);
            break;
        case 'b':
        {
            if (cmd != xcldev::DMATEST) {
                std::cout << "ERROR: '-b' only allowed with 'dmatest' command\n";
                return -1;
            }
            std::string tmp(optarg);
            if ((tmp[0] == '0') && (std::tolower(tmp[1]) == 'x')) {
                blockSize = std::stoll(tmp, 0, 16);
            }
            else {
                blockSize = std::stoll(tmp, 0, 10);
            }

            if (blockSize & (blockSize - 1)) {
                std::cout << "ERROR: block size should be power of 2\n";
                return -1;
            }

            if (blockSize > 0x100000) {
                std::cout << "ERROR: block size cannot be greater than 0x100000 MB\n";
                return -1;
            }
            blockSize *= 1024; // convert kilo bytes to bytes
            break;
        }
        case 'h':
        {
            if (cmd != xcldev::RESET) {
                std::cout << "ERROR: '-h' only allowed with 'reset' command\n";
                return -1;
            }
            hot = true;
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
    case xcldev::RUN:
    case xcldev::FAN:
    case xcldev::DMATEST:
    case xcldev::MEM:
    case xcldev::QUERY:
    case xcldev::SCAN:
    case xcldev::STATUS:
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

    if (cmd == xcldev::SCAN) {
        xcldev::pci_device_scanner devScanner;
        try
        {
            return devScanner.scan(true);
        }
        catch (...)
        {
            std::cout << "ERROR: scan failed" << std::endl;
            return -1;
        }
    }

    std::vector<std::unique_ptr<xcldev::device>> deviceVec;

    try {
        unsigned int count = xclProbe();
        if (count == 0) {
            std::cout << "ERROR: No device found\n";
            return 1;
        }

        for (unsigned i = 0; i < count; i++) {
            deviceVec.emplace_back(new xcldev::device(i, nullptr));
        }
    }
    catch (const std::exception& ex) {
        std::cout << ex.what() << std::endl;
        return 1;
    }

    std::cout << "INFO: Found " << deviceVec.size() << " device(s)\n";

    if (cmd == xcldev::LIST) {
        for (unsigned i = 0; i < deviceVec.size(); i++) {
            std::cout << '[' << i << "] "
                << std::setw(2) << std::setfill('0') << deviceVec[i]->mBus << ":"
                << std::setw(2) << std::setfill('0') << deviceVec[i]->mDev << "."
                << deviceVec[i]->mUserFunc << " "
                << deviceVec[i]->name() << std::endl;
        }
        return 0;
    }

    if (index >= deviceVec.size()) {
        std::cout << "ERROR: Device index " << index << " out of range\n";
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
    case xcldev::FAN:
        result = deviceVec[index]->fan(fanSpeed);
        break;
    case xcldev::FLASH:
        result = deviceVec[index]->flash(mcsFile1, mcsFile2, flashType);
        break;
    case xcldev::PROGRAM:
        result = deviceVec[index]->program(xclbin, regionIndex);
        break;
    case xcldev::QUERY:
        try
        {
            result = deviceVec[index]->dump(std::cout);
        }
        catch (...)
        {
            std::cout << "ERROR: query failed" << std::endl;
        }
        break;
    case xcldev::RESET:
        if (hot) regionIndex = 0xffffffff;
        result = deviceVec[index]->reset(regionIndex);
        break;
    case xcldev::RUN:
        result = deviceVec[index]->run(regionIndex, computeIndex);
        break;
    case xcldev::DMATEST:
        result = deviceVec[index]->dmatest(blockSize, true);
        break;
    case xcldev::MEM:
        if (subcmd == xcldev::MEM_READ) {
            result = deviceVec[index]->memread(outMemReadFile, startAddr, sizeInBytes);
        }
        else if (subcmd == xcldev::MEM_WRITE) {
            result = deviceVec[index]->memwrite(startAddr, sizeInBytes, pattern_byte);
        }
        break;
    case xcldev::DD:
        result = deviceVec[index]->do_dd( ddArgs );
        break;
    case xcldev::STATUS:
        if (ipmask == xcldev::STATUS_NONE_MASK) {
            //if no ip specified then read all
            //ipmask = static_cast<unsigned int>(xcldev::STATUS_SPM_MASK);
            //if (!(getuid() && geteuid())) {
            //  ipmask |= static_cast<unsigned int>(xcldev::STATUS_LAPC_MASK);
            //}
            result = deviceVec[index]->print_debug_ip_list(0);
        }
        if (ipmask & static_cast<unsigned int>(xcldev::STATUS_LAPC_MASK)) {
            result = deviceVec[index]->readLAPCheckers(1);
        }
        if (ipmask & static_cast<unsigned int>(xcldev::STATUS_SPM_MASK)) {
            result = deviceVec[index]->readSPMCounters();
        }
        break;
    default:
        std::cout << "ERROR: Not implemented\n";
        result = -1;
    }

    if(result == 0) {
        std::cout << "INFO: xbutil " << v->first << " succeeded." << std::endl;
    } else {
        std::cout << "ERROR: xbutil " << v->first  << " failed." << std::endl;
    }

    return result;
}

void xcldev::printHelp(const std::string& exe)
{
    std::cout << "Running xbutil for 4.0+ DSA's \n\n";
    std::cout << "Usage: " << exe << " <command> [options]\n\n";
    std::cout << "Command and option summary:\n";
    std::cout << "  clock   [-d device] [-r region] [-f clock1_freq_MHz] [-g clock2_freq_MHz]\n";
    std::cout << "  dmatest [-d device] [-b [0x]block_size_KB]\n";
    std::cout << "  help\n";
    std::cout << "  list\n";
    std::cout << "  mem     --read [-d device] [-a [0x]start_addr] [-i size_bytes] [-o output filename]\n";
    std::cout << "  mem     --write [-d device] [-a [0x]start_addr] [-i size_bytes] [-e pattern_byte]\n";
    std::cout << "  program [-d device] [-r region] -p xclbin\n";
    std::cout << "  query   [-d device [-r region]]\n";
    std::cout << "  reset   [-d device] [-h | -r region]\n";
    std::cout << "  status  [--debug_ip_name]\n";   
    std::cout << "  scan\n";
    std::cout << "  top [-i seconds]\n";
    std::cout << "  validate [-d device]\n";
    std::cout << " Requires root privileges:\n";
    std::cout << "  boot    [-d device]\n";
    std::cout << "  flash   [-d device] -m primary_mcs [-n secondary_mcs] [-o bpi|spi]\n";
    std::cout << "  flash   [-d device] -a <all | dsa> [-t timestamp]\n";
    std::cout << "  flash   [-d device] -p msp432_firmware\n";
    std::cout << "  flash   scan [-v]\n";
    std::cout << "\nExamples:\n";
    std::cout << "List all devices\n";
    std::cout << "  " << exe << " list\n";
    std::cout << "Scan for Xilinx PCIe device(s) & associated drivers (if any) and relevant system information\n";
    std::cout << "  " << exe << " scan\n";
    std::cout << "Boot device 1 from PROM and retrain the PCIe link without rebooting the host\n";
    std::cout << "  " << exe << " boot -d 1\n";
    std::cout << "Change the clock frequency of region 0 in device 0 to 100 MHz\n";
    std::cout << "  " << exe << " clock -f 100\n";
    std::cout << "For device 0 which supports multiple clocks, change the clock 1 to 200MHz and clock 2 to 250MHz\n";
    std::cout << "  " << exe << " clock -f 200 -g 250\n";
    std::cout << "Download the accelerator program for device 2\n";
    std::cout << "  " << exe << " program -d 2 -p a.xclbin\n";
    std::cout << "Run DMA test on device 1 with 32 KB blocks of buffer\n";
    std::cout << "  " << exe << " dmatest -d 1 -b 0x2000\n";
    std::cout << "Read 256 bytes from DDR starting at 0x1000 into file read.out\n";
    std::cout << "  " << exe << " mem --read -a 0x1000 -i 256 -o read.out\n";
    std::cout << "  " << "Default values for address is 0x0, size is DDR size and file is memread.out\n";
    std::cout << "Write 256 bytes to DDR starting at 0x1000 with byte 0xaa \n";
    std::cout << "  " << exe << " mem --write -a 0x1000 -i 256 -e 0xaa\n";
    std::cout << "  " << "Default values for address is 0x0, size is DDR size and pattern is 0x0\n";
    std::cout << "List the debug IPs available on the platform\n";
    std::cout << "  " << exe << " status \n";
    std::cout << "Flash all installed DSA for all boards, if not done\n";
    std::cout << "  sudo " << exe << " flash -a all\n";
    std::cout << "Show DSA related information for all boards in the system\n";
    std::cout << "  sudo " << exe << " flash scan\n";
    std::cout << "Validate installation on device 1\n";
    std::cout << "  " << exe << " validate -d 1\n";
}

std::unique_ptr<xcldev::device> xcldev::xclGetDevice(unsigned index)
{
    try {
        unsigned int count = xclProbe();
        if (count == 0) {
            std::cout << "ERROR: No devices found" << std::endl;
        } else if (index >= count) {
            std::cout << "ERROR: Device index " << index << " out of range";
            std::cout << std::endl;
        } else {
            return std::unique_ptr<xcldev::device>
                (new xcldev::device(index, nullptr));
        }
    }
    catch (const std::exception& ex) {
        std::cout << "ERROR: " << ex.what() << std::endl;
    }

    return std::unique_ptr<xcldev::device>(nullptr);
}

struct topThreadCtrl {
    int interval;
    std::unique_ptr<xcldev::device> dev;
    bool quit;
    int status;
};

static void topPrintUsage(const xcldev::device *dev, xclDeviceUsage& devstat, xclDeviceInfo2 &devinfo)
{
    std::vector<std::string> lines;

    dev->m_mem_usage_bar(devstat, lines, 0, devinfo.mDDRBankCount);

    dev->m_devinfo_stringize_power(&devinfo, lines);
    
    dev->m_mem_usage_stringize_dynamics(devstat, &devinfo, lines, 0, devinfo.mDDRBankCount);

    for(auto line:lines){
            printw("%s\n", line.c_str());
    } 
}

static void topThreadFunc(struct topThreadCtrl *ctrl)
{
    int i = 0;

    while (!ctrl->quit) {
        if ((i % ctrl->interval) == 0) {
            xclDeviceUsage devstat;
            xclDeviceInfo2 devinfo;
            int result = ctrl->dev->usageInfo(devstat);
            if (result) {
                ctrl->status = result;
                return;
            }
            result = ctrl->dev->deviceInfo(devinfo);
            if (result) {
                ctrl->status = result;
                return;
            }
            clear();
            topPrintUsage(ctrl->dev.get(), devstat, devinfo);
            refresh();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i++;
    }
}

int xcldev::xclTop(int argc, char *argv[])
{
    int interval = 1;
    unsigned index = 0;
    int c;
    const std::string usage("Options: [-d index] [-i <interval>]");
    struct topThreadCtrl ctrl = { 0 };

    while ((c = getopt(argc, argv, "d:i:")) != -1) {
        switch (c) {
        case 'i':
            interval = std::atoi(optarg);
            break;
        case 'd': {
            int ret = str2index(optarg, index);
            if (ret != 0)
                return ret;
            break;
        }
        default:
            std::cerr << usage << std::endl;
            return -EINVAL;
        }
    }

    if (optind != argc) {
        std::cerr << usage << std::endl;
        return -EINVAL;
    }

    ctrl.interval = interval;

    ctrl.dev = xcldev::xclGetDevice(index);
    if (!ctrl.dev) {
        return -ENOENT;
    }

    std::cout << "top interval is " << interval << std::endl;

    initscr();
    cbreak();
    noecho();

    std::thread t(topThreadFunc, &ctrl);

    // Waiting for and processing control command from stdin
    while (!ctrl.quit) {
        switch (getch()) {
        case 'q':
        case ERR:
            ctrl.quit = true;
            break;
        default:
            break;
        }
    }

    t.join();
    endwin();
    return ctrl.status;
}

const std::string dsaPath("/opt/xilinx/dsa/");

void testCaseProgressReporter(bool *quit)
{
    int i = 0;
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
        output += ", DSA package not installed properly.";
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

/*
 * validate
 */
int xcldev::device::validate()
{
    std::string output;
    bool testKernelBW = true;

    // Check pcie training
    std::cout << "INFO: Checking PCIE link status: " << std::flush;
    if (m_devinfo.mPCIeLinkSpeed != m_devinfo.mPCIeLinkSpeedMax ||
        m_devinfo.mPCIeLinkWidth != m_devinfo.mPCIeLinkWidthMax) {
        std::cout << "FAILED" << std::endl;
        std::cout << "WARNING: Device trained to lower spec. "
            << "Expect: Gen" << m_devinfo.mPCIeLinkSpeedMax << "x"
            << m_devinfo.mPCIeLinkWidthMax
            << ", Current: Gen" << m_devinfo.mPCIeLinkSpeed << "x"
            << m_devinfo.mPCIeLinkWidth
            << std::endl;
        // Non-fatal, continue validating.
    }
    else
    {
        std::cout << "PASSED" << std::endl;
    }

    // Run various test cases

    // Test verify kernel
    std::cout << "INFO: Testing verify kernel: " << std::flush;
    int ret = runTestCase(std::string("validate.exe"),
        std::string("verify.xclbin"), output);
    if (ret == -ENOENT) {
        if (m_idx == 0) {
            // Fall back to verify.exe
            ret = runTestCase(std::string("verify.exe"),
                std::string("verify.xclbin"), output);
            if (ret == 0) {
                // Probably testing with old package, skip kernel bandwidth test.
                testKernelBW = false;
            }
        }
    }
    if (ret != 0 || output.find("Hello World") == std::string::npos) {
        std::cout << "FAILED" << std::endl << output << std::endl;
        return ret == 0 ? -EINVAL : ret;
    }
    std::cout << "PASSED" << std::endl;


    // Perform DMA test
    std::cout << "INFO: Starting DMA test" << std::endl;
    ret = dmatest(0, false);
    if (ret != 0) {
        std::cout << "INFO: DMA test FAILED" << std::endl;
        return ret;
    }
    std::cout << "INFO: DMA test PASSED" << std::endl;

    if (!testKernelBW)
        return 0;


    // Test kernel bandwidth kernel
    std::cout << "INFO: Starting DDR bandwidth test: " << std::flush;
    ret = runTestCase(std::string("kernel_bw.exe"),
        std::string("bandwidth.xclbin"), output);
    if (ret != 0 || output.find("PASS") == std::string::npos) {
        std::cout << "FAILED" << std::endl << output << std::endl;
        return ret == 0 ? -EINVAL : ret;
    }
    std::cout << "PASSED" << std::endl;
    // Print out max thruput
    size_t st = output.find("Maximum");
    if (st != std::string::npos) {
        size_t end = output.find("\n", st);
        std::cout << output.substr(st, end - st) << std::endl;
    }

    return 0;
}

int xcldev::xclValidate(int argc, char *argv[])
{
    unsigned index = UINT_MAX;
    const std::string usage("Options: [-d index]");
    int c;

    while ((c = getopt(argc, argv, "d:")) != -1) {
        switch (c) {
        case 'd': {
            int ret = str2index(optarg, index);
            if (ret != 0)
                return ret;
            break;
        }
        default:
            std::cerr << usage << std::endl;
            return -EINVAL;
        }
    }
    if (optind != argc) {
        std::cerr << usage << std::endl;
        return -EINVAL;
    }

    unsigned int count = xclProbe();

    std::vector<unsigned> boards;
    if (index == UINT_MAX) {
        if (count == 0) {
            std::cout << "ERROR: No device found" << std::endl;
            return -ENOENT;
        }
        for (unsigned i = 0; i < count; i++)
            boards.push_back(i);
    } else {
        if (index >= count) {
            std::cout << "ERROR: Device[" << index << "] not found" << std::endl;
            return -ENOENT;
        }
        boards.push_back(index);
    }

    std::cout << "INFO: Found " << boards.size() << " devices" << std::endl;

    bool validated = true;
    for (unsigned i : boards) {
        std::unique_ptr<device> dev = xclGetDevice(i);
        if (!dev) {
            std::cout << "ERROR: Can't open device[" << i << "]" << std::endl;
            return -EINVAL;
        }

        std::cout << std::endl << "INFO: Validating device[" << i << "]: "
            << dev->name() << std::endl;

        if (dev->validate() != 0) {
            validated = false;
            std::cout << "INFO: Device[" << i << "] failed to validate." << std::endl;
        } else {
            std::cout << "INFO: Device[" << i << "] validated successfully." << std::endl;
        }
    }
    std::cout << std::endl;

    if (!validated) {
        std::cout << "ERROR: Some devices failed to validate." << std::endl;
        return -EINVAL;
    }

    std::cout << "INFO: All devices validated successfully." << std::endl;
    return 0;
}
