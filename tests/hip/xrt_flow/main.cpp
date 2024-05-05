// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// Based on https://gitenterprise.xilinx.com/XRT/testcases/blob/master/xrt_flow/src/host_hal.cpp

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "buffer_ops.h"
#include "config.h"

#include "common.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

// xrt flow test which runs on RyzenAI using HIP apis
// By default this test uses hipMalloc api for creating buffers but can be changed
// to create buffers using hipHostMalloc by using config -b/--buffer host
// Usage : ./xrt_flow -d 0 -x 1x4.xclbin -c DPU_PDI_0:{IPUV1CNN} -w C:/workspace_data/ 

namespace {
std::string xclbin_path;
std::string workspace_path;
std::string cu_name;
unsigned int device_id = 0;
std::string buffer_type = "device";
unsigned int failed = 0;
bool dump_output = false;
bool dump_output_diff_only = false;

struct Workspace
{
  std::string instr_path;
  std::string ifm_path;
  std::string param_path;
  std::string ofm_format_path;
  std::string ofm_gold_path;
  std::string ofm_dump_path;
  std::string config_path;
  std::string mc_blob_path;
} workspace;

static void
usage()
{
  std::cout << "usage: <exe> [options] \n\n";
  std::cout << "  -d <device index>\n";
  std::cout << "  -x <xclbin>\n";
  std::cout << "  -c <name of compute unit in xclbin>\n";
  std::cout << "  -w <workspace path>\n";
  std::cout << "  -b <buffer type 'device/host'>";
  std::cout << "  -h <print this usage>\n\n";
  std::cout << "xclbin is required\n";
  std::cout << "Name of compute unit from loaded xclbin is required\n";
  std::cout << "workspace space path is required\n";
  std::cout << "By default buffer type is device which uses hipMalloc\n";
  std::cout << "If buffer type is host then hipHostMalloc is used\n";
}

static void
check_file_open(const std::ifstream& ifs, const std::string& filename)
{
  if (!ifs.is_open())
    throw std::runtime_error("unable to open file " + filename + "\n");
}

static void
run_kernel(hipFunction_t function, hipStream_t stream, std::array<void *, 8> &args, void* ofm_ptr)
{
  xrt_hip_test_common::test_hip_check(hipModuleLaunchKernel(function,
                                         1, 1, 1,
                                         1, 1, 1,
                                         0, stream, args.data(), nullptr), cu_name.c_str());

  xrt_hip_test_common::test_hip_check(hipStreamSynchronize(stream));

  // Validate the data after the test run
  // Extract all the info we need from ofm_format.txt
  unsigned num_outputs = 0;
  std::vector<std::string> golden_out_files;
  std::vector<std::string> dump_out_files;
  std::vector<unsigned> output_ddr_addrs;
  std::vector<std::vector<unsigned>> output_shapes;
  std::vector<std::vector<unsigned>> output_strides;

  std::ifstream ofm_instream(workspace_path + "/ofm_format.txt");
        
  if (!ofm_instream) throw std::runtime_error("Can't open ofm_format.txt");

  std::string line;
  std::string field;

  // Get number of outputs
  getline(ofm_instream, line);
  std::stringstream ss(line);
  ss >> line >> num_outputs;

  // For every output
  for (unsigned i = 0; i < num_outputs; i++) {
    // Get output name
    {
      getline(ofm_instream, line);
      std::stringstream ss(line);
      ss >> field;
      ss >> field;
      golden_out_files.push_back(workspace_path + "/golden_" + field + ".bin");
      dump_out_files.push_back("dump_" + field + ".txt");
    }

    // Get output addr
    {
      unsigned ddr_addr;
      getline(ofm_instream, line);
      std::stringstream ss(line);
      ss >> line >> ddr_addr;
      output_ddr_addrs.push_back(ddr_addr);
    }

    // Get shape, assume its always 4D
    {
      unsigned shape;
      std::vector<unsigned> shapes;
      for (unsigned j = 0; j < 4; j++) {
        getline(ofm_instream, line);
        std::stringstream ss(line);
        ss >> line >> shape;
        shapes.push_back(shape);
      }
      output_shapes.push_back(shapes);
    }

    // Get strides, assume its always 4D
    {
      unsigned stride;
      std::vector<unsigned> strides;
      for (unsigned j = 0; j < 4; j++) {
        getline(ofm_instream, line);
        std::stringstream ss(line);
        ss >> line >> stride;
        strides.push_back(stride);
      }
      output_strides.push_back(strides);
    }
  }
  // Done Extracting all the info we need from ofm_format.txt

  auto *ptr = reinterpret_cast<int8_t*>(ofm_ptr);

  int total_mismatches = 0;
  for (unsigned int i = 0; i < num_outputs; i++) {
    std::cout << "Examining output: " << golden_out_files[i];
    int num_mismatches = comp_buf_strides(ptr + output_ddr_addrs[i],
                                          golden_out_files[i],
                                          dump_out_files[i],
                                          output_shapes[i],
                                          output_strides[i],
                                          dump_output,
                                          dump_output_diff_only);
    std::cout << ", num_mismatches: " << num_mismatches << std::endl;
    total_mismatches += num_mismatches;
  }

  if (total_mismatches != 0) {
    failed = 1;
  }

  if (!failed) {
    std::cout << "TEST PASSED!" << std::endl;
    print_dolphin();
  }
  else {
    std::cout << "TEST FAILED!" << std::endl;
  }
}

static void
run_malloc_test(hipFunction_t function, hipStream_t stream, size_t instr_size)
{
  // Create BOs
  xrt_hip_test_common::hip_test_host_bo<int> bo_instr(instr_size, hipHostMallocWriteCombined);
  xrt_hip_test_common::hip_test_device_bo<int> bo_ifm(IFM_SIZE / sizeof(int));
  xrt_hip_test_common::hip_test_device_bo<int> bo_param(PARAM_SIZE / sizeof(int));
  xrt_hip_test_common::hip_test_device_bo<int> bo_ofm(OFM_SIZE / sizeof(int));
  xrt_hip_test_common::hip_test_device_bo<int> bo_inter(INTER_SIZE / sizeof(int));
  xrt_hip_test_common::hip_test_device_bo<int> bo_mc(std::max(MC_CODE_SIZE, DUMMY_MC_CODE_BUFFER_SIZE) / sizeof(int));

  init_hex_buf(bo_instr.get(), instr_size, workspace.instr_path);

  std::vector<int> map_ifm(IFM_SIZE / 4, 0);
  init_buf_bin_offset(map_ifm.data(), IFM_SIZE - IFM_DIRTY_BYTES, IFM_DIRTY_BYTES, workspace.ifm_path);
  xrt_hip_test_common::test_hip_check(hipMemcpy(bo_ifm.get(), map_ifm.data(), IFM_SIZE, hipMemcpyHostToDevice));

  std::vector<int> map_param(PARAM_SIZE / 4, 0);
  init_buf_bin(map_param.data(), PARAM_SIZE, workspace.param_path);
  xrt_hip_test_common::test_hip_check(hipMemcpy(bo_param.get(), map_param.data(), PARAM_SIZE, hipMemcpyHostToDevice));

  std::vector<int> map_mc(MC_CODE_SIZE / 4, 0);

  if (MC_CODE_SIZE) {
    init_buf_bin(map_mc.data(), MC_CODE_SIZE, workspace.mc_blob_path);
    xrt_hip_test_common::test_hip_check(hipMemcpy(bo_mc.get(), map_mc.data(), MC_CODE_SIZE, hipMemcpyHostToDevice));

    patchMcCodeDDR(reinterpret_cast<uint64_t>(bo_ifm.get()) + DDR_AIE_ADDR_OFFSET,
                   reinterpret_cast<uint64_t>(bo_param.get()) + DDR_AIE_ADDR_OFFSET,
                   reinterpret_cast<uint64_t>(bo_ofm.get()) + DDR_AIE_ADDR_OFFSET,
                   reinterpret_cast<uint64_t>(bo_inter.get()) + DDR_AIE_ADDR_OFFSET,
                   static_cast<uint32_t*>(bo_mc.get()),
                   MC_CODE_SIZE,
                   PAD_CONTROL_PACKET);
  }

  // Set kernel argument and trigger it to run
  uint64_t opcode = 1;

  std::array<void*, 8> args = {&opcode, bo_ifm.get(), bo_param.get(), bo_ofm.get(), bo_inter.get(), bo_instr.get(), &instr_size, bo_mc.get()};
  run_kernel(function, stream, args, bo_ofm.get());
}

static void
run_host_malloc_test(hipFunction_t function, hipStream_t stream, size_t instr_size)
{
  // Create BOs
  xrt_hip_test_common::hip_test_host_bo<int> bo_instr(instr_size, hipHostMallocWriteCombined);

  xrt_hip_test_common::hip_test_host_bo<int> bo_ifm(IFM_SIZE / sizeof(int), hipHostMallocMapped);
  xrt_hip_test_common::hip_test_host_bo<int> bo_param(PARAM_SIZE / sizeof(int), hipHostMallocMapped);
  xrt_hip_test_common::hip_test_host_bo<int> bo_ofm(OFM_SIZE / sizeof(int), hipHostMallocMapped);
  xrt_hip_test_common::hip_test_host_bo<int> bo_inter(INTER_SIZE / sizeof(int), hipHostMallocMapped);
  xrt_hip_test_common::hip_test_host_bo<int> bo_mc(std::max(MC_CODE_SIZE, DUMMY_MC_CODE_BUFFER_SIZE) / sizeof(int), hipHostMallocMapped);

  // Get device ptrs for all host BO's  
  void* d_bo_ifm;
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&d_bo_ifm, bo_ifm.get(), 0));

  void* d_bo_param;
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&d_bo_param, bo_param.get(), 0));
  
  void* d_bo_ofm;
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&d_bo_ofm, bo_ofm.get(), 0));

  void* d_bo_inter;
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&d_bo_inter, bo_inter.get(), 0));

  void* d_bo_mc;
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&d_bo_mc, bo_mc.get(), 0));

  init_hex_buf(bo_instr.get(), instr_size, workspace.instr_path);

  init_buf_bin_offset(bo_ifm.get(), IFM_SIZE - IFM_DIRTY_BYTES, IFM_DIRTY_BYTES, workspace.ifm_path);

  init_buf_bin(bo_param.get(), PARAM_SIZE, workspace.param_path);

  if (MC_CODE_SIZE) {
    init_buf_bin(bo_mc.get(), MC_CODE_SIZE, workspace.mc_blob_path);

    patchMcCodeDDR(reinterpret_cast<uint64_t>(d_bo_ifm) + DDR_AIE_ADDR_OFFSET,
                   reinterpret_cast<uint64_t>(d_bo_param) + DDR_AIE_ADDR_OFFSET,
                   reinterpret_cast<uint64_t>(d_bo_ofm) + DDR_AIE_ADDR_OFFSET,
                   reinterpret_cast<uint64_t>(d_bo_inter) + DDR_AIE_ADDR_OFFSET,
                   static_cast<uint32_t*>(d_bo_mc),
                   MC_CODE_SIZE,
                   PAD_CONTROL_PACKET);
  }

  // Set kernel argument and trigger it to run
  uint64_t opcode = 1;
  std::array<void*, 8> args = {&opcode, d_bo_ifm, d_bo_param, d_bo_ofm, d_bo_inter, bo_instr.get(), &instr_size, d_bo_mc};

  run_kernel(function, stream, args, bo_ofm.get());
}

