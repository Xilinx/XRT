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
#include "xmmult_hw_64.h"      
#else
#include "xmmult_hw.h"
#endif

static const int SIZE = 256;
int DATA_SIZE = SIZE*SIZE;

/**
 * Runs an OpenCL kernel which writes known 16 integers into a 64 byte buffer. Does not use OpenCL
 * runtime but directly exercises the HAL driver API.
 */


const static struct option long_options[] = {
    {"hal_driver",      required_argument, 0, 's'},
    {"bitstream",       required_argument, 0, 'k'},
    {"hal_logfile",     required_argument, 0, 'l'},
    {"device",          required_argument, 0, 'd'},
    {"random",          no_argument,       0, 'r'},
    {"verbose",         no_argument,       0, 'v'},
    {"help",            no_argument,       0, 'h'},
    // enable embedded runtime
    {"ert",             no_argument,       0, '1'},
    {0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -s <hal_driver>\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -l <hal_logfile>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -r Random input data.\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* If HAL driver is not specified, application will try to find the HAL driver\n";
    std::cout << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n";
    std::cout << "* Bitstream is required\n";
    std::cout << "* HAL logfile is optional but useful for capturing messages from HAL driver\n";
}




static int runKernel(xclDeviceHandle &handle, uint64_t cu_base_addr, size_t alignment, bool ert, bool verbose, bool bRandom, int first_mem)
{
    try {
        // Allocate the device memory
        unsigned boHandle1 = xclAllocBO(handle, 2*DATA_SIZE*sizeof(float), XCL_BO_DEVICE_RAM, first_mem); // input a and b
        unsigned boHandle2 = xclAllocBO(handle, DATA_SIZE*sizeof(float), XCL_BO_DEVICE_RAM, first_mem);   // output 

        // Create the mapping to the host memory
        float *bo1 = (float*)xclMapBO(handle, boHandle1, true);

        int i, j, k;
        std::cout << "Populate the input and reference vectors.\n";
        
        const int MY_MAX = 4096;
        
        float A[SIZE][SIZE], B[SIZE][SIZE], C[SIZE*SIZE];
        std::srand(std::time(0));
        for (i = 0; i < SIZE; ++i) {
            for (j = 0; j < SIZE; ++j) {
                A[i][j] = bRandom ?  static_cast <float> (std::rand()) / (static_cast <float> (RAND_MAX/MY_MAX)):(float)(i+j);
                B[i][j] = bRandom ?  static_cast <float> (std::rand()) / (static_cast <float> (RAND_MAX/MY_MAX)):(float)(i+j);
            }
        }
        
        for (i = 0; i < SIZE; ++i) {
            for (j = 0; j < SIZE; ++j) {
                C[i*SIZE+j] = 0.0;
                for (k = 0; k < SIZE; ++k) {
                    C[i*SIZE+j] += A[i][k] * B[k][j];
                }
            }
        }
        
        std::memcpy(bo1, A, DATA_SIZE*sizeof(float));
        std::memcpy(bo1 + DATA_SIZE, B, DATA_SIZE*sizeof(float));
        //for (i = 0; i < 2*DATA_SIZE ; ++i) { printf("======i: %d, bo1: %0.4f\n",i,bo1[i]); }


        // Send the input data to the device memory
        std::cout << "Send the input data to the device memory.\n";
        if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_TO_DEVICE , 2*DATA_SIZE*sizeof(float), 0)) {
            return 1;
        }

        // Get & check the device memory address        
        xclBOProperties p;
        uint64_t bo1devAddr = !xclGetBOProperties(handle, boHandle1, &p) ? p.paddr : -1;
        uint64_t bo2devAddr = !xclGetBOProperties(handle, boHandle2, &p) ? p.paddr : -1;
        
        if( (bo2devAddr == (uint64_t)(-1)) || (bo1devAddr == (uint64_t)(-1))) {
            return 1;
        }

        // Create an execution buffer to configure the FPGA (ERT)
        unsigned execHandle = xclAllocBO(handle, DATA_SIZE*sizeof(float), xclBOKind(0), (1<<31));
        void* execData = xclMapBO(handle, execHandle, true);
        
        std::cout << "Construct the exe buf cmd to confire FPGA" << std::endl;
        //construct the exec buffer cmd to configure.
        {
            auto ecmd = reinterpret_cast<ert_configure_cmd*>(execData);
            
            std::memset(ecmd, 0, DATA_SIZE*sizeof(float));
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

            // Clear the command in case it was recycled
            size_t regmap_size = XMMULT_CONTROL_ADDR_REPEAT_R_DATA/4 + 1; // regmap
            std::memset(ecmd,0,(sizeof *ecmd) + regmap_size);

            // Program the command packet header
            ecmd->state = ERT_CMD_STATE_NEW;
            ecmd->opcode = ERT_START_CU;
            ecmd->count = 1 + regmap_size;  // cu_mask + regmap

            // Program the CU mask. One CU at index 0
            ecmd->cu_mask = 0x1;

            // Program the register map
            ecmd->data[XMMULT_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
            //ecmd->data[0x00] = 0x0; // ap_start


#if defined(DSA64)
            ecmd->data[XMMULT_CONTROL_ADDR_A_DATA/4] = bo1devAddr & 0xFFFFFFFF; // a input
            ecmd->data[XMMULT_CONTROL_ADDR_A_DATA/4 + 1] = (bo1devAddr >> 32) & 0xFFFFFFFF; // a input
            ecmd->data[XMMULT_CONTROL_ADDR_OUTPUT_R_DATA/4] = bo2devAddr & 0xFFFFFFFF; // output
            ecmd->data[XMMULT_CONTROL_ADDR_OUTPUT_R_DATA/4 + 1] = (bo2devAddr >> 32) & 0xFFFFFFFF; // output
#else
            ecmd->data[XMMULT_CONTROL_ADDR_A_DATA/4] = bo1devAddr; // a input
            ecmd->data[XMMULT_CONTROL_ADDR_OUTPUT_R_DATA/4] = bo2devAddr; // output
#endif
            ecmd->data[XMMULT_CONTROL_ADDR_REPEAT_R_DATA/4] = 1;
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
        if(xclSyncBO(handle, boHandle2, XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE*sizeof(float), 0)) {
            return 1;
        }
        float *bo2 = (float*)xclMapBO(handle, boHandle2, false);


        // Validate FPGA results
        int err = 0;
        for (i = 0; i < SIZE * SIZE; i++) {
            bool bShow = verbose;
            if (C[i] != bo2[i]) {
                bShow = true;
                err++;
            }
            if (bShow) {
                std::cout<<std::hex<<i<<" : "<<std::fixed<<C[i]<< " vs "<<bo2[i]<<std::endl;
            }
        }

        if (err) {
            std::cout<<std::dec<<"FAILED TEST. mismatch count = "<<err<<std::endl;
            return 1;
        }

        munmap(bo1, DATA_SIZE*sizeof(float));
        munmap(bo2, DATA_SIZE*sizeof(float));
        xclFreeBO(handle,boHandle1);
        xclFreeBO(handle,boHandle2);
        xclFreeBO(handle,execHandle);
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


int main(int argc, char** argv)
{
    std::string sharedLibrary;
    std::string bitstreamFile;
    std::string halLogfile;
    unsigned index = 0;
    size_t alignment = 128;
    int option_index = 0;
    bool verbose = false;
    bool bRandom = false;
    int c;
    unsigned cu_index = 0;
    bool ert = false;

    //findSharedLibrary(sharedLibrary);
    while ((c = getopt_long(argc, argv, "s:k:l:d:vhr", long_options, &option_index)) != -1)
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
        case 'd':
            index = std::atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        case 'r':
            bRandom = true;
            break;
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
    	
    	if(initXRT(bitstreamFile.c_str(), index, halLogfile.c_str(), handle, cu_index, cu_base_addr, first_mem)) {
	        return 1;
	    }
	    
	    if (first_mem < 0)
	        return 1;
       
        
        if (runKernel(handle, cu_base_addr, alignment, ert, verbose, bRandom, first_mem)) {
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
