#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <string.h>

#include "cmdlineparser.h"

#include "xilutil.hpp"
#include "xrt.h"
#include "experimental/xrt-next.h"
/* Get internal shim API "xclOpenByBDF" */
#include "shim_int.h"
#include "ert.h"
#include "xclbin.h"

using ms_t = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

struct task_info {
    unsigned int            boh;
    unsigned int            exec_bo;
    ert_start_kernel_cmd   *ecmd;
};

typedef struct task_args {
    int thread_id;
    int queueLength;
    unsigned int total;
    std::string dev_str;
    std::string xclbin_fn;
    Clock::time_point start;
    Clock::time_point end;
} arg_t;

struct krnl_info {
    std::string     name;
    bool            new_style;
    int             cu_idx;
};

bool verbose = false;
barrier barrier;
struct krnl_info krnl = {"hello", false};

static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p, --path <path>\n";
    std::cout << "  -k, --kernel <kernel> (imply old style verify.xclbin is used) \n";
    std::cout << "  -d, --device <device> \n";
    std::cout << "  -t, --threads <number of threads> \n";
    std::cout << "  -l, --length <length of queue> (send how many commands without waiting) \n";
    std::cout << "  -a, --total <total amount of commands per thread>\n";
    std::cout << "  -v, --verbose <verbose result>\n";
    std::cout << "  -s, --supported <supported>\n";
    std::cout << "  -h, --help <help>\n";
}

static std::vector<char>
load_file_to_memory(const std::string& fn)
{
    if (fn.empty())
        throw std::runtime_error("No xclbin specified");

    // load bit stream
    std::ifstream stream(fn);
    stream.seekg(0,stream.end);
    size_t size = stream.tellg();
    stream.seekg(0,stream.beg);

    std::vector<char> bin(size);
    stream.read(bin.data(), size);

    return bin;
}

double runTest(xclDeviceHandle handle, std::vector<std::shared_ptr<task_info>>& cmds,
               unsigned int total, arg_t &arg)
{
    int i = 0;
    unsigned int issued = 0, completed = 0;
    arg.start = Clock::now();

    for (auto& cmd : cmds) {
        if (xclExecBuf(handle, cmd->exec_bo))
            throw std::runtime_error("Unable to issue exec buf");
        if (++issued == total)
            break;
    }

    while (completed < total) {
        /* assume commands to the same CU finished in order */
        while (cmds[i]->ecmd->state < ERT_CMD_STATE_COMPLETED) {
            while (xclExecWait(handle, -1) == 0);
        }
        if (cmds[i]->ecmd->state != ERT_CMD_STATE_COMPLETED)
            throw std::runtime_error("CU execution failed");

        completed++;
        if (issued < total) {
            if (xclExecBuf(handle, cmds[i]->exec_bo))
                throw std::runtime_error("Unable to issue exec buf");
            issued++;
        }

        if (++i == cmds.size())
            i = 0;
    }

    arg.end = Clock::now();
    return (std::chrono::duration_cast<ms_t>(arg.end - arg.start)).count();
}

