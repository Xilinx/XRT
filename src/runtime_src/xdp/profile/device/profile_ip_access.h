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

#ifndef XDP_PROFILE_DEVICE_PROFILE_IP_ACCESS_H_
#define XDP_PROFILE_DEVICE_PROFILE_IP_ACCESS_H_

#include <stdexcept>
#include <fstream>
#include <iostream>
#include "xclhal2.h"
#include "xclbin.h"

namespace xdp {

/**
 * ProfileIP (IP with safe access)
 * 
 * Description:
 * 
 * This class represents the high level exclusive and OS protected 
 * access to a profiling IP on the device.
 * 
 * Note:
 * 
 * This class only aims at providing interface for easy and
 * safe access to a single profiling IP. Managing the 
 * association between IPs and devices should be done in a 
 * different data structure that is built on top of this class.
 */
class ProfileIP {
public:

    /**
     * The constructor takes a device handle and a ip index
     * means that the instance of this class has a one-to-one
     * association with one specific IP on one specific device.
     * During the construction, the exclusive access to this
     * IP will be requested, otherwise exception will be thrown.
     */
    ProfileIP(xclDeviceHandle handle /** < [in] the xrt hal device handle */, 
                int index /** < [in] the index of the IP in debug_ip_layout */);

    /**
     * The exclusive access should be release in the destructor
     * to prevent potential card hang.
     */
    ~ProfileIP();

    /**
     * The request_exclusive_ip_access API tries to claim exclusive
     * access to a certain IP on a certain device through hal
     * (driver) and set the exclusive flag if exclusive access is 
     * granted.
     */
    void request_exclusive_ip_access(xclDeviceHandle handle, int index);

    /**
     * The release_exclusive_ip_access API will release the exclusive
     * access granted to this IP to prevent potential card hang, and clear
     * the exclusive flag if success.
     */
    void release_exclusive_ip_access(xclDeviceHandle handle, int index);

    /**
     * The map API tries to map the IP specified into user space. The 
     * mapped_address and mapped will be set if success. Exception will
     * be thrown if it fails to map.
     */
    void map();

    /**
     * The unmap API tries to clean up the association between user space
     * address and the IP registers. The mapped_address and mapped will
     * be cleared if success. Exception will be thrown if it fails to 
     * unmap.
     */
    void unmap();

    /**
     * The read method act the same way as xclRead with the 
     * following differences:
     *  1. It does not take a base address as the base address
     *     will be determined from the ip_index at construction
     *  2. The address space is OS protected, so if the size
     *     goes out of the allow address space, a excpetion will
     *     be thrown.
     *  3. It provides exclusive access, so if it instantiates
     *     without error, exclusive access to the ip specified 
     *     is guaranteed.
     */
    int read(uint64_t offset, size_t size, void* data);

    /**
     * The write method act the same way as xclWrite with the 
     * following differences:
     *  1. It does not take a base address as the base address
     *     will be determined from the ip_index at construction
     *  2. The address space is OS protected, so if the size
     *     goes out of the allow address space, a excpetion will
     *     be thrown.
     *  3. It provides exclusive access, so if it instantiates
     *     without error, exclusive access to the ip specified 
     *     is guaranteed.
     */
    int write(uint64_t offset, size_t size, void* data);

    /**
     * Since this API as part of the profiling code should not
     * crash the rest of the code, it will need to simply warn
     * the user about the failure and move on.
     */
    void show_warning(std::string reason);

private:
    xclDeviceHandle device_handle; /** < the xrt device handle from the hal layer */
    uint64_t mapped_address; /** < the mapped address in user space used 
                                        to access the registers */
    bool mapped; /** < a flag to keep track of if the ip has been mapped */
    bool exclusive; /** < a flag indicating if the IP has exclusive access */
    int ip_index; /** < the index of the IP in debug_ip_layout */
    std::string ip_name; /** < the string name of the IP for better debuggability */ 

    /**
     * TODO: the exclusive context from hal
     */
};

} //  xdp

#endif

