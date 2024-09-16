// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestSpatialSharingOvd.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
#include <thread>

namespace XBU = XBUtilities;

static constexpr size_t host_app = 1; //opcode
static constexpr size_t buffer_size = 1024; // 1KB
static constexpr int itr_count = 1000;

// Method to wait for threads to be ready
// Parameters:
// - thread_num: Number of threads to wait for
// - mut: Mutex to lock the critical section
// - cond_var: Condition variable to wait on
// - thread_ready: Counter to track the number of ready threads
void TestSpatialSharingOvd::wait_for_threads_ready(uint32_t thread_num, std::mutex& mut, std::condition_variable& cond_var, uint32_t& thread_ready) {
  std::unique_lock<std::mutex> lock(mut);
  while (thread_ready != thread_num) {
    lock.unlock();
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    lock.lock();
  }
  cond_var.notify_all();
  lock.unlock();
}

// Constructor for BO_set
// BO_set is a collection of all the buffer objects so that the operations on all buffers can be done from a single object
// Parameters:
// - device: Reference to the xrt::device object
// - kernel: Reference to the xrt::kernel object
BO_set::BO_set(xrt::device& device, xrt::kernel& kernel) {
  // Initialize buffer objects with appropriate flags and group IDs
  bo_instr = xrt::bo(device, buffer_size, XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
  bo_ifm = xrt::bo(device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1));
  bo_param = xrt::bo(device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(2));
  bo_ofm = xrt::bo(device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3));
  bo_inter = xrt::bo(device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(4));
  bo_mc = xrt::bo(device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(7));

  // no-op instruction buffer
  std::memset(bo_instr.map<char*>(), (uint8_t)0, buffer_size);
}

// Method to synchronize buffer objects to the device
void BO_set::sync_bos_to_device() {
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_param.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_mc.sync(XCL_BO_SYNC_BO_TO_DEVICE);
}

// Method to set kernel arguments
// Parameters:
// - run: Reference to the xrt::run object
void BO_set::set_kernel_args(xrt::run& run) {
  uint64_t opcode = 1;
  run.set_arg(0, opcode);
  run.set_arg(1, bo_ifm);
  run.set_arg(2, bo_param);
  run.set_arg(3, bo_ofm);
  run.set_arg(4, bo_inter);
  run.set_arg(5, bo_instr);
  run.set_arg(6, buffer_size/sizeof(int));
  run.set_arg(7, bo_mc);
}

// Method to run the test case
// Parameters:
// - mut: Mutex to lock the critical section
// - cond_var: Condition variable to wait on
// - thread_ready: Counter to track the number of ready threads
void
TestCase::run(std::mutex& mut, std::condition_variable& cond_var, uint32_t& thread_ready)
{
  std::vector<xrt::kernel> kernels;
  std::vector<BO_set> bo_set_list;
  std::vector<xrt::run> run_list;

  // Initialize kernels, buffer objects, and runs
  for (uint32_t j = 0; j < queue_len; j++) {
    auto kernel = xrt::kernel(hw_ctx, kernel_name);
    auto bos = BO_set(device, kernel);
    bos.sync_bos_to_device();
    auto run = xrt::run(kernel);
    bos.set_kernel_args(run);
    run.start();
    run.wait2();

    kernels.push_back(kernel);
    bo_set_list.push_back(bos);
    run_list.push_back(run);
  }

  // Signal that the current thread is ready to run
  thread_ready_to_run(mut, cond_var, thread_ready);

  for (int i = 0; i < itr_count; i++) {
    // Start all runs in the queue so that they run in parallel
    for (uint32_t cnt = 0; cnt < queue_len; cnt++) {
      try{
        run_list[cnt].start();
      } catch (const std::exception& ex) {
        logger(ptree, "Error", ex.what());
        ptree.put("status", test_token_failed);
        return;
      }
    }
    // Wait for all runs in the queue to complete
    for (uint32_t cnt = 0; cnt < queue_len; cnt++) {
      try{
        run_list[cnt].wait2();
      } catch (const std::exception& ex) {
        logger(ptree, "Error", ex.what());
        ptree.put("status", test_token_failed);
        return;
      }
    }
  }
}

// Method to signal that a thread is ready to run
void TestCase::thread_ready_to_run(std::mutex& mut, std::condition_variable& cond_var, uint32_t& thread_ready) {
  std::unique_lock<std::mutex> lock(mut);
  thread_ready++;
  cond_var.wait(lock);
  lock.unlock();
}