void fillCmdVector(xclDeviceHandle handle, std::vector<std::shared_ptr<task_info>> &cmds,
        int bank, int expected_cmds)
{
    int rsz;

    for (int i = 0; i < expected_cmds; i++) {
        task_info cmd;
        cmd.boh = xclAllocBO(handle, 20, 0, bank);
        if (cmd.boh == NULLBO) {
            std::cout << "Could not allocate more output buffers" << std::endl;
            break;
        }
        xclBOProperties prop;
        if (xclGetBOProperties(handle, cmd.boh, &prop)) {
            std::cout << "Could not get bo properties" << std::endl;
            xclFreeBO(handle, cmd.boh);
            break;
        }
        uint64_t boh_addr = prop.paddr;

        cmd.exec_bo = xclAllocBO(handle, 4096, 0, XCL_BO_FLAGS_EXECBUF);
        if (cmd.exec_bo == NULLBO) {
            std::cout << "Could not allocate more exec buf" << std::endl;
            xclFreeBO(handle, cmd.boh);
            break;
        }
        cmd.ecmd = reinterpret_cast<ert_start_kernel_cmd *>(xclMapBO(handle, cmd.exec_bo, true));
        if (!cmd.ecmd) {
            std::cout << "Could not map more exec buf" << std::endl;
            xclFreeBO(handle, cmd.boh);
            xclFreeBO(handle, cmd.exec_bo);
            break;
        }

        if (krnl.new_style)
            /* Old style kernel has 1 argument */
            rsz = 5;
        else
            /* Old style kernel has 7 arguments */
            rsz = 17;

        cmd.ecmd->opcode = ERT_START_CU;
        cmd.ecmd->count = rsz;
        cmd.ecmd->cu_mask = 0x1 << krnl.cu_idx;
        cmd.ecmd->data[rsz - 1] = boh_addr;
        cmd.ecmd->data[rsz] = boh_addr >> 32;

        cmds.push_back(std::make_shared<task_info>(cmd));
    }
    //std::cout << "Allocated commands, expect " << expected_cmds << ", created " << cmds.size() << std::endl;
}

void runTestThread(arg_t &arg)
{
    xclDeviceHandle handle;
    xuid_t uuid;
    int bank = 0;
    std::vector<std::shared_ptr<task_info>> cmds;

    if (arg.dev_str.find(":") == std::string::npos)
        handle = xclOpen(std::stoi(arg.dev_str), "", XCL_QUIET);
    else
        handle = xclOpenByBDF(arg.dev_str.c_str());

    if (!handle)
        throw std::runtime_error("Could not open device");

    auto xclbin = load_file_to_memory(arg.xclbin_fn);
    auto top = reinterpret_cast<const axlf*>(xclbin.data());
    auto topo = xclbin::get_axlf_section(top, MEM_TOPOLOGY);
    auto topology = reinterpret_cast<mem_topology*>(xclbin.data() + topo->m_sectionOffset);
    if (xclLoadXclBin(handle, top))
        throw std::runtime_error("Bitstream download failed");

    uuid_copy(uuid, top->m_header.uuid);

    for (int i = 0; i < topology->m_count; ++i) {
        if (topology->m_mem_data[i].m_used) {
            bank = i;
            break;
        }
    }

    // CU name shoue be "hello:hello_1" or "verify:verify_1"
    std::string cu_name = krnl.name + ":" + krnl.name + "_1";
    // Do not store cu_idx directly in krnl object. This object is shared between multiple threads
    // Update this object when we get the valid index.
    int cu_idx = xclIPName2Index(handle, cu_name.c_str());
    if (cu_idx < 0) {
        // hello:hello_cu0 is U2 shell special
        cu_name = krnl.name + ":" + krnl.name + "_cu0";
        cu_idx = xclIPName2Index(handle, cu_name.c_str());
        if (cu_idx < 0)
            throw std::runtime_error(cu_name + " not found");
    }
    krnl.cu_idx = cu_idx;

    if (xclOpenContext(handle, uuid, krnl.cu_idx, true))
        throw std::runtime_error("Cound not open context");

    fillCmdVector(handle, cmds, bank, arg.queueLength);

    barrier.wait();

    double duration = runTest(handle, cmds, arg.total, arg);

    barrier.wait();

    for (auto& cmd : cmds) {
        xclFreeBO(handle, cmd->boh);
        xclUnmapBO(handle, cmd->exec_bo, cmd->ecmd);
        xclFreeBO(handle, cmd->exec_bo);
    }

    xclCloseContext(handle, uuid, 0);
}

