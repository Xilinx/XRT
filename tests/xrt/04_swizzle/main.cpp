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

// driver includes
#include "ert.h"

// host_src includes
#include "xclhal2.h"
#include "xclbin.h"

// lowlevel common include

#include "utils.h"

#if defined(DSA64)
#include "xvectorswizzle_hw_64.h"
#else
#include "xvectorswizzle_hw.h"
#endif

#include <fstream>

static const int DATA_SIZE = 4096;



/**
 * Trivial loopback example which runs OpenCL loopback kernel. Does not use OpenCL
 * runtime but directly exercises the XRT driver API.
 */


const static struct option long_options[] = {
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
    std::cout << "Compiled kernel = " << bitstreamFile << "\n" << std::endl;

    try
    {
        xclDeviceHandle handle;
        uint64_t cu_base_addr = 0;
        int first_mem = -1;
        uuid_t xclbinId;
        if (initXRT(bitstreamFile.c_str(), index, halLogfile.c_str(), handle, cu_index, cu_base_addr, first_mem, xclbinId))
            return 1;

        if (first_mem < 0)
            return 1;

        if (xclOpenContext(handle, xclbinId, cu_index, true))
            throw std::runtime_error("Cannot create context");

        unsigned boHandle = xclAllocBO(handle, DATA_SIZE*sizeof(int), 0, first_mem);
        int* bo = (int*)xclMapBO(handle, boHandle, true);
        memset(bo, 0, DATA_SIZE*sizeof(int));

        //Populate the input and reference vectors.
        int reference[DATA_SIZE];
        for (int i = 0; i < DATA_SIZE; i++) {
            bo[i] = i;
            int val = 0;
            if(i%4==0)  val = i+2;
            if(i%4==1)  val = i+2;
            if(i%4==2)  val = i-2;
            if(i%4==3)  val = i-2;
            reference[i] = val;
        }

        if(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_TO_DEVICE , DATA_SIZE*4,0))
            return 1;

        xclBOProperties p;
        uint64_t bodevAddr = !xclGetBOProperties(handle, boHandle, &p) ? p.paddr : -1;

        if((bodevAddr == uint64_t (-1)))
            return 1;

        //Allocate the exec_bo
        unsigned execHandle = xclAllocBO(handle, DATA_SIZE*4, 0, (1<<31));
        void* execData = xclMapBO(handle, execHandle, true);

        //construct the exec buffer cmd to configure.
        {
            auto ecmd = reinterpret_cast<ert_configure_cmd*>(execData);

            std::memset(ecmd, 0, DATA_SIZE);
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

        //Send the command.
        if(xclExecBuf(handle, execHandle)) {
            std::cout << "Unable to issue xclExecBuf" << std::endl;
            return 1;
        }

        //Wait on the command finish
        while (xclExecWait(handle,1000) == 0);

        std::cout << "Construct the exec command to run the kernel on FPGA" << std::endl;

        //--
        //construct the exec buffer cmd to start the kernel.
        {
            auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(execData);

            // Clear the command in case it was recycled
            size_t regmap_size = (XVECTORSWIZZLE_CONTROL_ADDR_A_DATA/4+1) + 1; // regmap
            std::memset(ecmd,0,(sizeof *ecmd) + regmap_size*4);

            // Program the command packet header
            ecmd->state = ERT_CMD_STATE_NEW;
            ecmd->opcode = ERT_START_CU;
            ecmd->count = 1 + regmap_size;  // cu_mask + regmap

            // Program the CU mask. One CU at index 0
            ecmd->cu_mask = 0x1;

            // Program the register map
            ecmd->data[XVECTORSWIZZLE_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
            ecmd->data[XVECTORSWIZZLE_CONTROL_ADDR_GROUP_ID_X_DATA/4] = 0x0; // group id
            ecmd->data[XVECTORSWIZZLE_CONTROL_ADDR_A_DATA/4] = bodevAddr; // s1 buffer
#if defined(DSA64)
            ecmd->data[XVECTORSWIZZLE_CONTROL_ADDR_A_DATA/4 + 1] = (bodevAddr >> 32) & 0xFFFFFFFF; // s1 buffer
#endif

            const size_t global[2] = {DATA_SIZE / 4}; // int4 vector count global range
            const size_t local[2] = {16}; // 16 int4 processed per work group
            const size_t groupSize = global[0] / local[0];

            if (verbose) {
                std::cout << "Global range " << global[0] << "\n";
                std::cout << "Group size " << local[0] << "\n";
                std::cout << "Starting kernel...\n";
            }

            for (size_t id = 0; id < groupSize; id++) {
                if (verbose)
                    std::cout << "group id = " << id << std::endl;

                ecmd->data[XVECTORSWIZZLE_CONTROL_ADDR_AP_CTRL] = 0x0; // reset the ap_start
                ecmd->data[XVECTORSWIZZLE_CONTROL_ADDR_GROUP_ID_X_DATA/4] = id; // group id

                // Execute the command
                if(xclExecBuf(handle, execHandle)) {
                    std::cout << "Unable to issue xclExecBuf" << std::endl;
                    return 1;
                }

                if (verbose)
                    std::cout << "Waiting for group id = " << id << " to finish...\n";

                // Wait for kernel
                while (xclExecWait(handle,1000) == 0)
                    std::cout << "reentering wait...\n";
            }

        }

        //Get the output;
        if(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_FROM_DEVICE , DATA_SIZE*4, false))
            return 1;

        xclCloseContext(handle, xclbinId, cu_index);

        if (std::memcmp(bo, reference, DATA_SIZE*4)) {
            std::cout << "FAILED TEST\n";
            std::cout << "Value read back does not match value written\n";
            return 1;
        }
        munmap(bo, DATA_SIZE*4);
        munmap(execData, DATA_SIZE*4);
        xclFreeBO(handle, boHandle);
        xclFreeBO(handle, execHandle);
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
