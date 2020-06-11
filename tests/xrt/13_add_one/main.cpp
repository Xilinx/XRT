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
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <time.h>
#include <chrono>
#include <thread>

// driver includes
#include "ert.h"

// host_src includes
#include "xclhal2.h"
#include "xclbin.h"

// lowlevel common include
#include "utils.h"

#if defined(DSA64)
#include "xaddone_hw_64.h"     
#else
#include "xaddone_hw.h"  
#endif

#define ARRAY_SIZE 8
#define CONTROL_SIZE 0x8000

static size_t num_commands = 1;
static size_t throttle=500;

static size_t
convert(const std::string& str)
{
    return str.empty() ? 0 : std::stoul(str,0,0);
}

/**
 * Runs an OpenCL kernel which writes known 16 integers into a 64 byte buffer. Does not use OpenCL
 * runtime but directly exercises the HAL driver API.
 */


const static struct option long_options[] = {
    // number of commands to start.  emulate workgroups
    // the same command is started specified number of times
    // (default: 1)
{"num_commands",    required_argument, 0, '5'},

// throttle status register reading (default: 500ns)
{"throttle",        required_argument, 0, '6'},

{"hal_driver",      required_argument, 0, 's'},
{"bitstream",       required_argument, 0, 'k'},
{"hal_logfile",     required_argument, 0, 'l'},
{"device",          required_argument, 0, 'd'},
{"num of elments",  required_argument, 0, 'n'},
{"verbose",         no_argument,       0, 'v'},
{"help",            no_argument,       0, 'h'},
{0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n"
              << "  [--num_commands=<number of commands to send to write to command queue>] (default: 1)\n"
              << "  [--throttle=<throttle interval in ns for checkin kernel status>] (default: 500)\n"
              << "\n"
              << "  -s <hal_driver>\n"
              << "  -k <bitstream>\n"
              << "  -l <hal_logfile>\n"
              << "  -d <index>\n"
              << "  -n <num of elements, default is 16>\n"
              << "  -c <compute_unit_index>\n"
              << "  -v\n"
              << "  -h\n\n"
              << "* If HAL driver is not specified, application will try to find the HAL driver\n"
              << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n"
              << "* Bitstream is required\n"
              << "* HAL logfile is optional but useful for capturing messages from HAL driver\n";
}

static int runKernel(xclDeviceHandle &handle, uint64_t cu_base_addr, size_t alignment, bool ert, bool verbose, size_t n_elements, int first_mem, unsigned cu_index, uuid_t xclbinId)
{
    if(xclOpenContext(handle, xclbinId, cu_index, true))
        throw std::runtime_error("Cannot create context");

    const size_t DATA_SIZE = n_elements * ARRAY_SIZE;

    unsigned boHandle1 = xclAllocBO(handle, DATA_SIZE, 0, first_mem); //input a
    unsigned boHandle2 = xclAllocBO(handle, DATA_SIZE, 0, first_mem); // output b
    unsigned long *bo1 = (unsigned long*)xclMapBO(handle, boHandle1, true);
    memset(bo1, 0, DATA_SIZE);
    unsigned long *bo2 = (unsigned long*)xclMapBO(handle, boHandle2, false);

    if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_TO_DEVICE , DATA_SIZE,0))
        return 1;

    unsigned long sw_results[DATA_SIZE];
    xclBOProperties p;
    uint64_t bo2devAddr = !xclGetBOProperties(handle, boHandle2, &p) ? p.paddr : -1;
    uint64_t bo1devAddr = !xclGetBOProperties(handle, boHandle1, &p) ? p.paddr : -1;

    if( (bo2devAddr == (uint64_t)(-1)) || (bo1devAddr == (uint64_t)(-1)))
        return 1;


    //Allocate the exec_bo
    unsigned execHandle = xclAllocBO(handle, DATA_SIZE, 0, (1<<31));
    void* execData = xclMapBO(handle, execHandle, true);

    // Fill our data sets with pattern
    bool bRandom = false;
    std::srand(std::time(0));
    for(size_t i = 0; i < DATA_SIZE; i++) {
        bo1[i] = bRandom ? std::rand() : i;
        //printf("i: %d, bo1: %d\n",i,bo1[i]);
        //bo2[i] = 0x55;
    }

    std::cout << "Construct the exec command to run the kernel on FPGA" << std::endl;

    //construct the exec buffer cmd to start the kernel.
    {
        auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(execData);
        auto rsz = XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4 + 1; // regmap array size
        std::memset(ecmd,0,(sizeof *ecmd) + rsz);
        ecmd->state = ERT_CMD_STATE_NEW;
        ecmd->opcode = ERT_START_CU;
        ecmd->count = 1 + rsz;
        ecmd->cu_mask = 0x1;

        ecmd->data[XADDONE_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
        ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4] = bo1devAddr;
        ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4] = bo2devAddr;
