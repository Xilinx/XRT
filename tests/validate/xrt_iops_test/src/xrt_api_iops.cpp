#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>

#include "xilutil.hpp"
#include "experimental/xrt_device.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_kernel.h"

using ms_t = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

typedef struct task_args {
  int thread_id;
  int queueLength;
  unsigned int total;
  Clock::time_point start;
  Clock::time_point end;
} arg_t;

bool verbose = false;
barrier barrier;

static void usage(char *prog)
{
  std::cout << "Usage: " << prog << " -k <xclbin> -d <dev id> [options]\n"
    << "options:\n"
    << "    -t       number of threads\n"
    << "    -l       length of queue (send how many commands without waiting)\n"
    << "    -a       total amount of commands per thread\n"
    << "    -v       verbose result\n"
    << std::endl;
}

double runTest(std::vector<xrt::run>& cmds, unsigned int total, arg_t &arg)
{
  int i = 0;
  unsigned int issued = 0, completed = 0;
  arg.start = Clock::now();

  for (auto& cmd : cmds) {
    cmd.start();
    if (++issued == total)
      break;
  }

  while (completed < total) {
    cmds[i].wait();

    completed++;
    if (issued < total) {
      cmds[i].start();
      issued++;
    }

    if (++i == cmds.size())
      i = 0;
  }

  arg.end = Clock::now();
  return (std::chrono::duration_cast<ms_t>(arg.end - arg.start)).count();
}

int testSingleThread(int dev_id, std::string &xclbin_fn)
{
  /* The command would incease */
  std::vector<unsigned int> cmds_per_run = { 50000,100000,500000,1000000 };
  int expected_cmds = 128;
  std::vector<arg_t> arg(1);

  auto device = xrt::device(dev_id);
  auto uuid = device.load_xclbin(xclbin_fn);

  auto hello = xrt::kernel(device, uuid.get(), "hello");

  arg[0].thread_id = 0;
  /* Create 'expected_cmds' commands if possible */
  std::vector<xrt::run> cmds;
  for (int i = 0; i < expected_cmds; i++) {
    auto run = xrt::run(hello);
    run.set_arg(0, xrt::bo(device, 20, hello.group_id(0)));
    cmds.push_back(std::move(run));
  }
  std::cout << "Allocated commands, expect " << expected_cmds << ", created " << cmds.size() << std::endl;

  for (auto num_cmds : cmds_per_run) {
    double duration = runTest(cmds, num_cmds, arg[0]);
    std::cout << "Commands: " << std::setw(7) << num_cmds
      << " IOPS: " << (num_cmds * 1000.0 * 1000.0 / duration)
      << std::endl;
  }

  return 0;
}

void runTestThread(xrt::device &device, xrt::kernel &hello, arg_t &arg)
{
  std::vector<xrt::run> cmds;

  for (int i = 0; i < arg.queueLength; i++) {
    auto run = xrt::run(hello);
    run.set_arg(0, xrt::bo(device, 20, hello.group_id(0)));
    cmds.push_back(std::move(run));
  }
  barrier.wait();

  double duration = runTest(cmds, arg.total, arg);

  barrier.wait();
}

int testMultiThreads(int dev_id, std::string &xclbin_fn, int threadNumber, int queueLength, unsigned int total)
{
  std::thread threads[threadNumber];
  std::vector<arg_t> arg(threadNumber);

  auto device = xrt::device(dev_id);
  auto uuid = device.load_xclbin(xclbin_fn);
  auto hello = xrt::kernel(device, uuid.get(), "hello");

  barrier.init(threadNumber + 1);

  for (int i = 0; i < threadNumber; i++) {
    arg[i].thread_id = i;
    arg[i].queueLength = queueLength;
    arg[i].total = total;
    threads[i] = std::thread([&](int i){ runTestThread(device, hello, arg[i]); }, i);
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
            << std::endl;
  return 0;
}

int _main(int argc, char* argv[])
{
  if (argc < 3) {
    usage(argv[0]);
    return 1;
  }

  std::string xclbin_fn;
  int dev_id = 0;
  int queueLength = 128;
  unsigned total = 50000;
  int threadNumber = 2;

  std::vector<std::string> args(argv + 1, argv + argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage(argv[0]);
      return 1;
    }
    else if (arg == "-v") {
      verbose = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-k")
      xclbin_fn = arg;
    else if (cur == "-d")
      dev_id = std::stoi(arg);
    else if (cur == "-t")
      threadNumber = std::stoi(arg);
    else if (cur == "-l")
      queueLength = std::stoi(arg);
    else if (cur == "-a")
      total = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  /* Sanity check */
  if (dev_id < 0)
    throw std::runtime_error("Negative device ID");

  if (queueLength <= 0)
    throw std::runtime_error("Negative/Zero queue length");

  if (threadNumber <= 0)
    throw std::runtime_error("Invalid thread number");

  //printf("The system has %d device(s)\n", xclProbe());
  auto device = xrt::device(dev_id);
  auto uuid = device.load_xclbin(xclbin_fn);

  //testSingleThread(dev_id, xclbin_fn);
  testMultiThreads(dev_id, xclbin_fn, threadNumber, queueLength, total);

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