// Method to run the test
// Parameters:
// - dev: Shared pointer to the device
// Returns:
// - Property tree containing the test results
boost::property_tree::ptree TestSpatialSharingOvd::run(std::shared_ptr<xrt_core::device> dev) {
  // Clear any existing "xclbin" entry in the property tree
  ptree.erase("xclbin");

  // Query the xclbin name from the device
  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::validate);

  // Find the platform file path for the xclbin
  auto xclbin_path = findPlatformFile(xclbin_name, ptree);

  // If the xclbin file does not exist, return the property tree
  if (!std::filesystem::exists(xclbin_path))
    return ptree;

  // Log the xclbin path
  logger(ptree, "Xclbin", xclbin_path);

  // Create an xclbin object
  xrt::xclbin xclbin;
  try {
    // Load the xclbin file
    xclbin = xrt::xclbin(xclbin_path);
  }
  catch (const std::runtime_error& ex) {
    // Log any runtime error and set the status to failed
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }

  // Determine The DPU Kernel Name
  auto xkernels = xclbin.get_kernels();

  // Find the first kernel whose name starts with "DPU"
  auto itr = std::find_if(xkernels.begin(), xkernels.end(), [](xrt::xclbin::kernel& k) {
    auto name = k.get_name();
    return name.rfind("DPU",0) == 0; // Starts with "DPU"
  });

  xrt::xclbin::kernel xkernel;
  if (itr != xkernels.end())
    xkernel = *itr;
  else {
    // Log an error if no kernel with "DPU" is found and set the status to failed
    logger(ptree, "Error", "No kernel with `DPU` found in the xclbin");
    ptree.put("status", test_token_failed);
    return ptree;
  }

  // Get the name of the found kernel
  auto kernelName = xkernel.get_name();

  // If verbose mode is enabled, log the kernel name
  if(XBU::getVerbose())
    logger(ptree, "Details", boost::str(boost::format("Kernel name is '%s'") % kernelName));

  // Create a working device from the provided device
  auto working_dev = xrt::device(dev);
  working_dev.register_xclbin(xclbin);

  std::mutex mut;
  std::condition_variable cond_var;
  uint32_t thread_ready = 0;

  /* Run 1 */
  std::vector<std::thread> threads;
  std::vector<TestCase> testcases;

  // Create two test cases and add them to the vector
  testcases.emplace_back(xclbin, kernelName);
  testcases.emplace_back(xclbin, kernelName);

  // Lambda function to run a test case. This will be sent to individual thread to be run.
  auto runTestcase = [&](TestCase& test) {
    try {
      test.run(mut, cond_var, thread_ready);
    } catch (const std::exception& ex) {
      logger(ptree, "Error", ex.what());
      ptree.put("status", test_token_failed);
    }
  };

  // Create two threads to run the test cases
  threads.emplace_back(runTestcase, std::ref(testcases[0]));
  threads.emplace_back(runTestcase, std::ref(testcases[1]));

  // Wait for both threads to be ready to begin clocking
  wait_for_threads_ready((uint32_t)threads.size(), mut, cond_var, thread_ready);

  // Measure the latency for running the test cases in parallel
  auto start = std::chrono::high_resolution_clock::now(); 
  for (uint32_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
  auto end = std::chrono::high_resolution_clock::now(); 
  float latencyShared = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();
  /* End of Run 1 */

  thread_ready = 0;

  /* Run 2 */
  // Create a single test case and run it in a single thread
  TestCase t(xclbin, kernelName);
  std::thread thr(runTestcase, std::ref(t));

  // Wait for the thread to be ready
  wait_for_threads_ready(1, mut, cond_var, thread_ready);

  // Measure the latency for running the test case in a single thread
  start = std::chrono::high_resolution_clock::now(); 
  thr.join();
  end = std::chrono::high_resolution_clock::now(); 
  float latencySingle =  std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count(); 
  /* End of Run 2 */

  // Log the latencies and the overhead
  if(XBU::getVerbose()){
    logger(ptree, "Details", boost::str(boost::format("LatencySingle: '%.1f' ms") % (latencySingle * 1000)));
    logger(ptree, "Details", boost::str(boost::format("LatencyShared: '%.1f' ms") % (latencyShared * 1000)));
  }
  logger(ptree, "Details", boost::str(boost::format("Overhead: '%.1f' ms") % ((latencyShared - latencySingle) * 1000)));

  // Set the test status to passed
  ptree.put("status", test_token_passed);
  return ptree;
}
