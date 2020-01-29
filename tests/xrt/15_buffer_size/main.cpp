/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <list>
#include <random>

// driver includes
#include "ert.h"

// host_src includes
#include "xclhal2.h"
#include "xclbin.h"
#include <cstring>

// lowlevel common include
#include "utils.h"

#define TEST_SIZE 0x40000000

#define TEST_SIZE_VCU1550   0x20000000 //
#define TEST_SIZE_HBM       0x8000000 // 

static uint64_t test_size = TEST_SIZE;
/**
 * Tests DMA transfer with various buffer sizes.
 */

const static struct option long_options[] = {
{"hal_driver",        required_argument, 0, 's'},
{"bitstream",         required_argument, 0, 'k'},
{"hal_logfile",       required_argument, 0, 'l'},
{"device",            required_argument, 0, 'd'},
{"alignment",         required_argument, 0, 'a'},
{"verbose",           no_argument,       0, 'v'},
{"help",              no_argument,       0, 'h'},
{0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s [options]\n\n";
    std::cout << "  -s <hal_driver>\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -l <hal_logfile>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -a <alignment>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* If HAL driver is not specified, application will try to find the HAL driver\n";
    std::cout << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n";
    std::cout << "* Bitstream is optional for PR platforms since they already have the base platform\n";
    std::cout << "  hardened and can do the DMA all by themselves\n";
    std::cout << "* HAL logfile is optional but useful for capturing messages from HAL driver\n";
}

static uint64_t getMemBankSize(xclDeviceHandle &handle,axlf_section_kind kind,uint32_t mem_idx)
{
    /*
    uint64_t section_size;
    struct mem_data section_info;
    xclDeviceInfo2 info;
    bool is_vcu1550;
    is_vcu1550 = std::memcmp(info.mName, "xilinx_vcu1550_dynamic_5_0", 26) ? false : true;
    if(is_vcu1550){
      return TEST_SIZE_VCU1550;
    }
    else{
      return TEST_SIZE;
    }
*/

#if 0
    uint64_t section_size;
    struct mem_data section_info;
    int ret = xclGetSectionInfo(handle, &section_info, &section_size, kind, mem_idx);
    if(ret)
        return 0xFFFFFFFF;
    std::cout <<"Mem Bank["<<mem_idx<<"]:"<< (section_info.m_size >> 10) <<"MB\n";
    return section_info.m_size;
#else
    xclDeviceInfo2 info;
    if (xclGetDeviceInfo2(handle, &info)) {
        std::cout << "Device query failed\n" << "FAILED TEST\n";
        return 1;
    }
    bool is_vcu1550;
    bool is_u280;
    bool is_u50;
    is_vcu1550 = std::memcmp(info.mName, "xilinx_vcu1550_dynamic_5_0", 26) ? false : true;
    is_u280    = std::memcmp(info.mName, "xilinx_u280", 11) ? false : true;
    is_u50     = std::memcmp(info.mName, "xilinx_u50", 10) ? false : true;
    if(is_vcu1550){
        return TEST_SIZE_VCU1550;
    }
    else if (is_u280 || is_u50) {
        return TEST_SIZE_HBM;
    }
    else{
        return TEST_SIZE;
    }
#endif

}
static int transferSizeTest1(xclDeviceHandle &handle, size_t alignment, unsigned maxSize, int first_mem, unsigned cu_index, uuid_t xclbinId)
{
    if (xclOpenContext(handle, xclbinId, cu_index, true))
        throw std::runtime_error("Cannot create context");

    std::cout << "transferSizeTest1 start\n";
    std::cout << "Allocate two buffers with size: " << maxSize/1024 << " KBytes ...\n";
    unsigned int boHandle1 = xclAllocBO(handle, maxSize, 0, first_mem); //buf1
    unsigned int boHandle2 = xclAllocBO(handle, maxSize, 0, first_mem); // buf2
    unsigned *writeBuffer = (unsigned *)xclMapBO(handle, boHandle1, true);
    memset(writeBuffer, 0, maxSize);
    std::list<uint64_t> deviceHandleList;
    for(unsigned j = 0; j < maxSize/4; j++){
        writeBuffer[j] = std::rand();
    }

    std::cout << "Running test with various transfer sizes...\n";

    size_t size = 1;
    bool flag = true;
    for (unsigned i = 0; flag; i++) {
        size <<= i;
        if (size > maxSize) {
            size = maxSize;
            flag = false;
        }
        
        if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_TO_DEVICE , size,0)) {
            std::cout << "FAILED TEST\n";
            std::cout << size << " B write failed\n";
            return 1;
        }
        
        xclBOProperties p;
        uint64_t bo2devAddr = !xclGetBOProperties(handle, boHandle2, &p) ? p.paddr : -1;
        uint64_t bo1devAddr = !xclGetBOProperties(handle, boHandle1, &p) ? p.paddr : -1;

        if( (bo2devAddr == (uint64_t)(-1)) || (bo1devAddr == (uint64_t)(-1)))
            return 1;

        //Allocate the exec_bo
        //unsigned execHandle = xclAllocBO(handle, maxSize, 0, (1<<31));
        //void* execData = xclMapBO(handle, execHandle, true);

        //Get the output;
        if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_FROM_DEVICE, size, 0)) {
            std::cout << "FAILED TEST\n";
            std::cout << size << " B read failed\n";
            return 1;
        }
        unsigned *readBuffer = (unsigned *)xclMapBO(handle, boHandle1, false);
        if (std::memcmp(writeBuffer, readBuffer, size)) {
            std::cout << "FAILED TEST\n";
            std::cout << size << " B verification failed\n";
            return 1;
        }
        munmap(readBuffer, maxSize);
    }

    std::cout << "transferSizeTest1 complete. Release buffer objects.\n";
    munmap(writeBuffer, maxSize);

    xclFreeBO(handle,boHandle1);
    xclFreeBO(handle,boHandle2);
    for (std::list<uint64_t>::const_iterator i = deviceHandleList.begin(), e = deviceHandleList.end(); i != e; ++i)
    {
        //xclFreeBO(deviceHandleList.i,boHandle1);
    }

    xclCloseContext(handle, xclbinId, cu_index);

    return 0;
}

