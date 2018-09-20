/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author(s): Hem C. Neema, Ryan Radjabi
 * PCIe HAL Driver layered on top of XOCL GEM kernel driver
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
#include "devices.h"

// Full list of discovered supported devices. Index 0 ~ (num_ready - 1) are
// boards ready for use. The rest, if any, are not ready, according to what
// is indicated by driver's "ready" sysfs entry. The applications only see
// ready-for-use boards since xclProbe returns num_ready, not the size of the
// full list.
std::vector<xcldev::pci_device_scanner::device_info> xcldev::pci_device_scanner::device_list;
int xcldev::pci_device_scanner::num_ready = 0;

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
 * get_val_long()
 *
 * Given a directory, get the value in a given key.
 * Returns the value as long int.
 */
long xcldev::get_val_long(std::string& dir, const char* key)
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
    if (get_mgmt_devinfo(device.vendor_id, device.device_id, device.subsystem_id)) {
        mgmt_devices.emplace_back(device);
    } else if ( get_user_devinfo(device.vendor_id, device.device_id, device.subsystem_id)) {
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
void xcldev::pci_device_scanner::print_pci_info(void)
{
    auto print = [](struct pci_device& dev) {
        std::cout << std::hex;
        std::cout << ":[" << std::setw(2) << std::setfill('0') << dev.bus
            << ":" << std::setw(2) << std::setfill('0') << dev.dev
            << "." << dev.func << "]";

        std::cout << std::hex;
        std::cout << ":0x" << std::setw(4) << std::setfill('0') << dev.device_id;
        std::cout << ":0x" << std::setw(4) << std::setfill('0') << dev.subsystem_id;

        std::cout << std::dec;
        std::cout << ":[";
        if(!dev.driver_name.empty()) {
            std::cout << dev.driver_name << ":" << dev.driver_version << ":";
            if( dev.instance == INVALID_DEV ) {
                std::cout << "???";
            } else {
                std::cout << dev.instance;
            }
        }
        std::cout << "]" << std::endl;;
    };

    int i = 0;
    int disabled = 0;
    for (auto& dev : device_list) {
        bool ready = false;

        for (auto& mdev : mgmt_devices) {
            if ((mdev.domain == dev.domain) &&
                 (mdev.bus == dev.bus) &&
                 (mdev.dev == dev.device))
            {
                ready = mdev.is_ready;
                std::cout << (ready ? "" : "*");
                std::cout << "[" << i << "]" << "mgmt";
                print(mdev);
                break;
            }
        }

        for (auto& udev : user_devices) {
            if ((udev.domain == dev.domain) &&
                 (udev.bus == dev.bus) &&
                 (udev.dev == dev.device))
            {
                std::cout << (ready ? "" : "*");
                std::cout << "[" << i << "]" << "user";
                print(udev);
                break;
            }
        }

        if (!ready)
            disabled++;
        ++i;
    }

    if (disabled != 0) {
        std::cout << "WARNING: " << disabled << " card(s) marked by '*' are not ready, "
            << "run xbutil flash scan -v to further check the details."
            << std::endl;
    }
}

/*
 * add_to_device_list()
 */
void xcldev::pci_device_scanner::add_to_device_list(void)
{
    int good_dev = 0;

    for (auto &mdev : mgmt_devices) {
        struct device_info temp = { 0, mdev.instance,
                                    "", mdev.device_name,
                                    mdev.user_bar, mdev.user_bar_size,
                                    mdev.domain, mdev.bus, mdev.dev,
                                    mdev.func, 0, mdev.flash_type,
                                    mdev.board_name, mdev.is_mfg, mdev.is_ready };

        // Boards not ready go to end of list.
        if(!mdev.is_ready) {
            device_list.emplace_back(temp);
            continue;
        }
        // Boards ready go to front of list.
        for (auto &udev : user_devices) {
            if( (mdev.domain == udev.domain) &&
                    (mdev.bus == udev.bus) &&
                    (mdev.dev == udev.dev) )
            {
                if( (temp.user_instance != INVALID_DEV) && (temp.mgmt_instance != INVALID_DEV) ) {
                    temp.user_instance = udev.instance;
                    temp.user_name = udev.device_name;
                    temp.user_func = udev.func;

                    device_list.emplace(device_list.begin(), temp);
                    good_dev++;
                }
            }
        }
    }
    num_ready = good_dev;
}

/*
 * bar_size()
 */
const size_t xcldev::pci_device_scanner::bar_size(const std::string &dir, unsigned bar)
{
    std::ifstream ifs(dir + "/resource");
    if (!ifs.good())
        return 0;
    std::string line;
    for (unsigned i = 0; i <= bar; i++) {
        line.clear();
        std::getline(ifs, line);
    }
    long long start, end, meta;
    if (sscanf(line.c_str(), "0x%llx 0x%llx 0x%llx", &start, &end, &meta) != 3)
        return 0;
    return end - start + 1;
}

/*
 * scan()
 *
 * TODO: Refactor to be much shorter function.
 */
int xcldev::pci_device_scanner::scan(bool print)
{
    int retVal = 0;
    // need to clear the following lists: mgmt_devices, user_devices, xcldev::device_list
    mgmt_devices.clear();
    user_devices.clear();
    device_list.clear();

    if( print ) {
        if( !print_system_info() ) {
            std::cout << "Unable to determine system info " << std::endl;
        }

        std::cout << "--- " << std::endl;
        if( !print_paths() ) {
            std::cout << "Unable to determine PATH/LD_LIBRARY_PATH info " << std::endl;
        }
        std::cout << "--- " << std::endl;
    }

    std::string dirname;
    DIR *dir;
    struct dirent *entry;
    unsigned int dom, bus, dev, func, bar;

    dirname = ROOT_DIR;
    dirname += "/devices/";
    dir = opendir(dirname.c_str());
    if( !dir ) {
        std::cout << "Cannot open " << dirname << std::endl;
        return -1;
    }

    while( ( entry = readdir(dir) ) ) {
        if( entry->d_name[0] == '.' ) {
            continue;
        }

        if( sscanf(entry->d_name, "%x:%x:%x.%d", &dom, &bus, &dev, &func) < 4 ) {
            std::cout << "scan: Couldn't parse entry name " << entry->d_name << std::endl;
        }

        std::string subdir = dirname + entry->d_name;
        std::string subdir2 = dirname + entry->d_name;

        pci_device device; // generic pci device
        device.domain = dom;
        device.bus = bus;
        device.dev = dev;
        device.func = func;
        device.device_name = entry->d_name;
        device.vendor_id = get_val_long(subdir, "vendor");
        if( ( device.vendor_id != XILINX_ID ) && ( device.vendor_id != ADVANTECH_ID ) ) {
            continue;
        }

        // Xilinx device from here
        device.device_id = get_val_long(subdir, "device");
        device.subsystem_id = get_val_long(subdir, "subsystem_device");

        struct xocl_board_info *board_info;
        struct xocl_board_private *priv;
        bool is_mgmt = false;

        if ((board_info = get_mgmt_devinfo(device.vendor_id,
            device.device_id, device.subsystem_id))) {
            is_mgmt = true;
        } else if ((board_info = get_user_devinfo(device.vendor_id,
            device.device_id, device.subsystem_id))) {
            is_mgmt = false;
        } else {
            continue;
        }

        priv = board_info->priv_data;
        bar = priv->user_bar;
        device.user_bar = bar;
        device.user_bar_size = bar_size(subdir, bar);
        if (priv->flash_type)
            device.flash_type = priv->flash_type;
        if (priv->board_name)
            device.board_name = priv->board_name;
        device.is_mfg = ((priv->flags & XOCL_DSAFLAG_MFG) != 0);

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
        if( err >= DRIVER_BUF_SIZE-1 ) {
            std::cout << "Driver name is too long." << std::endl;
            retVal = -ENAMETOOLONG;
            break;
        }

        device.driver_name = driverName;
        size_t found = device.driver_name.find_last_of("/");
        if( found != std::string::npos ) {
            device.driver_name = device.driver_name.substr(found + 1);
        }

        //Get driver version
        subdir += "/module/";
        std::string version = get_val_string(subdir, "version");
        version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
        device.driver_version = version;

        if (is_mgmt) {
            device.instance = get_val_long(subdir2, "instance");
            device.is_ready = get_val_long(subdir2, "ready");
        } else {
            std::string drm_dir = subdir2;
            drm_dir += "/drm";
            device.instance = get_render_value(drm_dir);
        }

        if( !add_device(device) )
        {
            retVal = -ENODEV;
            break;
        }
    }

    if( closedir(dir) < 0 ) {
        std::cout << "Cannot close " << dirname << std::endl;
        return errno;
    }

    if( retVal < 0 ) {   // return from other errors above
        return retVal;
    } else {
        add_to_device_list();
    }

    if (print)
        print_pci_info();
    return 0;
}

bool xcldev::pci_device_scanner::get_mgmt_device_name(std::string &devName , unsigned int devIdx)
{
    bool retVal = false;
    if( !mgmt_devices.empty() )
    {
        devName = mgmt_devices[ devIdx ].device_name;
        retVal = true;
    }
    return retVal;
}

int xcldev::pci_device_scanner::get_feature_rom_bar_offset(unsigned int devIdx,
    unsigned long long &offset)
{
    int ret = -ENOENT;

    if (devIdx >= device_list.size())
        return ret;
    auto& dev = device_list[devIdx];

    pci_device *mdevice = nullptr;
    for (auto& mdev : mgmt_devices) {
        if ((mdev.domain == dev.domain) &&
             (mdev.bus == dev.bus) &&
             (mdev.dev == dev.device))
        {
            mdevice = &mdev;
            break;
        }
    }

    if(mdevice != nullptr)
    {
        struct xocl_board_info *board_info;

        if ((board_info = get_mgmt_devinfo(mdevice->vendor_id,
            mdevice->device_id, mdevice->subsystem_id))) {
            struct xocl_board_private *priv = board_info->priv_data;
            for (unsigned int i = 0; i < priv->subdev_num; i++)
            {
                if (priv->subdev_info[i].id == XOCL_SUBDEV_FEATURE_ROM)
                {
                    offset = priv->subdev_info[i].res[0].start;
                    ret = 0;
                    break;
                }
            } 
        }
    }

    return ret;
}
