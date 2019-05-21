/**
 * Copyright (C) 2017-2018 Xilinx, Inc
 * Author: Hem Neema, Ryan Radjabi
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

#include "scan.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cassert>
#include <fstream>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <sys/utsname.h>
#include <cstdlib>
#include <gnu/libc-version.h>

#ifndef INTERNAL_TESTING
#include <sstream>
#include <iomanip>
#endif

std::vector<xcldev::pci_device_scanner::device_info> xcldev::pci_device_scanner::device_list;

static const std::string sysfs_root = "/sys/bus/pci/devices/";

static std::string get_name(const std::string& dir, const std::string& subdir)
{
    std::string line;
    std::ifstream ifs(dir + "/" + subdir + "/name");

    if (ifs.is_open())
        std::getline(ifs, line);

    return line;
}   
// Helper to find subdevice directory name
// Assumption: all subdevice's sysfs directory name starts with subdevice name!!
static int get_subdev_dir_name(const std::string& dir,
    const std::string& subDevName, std::string& subdir)
{
    DIR *dp;
    size_t sub_nm_sz = subDevName.size();

    subdir = "";
    if (subDevName.empty())
        return 0;

    int ret = -ENOENT;
    dp = opendir(dir.c_str());
    if (dp) {
        struct dirent *entry;
        while ((entry = readdir(dp))) {
            std::string nm = get_name(dir, entry->d_name);
            if (!nm.empty()) {
                if (nm != subDevName)
                    continue;
            } else if(strncmp(entry->d_name, subDevName.c_str(), sub_nm_sz) ||
                entry->d_name[sub_nm_sz] != '.') {
                continue;
            }
            // found it
            subdir = entry->d_name;
            ret = 0;
            break;
        }
        closedir(dp);
    }

    return ret;
}

std::string xcldev::get_sysfs_path(const std::string& sysfs_name, const std::string& subdev,
    const std::string& entry)
{
    std::string subdir;
    if (get_subdev_dir_name(sysfs_root + sysfs_name, subdev, subdir) != 0)
        return "";

    std::string path = sysfs_root;
    path += sysfs_name;
    path += "/";
    path += subdir;
    path += "/";
    path += entry;
    return path;
}

static std::fstream sysfs_open_path(const std::string& path, std::string& err,
    bool write, bool binary)
{
    std::fstream fs;
    std::ios::openmode mode = write ? std::ios::out : std::ios::in;

    if (binary)
        mode |= std::ios::binary;

    err.clear();
    fs.open(path, mode);
    if (!fs.is_open()) {
        std::stringstream ss;
        ss << "Failed to open " << path << " for "
            << (binary ? "binary " : "")
            << (write ? "writing" : "reading") << ": "
            << strerror(errno) << std::endl;
        err = ss.str();
    }
    return fs;
}

std::fstream xcldev::sysfs_open(const std::string& sysfs_name, const std::string& subdev,
    const std::string& entry, std::string& err, bool write, bool binary)
{
    std::fstream fs;
    const std::string path = get_sysfs_path(sysfs_name, subdev, entry);

    if (path.empty()) {
        std::stringstream ss;
        ss << "Failed to find subdirectory for " << subdev
            << " under " << sysfs_root + sysfs_name << std::endl;
        err = ss.str();
    } else {
        fs = sysfs_open_path(path, err, write, binary);
    }

    return fs;
}

void xcldev::sysfs_get(
    const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<char>& buf)
{
    std::fstream fs = sysfs_open(sysfs_name, subdev, entry, err_msg, false, true);
    if (!err_msg.empty())
        return;

    buf.insert(std::end(buf),std::istreambuf_iterator<char>(fs),std::istreambuf_iterator<char>());
}

void xcldev::sysfs_get(
    const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<std::string>& sv)
{
    std::fstream fs = sysfs_open(sysfs_name, subdev, entry, err_msg, false, false);
    if (!err_msg.empty())
        return;

    sv.clear();
    std::string line;
    while (std::getline(fs, line))
        sv.push_back(line);
}

void xcldev::sysfs_get(
    const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<uint64_t>& iv)
{
    uint64_t n;
    std::vector<std::string> sv;

    iv.clear();

    sysfs_get(sysfs_name, subdev, entry, err_msg, sv);
    if (!err_msg.empty())
        return;

    char *end;
    for (auto& s : sv) {
        std::stringstream ss;

        if (s.empty()) {
            ss << "Reading " << get_sysfs_path(sysfs_name, subdev, entry) << ", ";
            ss << "can't convert empty string to integer" << std::endl;
            err_msg = ss.str();
            break;
        }
        n = std::strtoull(s.c_str(), &end, 0);
        if (*end != '\0') {
            ss << "Reading " << get_sysfs_path(sysfs_name, subdev, entry) << ", ";
            ss << "failed to convert string to integer: " << s << std::endl;
            err_msg = ss.str();
            break;
        }
        iv.push_back(n);
    }
}

void xcldev::sysfs_get(
    const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::string& s)
{
    std::vector<std::string> sv;

    sysfs_get(sysfs_name, subdev, entry, err_msg, sv);
    if (!sv.empty())
        s = sv[0];
    else
        s = ""; // default value
}

void xcldev::sysfs_get(
    const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, bool& b)
{
    std::vector<uint64_t> iv;

    sysfs_get(sysfs_name, subdev, entry, err_msg, iv);
    if (!iv.empty())
        b = (iv[0] == 1);
    else
        b = false; // default value
}

/*
 * get_val_string()
 *
 * Given a directory, get the value in a given key.
 * Returns the value as string.
 */
