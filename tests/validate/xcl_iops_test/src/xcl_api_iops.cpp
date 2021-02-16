#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

#include "cmdlineparser.h"

#include "xilutil.hpp"
#include "xrt.h"
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
    int dev_id;
    int queueLength;
    unsigned int total;
    std::string xclbin_fn;
    Clock::time_point start;
    Clock::time_point end;
} arg_t;

struct krnl_info {
    std::string     name;
    bool            new_style;
};

bool verbose = false;
barrier barrier;
struct krnl_info krnl = {"hello", false};

static void usage(char *prog)
{
  std::cout << "Usage: " << prog << " <Platform Test Area Path> [options]\n"
            << "options:\n"
            << "    -d       device BDF\n"
            << "    -t       number of threads\n"
            << "    -l       length of queue (send how many commands without waiting)\n"
            << "    -a       total amount of commands per thread\n"
            << "    -v       verbose result\n"
            << std::endl;
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
        cmd.ecmd->cu_mask = 0x1;
        cmd.ecmd->data[rsz - 1] = boh_addr;
        cmd.ecmd->data[rsz] = boh_addr >> 32;

        cmds.push_back(std::make_shared<task_info>(cmd));
    }
    //std::cout << "Allocated commands, expect " << expected_cmds << ", created " << cmds.size() << std::endl;
}

int testSingleThread(int dev_id, std::string &xclbin_fn)
{
    xclDeviceHandle handle;
    xuid_t uuid;
    int bank = 0;
    std::vector<std::shared_ptr<task_info>> cmds;
    /* The command would incease */
    std::vector<unsigned int> cmds_per_run = { 50000,100000,500000,1000000 };
    /* There is performance and reach maximum FD limited issue */
    //int expected_cmds = 100000;
    int expected_cmds = 128;
    std::vector<arg_t> arg(1);

    handle = xclOpen(dev_id, "", XCL_QUIET);
    if (!handle) {
        printf("Could not open device\n");
        return 1;
    }

    auto xclbin = load_file_to_memory(xclbin_fn);
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

    if (xclOpenContext(handle, uuid, 0, true))
        throw std::runtime_error("Cound not open context");

    /* Create 'expected_cmds' commands if possible */
    fillCmdVector(handle, cmds, bank, expected_cmds);

    arg[0].thread_id = 0;
    for (auto& num_cmds : cmds_per_run) {
#if 0
        double total = 0;
        for (int i = 0; i < 5; i++) {
            total += runTest(handle, cmds, num_cmds);
        }
        double duration = total / 5;
#else
        double duration = runTest(handle, cmds, num_cmds, arg[0]);
#endif
        std::cout << "Commands: " << std::setw(7) << num_cmds
                  << " IOPS: " << (num_cmds * 1000.0 * 1000.0 / duration)
                  << std::endl;
    }

    for (auto& cmd : cmds) {
        xclFreeBO(handle, cmd->boh);
        xclUnmapBO(handle, cmd->exec_bo, cmd->ecmd);
        xclFreeBO(handle, cmd->exec_bo);
    }

    xclCloseContext(handle, uuid, 0);
    xclClose(handle);
    return 0;
}

void runTestThread(arg_t &arg)
{
    xclDeviceHandle handle;
    xuid_t uuid;
    int bank = 0;
    std::vector<std::shared_ptr<task_info>> cmds;

    handle = xclOpen(arg.dev_id, "", XCL_QUIET);
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

    if (xclOpenContext(handle, uuid, 0, true))
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

int testMultiThreads(int dev_id, std::string &xclbin_fn, int threadNumber, int queueLength, unsigned int total)
{
    std::thread threads[threadNumber];
    std::vector<arg_t> arg(threadNumber);

    barrier.init(threadNumber + 1);

    for (int i = 0; i < threadNumber; i++) {
        arg[i].thread_id = i;
        arg[i].dev_id = dev_id;
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
    if (argc < 2) {
        usage(argv[0]);
        throw std::runtime_error("Number of argument should not less than 2");
    }

    // Command Line Parser
    sda::utils::CmdLineParser parser;

    // Switches
    //**************//"<Full Arg>",  "<Short Arg>", "<Description>", "<Default>"
    parser.addSwitch("--kernel",  "-k", "kernel (imply old style verify.xclbin is used)", "");
    parser.addSwitch("--device",  "-d", "device id", "0");
    parser.addSwitch("--threads", "-t", "number of threads", "2");
    parser.addSwitch("--length",  "-l", "length of queue", "128");
    parser.addSwitch("--total",   "-a", "total amount of commands per thread", "50000");
    parser.addSwitch("--verbose", "-v", "verbose output", "", true);
    parser.parse(argc, argv);

    /* Could be BDF or device index */
    std::string device_str = parser.value("device");
    int threadNumber = parser.value_to_int("threads");
    int queueLength = parser.value_to_int("length");
    int total = parser.value_to_int("total");
    std::string xclbin_fn = parser.value("kernel");
    if (xclbin_fn.empty()) {
        std::string test_path = argv[1];
        xclbin_fn = test_path + "/verify.xclbin";
        krnl.name = "verify";
        krnl.new_style = true;
    }
    verbose = parser.isValid("verbose");

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

    int dev_id = 0;
    if (device_str.find(":") == std::string::npos) {
        dev_id = std::stoi(device_str);
        if (dev_id < 0)
            throw std::runtime_error("Negative device index");
    } else
        throw std::runtime_error("Not support BDF");

    testMultiThreads(dev_id, xclbin_fn, threadNumber, queueLength, total);

    return 0;
}

int main(int argc, char *argv[])
{
    try {
        _main(argc, argv);
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
