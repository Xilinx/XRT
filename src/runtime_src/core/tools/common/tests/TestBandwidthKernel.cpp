// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestBandwidthKernel.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <math.h>
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#ifdef _WIN32
#pragma warning(disable : 4996) //std::getenv
#endif

static const int reps = (std::getenv("XCL_EMULATION_MODE") != nullptr) ? 2 : 10000;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestBandwidthKernel::TestBandwidthKernel()
  : TestRunner("mem-bw", 
              "Run 'bandwidth kernel' and check the throughput", 
              "bandwidth.xclbin"){}

boost::property_tree::ptree
TestBandwidthKernel::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string name;
  try {
    name = xrt_core::device_query<xrt_core::query::rom_vbnv>(dev);
  } catch (const std::exception&) {
    XBValidateUtils::logger(ptree, "Error", "Unable to find device VBNV");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return ptree;
  }
  runTest(dev, ptree);
  return ptree;
}

static void
marshal_build_metadata(std::string test_path, unsigned int* num_kernel, unsigned int* num_kernel_ddr, bool* chk_hbm_mem, std::vector<std::string>& bank_names)
{
  static const std::string filename = "platform.json";
  auto platform_json = std::filesystem::path(test_path) / filename;

  boost::property_tree::ptree load_ptree_root;
  boost::property_tree::read_json(platform_json.string(), load_ptree_root);

  auto temp = load_ptree_root.get_child("total_ddr_banks");
  *num_kernel = temp.get_value<int>();
  *num_kernel_ddr = *num_kernel;
  auto pt_mem_array = load_ptree_root.get_child("meminfo");
  for (const auto& mem_entry : pt_mem_array) {
    boost::property_tree::ptree pt_mem_entry = mem_entry.second;
    auto sValue = pt_mem_entry.get<std::string>("type");
    if (sValue == "HBM")
      *chk_hbm_mem = true;

    else if (sValue == "DDR" || sValue == "LPDDR4_SDRAM") {
      auto banks = pt_mem_entry.get_child("banks");
      for (const auto&bank : banks) {
        auto bank_name = bank.second.get<std::string>("name");
        bank_names.push_back(bank_name);
      }
    }
  }

  if (*chk_hbm_mem) {
    // As HBM is part of platform, number of ddr kernels is total count reduced by 1(single HBM)
    *num_kernel_ddr = *num_kernel - 1;
  }
}

static std::vector<xrt::kernel>
create_kernel_objects(xrt::device device, xrt::uuid xclbin_uuid, int num_kernel)
{
  std::string krnl_name = "bandwidth";
  std::vector<xrt::kernel> krnls(num_kernel);
  for (int i = 0; i < num_kernel; i++) {
    std::string cu_id = std::to_string(i + 1);
    std::string krnl_name_full = krnl_name + ":{" + "bandwidth_" + cu_id + "}";

    // Here Kernel object is created by specifying kernel name along with
    // compute unit.
    // For such case, this kernel object can only access the specific
    // Compute unit
    krnls[i] = xrt::kernel(device, xclbin_uuid, krnl_name_full.c_str());
  }
  return krnls;
}

static std::vector<unsigned char>
initialize_input_host(unsigned int data_size)
{
  std::vector<unsigned char> input_host(data_size);
  // Filling up memory with an incremental byte pattern
  for (uint32_t j = 0; j < data_size; j++)
    input_host[j] = static_cast<unsigned char>(j % 256);
  return input_host;
}

std::vector<std::vector<unsigned char>>
initialize_output_host_ddr(unsigned int data_size, int num_kernel_ddr)
{
  std::vector<std::vector<unsigned char>> output_host(num_kernel_ddr);
  for (int i = 0; i < num_kernel_ddr; i++)
    output_host[i].resize(data_size);

  for (int i = 0; i < num_kernel_ddr; i++)
    std::fill(output_host[i].begin(), output_host[i].end(), static_cast<unsigned char>(0));

  return output_host;
}


static std::vector<unsigned char>
initialize_output_host_hbm(unsigned int data_size)
{
  std::vector<unsigned char> output_host(data_size);
  std::fill(output_host.begin(), output_host.end(), static_cast<unsigned char>(0));
  return output_host;
}