std::string xcldev::get_val_string(std::string& dir, const char* key)
{
    std::string file = dir + "/" + key;
    int fd = open(file.c_str(), O_RDONLY);
    if( fd < 0 ) {
        std::cout << "Unable to open " << file << std::endl;
        throw std::runtime_error("Could not open file: " + dir + "/" + key );
    }

    char buf[OBJ_BUF_SIZE];
    memset(buf, 0, OBJ_BUF_SIZE);
    ssize_t err = read(fd, buf, (OBJ_BUF_SIZE-1)); // read 1 less to make room for null terminator
    if( err >= 0 )
    {
        buf[err] = 0; // null terminate after successful read()
    }

    if( (err < 0) || (err >= OBJ_BUF_SIZE) ) {
        std::cout << "Unable to read contents of " << file << std::endl;
    }

    if( close(fd) < 0 ) {
        std::cout << "Unable to close " << file << std::endl;
        throw std::runtime_error("Could not close file: " + dir + "/" + key );
    }

    return buf;
}

/*
 * get_val_int()
 *
 * Given a directory, get the value in a given key.
 * Returns the value as long int.
 */
long xcldev::get_val_int(std::string& dir, const char* key)
{
    std::string buf = get_val_string(dir, key);
    return strtol(buf.c_str(), NULL, 0);
}

/*
 * get_render_value()
 */
int xcldev::get_render_value(std::string& dir)
{
    struct dirent *entry;
    DIR *dp;
    int instance_num = 128;

    dp = opendir(dir.c_str());
    if (dp == NULL) {
      std::string tmp = "opendir: Path " + dir + " does not exist or could not be read";
      perror(tmp.c_str());
        return -1;
    }

    while ((entry = readdir(dp))) {
        if(strncmp(entry->d_name, "renderD", 7) == 0) {
            sscanf(entry->d_name, "renderD%d", &instance_num);
            break;
        }
    }
    
    closedir(dp);
    return instance_num;
}

/*
 * add_device()
 */
bool xcldev::pci_device_scanner::add_device(struct pci_device& device)
{
        if ( device.func == 2) {//AWS Pegasus mgmtPF is 2; On AWS F1 mgmtPF is not visible
            mgmt_devices.emplace_back(device);
            //std::cout << "scan: add mgmt device instance = " << std::dec << device.instance << std::endl;
        } else if ( device.func == 0) {
            //std::cout << "scan: add user device instance = " << std::dec << device.instance << std::endl;
            user_devices.emplace_back(device);
        } else {
            assert(0);
            return false;
        }
        return true;
    }

/*
 * print_paths()
 */
