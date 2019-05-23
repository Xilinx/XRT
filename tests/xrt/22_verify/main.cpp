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

#include "xhello_hw.h"

/**
 * Runs an OpenCL kernel which writes known 16 integers into a 64 byte buffer. Does not use OpenCL
 * runtime but directly exercises the HAL driver API.
 */

#define ARRAY_SIZE 20
////////////////////////////////////////////////////////////////////////////////

#define LENGTH (20)

////////////////////////////////////////////////////////////////////////////////

const static struct option long_options[] = {
{"hal_driver",      required_argument, 0, 's'},
{"bitstream",       required_argument, 0, 'k'},
{"hal_logfile",     required_argument, 0, 'l'},
{"device",          required_argument, 0, 'd'},
{"num of elments",  required_argument, 0, 'n'},
{"verbose",         no_argument,       0, 'v'},
{"help",            no_argument,       0, 'h'},
{0, 0, 0, 0}
};

static const char gold[] = "Hello World\n";

static void printHelp()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -s <hal_driver>\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -l <hal_logfile>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -n <num of elements, default is 16>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* If HAL driver is not specified, application will try to find the HAL driver\n";
    std::cout << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n";
    std::cout << "* Bitstream is required\n";
    std::cout << "* HAL logfile is optional but useful for capturing messages from HAL driver\n";
}

static int runKernel(xclDeviceHandle &handle, uint64_t cu_base_addr, size_t alignment, bool ert, bool verbose, size_t n_elements, int first_mem, unsigned cu_index, uuid_t xclbinId)
{
    if(xclOpenContext(handle, xclbinId, cu_index, true))
        throw std::runtime_error("Cannot create context");

    unsigned boHandle = xclAllocBO(handle, 1024, XCL_BO_DEVICE_RAM, first_mem);//buf1
    char* bo = (char*)xclMapBO(handle, boHandle, true);

    memset(bo, 0, 1024);

    if(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_TO_DEVICE, 1024,0))
        return 1;

    xclBOProperties p;
    uint64_t bodevAddr = !xclGetBOProperties(handle, boHandle, &p) ? p.paddr : -1;

    if((bodevAddr == uint64_t (-1)))
        return 1;

    //Allocate the exec_bo
    unsigned execHandle = xclAllocBO(handle, 1024, xclBOKind(0), (1<<31));
    void* execData = xclMapBO(handle, execHandle, true);

    std::cout << "Construct the exe buf cmd to configure FPGA" << std::endl;
    //construct the exec buffer cmd to configure.
    {
        auto ecmd = reinterpret_cast<ert_configure_cmd*>(execData);

        std::memset(ecmd, 0, 1024);
        ecmd->state = ERT_CMD_STATE_NEW;
        ecmd->opcode = ERT_CONFIGURE;

        ecmd->slot_size = 1024;
        ecmd->num_cus = 1;
        ecmd->cu_shift = 16;
        ecmd->cu_base_addr = cu_base_addr;

        ecmd->ert = ert;
        if (ert) {
            ecmd->cu_dma = 1;
            ecmd->cu_isr = 1;
        }

        // CU -> base address mapping
        ecmd->data[0] = cu_base_addr;
        ecmd->count = 5 + ecmd->num_cus;
    }

    std::cout << "Send the exec command and configure FPGA (ERT)" << std::endl;
    //Send the command.
    if(xclExecBuf(handle, execHandle)) {
        std::cout << "Unable to issue xclExecBuf" << std::endl;
        return 1;
    }

    std::cout << "Wait until the command finish" << std::endl;
    //Wait on the command finish
    while (xclExecWait(handle,1000) == 0);


    std::cout << "Construct the exec command to run the kernel on FPGA" << std::endl;

    //--
    //construct the exec buffer cmd to start the kernel.
    {
        auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(execData);
        auto rsz = (XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA/4+1) + 1; // regmap array size
        std::memset(ecmd,0,(sizeof *ecmd) + rsz);
        ecmd->state = ERT_CMD_STATE_NEW;
        ecmd->opcode = ERT_START_CU;
        ecmd->count = 1 + rsz;
        ecmd->cu_mask = 0x1;

        ecmd->data[XHELLO_HELLO_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
        ecmd->data[XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA/4] = bodevAddr; // low part of a
        ecmd->data[XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA/4 + 1] = (bodevAddr >> 32) & 0xFFFFFFFF; // high part of a
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
    if(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0)) {
        return 1;
    }

    // Since the kernel uses only one argument, we don't need to map the device again.
    //char* bo1 = (char*)xclMapBO(handle, boHandle, false);


    std::cout << "RESULT: " << std::endl;
    for (unsigned i = 0; i < 20; ++i)
        std::cout << bo[i];
    std::cout << std::endl;
    size_t result = std::memcmp(bo, gold, sizeof(gold));
    if (result) {
        std::cout << "FAILED TEST\n";
        std::cout << "Incorrect data received\n";
        return 1;
    }

    // Clean up stuff
    munmap(bo, 1024);
    munmap(execData, 1024);
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
    size_t n_elements = 16;
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

        if (runKernel(handle, cu_base_addr, alignment, ert, verbose,n_elements, first_mem, cu_index, xclbinId))
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
