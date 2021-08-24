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
#include "xcl2.hpp"
#include <algorithm>
#include <vector>
#include "plugin_dec.h"

#define TEST_INSTANCE_ID 0

static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p, --path <path>\n";
    std::cout << "  -d, --device <device> \n";
    std::cout << "  -s, --supported <supported>\n";
    std::cout << "  -h, --help <help>\n";
}

int main(int argc, char** argv) {
  std::string dev_id = "0";
  std::string test_path;
  bool flag_s = false;

  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--path") == 0)) {
        test_path = argv[i + 1];
    } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--device") == 0)) {
        dev_id = argv[i + 1];
    } else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
        printHelp();
        return 1;
    }
  }

  if (test_path.empty()) {
    std::cout << "ERROR : please provide the platform test path to -p option\n";
    return EXIT_FAILURE;
  }

  std::string b_file = "/transcode.xclbin";
  std::string binaryFile = test_path + b_file;
  std::ifstream infile(binaryFile);
  if (flag_s) {    
    if (!infile.good()) {
        std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    } else {
        std::cout << "\nSUPPORTED" << std::endl;
        return EXIT_SUCCESS;
    }
  }

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