int testMultiThreads(std::string &dev, std::string &xclbin_fn, int threadNumber, int queueLength, unsigned int total)
{
    std::thread threads[threadNumber];
    std::vector<arg_t> arg(threadNumber);

    barrier.init(threadNumber + 1);

    for (int i = 0; i < threadNumber; i++) {
        arg[i].thread_id = i;
        arg[i].dev_str = dev;
        arg[i].queueLength = queueLength;
        arg[i].total = total;
        arg[i].xclbin_fn = xclbin_fn;
        threads[i] = std::thread([&](int i){ runTestThread(arg[i]); }, i);
    }

    /* Wait threads to prepare to start */
    barrier.wait();
    auto start = Clock::now();

    /* Wait threads done */
    barrier.wait();
    auto end = Clock::now();

    for (int i = 0; i < threadNumber; i++)
        threads[i].join();

    /* calculate performance */
    int overallCommands = 0;
    double duration;
    for (int i = 0; i < threadNumber; i++) {
        if (verbose) {
            duration = (std::chrono::duration_cast<ms_t>(arg[i].end - arg[i].start)).count();
            std::cout << "Thread " << arg[i].thread_id
                << " Commands: " << std::setw(7) << total
                << std::setprecision(0) << std::fixed
                << " IOPS: " << (total * 1000000.0 / duration)
                << std::endl;
        }
        overallCommands += total;
    }

    duration = (std::chrono::duration_cast<ms_t>(end - start)).count();
    std::cout << "Overall Commands: " << std::setw(7) << overallCommands
              << std::setprecision(0) << std::fixed
              << " IOPS: " << (overallCommands * 1000000.0 / duration)
              << " (" << krnl.name << ")"
              << std::endl;
    return 0;
}

int _main(int argc, char* argv[])
{
    std::string device_str = "0";
    std::string test_path;
    int threadNumber = 2;
    int queueLength = 128;
    int total = 50000;
    std::string xclbin_fn;
    bool verbose = true;
    bool flag_s = false;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--path") == 0)) {
            test_path = argv[i + 1];
        } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--device") == 0)) {
            device_str = argv[i + 1];
        } else if ((strcmp(argv[i], "-k") == 0) || (strcmp(argv[i], "--kernel") == 0)) {
            xclbin_fn = test_path + argv[i + 1];
        } else if ((strcmp(argv[i], "-t") == 0) || (strcmp(argv[i], "--threads") == 0)) {
            threadNumber = atoi(argv[i + 1]);
        } else if ((strcmp(argv[i], "-l") == 0) || (strcmp(argv[i], "--length") == 0)) {
            queueLength = atoi(argv[i + 1]);
        } else if ((strcmp(argv[i], "-a") == 0) || (strcmp(argv[i], "--total") == 0)) {
            total = atoi(argv[i + 1]);
        } else if ((strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--verbose") == 0)) {
            verbose = atoi(argv[i + 1]);
        } else if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--supported") == 0)) {
            flag_s = true;
        } else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            printHelp();
            return 1;
        }
    }

    if (test_path.empty() && xclbin_fn.empty()) {
        std::cout << "ERROR : please provide the platform test path to -p option\n";
        return EXIT_FAILURE;
    }

    if (xclbin_fn.empty()) {
        xclbin_fn = test_path + "/verify.xclbin";
        krnl.name = "verify";
        krnl.new_style = true;
    }
    /* Sanity check */
    std::ifstream infile(xclbin_fn);
    if (!infile.good())
        throw std::runtime_error("Wrong xclbin file " + xclbin_fn);

    if (queueLength <= 0)
        throw std::runtime_error("Negative/Zero queue length");

    if (total <= 0)
        throw std::runtime_error("Negative/Zero total command number");

    if (threadNumber <= 0)
        throw std::runtime_error("Invalid thread number");

    if (flag_s) {
        if (!infile.good()) {
            std::cout << "\nNOT SUPPORTED" << std::endl;
            return EOPNOTSUPP;
        } else {
            std::cout << "\nSUPPORTED" << std::endl;
            return EXIT_SUCCESS;
        }
    }
        testMultiThreads(device_str, xclbin_fn, threadNumber, queueLength, total);

    return 0;
}

int main(int argc, char *argv[])
{
    try {
        _main(argc, argv);
        std::cout << "TEST PASSED" << std::endl;
        return EXIT_SUCCESS;
    }
    catch (const std::exception& ex) {
        std::cout << "TEST FAILED: " << ex.what() << std::endl;
    }
    catch (...) {
        std::cout << "TEST FAILED" << std::endl;
    }

    return EXIT_FAILURE;
};
