/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

// host_src includes
#include "xrt.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_xclbin.h"
#include "xclbin.h"

// lowlevel common include
#include "utils.h"

/**
 * Runs an OpenCL kernel which writes "Hello World\n" into the buffer passed
 */

#define ARRAY_SIZE 20
////////////////////////////////////////////////////////////////////////////////

#define LENGTH (20)

////////////////////////////////////////////////////////////////////////////////

const static struct option long_options[] = {
{"bitstream",       required_argument, 0, 'k'},
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
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -n <num of elements, default is 16>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

static int runKernel(xclDeviceHandle handle, bool verbose, int first_mem, const uuid_t xclbinId)
{
    xrtKernelHandle khandle = xrtPLKernelOpen(handle, xclbinId, "hello:hello_1");
    unsigned boHandle = xclAllocBO(handle, 1024, 0, first_mem);
    validHandleOrError(boHandle);
    char* bo = (char*)xclMapBO(handle, boHandle, true);

    memset(bo, 0, 1024);

    validOrError(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_TO_DEVICE, 1024,0), "xclSyncBO");

    xrtRunHandle run = xrtKernelRun(khandle, boHandle);
    std::cout << "Kernel start command issued" << std::endl;
    std::cout << "Now wait until the kernel finish" << std::endl;

    ert_cmd_state status = xrtRunWait(run);

    //Get the output;
    std::cout << "Get the output data from the device" << std::endl;
    validOrError(xclSyncBO(handle, boHandle, XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0), "xclSyncBO");

    std::cout << "RESULT: " << std::endl;
    for (unsigned i = 0; i < 20; ++i)
        std::cout << bo[i];
    std::cout << std::endl;
    if (std::memcmp(bo, gold, sizeof(gold)))
        throw std::runtime_error("Incorrect value obtained");

    // Clean up stuff
    xclUnmapBO(handle, boHandle, bo);
    xclFreeBO(handle, boHandle);
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

    while ((c = getopt_long(argc, argv, "s:k:a:c:d:vh", long_options, &option_index)) != -1)
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
        std::cout << "Please use xrt.ini to specify logging";
    }

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

        runKernel(handle, verbose, first_mem, xclbinId);
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
