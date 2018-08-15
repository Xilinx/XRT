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

//TODO: can get this from config.h : PCI_PATH_SYS_BUS_PCI
#define ROOT_DIR "/sys/bus/pci"
#define XILINX_ID    0x10ee
#define ADVANTECH_ID 0x13fe
#define OBJ_BUF_SIZE 1024
#define DRIVER_BUF_SIZE 1024
#define INVALID_DEV 0xffff

namespace xcldev {

std::string get_val_string(std::string& dir, const char* key);
long get_val_long(std::string& dir, const char* key);
int get_render_value(std::string& dir);

class pci_device_scanner {
public:
    struct device_info {
        unsigned user_instance;
        unsigned mgmt_instance;
        std::string user_name;
        std::string mgmt_name;
        int user_bar;
        size_t user_bar_size;
        int domain;
        uint8_t bus, device, mgmt_func, user_func;
	std::string flash_type;
    };
    static std::vector<struct device_info> device_list; // userpf instance, mgmt instance, device
    int scan(bool print);
    int scan_without_driver( void );
    bool get_mgmt_device_name(std::string &devName, unsigned int devIdx);
    int get_feature_rom_bar_offset(unsigned int devIdx, unsigned long long &offset);

private:
    struct pci_device {
        int domain;
        uint8_t bus, dev, func;
        uint16_t vendor_id = 0, device_id = 0, subsystem_id = 0;
        uint16_t instance = INVALID_DEV;
        std::string device_name;
        std::string driver_name = "???", driver_version = "???";
        int user_bar;
        size_t user_bar_size;
	std::string flash_type;
    };
    bool add_device(struct pci_device& device);
    bool print_paths();
    bool print_system_info();
    bool print_pci_info();
    void add_to_device_list( bool skipValidDeviceCheck = false );
    const size_t bar_size(const std::string &dir, unsigned bar);
    std::vector<pci_device> mgmt_devices;
    std::vector<pci_device> user_devices;
}; /* pci_device_scanner */

} /* xcldev */

#endif


