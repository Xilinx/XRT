/*
 * Copyright (C) 2019-2020, Xilinx Inc - All rights reserved
 * Xilinx Debug & Profile (XDP) APIs
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

#include "profile_ip_access.h"

namespace xdp {

ProfileIP::ProfileIP(xclDeviceHandle handle, int index) {
    // initialize member variables
    exclusive = false;
    mapped = false;
    mapped_address = 0;
    device_handle = nullptr;
    ip_index = -1;

    // check for exclusive access to this IP
    request_exclusive_ip_access(handle, index);
    if (exclusive) {
        device_handle = handle;
        ip_index = index;
    } else {
        show_warning();
    }
}

ProfileIP::~ProfileIP() {
    if (mapped) {
        unmap();
    }
    if (exclusive) {
        release_exclusive_ip_access(device_handle, ip_index);
    }
} 

void ProfileIP::request_exclusive_ip_access(xclDeviceHandle handle, int index) {
    exclusive = true;
    return;
}

void ProfileIP::release_exclusive_ip_access(xclDeviceHandle handle, int index) {
    exclusive = false;
    return;
}

void ProfileIP::map() {
    if (!exclusive) {
        return;
    }
    std::string subdev = "icap";
    std::string entry = "debug_ip_layout";
    size_t max_path_size = 256;
    char raw_sysfs_path[max_path_size];
    int sysfs_ret = xclGetSysfsPath(device_handle, subdev.c_str(), entry.c_str(), raw_sysfs_path, max_path_size);
    if (sysfs_ret < 0) {
        show_warning();
        return;
    }
    std::string sysfs_open_path(raw_sysfs_path);
    return;
}

void ProfileIP::unmap() {
    return;
}

int read(size_t offset, size_t size, void* data) {
    return 0;
}

int write(size_t offset, size_t size, void* data) {
    return 0;
}

} //  xdp