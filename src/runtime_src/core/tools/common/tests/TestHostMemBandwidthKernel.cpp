// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestHostMemBandwidthKernel.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include <math.h>
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"


// ----- C L A S S   M E T H O D S -------------------------------------------
TestHostMemBandwidthKernel::TestHostMemBandwidthKernel()
  : TestRunner("hostmem-bw", 
              "Run 'bandwidth kernel' when host memory is enabled", 
              "bandwidth.xclbin"){}

boost::property_tree::ptree
TestHostMemBandwidthKernel::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();

  // TODO: Fix hostmem-bw test. Test will always be skipped for now.
  ptree.put("status", test_token_skipped);
  return ptree;

  uint64_t shared_host_mem = 0;
  try {
    shared_host_mem = xrt_core::device_query<xrt_core::query::shared_host_mem>(dev);
  } catch (const std::exception& ) {
    logger(ptree, "Details", "Address translator IP is not available");
    ptree.put("status", test_token_skipped);
    return ptree;
  }

  if (!shared_host_mem) {
    logger(ptree, "Details", "Host memory is not enabled");
    ptree.put("status", test_token_skipped);
    return ptree;
  }
  runTest(dev, ptree);
  return ptree;
}

static bool 
is_emulation() {
  bool ret = false;
  char* xcl_mode = getenv("XCL_EMULATION_MODE");
  if (xcl_mode != nullptr) {
    ret = true;
  }
  return ret;
}

void
TestHostMemBandwidthKernel::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  const auto bdf_tuple = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  const std::string bdf = xrt_core::query::pcie_bdf::to_string(bdf_tuple);
  const std::string test_path = findPlatformPath(dev, ptree);
  const std::string b_file = findXclbinPath(dev, ptree); // bandwidth.xclbin
  std::string old_b_file = "/slavebridge.xclbin";
  bool flag_s = false;

  xrt::device device(dev->get_device_id());

  if (test_path.empty()) {
    logger(ptree, "Error", "Platform test path was not found.");
    ptree.put("status", test_token_failed);
    return;
  }

  auto retVal = validate_binary_file(b_file);
  // This is for backward compatibility support when older platforms still having slavebridge.xclbin.
  auto old_binary_file = boost::filesystem::path(test_path) / old_b_file;
  auto check_old_b_file = validate_binary_file(old_binary_file.string());
  if (flag_s || retVal == EOPNOTSUPP) {
    if (check_old_b_file == EOPNOTSUPP) {
      logger(ptree, "Details", "Test is not supported on this device.");
      ptree.put("status", test_token_skipped);
      return;
    }
  } else if (retVal != EXIT_SUCCESS) {
    if (check_old_b_file != EXIT_SUCCESS) {
      logger(ptree, "Error", "Unknown error validating bandwidth xclbin");
      ptree.put("status", test_token_failed);
      return;
    }
  } 

  int num_kernel;
  std::string filename = "/platform.json";
  auto platform_json = boost::filesystem::path(test_path) / filename;

  try {
    boost::property_tree::ptree load_ptree_root;
    boost::property_tree::read_json(platform_json.string(), load_ptree_root);
    auto temp = load_ptree_root.get_child("total_host_banks");
    num_kernel = temp.get_value<int>();
  } catch (const std::exception& e) {
    logger(ptree, "Details", "Bad JSON format detected while marshaling build metadata");
    ptree.put("status", test_token_skipped);
    return;
  }

  std::string krnl_name = "bandwidth";
  xrt::uuid xclbin_uuid;
  if (retVal == EOPNOTSUPP) {
    krnl_name = "slavebridge";
    xclbin_uuid = device.load_xclbin(old_binary_file.string());
  } else {
    xclbin_uuid = device.load_xclbin(b_file);
  }
  std::vector<xrt::kernel> krnls(num_kernel);

  for (int i = 0; i < num_kernel; i++) {
    std::string cu_id = std::to_string(i + 1);
    std::string krnl_name_full;
    if (retVal == EXIT_SUCCESS) {
      krnl_name_full = krnl_name + ":{" + "bandwidth_" + cu_id + "}";
    } else {
      krnl_name_full = krnl_name + ":{" + "slavebridge_" + cu_id + "}";
    }

    // Here Kernel object is created by specifying kernel name along with
    // compute unit.
    // For such case, this kernel object can only access the specific
    // Compute unit
    krnls[i] = xrt::kernel(device, xclbin_uuid, krnl_name_full.c_str());
  }

  double max_throughput = 0;
  int reps = 10000;

  // Starting at 4K and going up to 1M with increments of power of 2
  // The minimum size of host-mem user can reserve is 4M,
  // The sum of the sizes of buffers can't excess the size of host-mem reserved.
  for (uint32_t i = 4 * 1024; i <= 1 * 1024 * 1024; i *= 2) {
    unsigned int data_size = i;

    if (is_emulation()) {
      reps = 2; // reducing the repeat count to 2 for emulation flow
      // Running only upto 8K for emulation flow
      if (data_size > 8 * 1024) {
        break;
      }
    }

    unsigned int vector_size_bytes = data_size;
    std::vector<unsigned char> input_host(data_size);

    // Filling up memory with an incremental byte pattern
    for (uint32_t j = 0; j < data_size; j++) {
        input_host[j] = j % 256;
    }

    std::vector<xrt::bo> input_buffer(num_kernel);
    std::vector<xrt::bo> output_buffer(num_kernel);

    for (int i = 0; i < num_kernel; i++) {
      input_buffer[i] = xrt::bo(device, vector_size_bytes, xrt::bo::flags::host_only, krnls[i].group_id(0));
      output_buffer[i] = xrt::bo(device, vector_size_bytes, xrt::bo::flags::host_only, krnls[i].group_id(1));
    }

    unsigned char* map_input_buffer[num_kernel];
    unsigned char* map_output_buffer[num_kernel];

    for (int i = 0; i < num_kernel; i++) {
      map_input_buffer[i] = input_buffer[i].map<unsigned char*>();
    }

    /* prepare data to be written to the device */
    for (int i = 0; i < num_kernel; i++) {
      for (size_t j = 0; j < vector_size_bytes; j++) {
        map_input_buffer[i][j] = input_host[j];
      }
      input_buffer[i].sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    auto time_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_kernel; i++) {
      auto run = krnls[i](input_buffer[i], output_buffer[i], data_size, reps);
      run.wait();
    }
    auto time_end = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_kernel; i++) {
      output_buffer[i].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    // check
    for (int i = 0; i < num_kernel; i++) {
      for (uint32_t j = 0; j < data_size; j++) {
        if (map_output_buffer[i][j] != map_input_buffer[i][j]) {
          logger(ptree, "Error", boost::str(boost::format("Kernel failed to copy entry %d input %d output %d") % j % map_input_buffer[i][j] % map_output_buffer[i][j]));
          ptree.put("status", test_token_failed);
          return;
        }
      }
    }

    double usduration =
      (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count() / reps);
    double dnsduration = (double)usduration;
    double dsduration = dnsduration / ((double)1000000000); // convert duration from nanoseconds to seconds
    double bpersec = (data_size * num_kernel) / dsduration;
    double mbpersec = (2 * bpersec) / ((double)1024 * 1024); // convert b/sec to mb/sec

    if (mbpersec > max_throughput)
      max_throughput = mbpersec;
  }
  logger(ptree, "Details", boost::str(boost::format("Throughput (Type: HOST) (Bank count: %d) : %f MB/s") % num_kernel % max_throughput));
  ptree.put("status", test_token_passed);
}
