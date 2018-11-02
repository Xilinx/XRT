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

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <mutex>

#include "scan.h"

// Supported vendors
#define XILINX_ID    0x10ee
#define ADVANTECH_ID 0x13fe

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

std::string pcidev::pci_func::get_sysfs_path(const std::string& subdev,
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

std::fstream pcidev::pci_func::sysfs_open(const std::string& subdev,
    const std::string& entry, std::string& err, bool write, bool binary)
{
    std::fstream fs;
    const std::string path = get_sysfs_path(subdev, entry);

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

void pcidev::pci_func::sysfs_put(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, const std::string& input)
{
    std::fstream fs = sysfs_open(subdev, entry, err_msg, true, false);
    if (!err_msg.empty())
        return;
    fs << input;
}

void pcidev::pci_func::sysfs_get(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<char>& buf)
{
    std::fstream fs = sysfs_open(subdev, entry, err_msg, false, true);
    if (!err_msg.empty())
        return;

    buf.insert(std::end(buf),std::istreambuf_iterator<char>(fs),std::istreambuf_iterator<char>());
}

void pcidev::pci_func::sysfs_get(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<std::string>& sv)
{
    std::fstream fs = sysfs_open(subdev, entry, err_msg, false, false);
    if (!err_msg.empty())
        return;

    sv.clear();
    std::string line;
    while (std::getline(fs, line))
        sv.push_back(line);
}

void pcidev::pci_func::sysfs_get(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<uint64_t>& iv)
{
    uint64_t n;
    std::vector<std::string> sv;

    iv.clear();

    sysfs_get(subdev, entry, err_msg, sv);
    if (!err_msg.empty())
        return;

    char *end;
    for (auto& s : sv) {
        std::stringstream ss;

        if (s.empty()) {
            ss << "Reading " << get_sysfs_path(subdev, entry) << ", ";
            ss << "can't convert empty string to integer" << std::endl;
            err_msg = ss.str();
            break;
        }
        n = std::strtoull(s.c_str(), &end, 0);
        if (*end != '\0') {
            ss << "Reading " << get_sysfs_path(subdev, entry) << ", ";
            ss << "failed to convert string to integer: " << s << std::endl;
            err_msg = ss.str();
            break;
        }
        iv.push_back(n);
    }
}

void pcidev::pci_func::sysfs_get(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::string& s)
{
    std::vector<std::string> sv;

    sysfs_get(subdev, entry, err_msg, sv);
    if (!sv.empty())
        s = sv[0];
    else
        s = ""; // default value
}

void pcidev::pci_func::sysfs_get(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, bool& b)
{
    std::vector<uint64_t> iv;

    sysfs_get(subdev, entry, err_msg, iv);
    if (!iv.empty())
        b = (iv[0] == 1);
    else
        b = false; // default value
}

static size_t bar_size(const std::string &dir, unsigned bar)
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

static int get_render_value(const std::string& dir)
{
#define RENDER_NM   "renderD"
    struct dirent *entry;
    DIR *dp;
    int instance_num = INVALID_ID;

    dp = opendir(dir.c_str());
    if (dp == NULL)
        return instance_num;

    while ((entry = readdir(dp))) {
        if(strncmp(entry->d_name, RENDER_NM, sizeof (RENDER_NM) - 1) == 0) {
            sscanf(entry->d_name, RENDER_NM "%d", &instance_num);
            break;
        }
    }

    closedir(dp);

    return instance_num;
}

pcidev::pci_func::pci_func(const std::string& sysfs) : sysfs_name(sysfs)
{
    const std::string dir = sysfs_root + sysfs;
    std::string err;
    std::vector<pci_func> mgmt_devices;
    std::vector<pci_func> user_devices;
    uint16_t dom, b, d, f, vendor, device, subdevice;
    bool is_mgmt = false;

    if((sysfs.c_str())[0] == '.')
        return;

    if(sscanf(sysfs.c_str(), "%hx:%hx:%hx.%hx", &dom, &b, &d, &f) < 4) {
        std::cout << "Couldn't parse entry name " << sysfs << std::endl;
        return;
    }

    // Determine if device is of supported vendor
    sysfs_get("", "vendor", err, vendor);
    if (!err.empty()) {
        std::cout << err << std::endl;
        return;
    }
    if((vendor != XILINX_ID) && (vendor != ADVANTECH_ID))
        return;

    device = subdevice = INVALID_ID;
    sysfs_get("", "device", err, device);
    sysfs_get("", "subsystem_device", err, subdevice);

    // Determine if the device is mgmt or user pf.
    std::string tmp;
    sysfs_get("", "mgmt_pf", err, tmp);
    if (err.empty()) {
        is_mgmt = true;
    } else {
        sysfs_get("", "user_pf", err, tmp);
        if (err.empty()) {
            is_mgmt = false;
        } else {
            return; // device not recognized
        }
    }

    // Found a supported PCIE function.
    vendor_id = vendor;
    domain = dom;
    bus = b;
    dev = d;
    func = f;
    device_id = device;
    subsystem_id = subdevice;
    sysfs_get("", "userbar", err, user_bar);
    user_bar_size = bar_size(dir, user_bar);
    mgmt = is_mgmt;

    if (is_mgmt) {
        sysfs_get("", "instance", err, instance);
    } else {
        instance = get_render_value(dir + "/drm");
    }

    //Get the driver name and version.
    char driverName[1024] = { 0 };
    unsigned int rc = readlink((dir + "/driver").c_str(),
        driverName, sizeof (driverName));

    if (rc > 0 && rc < sizeof (driverName)) {
        driver_name = driverName;
        size_t found = driver_name.find_last_of("/");
        if( found != std::string::npos )
            driver_name = driver_name.substr(found + 1);

        //Get driver version
        std::string version;
        std::fstream fs = sysfs_open_path(dir + "/driver/module/version", err,
            false, false);
        if (fs.is_open()) {
            std::getline(fs, version);
            version.erase(std::remove(version.begin(), version.end(), '\n'),
                version.end());
            driver_version = version;
        }
    }
}

static int add_to_device_list(
    std::vector<std::unique_ptr<pcidev::pci_func>>& mgmt_devices,
    std::vector<std::unique_ptr<pcidev::pci_func>>& user_devices,
    std::vector<std::unique_ptr<pcidev::pci_device>>& devices)
{
    int good_dev = 0;
    std::string errmsg;

    for (auto &mdev : mgmt_devices) {
        for (auto &udev : user_devices) {
            if (udev == nullptr)
                continue;

            // Found the matching user pf.
            if( (mdev->domain == udev->domain) &&
                (mdev->bus == udev->bus) && (mdev->dev == udev->dev) ) {
                auto dev = std::unique_ptr<pcidev::pci_device>(
                    new pcidev::pci_device(mdev, udev));
                // Board not ready goes to end of list, so they are not visible
                // to applications. Only xbutil sees them.
                if(!dev->is_ready) {
                    devices.push_back(std::move(dev));
                } else {
                    devices.insert(devices.begin(), std::move(dev));
                    good_dev++;
                }
                break;
            }
        }
        if (mdev != nullptr) { // mgmt pf without matching user pf
            std::unique_ptr<pcidev::pci_func> udev;
            auto dev = std::unique_ptr<pcidev::pci_device>(
                new pcidev::pci_device(mdev, udev));
            devices.push_back(std::move(dev));
        }
    }

    return good_dev;
}

pcidev::pci_device::pci_device(std::unique_ptr<pci_func>& mdev,
    std::unique_ptr<pci_func>& udev) :
    mgmt(std::move(mdev)), user(std::move(udev)), is_mfg(false)
{
    std::string errmsg;

    mgmt->sysfs_get("", "ready", errmsg, is_ready);
    if (!errmsg.empty())
        std::cout << errmsg << std::endl;

    mgmt->sysfs_get("", "mfg", errmsg, is_mfg);
    mgmt->sysfs_get("", "flash_type", errmsg, flash_type);
    mgmt->sysfs_get("", "board_name", errmsg, board_name);
}

class pci_device_scanner {
public:

    static pci_device_scanner *get_scanner() {
        static pci_device_scanner scanner;
        return &scanner;
    }
    void rescan() {
        std::lock_guard<std::mutex> l(lock);
        rescan_nolock();
    }

    // Full list of discovered supported devices. Index 0 ~ (num_ready - 1) are
    // boards ready for use. The rest, if any, are not ready, according to what
    // is indicated by driver's "ready" sysfs entry. The applications only see
    // ready-for-use boards since xclProbe returns num_ready, not the size of
    // the full list.
    std::vector<std::unique_ptr<pcidev::pci_device>> dev_list;
    int num_ready;

private:
    std::mutex lock;
    void rescan_nolock();
    pci_device_scanner();
    pci_device_scanner(const pci_device_scanner& s);
    pci_device_scanner& operator=(const pci_device_scanner& s);
}; /* pci_device_scanner */

void pci_device_scanner::pci_device_scanner::rescan_nolock()
{
    std::vector<std::unique_ptr<pcidev::pci_func>> mgmt_devices;
    std::vector<std::unique_ptr<pcidev::pci_func>> user_devices;
    DIR *dir;
    struct dirent *entry;

    dev_list.clear();

    dir = opendir(sysfs_root.c_str());
    if(!dir) {
        std::cout << "Cannot open " << sysfs_root << std::endl;
        return;
    }

    while((entry = readdir(dir))) {
        std::unique_ptr<pcidev::pci_func> pf(
            new pcidev::pci_func(std::string(entry->d_name)));
        if(pf->vendor_id == INVALID_ID)
            continue;

        if (pf->mgmt) {
            mgmt_devices.push_back(std::move(pf));
        } else {
            user_devices.push_back(std::move(pf));
        }
    }

    (void) closedir(dir);

    num_ready = add_to_device_list(mgmt_devices, user_devices, dev_list);
}

pci_device_scanner::pci_device_scanner()
{
    rescan_nolock();
}

void pcidev::rescan(void)
{
    return pci_device_scanner::get_scanner()->rescan();
}

size_t pcidev::get_dev_ready()
{
    return pci_device_scanner::get_scanner()->num_ready;
}

size_t pcidev::get_dev_total()
{
    return pci_device_scanner::get_scanner()->dev_list.size();
}

const pcidev::pci_device* pcidev::get_dev(int index)
{
    return pci_device_scanner::get_scanner()->dev_list[index].get();
}