#if defined(DSA64)
        ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4 + 1] = (bo1devAddr >> 32) & 0xFFFFFFFF; // input
        ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4 + 1] = (bo2devAddr >> 32) & 0xFFFFFFFF; // output
#endif
        ecmd->data[XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4] = n_elements;
    }


    //Send the "start kernel" command.
    if(xclExecBuf(handle, execHandle)) {
        std::cout << "Unable to issue xclExecBuf : start_kernel" << std::endl;
        std::cout << "FAILED TEST\n";
        std::cout << "Write failed\n";
        return 1;
    }
    else {
        std::cout << "Kernel start command issued through xclExecBuf : start_kernel" << std::endl;
        std::cout << "Now wait until the kernel finish" << std::endl;
    }


    //Wait on the command finish
    while (xclExecWait(handle,1000) == 0) {
        std::cout << "reentering wait...\n";
    };

    //Get the output;
    std::cout << "Get the output data from the device" << std::endl;
    if(xclSyncBO(handle, boHandle2, XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0)) {
        return 1;
    }

    // Validate our results
    //
    int err = 0;
    for(size_t i = 0; i < DATA_SIZE; i++)
    {
        if (i % ARRAY_SIZE == 0)
            sw_results[i] = bo1[i]+1;
        else
            sw_results[i] = bo1[i];
    }

    for (size_t col = 0; col < n_elements; col++) {
        bool bShowResult = verbose;
        for (size_t row = 0; row < ARRAY_SIZE; row++ ) {
            if(bo2[col*ARRAY_SIZE + row] != sw_results[col*ARRAY_SIZE + row]) {
                err++;
                bShowResult = true;
                break;
            }
        }
        if (bShowResult)
        {
            std::cout<<std::hex<<"["<<col<<"]=";
            for (size_t  row = 0; row < ARRAY_SIZE; row++ )
                std::cout<<bo2[col*ARRAY_SIZE + row]<<" ";
            std::cout<<std::endl;
        }
    }

    //Clean up stuff
    munmap(bo1, DATA_SIZE);
    munmap(bo2, DATA_SIZE);
    munmap(execData, DATA_SIZE);
    xclFreeBO(handle, boHandle1);
    xclFreeBO(handle, boHandle2);
    xclFreeBO(handle, execHandle);

    xclCloseContext(handle, xclbinId, cu_index);

    return 0;
}


int main(int argc, char** argv)
{
    std::string sharedLibrary;
    std::string bitstreamFile;
    std::string halLogfile;
    size_t alignment = 128;
    int option_index = 0;
    unsigned index = 0;
    unsigned cu_index = 0;
    bool verbose = false;
    bool ert = false;
    int c;
    size_t n_elements = 16;
    //int cu = 0;
    //findSharedLibrary(sharedLibrary);

    while ((c = getopt_long(argc, argv, "c:s:k:l:n:d:vh", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case '5':
            num_commands = convert(optarg);
            break;
        case '6':
            throttle = convert(optarg);
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
        case 'd':
            index = std::atoi(optarg);
            break;
        case 'c':
            //cu = std::atoi(optarg);
            break;
        case 'n':
            n_elements = std::atoi(optarg);
            if (n_elements == 0) {
                std::cout<<"ERR: num of elements can't be set to 0."<<std::endl;
                return 1;
            }
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
        std::cout << "FAILED TEST\n";
        std::cout << "No bitstream specified\n";
        return -1;
    }

    if (halLogfile.size()) {
        std::cout << "Using " << halLogfile << " as HAL driver logfile\n";
    }

    std::cout << "HAL driver = " << sharedLibrary << "\n";
    std::cout << "Host buffer alignment = " << alignment << " bytes\n";
    std::cout << "Compiled kernel = " << bitstreamFile << "\n";


    try {
        xclDeviceHandle handle;
        uint64_t cu_base_addr = 0;
        int first_mem = -1;
        uuid_t xclbinId;
        if (initXRT(bitstreamFile.c_str(), index, halLogfile.c_str(), handle, cu_index, cu_base_addr, first_mem, xclbinId))
            return 1;

        if (first_mem < 0)
            return 1;
        
        if (runKernel(handle, cu_base_addr, alignment, ert, verbose, n_elements, first_mem, cu_index, xclbinId))
            return 1;
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
