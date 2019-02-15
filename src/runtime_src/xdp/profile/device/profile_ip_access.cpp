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

ProfileIP::ProfileIP(xclDeviceHandle handle, int index) : 
device_handle(nullptr),
mapped_address(0),
mapped(false),
exclusive(false),
ip_index(-1) {
    // check for exclusive access to this IP
    request_exclusive_ip_access(handle, index);
    if (exclusive) {
        device_handle = handle;
        ip_index = index;
    } else {
        show_warning("Cannot get exclusive access");
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
    /**
     * TODO: when the XRT implements the exclusive context hal API, this
     * method should try to open a exclusive context here and set the
     * exclusive flag to be true if successful and save the context object
     * into a member variable. If it fails, it should show a proper warning
     * and return gracefully.
     */
    exclusive = true;
    return;
}

void ProfileIP::release_exclusive_ip_access(xclDeviceHandle handle, int index) {
    /**
     * TODO: when the XRT implements the exclusive context hal API, this
     * method should close the previously requested exclusive context if
     * one was request and then set the exclusive flag back to false. If
     * it fails, it should show a proper warning and return gracefully.
     */
    exclusive = false;
    return;
}

void ProfileIP::map() {
    /**
     * TODO: so far we are asking the debug_ip_layout where the IP is. Once the XRT hal implements
     * the function that maps the IP registers to user memory space, this method should be simplified
     * to one function call to the hal API and saves the result in a mapped_address and set mapped flag.
     */
    if (!exclusive) {
        return;
    }
    std::string subdev = "icap";
    std::string entry = "debug_ip_layout";
    size_t max_path_size = 256;
    char raw_debug_ip_layout_path[max_path_size] = {0};
    int get_sysfs_ret = xclGetSysfsPath(device_handle, subdev.c_str(), entry.c_str(), raw_debug_ip_layout_path, max_path_size);
    if (get_sysfs_ret < 0) {
        show_warning("Get debug_ip_layout path failed");
        return;
    }
    raw_debug_ip_layout_path[max_path_size - 1] = '\0';
    std::string debug_ip_layout_path(raw_debug_ip_layout_path);
    std::ifstream debug_ip_layout_fs(debug_ip_layout_path.c_str(), std::ifstream::binary);
    size_t max_sysfs_size = 65536;
    char buffer[max_sysfs_size] = {0};
    if (debug_ip_layout_fs) {
        debug_ip_layout_fs.read(buffer, max_sysfs_size);
        if (debug_ip_layout_fs.gcount() > 0) {
            debug_ip_layout* layout = reinterpret_cast<debug_ip_layout*>(buffer);
            if (ip_index >= layout->m_count) {
                show_warning("ip_index out of bound");
                return;
            }
            debug_ip_data ip_data = layout->m_debug_ip_data[ip_index];
            ip_name.assign(reinterpret_cast<const char*>(&ip_data.m_name), 64);
            mapped_address = ip_data.m_base_address;
            std::cout << "Mapping " << ip_name << " to address 0x" << std::hex << mapped_address << std::dec << std::endl;
            mapped = true;
            return;
        } else {
            show_warning("Reading from debug_ip_layout failed");
            return;
        }
    } else {
        show_warning("Cannot open debug_ip_layout");
        return;
    }
}

void ProfileIP::unmap() {
    /**
     * TODO: This should use the unmapping API provided by XRT hal in
     * the future. Now the API is not in place
     */
    if (!exclusive || !mapped) {
        return;
    }
    mapped = false;
    mapped_address = 0;
    return;
}

int ProfileIP::read(uint64_t offset, size_t size, void* data) {
    /**
     * TODO: so far we are using xclRead under the hood because the hal API that maps
     * the IP is not ready yet. Once the API is ready, xclRead should be replaced by a
     * memcpy from the mapped address with exception handling.
     */
    if (!exclusive || !mapped) {
        return -1;
    }
    uint64_t absolute_offset = mapped_address + offset;
    size_t read_size = xclRead(device_handle, XCL_ADDR_SPACE_DEVICE_PERFMON, absolute_offset, data, size);
    if (read_size < 0) {
        show_warning("xclRead failed");
        return read_size;
    }
    return 0;
}

int ProfileIP::write(uint64_t offset, size_t size, void* data) {
    /**
     * TODO: so far we are using xclWrite under the hood because the hal API that maps
     * the IP is not ready yet. Once the API is ready, xclWrite should be replaced by a
     * memcpy to the mapped address with exception handling.
     */
    if (!exclusive || !mapped) {
        return -1;
    }
    uint64_t absolute_offset = mapped_address + offset;
    size_t write_size = xclWrite(device_handle, XCL_ADDR_SPACE_DEVICE_PERFMON, absolute_offset, data, size);
    if (write_size < 0) {
        show_warning("xclWrite failed");
        return write_size;
    }
    return 0;
}

void ProfileIP::show_warning(std::string reason) {
    /**
     * TODO: we will need to discuss more on how xdp should
     * handle failure, and what are the effective ways of
     * notifying the user that there is a problem in xde and
     * do not expect any profiling information.
     */
    std::cout << "Error: profiling will not be avaiable. Reason: " << reason << std::endl;
    return;
}

} //  xdp