bool xcldev::pci_device_scanner::print_paths()
{
        std::cout << "XILINX_OPENCL=\"";
        if ( const char* opencl = std::getenv("XILINX_OPENCL")) {
            std::cout << opencl << "\"";
        } else {
            std::cout << "\"";
        }
        std::cout << std::endl;

        std::cout << "LD_LIBRARY_PATH=\"";
        if ( const char* ld = std::getenv("LD_LIBRARY_PATH")) {
            std::cout << ld << "\"";
        } else {
            std::cout << "\"";
        }
        std::cout << std::endl;
        return true;
    }

    /*
 * print_system_info()
 *
     * Print Linux release and distribution.
     */
bool xcldev::pci_device_scanner::print_system_info()
{
        struct utsname sysinfo;
        if ( uname(&sysinfo) < 0) {
            return false;
        }
        std::cout << sysinfo.sysname << ":" << sysinfo.release << ":" << sysinfo.version << ":" << sysinfo.machine << std::endl;

        // print linux distribution name and version from /etc/os-release file
        std::ifstream ifs;
        bool found = false;
        std::string distro;
        ifs.open( "/etc/system-release", std::ifstream::binary );
        if( ifs.good() ) { // confirmed for RHEL/CentOS
            std::getline( ifs, distro );
            found = true;
        } else { // confirmed for Ubuntu
            ifs.open( "/etc/lsb-release", std::ifstream::binary );
            if( ifs.good() ) {
                std::string readString;
                while( std::getline( ifs, readString ) && !found ) {
                    if( readString.find( "DISTRIB_DESCRIPTION=" ) == 0 ) {
                        distro = readString.substr( readString.find("=")+2, readString.length() ); // +2 excludes equals and quotes (2 chars)
                        distro = distro.substr( 0, distro.length()-1 ); // exclude the final quotes char
                        found = true;
                    }
                }
            }
        }

        if( found ) {
            std::cout << "Distribution: " << distro << std::endl;
        } else {
            std::cout << "Unable to find OS distribution and version." << std::endl;
        }

        std::cout << "GLIBC: " << gnu_get_libc_version() << std::endl;
        return true;
    }

/*
 * print_pci_info()
 */
bool xcldev::pci_device_scanner::print_pci_info()
{
        auto print = [](struct pci_device& dev) {
            std::cout << std::hex << dev.device_id << ":0x" << dev.subsystem_id << ":[" << std::dec;
            if(!dev.driver_name.empty())
                std::cout << dev.driver_name << ":" << dev.driver_version << ":" << dev.instance << "]" << std::endl;
            else
                std::cout << "]" << std::endl;
        };

        int i = 0;
        for (auto mdev : mgmt_devices) {
            std::cout << "[" << i << "]" << "mgmt:0x";
            print(mdev);
            for (auto udev : user_devices) {
                if ( udev.device_id == mdev.device_id + 1) {
                    std::cout << "[" << i << "]" << "user:0x";
                    print(udev);
                }
            }
            ++i;
        }

        return true;
    }

/*
 * print_f1_pci_info()
 */
bool xcldev::pci_device_scanner::print_f1_pci_info()
{
        auto print = [](struct pci_device& dev) {
            std::cout << std::hex << dev.device_id << ":0x" << dev.subsystem_id << ":[" << std::dec;
            if(!dev.driver_name.empty())
                std::cout << dev.driver_name << ":" << dev.driver_version << ":" << dev.instance << "]" << std::endl;
            else
                std::cout << "]" << std::endl;
        };

        int i = 0;
        for (auto udev : user_devices) {
            std::cout << "[" << i << "]" << "user:0x";
            print(udev);
            ++i;
        }

        return true;
    }

/*
 * add_to_device_list()
 */
void xcldev::pci_device_scanner::add_to_device_list()
{
        //Sarab: For Pegasus mgmtPF and userPF; On AWS F1 mgmtPF is not visible
        for (auto &udev : user_devices) {
            //struct device_info temp = {udev.instance, mdev.instance,  udev.device_name, mdev.device_name};
            struct device_info temp = {udev.instance, udev.instance,  udev.device_name, udev.device_name};
            //Note: On AWS F1 mgmtPF is not visible
            for (auto &mdev : mgmt_devices) {
                if( (mdev.domain == udev.domain) &&
                        (mdev.bus == udev.bus) &&
                        (mdev.dev == udev.dev)
                        ) {
                    temp.mgmt_name = mdev.device_name;
                    temp.mgmt_instance = mdev.instance;
                }
            }
            device_list.emplace_back(temp);
        }
    }

