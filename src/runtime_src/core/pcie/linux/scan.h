/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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
#ifndef _XCL_SCAN_H_
#define _XCL_SCAN_H_

#include <string>
#include <vector>
#include <memory>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mutex>

#define INVALID_ID      0xffff
#define MFG_REV_OFFSET  0x131008 // For obtaining Golden image version number

#define FDT_BEGIN_NODE  0x1
#define FDT_END_NODE    0x2
#define FDT_PROP        0x3
#define FDT_NOP         0x4
#define FDT_END         0x9
#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long)(p), (a))))
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};


namespace pcidev {

// One PCIE function on FPGA board
class pci_device {
public:
    // Fundamental and static information for this device are defined as class
    // members and initialized during object construction.
    //
    // The rest of information related to the device shall be obtained
    // dynamically via sysfs APIs below.

    uint16_t domain =           INVALID_ID;
    uint16_t bus =              INVALID_ID;
    uint16_t dev =              INVALID_ID;
    uint16_t func =             INVALID_ID;
    uint32_t instance =         INVALID_ID;
    std::string sysfs_name =    ""; // dir name under /sys/bus/pci/devices
    int user_bar =              0;  // BAR mapped in by tools, default is BAR0
    size_t user_bar_size =      0;
    bool is_mgmt =              false;
    bool is_ready =             false;

    pci_device(const std::string& sysfs_name);
    ~pci_device();

    void sysfs_get(const std::string& subdev, const std::string& entry,
        std::string& err_msg, std::vector<std::string>& sv);
    void sysfs_get(const std::string& subdev, const std::string& entry,
        std::string& err_msg, std::vector<uint64_t>& iv);
    void sysfs_get(const std::string& subdev, const std::string& entry,
        std::string& err_msg, std::string& s);
    void sysfs_get(const std::string& subdev, const std::string& entry,
        std::string& err_msg, bool& b);
    void sysfs_get(const std::string& subdev, const std::string& entry,
        std::string& err_msg, std::vector<char>& buf);
    template <typename T>
    void sysfs_get(const std::string& subdev, const std::string& entry,
        std::string& err_msg, T& i) {
        std::vector<uint64_t> iv;

        sysfs_get(subdev, entry, err_msg, iv);
        if (!iv.empty())
            i = static_cast<T>(iv[0]);
        else
            i = static_cast<T>(-1); // default value
    }
    void sysfs_put(const std::string& subdev, const std::string& entry,
        std::string& err_msg, const std::string& input);
    void sysfs_put(const std::string& subdev, const std::string& entry,
        std::string& err_msg, const std::vector<char>& buf);
    std::string get_sysfs_path(const std::string& subdev,
        const std::string& entry);

    int pcieBarRead(uint64_t offset, void *buf, uint64_t len);
    int pcieBarWrite(uint64_t offset, const void *buf, uint64_t len);

    int open(const std::string& subdev, int flag);
    void close(int devhdl);
    int ioctl(int devhdl, unsigned long cmd, void *arg = nullptr);
    int poll(int devhdl, short events, int timeoutMilliSec);
    void *mmap(int devhdl, size_t len, int prot, int flags, off_t offset);
    int flock(int devhdl, int op);
    int get_partinfo(std::vector<std::string>& info, void *blob = nullptr);

private:
    std::fstream sysfs_open(const std::string& subdev,
        const std::string& entry, std::string& err,
        bool write = false, bool binary = false);
    int map_usr_bar(void);

    std::mutex lock;
    char *user_bar_map = reinterpret_cast<char *>(MAP_FAILED);
};

void rescan(void);
size_t get_dev_total(bool user = true);
size_t get_dev_ready(bool user = true);
std::shared_ptr<pci_device> get_dev(unsigned index, bool user = true);

int get_axlf_section(std::string filename, int kind, std::shared_ptr<char>& buf);
int get_uuids(std::shared_ptr<char>& dtbbuf, std::vector<std::string>& uuids);
} /* pcidev */

// For print out per device info
std::ostream& operator<<(std::ostream& stream,
    const std::shared_ptr<pcidev::pci_device>& dev);

#endif
