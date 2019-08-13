// Copyright (C) 2018 Xilinx Inc.
// All rights reserved.
// Author: sonals

#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <time.h>
#include <uuid/uuid.h>

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


class xclBO {
    unsigned bo;
    char *ptr;
};


static int runKernel(xclDeviceHandle handle, uint64_t cu_base_addr, bool verbose, size_t n_elements)
{
    const int size = 1024;
    std::vector<std::pair<unsigned, void*>> dataBO(n_elements, std::pair<unsigned, void*>(0xffffffff, nullptr));
    std::vector<std::pair<unsigned, void*>> cmdBO(n_elements, std::pair<unsigned, void*>(0xffffffff, nullptr));

    try {
        int result = 0;
        for (auto &bo : dataBO) {
            bo.first = xclAllocBO(handle, size, 0, 0x0);
            bo.second = xclMapBO(handle, bo.first, true);
            std::memset(bo.second, 0, size);
            result += xclSyncBO(handle, bo.first, XCL_BO_SYNC_BO_TO_DEVICE, size, 0);
        }

        if (result) {
            throw std::runtime_error("data BO failure");
        }


        for (auto &bo : cmdBO) {
            bo.first = xclAllocBO(handle, 4096, 0, (1<<31));
            bo.second = xclMapBO(handle, bo.first, true);
            std::memset(bo.second, 0, 4096);
            ert_start_kernel_cmd *ecmd = reinterpret_cast<ert_start_kernel_cmd*>(bo.second);
            auto rsz = (XHELLO_CONTROL_ADDR_BUF_R_DATA/4+2) + 1; // regmap array size
            std::memset(ecmd,0,(sizeof *ecmd) + rsz);
            ecmd->state = ERT_CMD_STATE_NEW;
            ecmd->opcode = ERT_START_CU;
            ecmd->count = 1 + rsz;
            ecmd->cu_mask = 0x1;
            ecmd->data[XHELLO_CONTROL_ADDR_AP_CTRL] = 0x1; // ap_start
            xclBOProperties p;
            const uint64_t paddr = xclGetBOProperties(handle, bo.first, &p) ? p.paddr : 0xfffffffffffffff;
            ecmd->data[XHELLO_CONTROL_ADDR_BUF_R_DATA] = paddr;
        }

        for (auto &bo : cmdBO) {
            result += xclExecBuf(handle, bo.first);
        }

        if (result) {
            throw std::runtime_error("cmd BO failure");
        }

        for (std::vector<std::pair<unsigned, void*>>::iterator iter = cmdBO.begin(); iter < cmdBO.end(); iter++) {
            const ert_start_kernel_cmd *ecmd = reinterpret_cast<ert_start_kernel_cmd*>(iter->second);
            switch (ecmd->state) {
            case ERT_CMD_STATE_COMPLETED:
            case ERT_CMD_STATE_ERROR:
            case ERT_CMD_STATE_ABORT:
                munmap(iter->second, 4096);
                xclFreeBO(handle, iter->first);
                cmdBO.erase(iter);
            }
            xclExecWait(handle, 1000);
        }

        for (auto &bo : dataBO) {
            result += xclSyncBO(handle, bo.first, XCL_BO_SYNC_BO_FROM_DEVICE, size, 0);
            size_t cmdresult = std::memcmp(bo.second, gold, sizeof(gold));
        }
    }
    catch (std::exception &ex) {
        return 1;
    }
    return 0;
}

static int runKernelLoop(xclDeviceHandle handle, uint64_t cu_base_addr, bool verbose, size_t n_elements)
{

    uuid_t xclbinId;
    uuid_parse("58c06b8c-c882-41ff-9ec5-116571d1d179", xclbinId);
    xclOpenContext(handle, xclbinId, 0, true);

    //Allocate the exec_bo
    unsigned execHandle = xclAllocBO(handle, 1024, 0, (1<<31));
    void* execData = xclMapBO(handle, execHandle, true);

    std::cout << "Construct the exe buf cmd to confire FPGA" << std::endl;
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

        ecmd->ert = 1;
        if (ecmd->ert) {
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
    xclCloseContext(handle, xclbinId, 0);
    return 0;
}

static int loadXclbin(xclDeviceHandle& handle, const std::string &bit)
{
    std::ifstream stream(bit.c_str());
    stream.seekg(0, stream.end);
    int size = stream.tellg();
    stream.seekg(0, stream.beg);

    char *header = new char[size];
    stream.read(header, size);

    if (std::strncmp(header, "xclbin2", 8)) {
        delete [] header;
        throw std::runtime_error("Invalid bitstream");
    }

    const axlf *qheader = (const axlf *)header;
    if (xclLoadXclBin(handle, qheader)) {
        delete [] header;
        throw std::runtime_error("Cannot load xclbin " + bit);
    }

    if (xclOpenContext(handle, qheader->m_header.uuid, 0, true)) {
        delete [] header;
        throw std::runtime_error("Cannot open context " + bit);
    }

    delete [] header;
    return 0;
}

int main(int argc, char** argv)
{
    std::string sharedLibrary;
    std::string bitstreamFile1;
    std::string bitstreamFile2;
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
        {
            if (bitstreamFile1.length())
                bitstreamFile2 = optarg;
            else
                bitstreamFile1 = optarg;
            break;
        }
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

    if (!bitstreamFile1.size() || !bitstreamFile2.size()) {
        std::cout << "FAILED TEST\n";
    	std::cout << "No bitstream specified\n";
    	return -1;
    }

    if (halLogfile.size()) {
        std::cout << "Using " << halLogfile << " as HAL driver logfile\n";
    }

    std::cout << "HAL driver = " << sharedLibrary << "\n";
    std::cout << "Host buffer alignment = " << alignment << " bytes\n";
    std::cout << "Compiled kernel = " << bitstreamFile1 << "\n";
    std::cout << "Compiled kernel = " << bitstreamFile2 << "\n";


    try {
        xclDeviceHandle handle;
    	uint64_t cu_base_addr = 0;
    	if(initXRT(0, index, halLogfile.c_str(), handle, cu_index, cu_base_addr)) {
            return 1;
        }

#if 1
        if (xclLockDevice(handle)) {
            throw std::runtime_error("Cannot lock device");
            return -1;
        }
#endif

        if (loadXclbin(handle, bitstreamFile1)) {
            throw std::runtime_error("Cannot lock device");
            return -1;
        }

        if (loadXclbin(handle, bitstreamFile2)) {
            throw std::runtime_error("Cannot lock device");
            return -1;
        }

//        if (runKernelLoop(handle, cu_base_addr, verbose, n_elements)) {
//            return 1;
//        }

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
