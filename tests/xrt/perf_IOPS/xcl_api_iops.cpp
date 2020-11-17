#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>
#include <sys/mman.h>
#include <getopt.h>

#include "xrt.h"
#include "ert.h"
#include "xclbin.h"

using ms_t = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

bool start = false;
bool stop = false;
pthread_barrier_t barrier;

struct task_info {
    unsigned                boh;
    unsigned                exec_bo;
    ert_start_kernel_cmd   *ecmd;
};

typedef struct task_args {
    int thread_id;
    int bank;
    int queueLength;
    unsigned int total;
    xclDeviceHandle handle;
    Clock::time_point start;
    Clock::time_point end;
} arg_t;

void usage()
{
    printf("Usage: test -k <xclbin>\n");
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
               unsigned int total, arg_t *arg)
{
    int i = 0;
    unsigned int issued = 0, completed = 0;
    arg->start = Clock::now();

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

    arg->end = Clock::now();
    return (std::chrono::duration_cast<ms_t>(arg->end - arg->start)).count();
}

void fillCmdVector(xclDeviceHandle handle, std::vector<std::shared_ptr<task_info>> &cmds,
        int bank, int expected_cmds)
{
    for (int i = 0; i < expected_cmds; i++) {
        task_info cmd;
        cmd.boh = xclAllocBO(handle, 20, 0, bank);
        if (cmd.boh == NULLBO) {
            std::cout << "Could not allocate more output buffers" << std::endl;
            break;
        }
        xclBOProperties prop;
        xclGetBOProperties(handle, cmd.boh, &prop);
        uint64_t boh_addr = prop.paddr;

        cmd.exec_bo = xclAllocBO(handle, 4096, 0, XCL_BO_FLAGS_EXECBUF);
        if (cmd.exec_bo == NULLBO) {
            std::cout << "Could not allocate more exec buf" << std::endl;
            xclFreeBO(handle, cmd.boh);
            break;
        }
        cmd.ecmd = reinterpret_cast<ert_start_kernel_cmd *>(xclMapBO(handle, cmd.exec_bo, true));
        if (cmd.ecmd == MAP_FAILED) {
            std::cout << "Could not map more exec buf" << std::endl;
            xclFreeBO(handle, cmd.boh);
            xclFreeBO(handle, cmd.exec_bo);
            break;
        }

        int rsz = 19;
        cmd.ecmd->opcode = ERT_START_CU;
        cmd.ecmd->count = rsz;
        cmd.ecmd->cu_mask = 0x1;
        cmd.ecmd->data[rsz - 3] = boh_addr;
        cmd.ecmd->data[rsz - 2] = boh_addr >> 32;

        cmds.push_back(std::make_shared<task_info>(cmd));
    }
    //std::cout << "Allocated commands, expect " << expected_cmds << ", created " << cmds.size() << std::endl;
}

int testSingleThread(xclDeviceHandle handle, xuid_t uuid, int bank)
{
    std::vector<std::shared_ptr<task_info>> cmds;
    /* The command would incease */
    //std::vector<unsigned int> cmds_per_run = { 50000,100000,500000,1000000 };
    std::vector<unsigned int> cmds_per_run = { 5000000 };
    /* There is performance and reach maximum FD limited issue */
    //int expected_cmds = 100000;
    int expected_cmds = 128;
    arg_t *arg = (arg_t *)malloc(sizeof(arg_t));

    if (xclOpenContext(handle, uuid, 0, true))
        throw std::runtime_error("Cound not open context");

    /* Create 'expected_cmds' commands if possible */
    fillCmdVector(handle, cmds, bank, expected_cmds);

    arg->thread_id = 0;
    for (auto& num_cmds : cmds_per_run) {
#if 0
        double total = 0;
        for (int i = 0; i < 5; i++) {
            total += runTest(handle, cmds, num_cmds);
        }
        double duration = total / 5;
#else
        double duration = runTest(handle, cmds, num_cmds, arg);
#endif
        std::cout << "Commands: " << std::setw(7) << num_cmds
                  << " iops: " << (num_cmds * 1000.0 * 1000.0 / duration)
                  << std::endl;
    }

    for (auto& cmd : cmds) {
        xclFreeBO(handle, cmd->boh);
        munmap(cmd->ecmd, 4096);
        xclFreeBO(handle, cmd->exec_bo);
    }

    xclCloseContext(handle, uuid, 0);
    return 0;
}