static double
calculate_throughput(std::chrono::time_point<std::chrono::high_resolution_clock> time_end,
                          std::chrono::time_point<std::chrono::high_resolution_clock> time_start,
                          unsigned int data_size, int num_bank)
{
  double usduration =
    (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count() / reps);

  double dnsduration = (double)usduration;
  double dsduration = dnsduration / ((double)1000000000); // Convert duration from nanoseconds to seconds
  double bpersec = (data_size * num_bank) / dsduration;
  double mbpersec = (2 * bpersec) / ((double)1024 * 1024); // Convert b/sec to mb/sec


  return mbpersec;
}

static std::pair<double, std::vector<double>>
test_bandwidth_ddr(xrt::device device, std::vector<xrt::kernel> krnls, int num_kernel_ddr)
{
  double max_throughput = 0;
  double mbpersec = 0;
  std::vector<double> throughput_per_kernel(num_kernel_ddr, 0);
  // Starting at 4K and going up to 16M with increments of power of 2
  for (uint32_t a = 4 * 1024; a <= 16 * 1024 * 1024; a *= 2) {
    unsigned int data_size = a;

    if ((std::getenv("XCL_EMULATION_MODE") != nullptr) && (data_size > 8 * 1024))
      break; // Running only up to 8K for emulation flow

    unsigned int vector_size_bytes = data_size;
    std::vector<unsigned char> input_host = initialize_input_host(data_size);
    std::vector<std::vector<unsigned char>> output_host = initialize_output_host_ddr(data_size, num_kernel_ddr);

    std::vector<xrt::bo> input_buffer(num_kernel_ddr);
    std::vector<xrt::bo> output_buffer(num_kernel_ddr);

    // Creating Buffers
    for (int i = 0; i < num_kernel_ddr; i++) {
      input_buffer[i] = xrt::bo(device, vector_size_bytes, krnls[i].group_id(0));
      output_buffer[i] = xrt::bo(device, vector_size_bytes, krnls[i].group_id(1));
    }

    for (int i = 0; i < num_kernel_ddr; i++) {
      input_buffer[i].write(input_host.data());
      input_buffer[i].sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    auto time_start = std::chrono::high_resolution_clock::now();
    std::vector<xrt::run> runs(num_kernel_ddr);
    std::vector<std::chrono::high_resolution_clock::time_point> start_time_per_kernel(num_kernel_ddr);
    std::vector<std::chrono::high_resolution_clock::time_point> end_time_per_kernel(num_kernel_ddr);

    for (int i = 0; i < num_kernel_ddr; i++) {
      start_time_per_kernel[i] = std::chrono::high_resolution_clock::now();
      runs[i] = krnls[i](input_buffer[i], output_buffer[i], data_size, reps);
    }
    for (int i = 0; i < num_kernel_ddr; i++) {
      runs[i].wait();
      end_time_per_kernel[i] = std::chrono::high_resolution_clock::now();
    }
    auto time_end = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_kernel_ddr; i++) {
        throughput_per_kernel[i] = std::max(throughput_per_kernel[i], calculate_throughput(end_time_per_kernel[i], start_time_per_kernel[i], data_size, 1));
  }
    for (int i = 0; i < num_kernel_ddr; i++) {
      output_buffer[i].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
      output_buffer[i].read(output_host[i].data());
    }

    // Check that each bank copied input correctly.
    for (int i = 0; i < num_kernel_ddr; i++) {
      for (uint32_t j = 0; j < data_size; j++) {
        if (output_host[i][j] != input_host[j]) {
          throw std::runtime_error(boost::str(boost::format("Kernel failed to copy entry %d input %d output %d") % j % (uint32_t)input_host[j] % (uint32_t)output_host[i][j]));
        }
      }
    }

     mbpersec = calculate_throughput(time_end, time_start, data_size, num_kernel_ddr);
     max_throughput = std::max(mbpersec, max_throughput);
  }
  return std::make_pair(max_throughput, throughput_per_kernel);
}

