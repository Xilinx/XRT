// Copyright (C) 2014-2016 Xilinx Inc.
// All rights reserved.

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
#include "xma_profile.h"

// lowlevel common include

#include "utils.h"

#if defined(DSA64)
#include "xloopback_hw_64.h"
#else
#include "xloopback_hw.h"
#endif

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
	uint64_t cu_base_addr = 0;
	if(initXRT(bitstreamFile.c_str(), index, halLogfile.c_str(), handle, cu_index, cu_base_addr))
	    return 1;

    profile_initialize(handle, 1, 1, "coarse", "all");
    profile_start(handle);	

	unsigned boHandle2 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM, 0x0);
	char* bo2 = (char*)xclMapBO(handle, boHandle2, true);
	memset(bo2, 0, DATA_SIZE);
	std::string testVector =  "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n";
	std::strcpy(bo2, testVector.c_str());

	if(xclSyncBOWithProfile(handle, boHandle2, XCL_BO_SYNC_BO_TO_DEVICE , DATA_SIZE,0))
	    return 1;

	unsigned boHandle1 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM, 0x0);


	//Allocate the exec_bo
	unsigned execHandle = xclAllocBO(handle, DATA_SIZE, xclBOKind(0), (1<<31));
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

	xclBOProperties p;
	uint64_t bo2devAddr = !xclGetBOProperties(handle, boHandle2, &p) ? p.paddr : -1;
	uint64_t bo1devAddr = !xclGetBOProperties(handle, boHandle1, &p) ? p.paddr : -1;

	if( (bo2devAddr == (uint64_t)(-1)) || (bo1devAddr == (uint64_t)(-1)))
	    return 1;

	//--
	//construct the exec buffer cmd to start the kernel.
	{
	    auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(execData);
	    auto rsz = (XLOOPBACK_CONTROL_ADDR_LENGTH_R_DATA/4+1) + 1; // regmap array size
	    std::memset(ecmd,0,(sizeof *ecmd) + rsz*4);
	    ecmd->state = ERT_CMD_STATE_NEW;
	    ecmd->opcode = ERT_START_CU;
	    ecmd->count = 1 + rsz;
	    ecmd->cu_mask = 0x1;

	    ecmd->data[XLOOPBACK_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
#if defined(DSA64)
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_S1_DATA/4] = bo1devAddr & 0xFFFFFFFF; // s1
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_S1_DATA/4 + 1] = (bo1devAddr >> 32) & 0xFFFFFFFF; // s1
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_S2_DATA/4] = bo2devAddr & 0xFFFFFFFF; // s2
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_S2_DATA/4 + 1] = (bo2devAddr >> 32) & 0xFFFFFFFF; // s2
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_LENGTH_R_DATA/4] = DATA_SIZE; // length
#else
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_S1_DATA/4] = bo1devAddr; // s1
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_S2_DATA/4] = bo2devAddr; // s2
	    ecmd->data[XLOOPBACK_CONTROL_ADDR_LENGTH_R_DATA/4] = DATA_SIZE; // length
#endif
	}

        std::cout << "Starting kernel..." << std::endl;

	//Send the "start kernel" command.
	if(xclExecBuf(handle, execHandle)) {
	    std::cout << "Unable to issue xclExecBuf : start_kernel" << std::endl;
            std::cout << "FAILED TEST\n";
            std::cout << "Write failed\n";
	    return 1;
	}

        //Wait on the command finish	
	while (xclExecWait(handle,1000) == 0) {
	    std::cout << "reentering wait...\n";
	};

	//Get the output;
	if(xclSyncBOWithProfile(handle, boHandle1, XCL_BO_SYNC_BO_FROM_DEVICE , DATA_SIZE, 0))
	    return 1;
	char* bo1 = (char*)xclMapBO(handle, boHandle1, false);

        if (std::memcmp(bo2, bo1, DATA_SIZE)) {
            std::cout << "FAILED TEST\n";
            std::cout << "Value read back does not match value written\n";
            return 1;
        }

	//Clean up stuff
	munmap(bo1, DATA_SIZE);
	munmap(bo2, DATA_SIZE);
	xclFreeBO(handle,boHandle1);
	xclFreeBO(handle,boHandle2);
	xclFreeBO(handle,execHandle);
    profile_stop(handle);
    profile_finalize(handle);	
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
