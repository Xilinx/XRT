/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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
#include <iomanip>

#define INVALID_ID 0xffff

namespace pcidev {

// One PCIE function on FPGA board
class pci_func {
public:
    uint16_t domain = INVALID_ID;
    uint16_t bus = INVALID_ID;
    uint16_t dev = INVALID_ID;
    uint16_t func = INVALID_ID;
    uint16_t vendor_id = INVALID_ID;
    uint16_t device_id = INVALID_ID;
    uint16_t subsystem_id = INVALID_ID;
    uint32_t instance = INVALID_ID;
    std::string sysfs_name; // dir name under /sys/bus/pci/devices
    std::string driver_name = "???";
    std::string driver_version = "???";
    int user_bar = 0;
    size_t user_bar_size = 0;
    bool mgmt = false;

    pci_func(const std::string& sysfs_name);
    std::fstream sysfs_open(const std::string& subdev,
        const std::string& entry, std::string& err,
        bool write = false, bool binary = false);
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

private:
    std::string get_sysfs_path(const std::string& subdev,
        const std::string& entry);
};

// One FPGA board with multiple PCIE functions
class pci_device {
public:
    std::unique_ptr<pci_func> mgmt;
    std::unique_ptr<pci_func> user;

    bool is_ready;
    bool is_mfg;
    std::string flash_type; // E.g. "spi"
    std::string board_name; // E.g. "u200"
    // More PCIe functions will be appended here...

    pci_device(std::unique_ptr<pci_func>& mgmt,
        std::unique_ptr<pci_func>& user);
};

void rescan(void);
size_t get_dev_total(void);
size_t get_dev_ready(void);
const pci_device* get_dev(int index);
} /* pcidev */

#endif