static double
test_bandwidth_hbm(xrt::device device, std::vector<xrt::kernel> krnls, int num_kernel)
{
  double max_throughput = 0;
  double mbpersec = 0;
  // Starting at 4K and going up to 16M with increments of power of 2
  for (uint32_t i = 4 * 1024; i <= 16 * 1024 * 1024; i *= 2) {
    unsigned int data_size = i;

    if ((std::getenv("XCL_EMULATION_MODE") != nullptr) && (data_size > 8 * 1024))
      break; // Running only up to 8K for emulation flow

    unsigned int vector_size_bytes = data_size;
    std::vector<unsigned char> input_host = initialize_input_host(data_size);
    std::vector<unsigned char> output_host = initialize_output_host_hbm(data_size);

    xrt::bo input_buffer, output_buffer;

    // Creating Buffers
    input_buffer = xrt::bo(device, vector_size_bytes, krnls[num_kernel - 1].group_id(0));
    output_buffer = xrt::bo(device, vector_size_bytes, krnls[num_kernel - 1].group_id(1));

    input_buffer.write(input_host.data());
    input_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    auto time_start = std::chrono::high_resolution_clock::now();
    auto run = krnls[num_kernel - 1](input_buffer, output_buffer, data_size, reps);
    run.wait();
    auto time_end = std::chrono::high_resolution_clock::now();

    output_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    output_buffer.read(output_host.data());

    // Check that input and output matches.
    for (uint32_t j = 0; j < data_size; j++) {
      if (output_host[j] != input_host[j]) {
        throw std::runtime_error(boost::str(boost::format("Kernel failed to copy entry %d input %d output %d") % j % (uint32_t)input_host[j] % (uint32_t)output_host[j]));
      }
    }

    mbpersec = calculate_throughput(time_end, time_start, data_size, 1);
    max_throughput = std::max(mbpersec, max_throughput);
  }
  return max_throughput;
}

void
TestBandwidthKernel::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  xrt::device device(dev);

  const std::string test_path = XBValidateUtils::findPlatformPath(dev, ptree);
  if (test_path.empty()) {
    XBValidateUtils::logger(ptree, "Error", "Platform test path was not found.");
    ptree.put("status", XBValidateUtils::test_token_failed);
    return;
  }
  auto json_exists = [test_path]() {
    const static std::string platform_metadata = "/platform.json";
    std::string platform_json_path(test_path + platform_metadata);
    return std::filesystem::exists(platform_json_path) ? true : false;
  };
  if (!json_exists()) {
    // Without a platform.json, we need to run the python test file.
    runPyTestCase(dev, "23_bandwidth.py", ptree);
    return;
  }

  unsigned int num_kernel = 0;
  unsigned int num_kernel_ddr = 0;
  bool chk_hbm_mem = false;
  std::vector<std::string> bank_names;
  try {
    marshal_build_metadata(test_path, &num_kernel, &num_kernel_ddr, &chk_hbm_mem, bank_names);
  } catch (const std::exception&) {
    XBValidateUtils::logger(ptree, "Error", "Bad JSON format detected while marshaling build metadata");
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return;
  }

  const std::string b_file = XBValidateUtils::findXclbinPath(dev, ptree);
  std::ifstream infile(b_file);
  if (!infile.good()) {
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return;
  }
  auto xclbin_uuid = device.load_xclbin(b_file);

  std::vector<xrt::kernel> krnls = create_kernel_objects(device, xclbin_uuid, num_kernel);

  try {
    if (num_kernel_ddr) {
      auto  throughputs = test_bandwidth_ddr(device, krnls, num_kernel_ddr);
      double max_throughput = throughputs.first;
      std::vector <double> throughput_per_kernel = throughputs.second;
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Throughput (Type: DDR) (Bank count: %d) : %.1f MB/s") % num_kernel_ddr % max_throughput));
      if (bank_names.size() == num_kernel_ddr && throughput_per_kernel.size() == num_kernel_ddr) {
        for (unsigned int i = 0; i < num_kernel_ddr; i++)
          XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Throughput of Memory Tag: %s : %.1f MB/s") % bank_names[i] % throughput_per_kernel[i]));
      }
    }
    if (chk_hbm_mem) {
      double max_throughput = test_bandwidth_hbm(device, krnls, num_kernel);
      XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Throughput (Type: HBM) (Bank count: 1) : %.1f MB/s") % max_throughput));
    }
  } catch (const std::runtime_error& e) {
    XBValidateUtils::logger(ptree, "Error", e.what());
    ptree.put("status", XBValidateUtils::test_token_failed);
    return;
  }
  ptree.put("status", XBValidateUtils::test_token_passed);
}