static void
mainworker()
{
  std::cout << "---------------------------------------------------------------------------------\n";
  xrt_hip_test_common::hip_test_device hdevice;
  // hdevice.show_info(std::cout); // Enable after all device apis are implemented

  // Load Module (xclbin)
  hipFunction_t function = hdevice.get_function(xclbin_path.c_str(), cu_name.c_str());

  hipStream_t stream = nullptr;
  xrt_hip_test_common::test_hip_check(hipStreamCreateWithFlags(&stream, hipStreamNonBlocking));

  size_t instr_word_size = get_instr_size(workspace.instr_path);
  if (instr_word_size == 0)
    throw std::runtime_error("Instruction size is zero");
  
  // Init BOs (Testcase specific per config.h)
  // Load them with data from files
  std::cout << "-------------REGULAR TEST-------------" << std::endl;
  std::cout << "IFM_SIZE: " << IFM_SIZE << std::endl;
  std::cout << "IFM_DIRTY_BYTES: " << IFM_DIRTY_BYTES << std::endl;
  std::cout << "PARAM_SIZE: " << PARAM_SIZE << std::endl;
  std::cout << "OFM_SIZE: " << OFM_SIZE << std::endl;
  std::cout << "INTER_SIZE: " << INTER_SIZE << std::endl;
  std::cout << "MC_CODE_SIZE: " << MC_CODE_SIZE << ", PAD_CONTROL_PACKET: " << PAD_CONTROL_PACKET << std::endl;
  std::cout << "instr_size_bytes: " << instr_word_size * sizeof(int) << std::endl;
  
  if (buffer_type == "device")
    run_malloc_test(function, stream, instr_word_size);
  else
    run_host_malloc_test(function, stream, instr_word_size);
}
}

