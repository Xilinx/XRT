/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <ctime>
#include <chrono>

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

#ifdef _WIN32
# pragma warning ( disable : 4244 )
#endif

static const int size = 256;
int data_size = size*size;

/**
 * Runs an OpenCL kernel which writes known 16 integers into a 64 byte
 * buffer. Does not use OpenCL runtime but directly exercises the HAL
 * driver API.
 */

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -s <hal_driver>\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -r Random input data.\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

static void
run(xrt::device& device, const xrt::uuid& uuid, bool random, bool verbose)
{
  auto mmult = xrt::kernel(device, uuid.get(), "mmult");

  auto a = xrt::bo(device, 2*data_size*sizeof(float), 0, mmult.group_id(0));
  auto output = xrt::bo(device, data_size*sizeof(float), 0, mmult.group_id(1));

  auto a_mapped = a.map<float*>();

  std::cout << "Populate the input and reference vectors.\n";

  const int MY_MAX = 4096;
  float A[size][size], B[size][size], C[size*size];
  std::srand(std::time(0));
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      A[i][j] = random ?  static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX/MY_MAX)):(float)(i+j);
      B[i][j] = random ?  static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX/MY_MAX)):(float)(i+j);
    }
  }

  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      C[i*size+j] = 0.0;
      for (int k = 0; k < size; ++k) {
        C[i*size+j] += A[i][k] * B[k][j];
      }
    }
  }

  std::memcpy(a_mapped, A, data_size*sizeof(float));
  std::memcpy(a_mapped + data_size, B, data_size*sizeof(float));

  // Send the input data to the device memory
  std::cout << "Send the input data to the device memory.\n";
  a.sync(XCL_BO_SYNC_BO_TO_DEVICE , 2*data_size*sizeof(float), 0);

  // Run kernel
  auto run = mmult(a, output, 1);
  run.wait();

  //Get the output;
  std::cout << "Get the output data from the device" << std::endl;
  output.sync(XCL_BO_SYNC_BO_FROM_DEVICE, data_size*sizeof(float), 0);
  auto output_mapped = output.map<float*>();

  // Validate FPGA results
  int err = 0;
  for (int i = 0; i < size * size; i++) {
    bool bShow = verbose;
    if (C[i] != output_mapped[i]) {
      bShow = true; // always show errors
      err++;
    }
    if (bShow)
      std::cout<< std::hex << i << " : " << std::fixed << C[i] << " vs " << output_mapped[i] << "\n";
  }

  if (err)
    throw std::runtime_error("mismatch count = " + std::to_string(err));
}


int
run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  bool verbose = false;
  unsigned int device_index = 0;
  bool random = false;

  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }
    else if (arg == "-v") {
      verbose = true;
      continue;
    }
    else if (cur == "-r") {
      random = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "-d")
      device_index = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);

  run(device, uuid, random, verbose);

  return 0;
}

int main(int argc, char** argv)
{
  try {
    auto ret = run(argc, argv);
    std::cout << "PASSED TEST\n";
    return ret;
  }
  catch (std::exception const& e) {
    std::cout << "Exception: " << e.what() << "\n";
    std::cout << "FAILED TEST\n";
    return 1;
  }

  std::cout << "PASSED TEST\n";
  return 0;
}
