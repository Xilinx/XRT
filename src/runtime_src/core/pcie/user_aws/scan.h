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
#ifndef _XCL_SCAN_H_
#define _XCL_SCAN_H_

#include <string>
#include <vector>

#ifndef INTERNAL_TESTING
#define XCLMGMT_NUM_SUPPORTED_CLOCKS 4
#define XCLMGMT_NUM_ACTUAL_CLOCKS 3
#include <fpga_mgmt.h>
#include <fpga_pci.h>
#endif

//TODO: can get this from config.h : PCI_PATH_SYS_BUS_PCI
#define ROOT_DIR "/sys/bus/pci"
#define XILINX_ID 0x1d0f
#define AWS_UserPF_DEVICE_ID 0x1042     //userPF device on AWS F1 & Pegasus
#define AWS_MgmtPF_DEVICE_ID 0x1040     //mgmtPF device on Pegasus (mgmtPF not visible on AWS)
#define AWS_UserPF_DEVICE_ID_SDx 0xf010 //userPF device on AWS F1 after downloading xclbin into FPGA (SHELL 1.4)
#define OBJ_BUF_SIZE 1024
#define DRIVER_BUF_SIZE 1024

namespace xcldev {

std::string get_val_string(std::string& dir, const char* key);
long get_val_int(std::string& dir, const char* key);
int get_render_value(std::string& dir);

std::fstream sysfs_open(const std::string& sysfs_name, const std::string& subdev,
    const std::string& entry, std::string& err,
    bool write = false, bool binary = false);
void sysfs_get(const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<std::string>& sv);
void sysfs_get(const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<uint64_t>& iv);
void sysfs_get(const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::string& s);
void sysfs_get(const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, bool& b);
void sysfs_get(const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, std::vector<char>& buf);
template <typename T>
void sysfs_get(const std::string& sysfs_name, const std::string& subdev, const std::string& entry,
    std::string& err_msg, T& i) {
    std::vector<uint64_t> iv;

    sysfs_get(sysfs_name, subdev, entry, err_msg, iv);
    if (!iv.empty())
        i = static_cast<T>(iv[0]);
    else
        i = static_cast<T>(-1); // default value
}
void sysfs_put(const std::string& subdev, const std::string& entry,
    std::string& err_msg, const std::string& input);

std::string get_sysfs_path(const std::string& sysfs_name, const std::string& subdev,
    const std::string& entry);

class pci_device_scanner {
public:
    struct device_info {
        unsigned user_instance;
        unsigned mgmt_instance;
        std::string user_name;
        std::string mgmt_name;
    };
    static std::vector<struct device_info> device_list; // userpf instance, mgmt instance, device
    int scan(bool print);
    void clear_device_list( void ) { device_list.clear(); }

private:
    struct pci_device {
        int domain;
        uint8_t bus, dev, func;
        uint16_t vendor_id = 0, device_id = 0, subsystem_id = 0;
        uint16_t instance;
        std::string device_name;
        std::string driver_name = "???", driver_version = "??";
    };
    bool add_device(struct pci_device& device);
    bool print_paths();
    bool print_system_info();
    bool print_pci_info();
    bool print_f1_pci_info();
    void add_to_device_list();
    std::vector<pci_device> mgmt_devices;
    std::vector<pci_device> user_devices;
}; /* pci_device_scanner */

} /* xcldev */
#endif /* _XCL_SCAN_H_ */
