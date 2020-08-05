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

// XRT includes
#include "experimental/xrt_xclbin.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

// This value is shared with worgroup size in kernel.cl
static const int COUNT = 1024;

static const int C_SUCCESS = 0;

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  [-d <index>]       (default: 0)\n";
    std::cout << "  [-v]\n";
    std::cout << "  [-h]\n\n";
    std::cout << "* Bitstream is required\n";
}

static int run(const xrt::device& device, const xrt::uuid& uuid, bool verbose, std::string cuName)
{
  const size_t DATA_SIZE = COUNT * sizeof(int);

  auto simple = xrt::kernel(device, uuid.get(), cuName);
  auto bo0 = xrt::bo(device, DATA_SIZE, XCL_BO_FLAGS_NONE, simple.group_id(0));
  auto bo1 = xrt::bo(device, DATA_SIZE, XCL_BO_FLAGS_NONE, simple.group_id(1));
  auto bo0_map = bo0.map<int*>();
  auto bo1_map = bo1.map<int*>();
  std::fill(bo0_map, bo0_map + COUNT, 0);
  std::fill(bo1_map, bo1_map + COUNT, 0);

  // Fill our data sets with pattern
  int foo = 0x10;
  int bufReference[COUNT];
  for (int i = 0; i < COUNT; ++i) {
    bo0_map[i] = 0;
    bo1_map[i] = i;
    bufReference[i] = i + i * foo;
  }

  bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);
  bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);

  auto run = simple(bo0, bo1, 0x10);
  run.wait();

  //Get the output;
  std::cout << "Get the output data from the device" << std::endl;
  bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);

  // Validate our results
  if (std::memcmp(bo0_map, bufReference, DATA_SIZE))
    throw std::runtime_error("Value read back does not match reference");

  return 0;
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

  if (device_index >= xclProbe())
    throw std::runtime_error("Cannot find device index (" + std::to_string(device_index) + ") specified");


  auto xclbin = xrt::xclbin(xclbin_fnm); // C++ API, construct xclbin object from filename
  
  std::cout << "Targeting xsa platform: " << xclbin.get_xsa_name() << std::endl;

  std::cout << "Xclbin has uuid: " << xclbin.get_uuid() << std::endl;

  std::vector<std::string> cuNames = xclbin.get_cu_names();
 
  for (auto& cuName : cuNames)
    std::cout << cuName << " ";
  std::cout << std::endl;

  if (cuNames[0] != "simple:simple_1")
    throw std::runtime_error("FAILED_TEST\nCould not read correct kernel name, expected: simple:simple_1");

  std::vector<char> data = xclbin.get_data();
  for(int i=0;i<7;++i)
    std::cout << data[i] << " ";
  std::cout << std::endl;

  auto xclbinHandle = xrtXclbinAllocFilename(xclbin_fnm.data()); // C API, construct xclbin object from filename

  if (xclbinHandle == nullptr)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinAllocFilename returned nullptr.");

  int xsaSz;
  if (xrtXclbinGetXSAName(xclbinHandle,NULL,0,&xsaSz) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinGetXSAName returned non-zero error code.");
  
  std::vector<char> xsaName_v(xsaSz);
  if (xrtXclbinGetXSAName(xclbinHandle,xsaName_v.data(),xsaSz,nullptr) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinGetXSAName returned non-zero error code.");
  std::string xsaName(xsaName_v.begin(), xsaName_v.end());
  std::cout << "Targeting xsa platform: " << xsaName << std::endl;

  xuid_t c_uuid;
  if (xrtXclbinGetUUID(xclbinHandle,c_uuid) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinGetUUID returned non-zero error code.");

  char** c_cuNames;
  int numNames;
  if (xrtXclbinGetCUNames(xclbinHandle,NULL,&numNames) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinGetCUNames returned non-zero error code.");

  c_cuNames = (char**) malloc(numNames*sizeof(char*));

  for (int i=0;i<numNames;++i)
    c_cuNames[i] = (char*) malloc(64*sizeof(char)); // 64 is max size of this field
 
  if (xrtXclbinGetCUNames(xclbinHandle,c_cuNames,&numNames) != C_SUCCESS) 
    throw std::runtime_error("FAILED_TEST\nxrtXclbinGetCUNames returned non-zero error code.");

  for (int i=0;i<numNames;++i)
    std::cout << c_cuNames[i] << " ";
  std::cout << std::endl;

  for (int i=0;i<numNames;++i)
    free(c_cuNames[i]); 
  free(c_cuNames);

  int dataSz;
  if (xrtXclbinGetData(xclbinHandle,NULL,0,&dataSz) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinGetData returned non-zero error code.");

  std::vector<char> c_data(dataSz);
  if (xrtXclbinGetData(xclbinHandle,c_data.data(),dataSz,nullptr) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinGetData returned non-zero error code.");

  for(int i=0;i<7;++i)
    std::cout << c_data[i] << " ";
  std::cout << std::endl;
  
  auto deviceHandle = xrtDeviceOpen(device_index);
  if (deviceHandle == nullptr)
    throw std::runtime_error("FAILED_TEST\nxrtDeviceOpen returned nullptr.");

  if (xrtDeviceLoadXclbinHandle(deviceHandle,xclbinHandle) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtDeviceLoadXclbinHandle returned non-zero error code.");

  if (xrtDeviceClose(deviceHandle) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtDeviceClose returned non-zero error code.");

  if (xrtXclbinFreeHandle(xclbinHandle) != C_SUCCESS)
    throw std::runtime_error("FAILED_TEST\nxrtXclbinFreeHandle returned non-zero error code.");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin);


  run(device, uuid, verbose, cuNames[0]);
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
