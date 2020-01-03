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

// driver includes
#include "ert.h"

// host_src includes
#include "xclhal2.h"
#include "xclbin.h"

// lowlevel common include
#include "utils.h"

#if defined(DSA64)
#include "xmysequence_hw_64.h"
#else
#include "xmysequence_hw.h"
#endif

/**
 * Runs an OpenCL kernel which writes known 16 integers into a 64 byte buffer. Does not use OpenCL
 * runtime but directly exercises the HAL driver API.
 */


static const int DATA_SIZE = 16;
const static struct option long_options[] = {
{"hal_driver",      required_argument, 0, 's'},
{"bitstream",       required_argument, 0, 'k'},
{"hal_logfile",     required_argument, 0, 'l'},
{"alignment",       required_argument, 0, 'a'},
{"cu_index",        required_argument, 0, 'c'},
{"device",          required_argument, 0, 'd'},
{"verbose",         no_argument,       0, 'v'},
{"help",            no_argument,       0, 'h'},
// enable embedded runtime
{"ert",             no_argument,       0, '1'},
{0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -l <hal_logfile>\n";
    std::cout << "  -a <alignment>\n";
    std::cout << "  -d <device_index>\n";
    std::cout << "  -c <cu_index>\n";
    std::cout << "  -s <hal_driver>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "";
    std::cout << "  [--ert] enable embedded runtime (default: false)\n";
    std::cout << "";
    std::cout << "* If HAL driver is not specified, application will try to find the HAL driver\n";
    std::cout << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n";
    std::cout << "* Bitstream is required\n";
    std::cout << "* HAL logfile is optional but useful for capturing messages from HAL driver\n";
}

const unsigned goldenSequence[16] = {
    0X586C0C6C,
    'X',
    0X586C0C6C,
    'I',
    0X586C0C6C,
    'L',
    0X586C0C6C,
    'I',
    0X586C0C6C,
    'N',
    0X586C0C6C,
    'X',
    0X586C0C6C,
    '\0',
    0X586C0C6C,
    '\0'
};

static int runKernel(xclDeviceHandle &handle, uint64_t cu_base_addr, size_t alignment, bool ert, bool verbose, int first_mem, unsigned cu_index, uuid_t xclbinId)
{
    if(xclOpenContext(handle, xclbinId, cu_index, true))
        throw std::runtime_error("Cannot create context");

    unsigned boHandle = xclAllocBO(handle, DATA_SIZE*sizeof(unsigned), 0, first_mem);
    unsigned* bo = (unsigned*)xclMapBO(handle, boHandle, true);
    
    memset(bo, 0, DATA_SIZE*sizeof(unsigned));

    if(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE*sizeof(unsigned),0))
        return 1;

    xclBOProperties p;
    uint64_t bodevAddr = !xclGetBOProperties(handle, boHandle, &p) ? p.paddr : -1;

    if((bodevAddr == (uint64_t) (-1)))
        return 1;

    //Allocate the exec_bo
    unsigned execHandle = xclAllocBO(handle, DATA_SIZE*sizeof(unsigned), 0, (1<<31));
    void* execData = xclMapBO(handle, execHandle, true);

    std::cout << "Construct the exec command to run the kernel on FPGA" << std::endl;
    
    //construct the exec buffer cmd to start the kernel.
    {
        auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(execData);
        auto rsz = (XMYSEQUENCE_CONTROL_ADDR_A_DATA/4+1) + 1; // regmap array size
        std::memset(ecmd,0,(sizeof *ecmd) + rsz*sizeof(unsigned));
        ecmd->state = ERT_CMD_STATE_NEW;
        ecmd->opcode = ERT_START_CU;
        ecmd->count = 1 + rsz;
        ecmd->cu_mask = 0x1;

        ecmd->data[XMYSEQUENCE_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
#if defined(DSA64)
        ecmd->data[XMYSEQUENCE_CONTROL_ADDR_A_DATA/4] = bodevAddr & 0xFFFFFFFF; // a
        ecmd->data[XMYSEQUENCE_CONTROL_ADDR_A_DATA/4 + 1] = (bodevAddr >> 32) & 0xFFFFFFFF; // a
#else
        ecmd->data[XMYSEQUENCE_CONTROL_ADDR_A_DATA/4] = bodevAddr; // a
#endif
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
    if(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE*sizeof(unsigned), 0)) {
        return 1;
    }

    // Since the kernel uses only one argument, we don't need to map the device again.
    //char* bo1 = (char*)xclMapBO(handle, boHandle, false);



    if (std::memcmp(goldenSequence, bo, DATA_SIZE*sizeof(unsigned))) {
        std::cout << "FAILED TEST\n";
        std::cout << "Value read back does not match value written\n";
        return 1;
    }

    //Clean up stuff
    munmap(bo, DATA_SIZE * sizeof(unsigned));
    munmap(execData, DATA_SIZE * sizeof(unsigned));
    xclFreeBO(handle, boHandle);
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
    //findSharedLibrary(sharedLibrary);

    while ((c = getopt_long(argc, argv, "s:k:l:a:c:d:vh", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 1:
            ert = true;
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
        case 'c':
            cu_index = std::atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        case 'v':
            verbose = true;
            break;
        default:
            printHelp();
            return -1;
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
        
        if (runKernel(handle, cu_base_addr, alignment, ert, verbose, first_mem, cu_index, xclbinId))
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