int main(int argc, char** argv)
{
  if (argc < 7) {
    usage();
    return 1;
  }

  std::vector<std::string> args(argv + 1, argv + argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-x")
      xclbin_path = arg;
    else if (cur == "-d")
      device_id = std::stoi(arg);
    else if (cur == "-c")
      cu_name = arg;
    else if (cur == "-w")
      workspace_path = arg;
    else if (cur == "-b")
      buffer_type = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_path.empty() || workspace_path.empty() || cu_name.empty()) {
    usage();
    throw std::runtime_error("FAILED_TEST\nAll required parameters not passed\n");
  }
  if (buffer_type != "device" && buffer_type != "host") {
    usage();
    throw std::runtime_error("Invalid buffer type passed, use device/host\n");
  }

  workspace =
  {
    workspace_path + "/mc_code.txt",     // instr_path
    workspace_path + "/ifm.bin",          // ifm_path
    workspace_path + "/param.bin",        // param_path
    workspace_path + "/ofm_format.txt",   // ofm_format_path
    get_ofm_gold(workspace_path),        // ofm_gold_path
    workspace_path + "/ofm_ddr_dump.txt", // ofm_dump_path
    workspace_path + "/ddr_range.txt",    // config_path
    workspace_path + "/mc_code_ddr.bin",  // mc_blob_path
  };

  InitBufferSizes(workspace.config_path);

  try {
    mainworker();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