void *runTestThread(void *data)
{
    arg_t *arg = (arg_t *)data;
    std::vector<std::shared_ptr<task_info>> cmds;
    /* The command would incease */

    fillCmdVector(arg->handle, cmds, arg->bank, arg->queueLength);

    pthread_barrier_wait(&barrier);

    //wait start from main thread

    double duration = runTest(arg->handle, cmds, arg->total, arg);

    pthread_barrier_wait(&barrier);

    for (auto& cmd : cmds) {
        xclFreeBO(arg->handle, cmd->boh);
        munmap(cmd->ecmd, 4096);
        xclFreeBO(arg->handle, cmd->exec_bo);
    }
}

/* let's start from test two threads */
int testMultiThreads(xclDeviceHandle handle, xuid_t uuid, int bank,
        int threadNumber, int queueLength, unsigned int total)
{
    pthread_t *tids = (pthread_t *)malloc(threadNumber * sizeof(pthread_t));
    arg_t *arg[threadNumber];

    if (xclOpenContext(handle, uuid, 0, true))
        throw std::runtime_error("Cound not open context");

    pthread_barrier_init(&barrier, NULL, threadNumber+1);

    for (int i = 0; i < threadNumber; i++) {
        arg[i] = (arg_t *)malloc(sizeof(arg_t));
        arg[i]->thread_id = i;
        arg[i]->bank = bank;
        arg[i]->handle = handle;
        arg[i]->queueLength = queueLength;
        arg[i]->total = total;
        pthread_create(&tids[i], NULL, runTestThread, arg[i]);
    }

    /* Wait threads to prepare to start */
    pthread_barrier_wait(&barrier);
    auto start = Clock::now();

    /* Wait threads done */
    pthread_barrier_wait(&barrier);
    auto end = Clock::now();

    for (int i = 0; i < threadNumber; i++)
        pthread_join(tids[i], NULL);

    int overallCommands = 0;
    double duration;
    for (int i = 0; i < threadNumber; i++) {
        duration = (std::chrono::duration_cast<ms_t>(arg[i]->end - arg[i]->start)).count();
        std::cout << "Thread " << arg[i]->thread_id
                  << " Commands: " << std::setw(7) << total
                  << std::setprecision(0) << std::fixed
                  << " iops: " << (total * 1000000.0 / duration)
                  << std::endl;
        overallCommands += total;
    }
    duration = (std::chrono::duration_cast<ms_t>(end - start)).count();
    std::cout << "Overall Commands: " << std::setw(7) << overallCommands
              << " iops: " << (overallCommands * 1000000.0 / duration)
              << std::endl;

    xclCloseContext(handle, uuid, 0);
    return 0;
}

int _main(int argc, char* argv[])
{
    xclDeviceHandle handle;
    std::string xclbin_fn;
    xuid_t uuid;
    int first_mem = 0;
    int dev_id = 0;
    int queueLength = 128;
    unsigned total = 50000;
    int threadNumber = 2;
    char c;

    while ((c = getopt(argc, argv, "k:d:l:t:a:h")) != -1) {
        switch (c) {
            case 'k':
                xclbin_fn = optarg; 
                break;
            case 'd':
                dev_id = std::stoi(optarg);
                break;
            case 't':
                threadNumber = std::stoi(optarg);
                break;
            case 'l':
                queueLength = std::stoi(optarg);
                break;
            case 'a':
                total = std::stoi(optarg);
                break;
            case 'h':
                usage();
        }
    }

    /* Sanity check */
    if (dev_id < 0)
        throw std::runtime_error("Negative device ID");

    if (queueLength <= 0)
        throw std::runtime_error("Negative/Zero queue length");

    if (threadNumber <= 0)
        throw std::runtime_error("Invalid thread number");

    printf("The system has %d device(s)\n", xclProbe());

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
            first_mem = i;
            break;
        }
    }

    //testSingleThread(handle, uuid, first_mem);
    testMultiThreads(handle, uuid, first_mem, threadNumber, queueLength, total);

    xclClose(handle);
    return 0;
}

int main(int argc, char *argv[])
{
    try {
        _main(argc, argv);
        return 0;
    }
    catch (const std::exception& ex) {
        std::cout << "TEST FAILED: " << ex.what() << std::endl;
    }
    catch (...) {
        std::cout << "TEST FAILED" << std::endl;
    }

    return 1;
};
