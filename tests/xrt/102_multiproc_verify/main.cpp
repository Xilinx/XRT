// Copyright (C) 2018 Xilinx Inc.
// All rights reserved.
// Author: sonals

#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
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
#include "utils.h"
#include "utils.hpp" // time_ns

/**
 * Testcase to demostrate XRT's multiprocess support.
 * Runs multiple processes each exercising the same shared hello world kernel in loop
 */

static const unsigned LOOP = 16;
static const unsigned CHILDREN = 8;

////////////////////////////////////////////////////////////////////////////////

int runChildren(int argc, char *argv[], char *envp[], unsigned count);

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


static int runKernel(xclDeviceHandle handle, uint64_t cu_base_addr, bool verbose, unsigned n_elements)
{
    const int size = 1024;
    std::vector<std::pair<unsigned, void*>> dataBO(n_elements, std::pair<unsigned, void*>(0xffffffff, nullptr));
    std::vector<std::pair<unsigned, void*>> cmdBO(n_elements, std::pair<unsigned, void*>(0xffffffff, nullptr));
    int result = 0;

    try {
        for (auto &bo : dataBO) {
            bo.first = xclAllocBO(handle, size, 0, 0x1);
            bo.second = xclMapBO(handle, bo.first, true);
            std::memset(bo.second, 0, size);
            if (xclSyncBO(handle, bo.first, XCL_BO_SYNC_BO_TO_DEVICE, size, 0))
                throw std::runtime_error("Data BO sync failure");
        }

        auto dbo = dataBO.begin();
        for (auto &cbo : cmdBO) {
            cbo.first = xclAllocBO(handle, 4096, 0, (1<<31));
            cbo.second = xclMapBO(handle, cbo.first, true);
            std::memset(cbo.second, 0, 4096);
            ert_start_kernel_cmd *ecmd = reinterpret_cast<ert_start_kernel_cmd*>(cbo.second);
            auto rsz = (XHELLO_CONTROL_ADDR_BUF_R_DATA/4+2) + 1; // regmap array size
            std::memset(ecmd,0,(sizeof *ecmd) + rsz);
            ecmd->state = ERT_CMD_STATE_NEW;
            ecmd->opcode = ERT_START_CU;
            ecmd->count = 1 + rsz;
            ecmd->cu_mask = 0x1;
            ecmd->data[XHELLO_CONTROL_ADDR_AP_CTRL/4] = 0x1; // ap_start
            xclBOProperties p;
            const uint64_t paddr = xclGetBOProperties(handle, dbo->first, &p) ? 0xfffffffffffffff : p.paddr;
            unsigned addr = (unsigned)(paddr & 0xFFFFFFFF);
            ecmd->data[XHELLO_CONTROL_ADDR_BUF_R_DATA/4] = addr;
            addr = (unsigned)((paddr & 0xFFFFFFFF00000000) >> 32);
            ecmd->data[XHELLO_CONTROL_ADDR_BUF_R_DATA/4 + 1] = addr;
            dbo++;
        }

        for (auto &cbo : cmdBO) {
            if (xclExecBuf(handle, cbo.first))
                throw std::runtime_error("Command BO submission failure");
            stamp(std::cout) << "Submit execute(" << cbo.first << ")" << std::endl;
        }

        std::vector<std::pair<unsigned, void*>>::iterator iter = cmdBO.begin();
        size_t count = 0;
        auto start = utils::time_ns();
        while (cmdBO.size() && (utils::time_ns() - start) * 10e-9 < 30) {
            if (iter == cmdBO.end())
                iter = cmdBO.begin();
            const ert_start_kernel_cmd *ecmd = reinterpret_cast<ert_start_kernel_cmd*>(iter->second);
            switch (ecmd->state) {
            case ERT_CMD_STATE_COMPLETED:
            case ERT_CMD_STATE_ERROR:
            case ERT_CMD_STATE_ABORT:
                stamp(std::cout) << "Done execute(" << iter->first << ") "
                                 << ertCmdCodes.find(static_cast<ert_cmd_state>(ecmd->state))->second << std::endl;
                munmap(iter->second, 4096);
                xclFreeBO(handle, iter->first);
                iter = cmdBO.erase(iter);
                break;
            default:
                iter++;
            }
            ++count;
            xclExecWait(handle, 1000);
        }

        
        stamp(std::cout) << "wait calls(" << count << ") wait time in (" << (utils::time_ns() - start) * 10e-6 << "ms)\n";

        if (cmdBO.size())
            throw std::runtime_error("Could not finish all kernel runs in 30 secs");

        for (auto &bo : dataBO) {
            if (xclSyncBO(handle, bo.first, XCL_BO_SYNC_BO_FROM_DEVICE, size, 0))
                throw std::runtime_error("Data BO sync failure");
            if (std::memcmp(bo.second, gold, sizeof(gold)))
                throw std::runtime_error("DATA validation failure");
            munmap(iter->second, size);
            xclFreeBO(handle, bo.first);
        }
    }
    catch (std::exception &ex) {
        stamp(std::cout) << "Error: " << ex.what() << std::endl;
        throw ex;
    }
    return result;
}

static int runKernelLoop(xclDeviceHandle handle, uint64_t cu_base_addr, bool verbose, size_t n_elements, unsigned cu_index, uuid_t xclbinId)
{
    if (xclOpenContext(handle, xclbinId, cu_index, true))
        throw std::runtime_error("Cannot create context");

    int result = runKernel(handle, cu_base_addr, verbose, n_elements);

    xclCloseContext(handle, xclbinId, cu_index);

    return result;
}


int main(int argc, char** argv, char *envp[])
{
    std::string sharedLibrary;
    std::string bitstreamFile;
    std::string halLogfile;
    size_t alignment = 128;
    int option_index = 0;
    unsigned index = 0;
    unsigned cu_index = 0;
    bool verbose = false;
    unsigned n_elements = LOOP;
    unsigned child = CHILDREN;
    int c;

    setenv("XCL_MULTIPROCESS_MODE", "1", 1);
    if (std::strlen(argv[0]))
        return runChildren(argc, argv, envp, child);

    //else children code here
    while ((c = getopt_long(argc, argv, "s:k:l:a:c:d:n:j:vh", long_options, &option_index)) != -1)
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
        case 'c':
            cu_index = std::atoi(optarg);
            break;
        case 'n':
            n_elements = std::atoi(optarg);
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
            return -1;

        if (runKernelLoop(handle, cu_base_addr, verbose, n_elements, cu_index, xclbinId))
            return -1;
    }
    catch (std::exception const& e)
    {
        stamp(std::cout) << "Exception: " << e.what() << "\n";
        stamp(std::cout) << "FAILED TEST\n";
        return 1;
    }

    stamp(std::cout) << "PASSED TEST\n";
    return 0;
}