/*
 * scan()
 */
int xcldev::pci_device_scanner::scan(bool print)
{
        pci_device device;
        int domain = 0;
        int bus = 0, dev = 0, func = 0;
        std::string dirname;
        dirname = ROOT_DIR;
        dirname += "/devices/";

#ifdef INTERNAL_TESTING
        DIR *dir;
        struct dirent *entry;
        if ( !print_system_info() ) {
            std::cout << "Unable to determine system info " << std::endl;
        }
        std::cout << "--- " << std::endl;
        if ( !print_paths() ) {
            std::cout << "Unable to determine PATH/LD_LIBRARY_PATH info " << std::endl;
        }
        std::cout << "--- " << std::endl;

        dir = opendir(dirname.c_str());
        if ( !dir ) {
            std::cout << "Cannot open " << dirname << std::endl;
            return -1;
        }

        while ((entry = readdir(dir))) {
            if ( entry->d_name[0] == '.')
                continue;

            if ( sscanf(entry->d_name, "%x:%x:%x.%d", &domain, &bus, &dev, &func) < 4) {
                std::cout << "scan: Couldn't parse entry name " << entry->d_name << std::endl;
            }

            std::string subdir = dirname + entry->d_name;
            std::string subdir2 = dirname + entry->d_name;

            //On Pegasus: 0 is userPF & 2 is mgmtPG.
            //On Pegasus & F1: userPF is for Device 1d0f:1042
            //if (func == 0 || func == 2) {
            //}
            //Using device id to only find userPF info
            device.domain = domain;
            device.bus = bus;
            device.dev = dev;
            device.func = func;
            device.vendor_id = get_val_int(subdir, "vendor");
            device.device_id = get_val_int(subdir, "device");
            device.subsystem_id = get_val_int(subdir, "subsystem_device");
            if (device.vendor_id != XILINX_ID)
                continue;
            if (device.device_id != AWS_UserPF_DEVICE_ID && device.device_id != AWS_MgmtPF_DEVICE_ID && device.device_id != AWS_UserPF_DEVICE_ID_SDx)
                continue;
            if ( device.func != 0 && device.func != 2)
                continue;
            //std::cout << "scan: Xilinx AWS device entry name " << entry->d_name << std::endl;

            //Get the driver name.
            char driverName[DRIVER_BUF_SIZE];
            memset(driverName, 0, DRIVER_BUF_SIZE);
            subdir += "/driver";
        int err = readlink(subdir.c_str(), driverName, DRIVER_BUF_SIZE-1); // read 1 less to make room for null terminator
        if( err >= 0 ) {
            driverName[err] = 0; // null terminate after successful readlink()
        } else {
            add_device(device); // add device even if it is incomplete
                continue;
            }
        if ( err >= DRIVER_BUF_SIZE-1) {
                std::cout << "Driver name is too big " << std::endl;
                return -1;
            }

            device.driver_name = driverName;
            size_t found = device.driver_name.find_last_of("/");
            if ( found != std::string::npos ) {
                device.driver_name = device.driver_name.substr(found + 1);
            }

            //Get driver version
            subdir += "/module/";
            std::string version = get_val_string(subdir, "version");
            version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
            device.driver_version = version;

            if(device.func == 2) {//mgmtPF on Pegasus; mgmtPF not visible on AWS F1
                device.instance = get_val_int(subdir2, "instance");
            } else if (device.func == 0)  {//userPF on Pegasus (AWS)
                std::string drm_dir = subdir2;
                drm_dir += "/drm";
                device.instance = get_render_value(drm_dir);
            }
            device.device_name = entry->d_name;

            if ( !add_device(device) )
        {
                return -1;
        }
        }
        //std::cout << "scan: Create device list" << std::endl;
        add_to_device_list();

    if( closedir(dir) < 0 ) {
        std::cout << "Cannot close " << dirname << std::endl;
        return -1;
    }

        if(print)
            return print_pci_info() ? 0 : -1;
        else
            return 0;
#else
        uint16_t vendor_id = 0, device_id = 0; // only used without INTERNAL_TESTING

        if (fpga_mgmt_init() || fpga_pci_init() ) {
            std::cout << "ERROR: xclProbe-scan failed to initialized fpga libraries" << std::endl;
            return -1;
        }
        fpga_slot_spec spec_array[16];
        std::memset(spec_array, 0, sizeof(fpga_slot_spec) * 16);
        if (fpga_pci_get_all_slot_specs(spec_array, 16)) {
            std::cout << "ERROR: xclProbe-scan failed at fpga_pci_get_all_slot_specs" << std::endl;
            return -1;
        }

        for (unsigned short i = 0; i < 16; i++) {
            if (spec_array[i].map[FPGA_APP_PF].vendor_id == 0)
                break;

            domain = spec_array[i].map[FPGA_APP_PF].domain;
            bus = spec_array[i].map[FPGA_APP_PF].bus;
            dev = spec_array[i].map[FPGA_APP_PF].dev;
            func = spec_array[i].map[FPGA_APP_PF].func;
            vendor_id = spec_array[i].map[FPGA_APP_PF].vendor_id;
            device_id = spec_array[i].map[FPGA_APP_PF].device_id;

            //On Pegasus: func=0 is userPF & func=2 is mgmtPG.
            //On Pegasus & F1: userPF is for Device 1d0f:1042
            if (vendor_id != XILINX_ID)
                continue;
            if (device_id != AWS_UserPF_DEVICE_ID && device_id != AWS_MgmtPF_DEVICE_ID && device_id != AWS_UserPF_DEVICE_ID_SDx)
                continue;
            if (func != 0) //userPF func == 0; mgmtPF not visible on AWS F1
                continue;

            //userPF func == 0; mgmtPF not visible on AWS F1
            std::stringstream domain_str;
            std::stringstream bus_str;
            std::stringstream dev_str;
            //Note: Below works with stringstream only for integers and not for uint8, etc.
            domain_str << std::setw(4) << std::setfill('0') << domain;
            bus_str << std::setw(2) << std::setfill('0') << std::hex << bus;
            dev_str << std::setw(2) << std::setfill('0') << std::hex << dev;
            std::string func_str = std::to_string(func);//stringstream is giving minimum of two chars

            device.device_name = domain_str.str() + ":" + bus_str.str() + ":" + dev_str.str() + "." + func_str + "\0";
            std::string subdir2 = dirname + device.device_name;

            device.domain = domain;
            device.bus = bus;
            device.dev = dev;
            device.func = func;
            device.vendor_id = vendor_id;
            device.device_id = device_id;
            device.subsystem_id = spec_array[i].map[FPGA_APP_PF].subsystem_device_id;

            //Get the driver name.
            char driverName[DRIVER_BUF_SIZE];
            memset(driverName, 0, DRIVER_BUF_SIZE);
            std::string driver_dir = subdir2;
            driver_dir += "/driver";
        int err = readlink(driver_dir.c_str(), driverName, DRIVER_BUF_SIZE-1); // read 1 less to make room for null terminator
        if( err >= 0 ) {
            driverName[err] = 0; // null terminate after successful readlink()
        } else {
                add_device(device);
                continue;
            }
        if (err >= DRIVER_BUF_SIZE-1) {
                std::cout << "ERROR: Driver name is too big " << std::endl;
                return -1;
            }

            device.driver_name = driverName;
            size_t found = device.driver_name.find_last_of("/");
            if ( found != std::string::npos) {
                device.driver_name = device.driver_name.substr(found + 1);
            }

            //Get driver version
            std::string module_dir = driver_dir;
            module_dir += "/module/";
            std::string version = get_val_string(module_dir, "version");
            version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
            device.driver_version = version;

            if (func == 0)  {//userPF on Pegasus & F1
                std::string drm_dir = subdir2;
                drm_dir += "/drm";
                device.instance = get_render_value(drm_dir);
            }

            if (!add_device(device))
                return -1;

        }
        //std::cout << "scan: Create device list" << std::endl;
        add_to_device_list();

        if(print)
            return print_f1_pci_info() ? 0 : -1;
        else
            return 0;
#endif
    }
