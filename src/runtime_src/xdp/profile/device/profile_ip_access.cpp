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
#include "xrt/device/device.h"

namespace xdp {

ProfileIP::ProfileIP(void* handle, int index, debug_ip_data* data)
          : xrt_device_handle(nullptr),
            mapped(false),
            exclusive(false),
            ip_index(-1),
            ip_base_address(0),
            mapped_address(0)
{
    // check for exclusive access to this IP
    request_exclusive_ip_access(handle, index);

    // For now, set these to true
    mapped = true;  
    exclusive = true;

    if (exclusive) {
        xrt_device_handle = handle;
        ip_index = index;
        ip_base_address = data->m_base_address;
        ip_name.assign(reinterpret_cast<const char*>(&data->m_name), 128);
        // Strip away extraneous null characters
        ip_name.assign(ip_name.c_str()); 

        mapped_address = 0; 
        /* 0 for now. This will be populated when XRT implements APIs to share the user-space address of Monitor IP registers.
         * Then data can be directly read using those addresses instead of xclRead/xclWrite/xclUnmgdPread
         */
    } else {
        showWarning("Cannot get exclusive access");
    }
}

ProfileIP::~ProfileIP() {
    if (mapped) {
        unmap();
    }
    if (exclusive) {
        release_exclusive_ip_access(xrt_device_handle, ip_index);
    }
} 

void ProfileIP::request_exclusive_ip_access(void* handle, int index) {
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

void ProfileIP::release_exclusive_ip_access(void* handle, int index) {
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
    uint64_t absolute_offset = ip_base_address + offset;
    xrt::device* xrtDevice = (xrt::device*)xrt_device_handle;
    
    size_t read_size = 1;
    xrtDevice->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, absolute_offset, data, size);
//    size_t read_size = xDevice->xclRead(device_handle, XCL_ADDR_SPACE_DEVICE_PERFMON, absolute_offset, data, size);
    if (read_size < 0) {
        showWarning("xclRead failed");
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
    uint64_t absolute_offset = ip_base_address + offset;
    xrt::device* xrtDevice = (xrt::device*)xrt_device_handle;

    size_t write_size = 1;
    xrtDevice->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, absolute_offset, data, size);
//    size_t write_size = xclWrite(xrt_device_handle, XCL_ADDR_SPACE_DEVICE_PERFMON, absolute_offset, data, size);
    if (write_size < 0) {
        showWarning("xclWrite failed");
        return write_size;
    }
    return 0;
}

int ProfileIP::unmgdRead(unsigned flags, void *buf, size_t count, uint64_t offset)
{
    if (!exclusive || !mapped) {
        return -1;
    }
    uint64_t absolute_offset = ip_base_address + offset;
    xrt::device* xrtDevice = (xrt::device*)xrt_device_handle;
    xrtDevice->xclUnmgdPread(flags, buf, count, absolute_offset);
    // warning ?
    return 0;
}

void ProfileIP::showWarning(std::string reason) {
    /**
     * TODO: we will need to discuss more on how xdp should
     * handle failure, and what are the effective ways of
     * notifying the user that there is a problem in xde and
     * do not expect any profiling information.
     */
    std::ostream* outputStream = (out_stream) ? out_stream : (&(std::cout));
    (*outputStream) << "Error: profiling will not be avaiable. Reason: " << reason << std::endl;
    return;
}

void ProfileIP::showProperties()
{
    std::ostream* outputStream = (out_stream) ? out_stream : (&(std::cout));
   
    std::ios_base::fmtflags formatF = outputStream->flags();

    (*outputStream) << "    IP Name : " << ip_name << std::endl
                    << "    Index   : " << ip_index << std::endl
                    << "    Base Address : " << std::hex << ip_base_address << std::endl
                    << std::endl;
    outputStream->flags(formatF);
}

uint32_t ProfileIP::setLogStream(std::ostream* oStream)
{
    if(!oStream)
        return 0;

    out_stream = oStream;
    return 1;
}

} //  xdp

