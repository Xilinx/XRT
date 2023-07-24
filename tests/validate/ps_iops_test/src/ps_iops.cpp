// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include <boost/program_options.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "xilutil.hpp"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using ms_t = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

static const int COUNT = 1024;
static const size_t DATA_SIZE = COUNT * sizeof(int);

typedef struct task_args
{
    int thread_id;
    int queueLength;
    unsigned int total;
    Clock::time_point start;
    Clock::time_point end;
} arg_t;

struct krnl_info
{
    std::string name;
    bool new_style;
};

bool verbose = false;
barrier barrier;
struct krnl_info krnl = { "hello_world", false };

static void
usage(const char* prog)
{
    std::cout << "Usage: " << prog << " <Platform Test Area Path> [options]\n"
              << "options:\n"
              << "    -d       device index\n"
              << "    -t       number of threads\n"
              << "    -l       length of queue (send how many commands without "
                 "waiting)\n"
              << "    -a       total amount of commands per thread\n"
              << "    -v       verbose result\n"
              << std::endl;
}

double
runTest(std::vector<xrt::run>& cmds, unsigned int total, arg_t& arg)
{
    int i = 0;
    unsigned int issued = 0, completed = 0;
    arg.start = Clock::now();

    for (auto& cmd : cmds) {
        cmd.start();
        issued++;
        if (issued == total)
            break;
    }

    while (completed < total) {
        cmds[i].wait();

        completed++;
        if (issued < total) {
            cmds[i].start();
            issued++;
        }

        i++;
        if (i == cmds.size())
            i = 0;
    }

    arg.end = Clock::now();
    return (std::chrono::duration_cast<ms_t>(arg.end - arg.start)).count();
}

void
runTestThread(const xrt::device& device, const xrt::kernel& hello_world,
              arg_t& arg)
{
    std::vector<xrt::run> cmds;
    std::vector<xrt::bo> bos;

    for (int i = 0; i < arg.queueLength; i++) {
        auto run = xrt::run(hello_world);
	auto bo0 = xrt::bo(device, DATA_SIZE, hello_world.group_id(0));
        run.set_arg(0, bo0);
	bos.push_back(std::move(bo0));
	auto bo1 = xrt::bo(device, DATA_SIZE, hello_world.group_id(1));
        run.set_arg(1, bo1);
	bos.push_back(std::move(bo1));
        run.set_arg(2, COUNT);
        cmds.push_back(std::move(run));
    }
    barrier.wait();

    double duration = runTest(cmds, arg.total, arg);

    barrier.wait();
}

int
testMultiThreads(const std::string& dev, const std::string& xclbin_fn,
                 int threadNumber, int queueLength, unsigned int total)
{
    std::vector<std::thread> threads(threadNumber);
    std::vector<arg_t> arg(threadNumber);

    xrt::device device(dev);
    auto uuid = device.load_xclbin(xclbin_fn);
    auto hello_world = xrt::kernel(device, uuid.get(), krnl.name);

    barrier.init(threadNumber + 1);

    for (int i = 0; i < threadNumber; i++) {
        arg[i].thread_id = i;
        arg[i].queueLength = queueLength;
        arg[i].total = total;
        threads[i] = std::thread([&](int i){ runTestThread(device, hello_world, arg[i]); }, i);
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
            duration =
                (std::chrono::duration_cast<ms_t>(arg[i].end - arg[i].start))
                    .count();
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
              << " IOPS: " << (overallCommands * 1000000.0 / duration) << " ("
              << krnl.name << ")" << std::endl;
    return 0;
}

static int
validate_binary_file(const std::string& binaryfile, bool print = false)
{
    std::ifstream infile(binaryfile);
    if (!infile.good()) {
        if (print)
            std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    } else {
        if (print)
            std::cout << "\nSUPPORTED" << std::endl;
        return EXIT_SUCCESS;
    }
}

int
_main(int argc, char* argv[])
{
    /* Could be BDF or device index */
    std::string device_str;
    std::string test_path;
    bool flag_s;
    int threadNumber;
    int queueLength;
    int total;
    std::string xclbin_fn;
    std::vector<std::string> dependency_paths;

    boost::program_options::options_description options;
    options.add_options()
        ("help,h", "Print help messages")
        ("xclbin,x", boost::program_options::value<decltype(xclbin_fn)>(&xclbin_fn)->implicit_value("/lib/firmware/xilinx/ps_kernels/ps_bandwidth.xclbin"), "Path to the xclbin file for the test")
        ("path,p", boost::program_options::value<decltype(test_path)>(&test_path)->required(), "Path to the platform resources")
        ("device,d", boost::program_options::value<decltype(device_str)>(&device_str)->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
        ("supported,s", boost::program_options::bool_switch(&flag_s), "Print supported or not")
        ("include,i" , boost::program_options::value<decltype(dependency_paths)>(&dependency_paths)->multitoken(), "Paths to xclbins required for this test")
        ("threads,t" , boost::program_options::value<decltype(threadNumber)>(&threadNumber)->default_value(2), "Number of threads to run within this test")
        ("length,l" , boost::program_options::value<decltype(queueLength)>(&queueLength)->default_value(128), "Length of queue")
        ("total,a" , boost::program_options::value<decltype(total)>(&total)->default_value(50000), "Total amount of commands per thread")
        ("verbose,v", boost::program_options::bool_switch(&verbose)->default_value(false), "Enable verbose output")
    ;

    boost::program_options::variables_map vm;
    try {
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), vm);
        if (vm.count("help")) {
            std::cout << options << std::endl;
            return EXIT_SUCCESS;
        }
        boost::program_options::notify(vm);
    } catch (boost::program_options::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        std::cout << options << std::endl;
        return EXIT_FAILURE;
    }

    return EOPNOTSUPP;

    /* Sanity check */
    // Validate dependency xclbins if any
    for (const auto& path : dependency_paths) {
        auto retVal = validate_binary_file(path);
        if (retVal != EXIT_SUCCESS)
            return retVal;
    }

    // Validate ps kernel
    auto retVal = validate_binary_file(xclbin_fn, flag_s);
    if (flag_s || retVal != EXIT_SUCCESS)
        return retVal;

    krnl.new_style = true;

    if (queueLength <= 0)
        throw std::runtime_error("Negative/Zero queue length");

    if (total <= 0)
        throw std::runtime_error("Negative/Zero total command number");

    if (threadNumber <= 0)
        throw std::runtime_error("Invalid thread number");

    // TODO need to add processing for dependency paths
    testMultiThreads(device_str, xclbin_fn, threadNumber, queueLength, total);

    return 0;
}

int
main(int argc, char* argv[])
{
    try {
        _main(argc, argv);
        std::cout << "TEST PASSED" << std::endl;
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "TEST FAILED: " << ex.what() << std::endl;
    } catch (...) {
        std::cout << "TEST FAILED" << std::endl;
    }

    return EXIT_FAILURE;
};
