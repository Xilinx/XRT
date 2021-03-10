/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#ifndef utils_h
#define utils_h

#include "xclbin.h"
#include "xclhal2.h"

#include <stdexcept>
#include <fstream>

static int initXRT(const char*bit, unsigned deviceIndex, const char* halLog, xclDeviceHandle& handle, int cu_index,
	uint64_t& cu_base_addr)
{
    xclDeviceInfo2 deviceInfo;

    if(deviceIndex >= xclProbe()) {
	throw std::runtime_error("Cannot find device index specified");
	return -1;
    }

    handle = xclOpen(deviceIndex, halLog, XCL_INFO);

    if (xclGetDeviceInfo2(handle, &deviceInfo)) {
	throw std::runtime_error("Unable to obtain device information");
	return -1;
    }

    std::cout << "Shell = " << deviceInfo.mName << "\n";
    std::cout << "Index = " << deviceIndex << "\n";
    std::cout << "PCIe = GEN" << deviceInfo.mPCIeLinkSpeed << " x " << deviceInfo.mPCIeLinkWidth << "\n";
    std::cout << "OCL Frequency = " << deviceInfo.mOCLFrequency[0] << " MHz" << "\n";
    std::cout << "DDR Bank = " << deviceInfo.mDDRBankCount << "\n";
    std::cout << "Device Temp = " << deviceInfo.mOnChipTemp << " C\n";
    std::cout << "MIG Calibration = " << std::boolalpha << deviceInfo.mMigCalib << std::noboolalpha << "\n";

    cu_base_addr = 0xffffffffffffffff;
    if (!bit || !std::strlen(bit))
	return 0;

    if(xclLockDevice(handle)) {
	throw std::runtime_error("Cannot lock device");
	return -1;
    }

    char tempFileName[1024];
    std::strcpy(tempFileName, bit);
    std::ifstream stream(bit);
    stream.seekg(0, stream.end);
    int size = stream.tellg();
    stream.seekg(0, stream.beg);

    char *header = new char[size];
    stream.read(header, size);

    if (std::strncmp(header, "xclbin2", 8)) {
        throw std::runtime_error("Invalid bitstream");
    }

    const xclBin *blob = (const xclBin *)header;
    if (xclLoadXclBin(handle, blob)) {
        delete [] header;
        throw std::runtime_error("Bitstream download failed");
    }
    std::cout << "Finished downloading bitstream " << bit << std::endl;

    const axlf* top = (const axlf*)header;
    auto ip = xclbin::get_axlf_section(top, IP_LAYOUT);
    struct ip_layout* layout =  (ip_layout*) (header + ip->m_sectionOffset);

    if(cu_index > layout->m_count) {
        delete [] header;
        throw std::runtime_error("Cant determine cu base address");
    }

    int cur_index = 0;
    for (int i =0; i < layout->m_count; ++i) {
	if(layout->m_ip_data[i].m_type != IP_KERNEL)
	    continue;
	if(cur_index++ == cu_index) {
	    cu_base_addr = layout->m_ip_data[i].m_base_address;
	    std::cout << "base_address " << std::hex << cu_base_addr << std::dec << std::endl;
	}
    }

    delete [] header;

    return 0;
}


#endif
