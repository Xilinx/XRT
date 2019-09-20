/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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
#include <cassert>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <sys/stat.h>
#include <sys/file.h>
#include <poll.h>
#include "scan.h"

// Supported vendors
#define XILINX_ID       0x10ee
#define ADVANTECH_ID    0x13fe
#define AWS_ID          0x1d0f

#define RENDER_NM       "renderD"

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

std::string pcidev::pci_device::get_sysfs_path(const std::string& subdev,
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

std::fstream pcidev::pci_device::sysfs_open(const std::string& subdev,
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

void pcidev::pci_device::sysfs_put(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, const std::string& input)
{
    std::fstream fs = sysfs_open(subdev, entry, err_msg, true, false);
    if (!err_msg.empty())
        return;
    fs << input;
    fs.flush();
    if (!fs.good()) {
        std::stringstream ss;
        ss << "Failed to write " << get_sysfs_path(subdev, entry) << ": "
            << strerror(errno) << std::endl;
        err_msg = ss.str();
    }
}

void pcidev::pci_device::sysfs_put(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, const std::vector<char>& buf)
{
    std::fstream fs = sysfs_open(subdev, entry, err_msg, true, true);
    if (!err_msg.empty())
        return;

    fs.write(buf.data(), buf.size());
    fs.flush();
    if (!fs.good()) {
        std::stringstream ss;
        ss << "Failed to write " << get_sysfs_path(subdev, entry) << ": "
            << strerror(errno) << std::endl;
        err_msg = ss.str();
    }
}

void pcidev::pci_device::sysfs_get(
    const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<char>& buf)
{
    std::fstream fs = sysfs_open(subdev, entry, err_msg, false, true);
    if (!err_msg.empty())
        return;

    buf.insert(std::end(buf),std::istreambuf_iterator<char>(fs),
        std::istreambuf_iterator<char>());
}

void pcidev::pci_device::sysfs_get(
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

void pcidev::pci_device::sysfs_get(
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

void pcidev::pci_device::sysfs_get(
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

void pcidev::pci_device::sysfs_get(
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

static std::string get_devfs_path(bool is_mgmt, uint32_t instance)
{
    std::string prefixStr = is_mgmt ? "/dev/xclmgmt" : "/dev/dri/" RENDER_NM;
    std::string instStr = std::to_string(instance);

    return prefixStr + instStr;
}

int pcidev::pci_device::open(const std::string& subdev, int flag)
{
    // Open xclmgmt/xocl node
    if (subdev.empty()) {
        std::string devfs = get_devfs_path(is_mgmt, instance);
        return ::open(devfs.c_str(), flag);
    }

    // Open subdevice node
    std::string file("/dev/xfpga/");
    file += subdev;
    file += is_mgmt ? ".m" : ".u";
    file += std::to_string((domain<<16) + (bus<<8) + (dev<<3) + func);
    return ::open(file.c_str(), flag);
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

static bool devfs_exists(bool is_mgmt, uint32_t instance)
{
    struct stat buf;
    std::string devfs = get_devfs_path(is_mgmt, instance);

    return (stat(devfs.c_str(), &buf) == 0);
}

pcidev::pci_device::pci_device(const std::string& sysfs) : sysfs_name(sysfs)
{
    const std::string dir = sysfs_root + sysfs;
    std::string err;
    std::vector<pci_device> mgmt_devices;
    std::vector<pci_device> user_devices;
    uint16_t dom, b, d, f, vendor;
    bool mgmt = false;

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
    if((vendor != XILINX_ID) && (vendor != ADVANTECH_ID) && (vendor != AWS_ID))
        return;

    // Determine if the device is mgmt or user pf.
    std::string tmp;
    sysfs_get("", "mgmt_pf", err, tmp);
    if (err.empty()) {
        mgmt = true;
    } else {
        sysfs_get("", "user_pf", err, tmp);
        if (err.empty()) {
            mgmt = false;
        } else {
            return; // device not recognized
        }
    }

    uint32_t inst = INVALID_ID;
    if (mgmt)
        sysfs_get("", "instance", err, inst);
    else
        inst = get_render_value(dir + "/drm");
    if (!devfs_exists(mgmt, inst))
        return; // device node is not available

    // Found a supported PCIE function.
    domain = dom;
    bus = b;
    dev = d;
    func = f;
    sysfs_get("", "userbar", err, user_bar);
    user_bar_size = bar_size(dir, user_bar);
    is_mgmt = mgmt;
    instance = inst;
    sysfs_get("", "ready", err, is_ready);
}

pcidev::pci_device::~pci_device()
{
    if (user_bar_map != MAP_FAILED)
        munmap(user_bar_map, user_bar_size);
}

int pcidev::pci_device::map_usr_bar()
{
        std::lock_guard<std::mutex> l(lock);

        if (user_bar_map != MAP_FAILED)
            return 0;

        int dev_handle = open("", O_RDWR);
        if (dev_handle < 0)
            return -errno;

        user_bar_map = (char *)::mmap(0, user_bar_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, dev_handle, 0);

        // Mapping should stay valid after handle is closed
        // (according to man page)
        (void)close(dev_handle);

        if (user_bar_map == MAP_FAILED)
            return -errno;

        return 0;
}

void pcidev::pci_device::close(int dev_handle)
{
    if (dev_handle != -1)
        (void)::close(dev_handle);
}

/*
 * wordcopy()
 *
 * Copy bytes word (32bit) by word.
 * Neither memcpy, nor std::copy work as they become byte copying
 * on some platforms.
 */
inline void* wordcopy(void *dst, const void* src, size_t bytes)
{
    // assert dest is 4 byte aligned
    assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

    using word = uint32_t;
    auto d = reinterpret_cast<word*>(dst);
    auto s = reinterpret_cast<const word*>(src);
    auto w = bytes/sizeof(word);

    for (size_t i=0; i<w; ++i)
        d[i] = s[i];

    return dst;
}

int pcidev::pci_device::pcieBarRead(uint64_t offset, void* buf, uint64_t len)
{
    if (user_bar_map == MAP_FAILED) {
        int ret = map_usr_bar();
        if (ret)
            return ret;
    }
    (void) wordcopy(buf, user_bar_map + offset, len);
    return 0;
}

int pcidev::pci_device::pcieBarWrite(uint64_t offset,
    const void* buf, uint64_t len)
{
    if (user_bar_map == MAP_FAILED) {
        int ret = map_usr_bar();
        if (ret)
            return ret;
    }
    (void) wordcopy(user_bar_map + offset, buf, len);
    return 0;
}

int pcidev::pci_device::ioctl(int dev_handle, unsigned long cmd, void *arg)
{
    if (dev_handle == -1) {
        errno = -EINVAL;
        return -1;
    }
    return ::ioctl(dev_handle, cmd, arg);
}

int pcidev::pci_device::poll(int dev_handle, short events, int timeoutMilliSec)
{
    pollfd info = {dev_handle, events, 0};
    return ::poll(&info, 1, timeoutMilliSec);
}

void *pcidev::pci_device::mmap(int dev_handle,
    size_t len, int prot, int flags, off_t offset)
{
    if (dev_handle == -1) {
        errno = -EINVAL;
        return MAP_FAILED;
    }
    return ::mmap(0, len, prot, flags, dev_handle, offset);
}

int pcidev::pci_device::flock(int dev_handle, int op)
{
    if (dev_handle == -1) {
        errno = -EINVAL;
        return -1;
    }
    return ::flock(dev_handle, op);
}

class pci_device_scanner {
public:

    static pci_device_scanner *get_scanner()
    {
        static pci_device_scanner scanner;
        return &scanner;
    }

    void rescan()
    {
        std::lock_guard<std::mutex> l(lock);
        rescan_nolock();
    }

    size_t get_num_ready(bool is_user)
    {
        std::lock_guard<std::mutex> l(lock);
        return is_user ? num_user_ready : num_mgmt_ready;
    }

    size_t get_num_total(bool is_user)
    {
        std::lock_guard<std::mutex> l(lock);
        return is_user ? user_list.size() : mgmt_list.size();
    }

    const std::shared_ptr<pcidev::pci_device> get_dev(unsigned index,bool user)
    {
        std::lock_guard<std::mutex> l(lock);
        auto list = user ? &user_list : &mgmt_list;
        if (index >= list->size())
            return nullptr;
        return (*list)[index];
    }

private:

    // Full list of discovered user devices. Index 0 ~ (num_user_ready - 1) are
    // boards ready for use. The rest, if any, are not ready, according to what
    // is indicated by driver's "ready" sysfs entry. The applications only see
    // ready-for-use boards since xclProbe returns num_user_ready, not the size
    // of the full list.
    std::vector<std::shared_ptr<pcidev::pci_device>> user_list;
    size_t num_user_ready;

    // Full list of discovered mgmt devices. Index 0 ~ (num_mgmt_ready - 1) are
    // boards ready for use. The rest, if any, are not ready, according to what
    // is indicated by driver's "ready" sysfs entry. Application does not see
    // mgmt devices.
    std::vector<std::shared_ptr<pcidev::pci_device>> mgmt_list;
    size_t num_mgmt_ready;

    std::mutex lock;
    void rescan_nolock();
    pci_device_scanner() {
        rescan_nolock();
    }
    pci_device_scanner(const pci_device_scanner& s);
    pci_device_scanner& operator=(const pci_device_scanner& s);
};

static bool is_in_use(std::vector<std::shared_ptr<pcidev::pci_device>>& vec)
{
    for (auto& d : vec)
        if (d.use_count() > 1)
            return true;
    return false;
}

void pci_device_scanner::pci_device_scanner::rescan_nolock()
{
    DIR *dir;
    struct dirent *entry;

    if (is_in_use(user_list) || is_in_use(mgmt_list)) {
        std::cout << "Device list is in use, can't rescan" << std::endl;
        return;
    }

    user_list.clear();
    mgmt_list.clear();

    dir = opendir(sysfs_root.c_str());
    if(!dir) {
        std::cout << "Cannot open " << sysfs_root << std::endl;
        return;
    }

    while((entry = readdir(dir))) {
        auto pf = std::make_shared<pcidev::pci_device>(
            std::string(entry->d_name));
        if(pf->domain == INVALID_ID)
            continue;

        auto list = pf->is_mgmt ? &mgmt_list : &user_list;
        auto num_ready = pf->is_mgmt ? &num_mgmt_ready : &num_user_ready;
        if (pf->is_ready) {
            list->insert(list->begin(), pf);
            ++(*num_ready);
        } else {
            list->push_back(pf);
        }
    }

    (void) closedir(dir);
}


void pcidev::rescan(void)
{
    pci_device_scanner::get_scanner()->rescan();
}

size_t pcidev::get_dev_ready(bool user)
{
    return pci_device_scanner::get_scanner()->get_num_ready(user);
}

size_t pcidev::get_dev_total(bool user)
{
    return pci_device_scanner::get_scanner()->get_num_total(user);
}

std::shared_ptr<pcidev::pci_device> pcidev::get_dev(unsigned index, bool user)
{
    return pci_device_scanner::get_scanner()->get_dev(index, user);
}

std::ostream& operator<<(std::ostream& stream,
    const std::shared_ptr<pcidev::pci_device>& dev)
{
    std::ios_base::fmtflags f(stream.flags());

    stream << std::hex << std::right << std::setfill('0');

    // [dddd:bb:dd.f]
    stream << std::setw(4) << dev->domain << ":"
        << std::setw(2) << dev->bus << ":"
        << std::setw(2) << dev->dev << "."
        << std::setw(1) << dev->func;

    // board/shell name
    std::string shell_name;
    std::string err;
    bool is_mfg = false;
    uint64_t ts = 0;
    dev->sysfs_get("", "mfg", err, is_mfg);
    if (is_mfg) {
        unsigned ver = 0;
        std::string nm;

        dev->sysfs_get("", "board_name", err, nm);
        dev->pcieBarRead(MFG_REV_OFFSET, &ver, sizeof(ver));
        shell_name += "xilinx_";
        shell_name += nm;
        shell_name += "_GOLDEN_";
        shell_name += std::to_string(ver);
    } else {
        dev->sysfs_get("rom", "VBNV", err, shell_name);
        dev->sysfs_get("rom", "timestamp", err, ts);
    }
    stream << " " << shell_name;
    if (ts != 0)
        stream << "(ts=0x" << std::hex << ts << ")";

    // instance number
    if (dev->is_mgmt)
        stream << " mgmt(inst=";
    else
        stream << " user(inst=";
    stream << std::dec << dev->instance << ")";

    stream.flags(f);
    return stream;
}