static int transferSizeTest2(xclDeviceHandle &handle, size_t alignment, unsigned maxSize, int first_mem, unsigned cu_index, uuid_t xclbinId)
{
    if (xclOpenContext(handle, xclbinId, cu_index, true))
        throw std::runtime_error("Cannot create context");

    std::cout << "transferSizeTest2 start\n";
    std::cout << "Allocate two buffers with size: " << maxSize/1024 << " KBytes ...\n";
    unsigned boHandle1 = xclAllocBO(handle, maxSize, 0, first_mem); //buf1
    unsigned boHandle2 = xclAllocBO(handle, maxSize, 0, first_mem); // buf2
    unsigned *writeBuffer = (unsigned *)xclMapBO(handle, boHandle1, true);
    memset(writeBuffer, 0, maxSize);
    std::list<uint64_t> deviceHandleList;
    for(unsigned j = 0; j < maxSize/4; j++){
        writeBuffer[j] = std::rand();
    }


    std::cout << "Running transfer test with various buffer sizes...\n";

    for (unsigned i = 1; i < maxSize; i++) {
        size_t size = i;

        if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_TO_DEVICE , size,0)) {
            std::cout << "FAILED TEST\n";
            std::cout << size << " B write failed\n";
            return 1;
        }
        
        xclBOProperties p;
        uint64_t bo2devAddr = !xclGetBOProperties(handle, boHandle2, &p) ? p.paddr : -1;
        uint64_t bo1devAddr = !xclGetBOProperties(handle, boHandle1, &p) ? p.paddr : -1;
        
        if( (bo2devAddr == (uint64_t)(-1)) || (bo1devAddr == (uint64_t)(-1)))
            return 1;

        //Allocate the exec_bo
        //unsigned execHandle = xclAllocBO(handle, maxSize, 0, (1<<31));
        //void* execData = xclMapBO(handle, execHandle, true);
        //Get the output;
        if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_FROM_DEVICE, size, 0)) {
            std::cout << "FAILED TEST\n";
            std::cout << size << " B read failed\n";
            return 1;
        }
        unsigned *readBuffer = (unsigned *)xclMapBO(handle, boHandle1, false);
        if (std::memcmp(writeBuffer, readBuffer, size)) {
            std::cout << "FAILED TEST\n";
            std::cout << size << " B verification failed\n";
            return 1;
        }
        munmap(readBuffer, maxSize);
    }
    std::cout << "transferSizeTest2 complete. Release buffer objects.\n";
    munmap(writeBuffer, maxSize);

    xclFreeBO(handle,boHandle1);
    xclFreeBO(handle,boHandle2);
    return 0;
}

