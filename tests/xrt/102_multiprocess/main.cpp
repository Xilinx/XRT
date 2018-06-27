// Copyright (C) 2014-2016 Xilinx Inc.
// All rights reserved.

#include <getopt.h>
#include <sys/mman.h>
#include <spawn.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/stat.h>

#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <string>
#include <cstring>

// driver includes
#include "ert.h"

// host_src includes
#include "xclhal2.h"
#include "xclbin.h"

// lowlevel common include

#include "utils.h"

#if defined(DSA64)
#include "xloopback_hw_64.h"
#else
#include "xloopback_hw.h"
#endif

#include <fstream>

static const int DATA_SIZE = 1024;

extern char **environ;

static std::string get_self_path()
{
#ifdef __GNUC__
    char buf[PATH_MAX] = {0};
    auto len = ::readlink("/proc/self/exe", buf, PATH_MAX);
    return std::string(buf, (len>0) ? len : 0);
#else
    return "";
#endif
}


/**
 * Trivial loopback example which runs OpenCL loopback kernel. Does not use OpenCL
 * runtime but directly exercises the XRT driver API.
 */


const static struct option long_options[] = {
    {"bitstream",       required_argument, 0, 'k'},
    {"hal_logfile",     required_argument, 0, 'l'},
    {"cu_index",        required_argument, 0, 'c'},
    {"device",          required_argument, 0, 'd'},
    {"verbose",         no_argument,       0, 'v'},
    {"help",            no_argument,       0, 'h'},
    // enable embedded runtime
    {"ert",             no_argument,       0, '1'},
    {"slave",           no_argument,       0, '2'},
    {0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -l <hal_logfile>\n";
    std::cout << "  -d <device_index>\n";
    std::cout << "  -c <cu_index>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "";
    std::cout << "  [--ert] enable embedded runtime (default: false)\n";
    std::cout << "  [--slave] run as slave process (default: false)\n";
    std::cout << "";
    std::cout << "* If HAL driver is not specified, application will try to find the HAL driver\n";
    std::cout << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n";
    std::cout << "* Bitstream is required\n";
    std::cout << "* HAL logfile is optional but useful for capturing messages from HAL driver\n";
}


static int runSlave(xclDeviceHandle handle, int i)
{
    unsigned boHandle1 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM, 0x0);
    unsigned boHandle2 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM, 0x0);
    char* bo2 = (char*)xclMapBO(handle, boHandle2, true);
    memset(bo2, 0, DATA_SIZE);
    std::string testVector =  "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n";
    std::strcpy(bo2, testVector.c_str());

    if(xclSyncBO(handle, boHandle2, XCL_BO_SYNC_BO_TO_DEVICE , DATA_SIZE,0))
        return 1;

    unsigned execHandle = xclAllocBO(handle, DATA_SIZE, xclBOKind(0), (1<<31));
    void* execData = xclMapBO(handle, execHandle, true);

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
    if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_FROM_DEVICE , DATA_SIZE, 0))
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
    return 0;
}


static int runSlaveLoop(xclDeviceHandle handle)
{
    int result = 0;
    pid_t pid = getpid();
    for (int i = 0; i < 1024; i++) {
        std::cout << pid << '.' << i << std::endl;
        result += runSlave(handle, i);
    }
    return result;
}


static int spawnSlaveProcess(int index)
{
    static const int count = 16;
    pid_t pids[count];
    const std::string path = get_self_path();
    const std::string slave = "--slave";
    const char *argv[] = {path.c_str(), slave.c_str(), 0};
    int result = 0;
    int i;
    for (i = 0; i < count; i++) {
        result = posix_spawn(&pids[i], path.c_str(), 0, 0, (char* const*)argv, environ);
        if (result < 0) {
            std::cout << '[' << index << ']' << std::strerror(errno) << '\n';
            break;
        }
        std::cout << '[' << index << ']' << pids[i] << '\n';
    }
    std::cout << "Spawned " << i << " slave processes\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return result;
}

int main(int argc, char** argv)
{
    std::string sharedLibrary;
    std::string bitstreamFile;
    std::string halLogfile;
    int option_index = 0;
    unsigned index = 0;
    int cu_index = 0;
    bool verbose = false;
    bool ert = false;
    bool slave = false;
    int c;
    while ((c = getopt_long(argc, argv, "k:l:c:d:vh", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 1:
            ert = true;
            break;
        case '2':
            slave = true;
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

    if (!slave && !bitstreamFile.size()) {
        std::cout << "FAILED TEST\n";
        std::cout << "No bitstream specified\n";
        return -1;
    }

    if (halLogfile.size()) {
        std::cout << "Using " << halLogfile << " as HAL driver logfile\n";
    }

    std::cout << "HAL driver = " << sharedLibrary << "\n";
    std::cout << "Compiled kernel = " << bitstreamFile << "\n" << std::endl;
    xclDeviceHandle handle;
    uint64_t cu_base_addr = 0;
    try
    {

	if(initXRT(bitstreamFile.c_str(), index, halLogfile.c_str(), handle, cu_index, cu_base_addr))
	    return 1;
    }
    catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << "\n";
        std::cout << "FAILED TEST\n";
        return 1;
    }

    if (slave) {
        pid_t pid = getpid();
        int result = runSlaveLoop(handle);
        if (!result)
            std::cout << "PASSED TEST (Child " << pid << ")\n";
        else
            std::cout << "PASSED TEST (Child " << pid << ")\n";
        return result;
    }

    try
    {
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
	xclFreeBO(handle,execHandle);
    }
    catch (std::exception const& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
        std::cout << "FAILED TEST\n";
        return 1;
    }
    if (!spawnSlaveProcess(index)) {
        std::cout << "PASSED TEST (Parent)\n";
        return 0;
    }
    else {
        std::cout << "FAILED TEST (Parent)\n";
        return 1;
    }
}
