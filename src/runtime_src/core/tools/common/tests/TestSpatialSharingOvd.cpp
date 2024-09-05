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
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace XBU = XBUtilities;

static constexpr size_t host_app = 1; //opcode
static constexpr size_t buffer_size = 1024; // 1KB
static constexpr int itr_count = 1000;
std::mutex m_mutex;
std::condition_variable m_condVar;
uint32_t thread_ready = 0;
bool stop = false;

void thread_ready_to_run() {
  std::unique_lock<std::mutex> lock(m_mutex);
  thread_ready++;
  m_condVar.wait(lock);
  lock.unlock();
}
void wait_for_threads_ready(uint32_t thread_num) {
  std::unique_lock<std::mutex> lock(m_mutex);
  while (thread_ready != thread_num) {
      lock.unlock();
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      lock.lock();
  }
  m_condVar.notify_all();
  lock.unlock();
}

BO_set::BO_set(xrt::device* device_ptr, xrt::kernel* kernel) {
    bo_instr = xrt::bo(*device_ptr, buffer_size, XCL_BO_FLAGS_CACHEABLE, kernel->group_id(5));
    bo_ifm = xrt::bo(*device_ptr, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel->group_id(1));
    bo_param = xrt::bo(*device_ptr, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel->group_id(2));
    bo_ofm = xrt::bo(*device_ptr, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel->group_id(3));
    bo_inter = xrt::bo(*device_ptr, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel->group_id(4));
    bo_mc = xrt::bo(*device_ptr, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel->group_id(7));

    std::memset(bo_instr.map<char*>(), (uint8_t)0, buffer_size);
}

void 
BO_set::sync_bos_to_device() {
    bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_param.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_mc.sync(XCL_BO_SYNC_BO_TO_DEVICE);
}

void 
BO_set::set_kernel_args(xrt::run* run) {
    uint64_t opcode = 1;
    run->set_arg(0, opcode);
    run->set_arg(1, bo_ifm);
    run->set_arg(2, bo_param);
    run->set_arg(3, bo_ofm);
    run->set_arg(4, bo_inter);
    run->set_arg(5, bo_instr);
    run->set_arg(6, buffer_size/sizeof(int));
    run->set_arg(7, bo_mc);
}

TestCase::TestCase(std::string& xclbin_str, std::string& kernel) {
  unsigned int device_index = 0;
  device = xrt::device(device_index);
  xclbin = xrt::xclbin(xclbin_str);
  device.register_xclbin(xclbin);
  kernel_name = kernel;
  hw_ctx = new xrt::hw_context(device, xclbin.get_uuid());
}

void
TestCase::run()
{
  std::vector<xrt::kernel*> kernels;
  std::vector<BO_set*> bo_set_list;
  std::vector<xrt::run*> run_list;
  for (uint32_t j = 0; j < queue_len; j++) {
    auto kernel = new xrt::kernel(*hw_ctx, kernel_name);
    auto bos = new BO_set(&device, kernel);
    bos->sync_bos_to_device();
    auto run = new xrt::run(*kernel);
    bos->set_kernel_args(run);
    run->start();
    run->wait2();

    kernels.push_back(kernel);
    bo_set_list.push_back(bos);
    run_list.push_back(run);
  }
  thread_ready_to_run();

  for (int i = 0; i < itr_count; i++) {
    for (uint32_t cnt = 0; cnt < queue_len; cnt++) {
      try{
        run_list[cnt]->start();
      } catch (const std::exception& ex) {
        std::cout << "ERROR: Caught exception: " << ex.what() << '\n';
      }
    }
    for (uint32_t cnt = 0; cnt < queue_len; cnt++) {
      try{
        run_list[cnt]->wait2();
      } catch (const std::exception& ex) {
        std::cout << "ERROR: Caught exception: " << ex.what() << '\n';
      }
    }
  }
}

TestSpatialSharingOvd::TestSpatialSharingOvd()
  : TestRunner("spatial-sharing-overhead", "Run Spatial Sharing Overhead Test")
{}

void 
runTestcase(TestCase* test)
{
  test->run();
}

boost::property_tree::ptree
TestSpatialSharingOvd::run(std::shared_ptr<xrt_core::device> dev){
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::validate);
  auto xclbin_path = findPlatformFile(xclbin_name, ptree);
  if (!std::filesystem::exists(xclbin_path))
    return ptree;

  logger(ptree, "Xclbin", xclbin_path);

  xrt::xclbin xclbin;
  try {
    xclbin = xrt::xclbin(xclbin_path);
  }
  catch (const std::runtime_error& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }
  // Determine The DPU Kernel Name
  auto xkernels = xclbin.get_kernels();

  auto itr = std::find_if(xkernels.begin(), xkernels.end(), [](xrt::xclbin::kernel& k) {
    auto name = k.get_name();
    return name.rfind("DPU",0) == 0; // Starts with "DPU"
  });

  xrt::xclbin::kernel xkernel;
  if (itr!=xkernels.end())
    xkernel = *itr;
  else {
    logger(ptree, "Error", "No kernel with `DPU` found in the xclbin");
    ptree.put("status", test_token_failed);
    return ptree;
  }
  auto kernelName = xkernel.get_name();
  if(XBU::getVerbose())
    logger(ptree, "Details", boost::str(boost::format("Kernel name is '%s'") % kernelName));

  auto working_dev = xrt::device(dev);
  working_dev.register_xclbin(xclbin);


  /*Run 2*/
  std::vector<std::thread> threads;
  std::vector<TestCase> testcases;

  testcases.emplace_back(xclbin_path, kernelName);
  testcases.emplace_back(xclbin_path, kernelName);
  threads.emplace_back(&runTestcase, &testcases[0]);
  threads.emplace_back(&runTestcase, &testcases[1]);
  wait_for_threads_ready((uint32_t)threads.size());
  auto start = std::chrono::high_resolution_clock::now(); 
  for (uint32_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
  auto end = std::chrono::high_resolution_clock::now(); 
  float latencyShared = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();;
  /* End of Run 2*/

  thread_ready = 0;
  /* Run 1*/
  TestCase t(xclbin_path, kernelName);
  std::thread thr(&runTestcase, &t);
  wait_for_threads_ready(1);
  start = std::chrono::high_resolution_clock::now(); 
  thr.join();
  end = std::chrono::high_resolution_clock::now(); 
  float latencySingle =  std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count(); 
  /*End of run 1*/

  logger(ptree, "Details", boost::str(boost::format("LatencySingle: '%.1f' ms") % (latencySingle * 1000)));
  logger(ptree, "Details", boost::str(boost::format("LatencyShared: '%.1f' ms") % (latencyShared * 1000)));
  logger(ptree, "Details", boost::str(boost::format("Overhead: '%.1f' ms") % ((latencyShared - latencySingle) * 1000)));
  ptree.put("status", test_token_passed);
  return ptree;
}