static int bufferSizeTest(xclDeviceHandle &handle, uint64_t totalSize, int first_mem)
{
    std::cout << "Start bufferSizeTest\n";
    std::list<uint64_t> deviceHandleList;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::default_random_engine generator;
    // Buffer size between 4 bytes and 4 MB
    std::uniform_int_distribution<int> dis(4, 0x400000);
    uint64_t maxAddress = 0;
    uint64_t totalAllocationSize = 0;
    // Fill the DDR with random size buffers and measure the utilization
    while (totalAllocationSize < totalSize) {
        size_t size = dis(generator);
        unsigned pos = xclAllocBO(handle, size, 0, first_mem); //buf1
        xclBOProperties p;
        uint64_t bodevAddr = !xclGetBOProperties(handle, pos, &p) ? p.paddr : -1;
        if (bodevAddr == (uint64_t)(-1)) {
            break;
        }

        totalAllocationSize += size;
        if (bodevAddr > maxAddress)
            maxAddress = bodevAddr;
        deviceHandleList.push_back(pos);
    }
    std::cout << "High address = " << std::hex << maxAddress << std::dec << std::endl;
    std::cout << "Total allocation = " << std::hex << totalAllocationSize / 1024 << std::dec << " KB" << std::endl;
    std::cout << "Total count = " << deviceHandleList.size() << std::endl;
    for (std::list<uint64_t>::const_iterator i = deviceHandleList.begin(), e = deviceHandleList.end(); i != e; ++i)
    {
        xclFreeBO(handle, *i);
    }

    const double utilization = static_cast<double>(totalAllocationSize)/static_cast<double>(totalSize);
    if (utilization < 0.6) {
        std::cout << "DDR utilization = " << utilization << std::endl;
        std::cout << "FAILED TEST" <<std::endl;
        return 1;
    }
    if ((maxAddress + 0x400000 * 2) < totalSize) {
        std::cout << "Could not allocate last buffer" << std::endl;
        std::cout << "FAILED TEST" <<std::endl;
        return 1;
    }

    return 0;
}

int main(int argc, char** argv)
{
    std::string sharedLibrary;
    std::string bitstreamFile;
    std::string halLogfile;
    size_t alignment = 128;
    unsigned index = 0;
    int option_index = 0;
    bool verbose = false;
    unsigned cu_index = 0;

    int c;
    //findSharedLibrary(sharedLibrary);
    while ((c = getopt_long(argc, argv, "s:k:l:a:d:vh", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 's':
            sharedLibrary = optarg;
            break;
        case 'k':
            bitstreamFile = optarg;
            break;
        case 'l':
            halLogfile = optarg;
            break;
        case 'a':
            alignment = std::atoi(optarg);
            break;
        case 'd':
            index = std::atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        case 'v':
            verbose = true;
            break;
        default:
            printHelp();
            return 1;
        }
    }

    (void)verbose;

    if (bitstreamFile.size() == 0) {
        std::cout << "No bitstream specified and hence no bitstream will be loaded\n";
    }

    if (halLogfile.size()) {
        std::cout << "Using " << halLogfile << " as HAL driver logfile\n";
    }

    std::cout << "HAL driver = " << sharedLibrary << "\n";
    std::cout << "Host buffer alignment = " << alignment << " bytes\n";
    try {
        xclDeviceHandle handle;
        uint64_t cu_base_addr = 0;
        int first_mem = -1;
        uuid_t xclbinId;

        if (initXRT(bitstreamFile.c_str(), index, halLogfile.c_str(), handle, cu_index, cu_base_addr, first_mem, xclbinId))
            return 1;

        test_size =  getMemBankSize(handle, MEM_TOPOLOGY, 0);

        if (first_mem < 0)
            return 1;

        xclDeviceInfo2 info;
        if (xclGetDeviceInfo2(handle, &info)) {
            std::cout << "Device query failed\n" << "FAILED TEST\n";
            return 1;
        }

        // Max size is 8 MB
        if (transferSizeTest1(handle, alignment, test_size, first_mem, cu_index, xclbinId)  || transferSizeTest2(handle, alignment, 0x400, first_mem, cu_index, xclbinId)) {
            std::cout << "transferSizeTest1 or transferSizeTest2\n";
            std::cout << "FAILED TEST\n";
            return 1;
        }

        // Try to fill half the DDR (8 GB); filling all 16 GB puts enormous
        // memory pressure due to backing pages on host RAM
        //printf("val = 0x%llx\n", info.mDDRSize/4);

        if (bufferSizeTest(handle, info.mDDRSize / 4, first_mem)) { //2
            std::cout << "FAILED TEST\n";
            return 1;
        }
    }
    catch (std::exception const& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
        std::cout << "FAILED TEST\n";
        return 1;
    }

    std::cout << "PASSED TEST\n";
    return 0;
}
