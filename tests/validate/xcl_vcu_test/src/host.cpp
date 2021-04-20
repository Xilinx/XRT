/**
* Copyright (C) 2021 Xilinx, Inc
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
#include "cmdlineparser.h"
#include "xcl2.hpp"
#include <algorithm>
#include <vector>
#include "plugin_dec.h"

#define TEST_INSTANCE_ID 1

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <Platform Test Area Path>"
      << "<optional> -d device_id" << std::endl;
    return EXIT_FAILURE;
  }

  // Command Line Parser
  sda::utils::CmdLineParser parser;

  // Switches
  //**************//"<Full Arg>",  "<Short Arg>", "<Description>", "<Default>"
  parser.addSwitch("--device", "-d", "device id", "0");
  parser.parse(argc, argv);

  // Read settings
  std::string dev_id = parser.value("device");

  std::string test_path = argv[1];

  std::string b_file = "/transcode.xclbin";
  std::string binaryFile = test_path + b_file;
  std::ifstream infile(binaryFile);
  if (!infile.good()) {
    std::cout << "NOT SUPPORTED" << std::endl;
    return EOPNOTSUPP;
  }

  auto devices = xcl::get_xil_devices();
  auto pos = dev_id.find(":");
  int device_index = -1;
  if (pos == std::string::npos) {
    device_index = stoi(dev_id);
  } else {
    if (xcl::is_emulation()) {
      std::cout << "Device bdf is not supported for the emulation flow\n";
      return EXIT_FAILURE;
    }

    char device_bdf[20];
    cl_int err;
    for (uint32_t i = 0; i < devices.size(); i++) {
      OCL_CHECK(err, err = devices[i].getInfo(CL_DEVICE_PCIE_BDF, &device_bdf));
      if (dev_id == device_bdf) {
        device_index = i;  
        break;
      }
    }
  }

  if (device_index >= devices.size()) {
    std::cout << "The device_index provided using -d flag is outside the range of "
      "available devices\n";
    return EXIT_FAILURE;
  }

  // Harcoding the number of processes/instances 
  int ret = vcu_dec_test(binaryFile.c_str(), TEST_INSTANCE_ID, device_index);
  if (ret == FALSE) {
    std::cout << "TEST FAILED\n";
    return EXIT_FAILURE;
  }
  else if (ret == NOTSUPP) {
    std::cout << "NOT SUPPORTED\n" << std::endl;
    return EOPNOTSUPP;
  }

  std::cout << "TEST PASSED\n";
  return 0;
}
