/**
 * Copyright (C) 2020 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#include "xcl2.hpp"
#include <algorithm>
#include <vector>
#define LENGTH 12

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " <Platform Test Area Path>" << std::endl;
    return EXIT_FAILURE;
  }

  std::string path = argv[1];
  std::string b_file = "/verify.xclbin";
  std::string binaryFile = path+b_file;
  cl_int err;
  cl::Context context;
  cl::Kernel krnl_verify;
  cl::CommandQueue q;

  std::vector<char, aligned_allocator<char>> h_buf(LENGTH);

  // Create the test data
  for (int i = 0; i < LENGTH; i++) {
    h_buf[i] = 0;
  }

  // OPENCL HOST CODE AREA START
  // get_xil_devices() is a utility API which will find the xilinx
  // platforms and will return list of devices connected to Xilinx platform
  auto devices = xcl::get_xil_devices();
  std::vector<cl::Platform> platforms;
  OCL_CHECK(err, err = cl::Platform::get(&platforms));
  std::string platform_vers(1024, '\0'), platform_prof(1024, '\0'),
      platform_exts(1024, '\0');
  OCL_CHECK(err,
            err = platforms[0].getInfo(CL_PLATFORM_VERSION, &platform_vers));
  OCL_CHECK(err,
            err = platforms[0].getInfo(CL_PLATFORM_PROFILE, &platform_prof));
  OCL_CHECK(err,
            err = platforms[0].getInfo(CL_PLATFORM_EXTENSIONS, &platform_exts));
  std::cout << "Platform Version: " << platform_vers << std::endl;
  std::cout << "Platform Profile: " << platform_prof << std::endl;
  std::cout << "Platform Extensions: " << platform_exts << std::endl;

  // read_binary_file() is a utility API which will load the binaryFile
  // and will return the pointer to file buffer.
  auto fileBuf = xcl::read_binary_file(binaryFile);
  cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
  int valid_device = 0;
  for (unsigned int i = 0; i < devices.size(); i++) {
    auto device = devices[i];
    // Creating Context and Command Queue for selected Device
    OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
    OCL_CHECK(err, q = cl::CommandQueue(context, device,
                                        CL_QUEUE_PROFILING_ENABLE, &err));
    std::cout << "Trying to program device[" << i
              << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
      std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
    } else {
      std::cout << "Device[" << i << "]: program successful!\n";
      OCL_CHECK(err, krnl_verify = cl::Kernel(program, "verify", &err));
      valid_device++;
      break; // we break because we found a valid device
    }
  }
  if (valid_device == 0) {
    std::cout << "Failed to program any device found, exit!\n";
    exit(EXIT_FAILURE);
  }

  OCL_CHECK(err,
            cl::Buffer d_buf(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                             sizeof(char) * LENGTH, h_buf.data(), &err));

  OCL_CHECK(err, err = krnl_verify.setArg(0, d_buf));

  // Launch the Kernel
  OCL_CHECK(err, err = q.enqueueTask(krnl_verify));

  // Copy Result from Device Global Memory to Host Local Memory
  OCL_CHECK(err, err = q.enqueueMigrateMemObjects({d_buf},
                                                  CL_MIGRATE_MEM_OBJECT_HOST));
  q.finish();
  for (int i = 0; i < LENGTH; i++) {
    std::cout << h_buf[i];
  }
  std::cout << "TEST PASSED\n";
  return 0;
}
