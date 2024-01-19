// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestIOPS.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include <fstream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <boost/format.hpp>

#include "xrt_iops_util/xilutil.hpp"
#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

using ms_t = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

typedef struct task_args {
  int thread_id;
  int queueLength;
  unsigned int total;
  Clock::time_point start;
  Clock::time_point end;
} arg_t;

struct krnl_info {
  std::string     name;
  bool            new_style;
};

static bool verbose = false;
static barrier barrier;
static struct krnl_info krnl = {"hello", false};

// ----- C L A S S   M E T H O D S -------------------------------------------
TestIOPS::TestIOPS()
  : TestRunner("iops", 
                "Run scheduler performance measure test", 
                "verify.xclbin"){}

boost::property_tree::ptree
TestIOPS::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  runTest(dev, ptree);
  return ptree;
}

static double 
runThread(std::vector<xrt::run>& cmds, unsigned int total, arg_t &arg)
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

    i++;
    if (static_cast<long unsigned int>(i) == cmds.size())
      i = 0;
  }

  arg.end = Clock::now();
  return static_cast<double>((std::chrono::duration_cast<ms_t>(arg.end - arg.start)).count());
}

static void runTestThread(const xrt::device& device, const xrt::kernel& hello, arg_t& arg)
{
  std::vector<xrt::run> cmds;

  for (int i = 0; i < arg.queueLength; i++) {
    auto run = xrt::run(hello);
    run.set_arg(0, xrt::bo(device, 20, hello.group_id(0)));
    cmds.push_back(std::move(run));
  }
  barrier.wait();

  runThread(cmds, arg.total, arg);

  barrier.wait();
}

void 
TestIOPS::testMultiThreads(const std::string &dev, const std::string &xclbin_fn, 
                          int threadNumber, int queueLength, unsigned int total, boost::property_tree::ptree& ptree)
{
  std::vector<std::thread> threads(threadNumber);
  std::vector<arg_t> arg(threadNumber);

  xrt::device device(dev);
  auto uuid = device.load_xclbin(xclbin_fn);
  xrt::kernel hello;
  try {
    hello = xrt::kernel(device, uuid.get(), krnl.name);
  } catch (const std::exception&) {
    krnl.name = "hello";
    krnl.new_style = false;
    try {
      hello = xrt::kernel(device, uuid.get(), krnl.name);
    } catch (const std::exception&) {
      logger(ptree, "Error", "Kernel could not be found.");
      ptree.put("status", test_token_failed);
      return;
    }
  }

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
      duration = static_cast<double>((std::chrono::duration_cast<ms_t>(arg[i].end - arg[i].start)).count());
      logger(ptree, boost::str(boost::format("Details for Thread %d") % arg[i].thread_id), 
                    boost::str(boost::format("Commands: %d IOPS: %f") % total % boost::io::group(std::setprecision(0), std::fixed, (total * 1000000.0 / duration))));
    }
    overallCommands += total;
  }

  duration = static_cast<double>((std::chrono::duration_cast<ms_t>(end - start)).count());
  logger(ptree, "Details", boost::str(boost::format("Overall Commands: %d, IOPS: %f (%s)")
                % total % boost::io::group(std::setprecision(0), std::fixed, (overallCommands * 1000000.0 / duration)) % krnl.name));
  ptree.put("status", test_token_passed);
}

void
TestIOPS::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  const std::string test_path = findPlatformPath(dev, ptree);
  std::string b_file = findXclbinPath(dev, ptree); // verify.xclbin
  const int threadNumber = 2;
  const int queueLength = 128;
  const int total = 50000;

  if (b_file.empty()) {
    if (test_path.empty()) {
      logger(ptree, "Error", "Platform test path could not be found.");
      ptree.put("status", test_token_failed);
      return;
    }
    b_file = (std::filesystem::path(test_path) / "verify.xclbin").string();
  }

  krnl.name = "verify";
  krnl.new_style = true;

  const std::string xclbin_fn = b_file;
  auto retVal = validate_binary_file(xclbin_fn);
  if (retVal == EOPNOTSUPP) {
    ptree.put("status", test_token_skipped);
    return;
  }

  const auto bdf_tuple = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  const std::string bdf = xrt_core::query::pcie_bdf::to_string(bdf_tuple);
  try {
    testMultiThreads(bdf, xclbin_fn, threadNumber, queueLength, total, ptree);
    return;
  }
  catch (const std::exception& ex) {
    logger(ptree, "Error", ex.what());
  }
  catch (...) {
    // Test failed.
  }
  ptree.put("status", test_token_failed);
}
