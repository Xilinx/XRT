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

#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <sys/mman.h>

#include "experimental/xrt_kernel.h"


// host_src includes
#include "xclhal2.h"
#include "xclbin.h"

//#if defined(DSA64)
//#include "xloopback_hw_64.h"
//#else
//#include "xloopback_hw.h"
//#endif

#include <fstream>

static const int DATA_SIZE = 1024;



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
    int cu_index = 0;
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
        int first_mem = -1;

        // load bit stream
        std::ifstream stream(bitstreamFile);
        stream.seekg(0,stream.end);
        size_t size = stream.tellg();
        stream.seekg(0,stream.beg);

        std::vector<char> header(size);
        stream.read(header.data(),size);


        handle = xclOpen(index, nullptr, XCL_INFO);


        if (xclLockDevice(handle))
           throw std::runtime_error("Cannot lock device");
        else 
           std::cout<<"\n Locked the device sucessfully";

        auto top = reinterpret_cast<const axlf*>(header.data());

        if (xclLoadXclBin(handle,top))
           throw std::runtime_error("Bitstream download failed");
        else 
           std::cout<<"\n Bitstream downloaded sucessfully";


       // Detecting the first used memory
       auto topo = xclbin::get_axlf_section(top, MEM_TOPOLOGY);
       auto topology = reinterpret_cast<mem_topology*>(header.data() + topo->m_sectionOffset);

       for (int i=0; i<topology->m_count; ++i) {
          if (topology->m_mem_data[i].m_used) {
             first_mem = i;
             break;
          }
       }

       if (first_mem < 0)
           return 1;


       std::string kname = "loopback";
       auto kernel = xrtPLKernelOpen(handle, header.data(), kname.c_str());

       auto boHandle2 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM, first_mem);
       char* bo2 = (char*)xclMapBO(handle, boHandle2, true);
       memset(bo2, 0, DATA_SIZE);
       std::string testVector =  "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n";
       std::strcpy(bo2, testVector.c_str());

       if(xclSyncBO(handle, boHandle2, XCL_BO_SYNC_BO_TO_DEVICE , DATA_SIZE,0))
           return 1;

       auto boHandle1 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM, first_mem);


       std::cout << "\nStarting kernel..." << std::endl;

       xrtRunHandle r = xrtKernelRun(kernel, boHandle1, boHandle2,  DATA_SIZE); // kernel, output, input, parameter
       xrtRunWait(r);
       xrtRunClose(r);

       //Get the output;
       if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_FROM_DEVICE , DATA_SIZE, 0))
           return 1;
       char* bo1 = (char*)xclMapBO(handle, boHandle1, false);

       if (std::memcmp(bo2, bo1, DATA_SIZE)) {
            std::cout << "FAILED TEST\n";
            std::cout << "Value read back does not match value written\n";
            return 1;
       }

        //Clean up stuff
        xclUnmapBO(handle, boHandle1, bo1);
        xclUnmapBO(handle, boHandle2, bo2);
        xclFreeBO(handle,boHandle1);
        xclFreeBO(handle,boHandle2);
        //xclCloseContext(handle, xclbinId, cu_index);

        xrtKernelClose(kernel);
        xclClose(handle);
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
