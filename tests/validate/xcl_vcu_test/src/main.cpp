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
#include <iostream>
#include <vector>
#include "plugin_dec.h"

static void usage(char *prog)
{
  std::cout << "Usage: " << prog << " -k <xclbin> -d <dev id> [options]\n"
    << "options:\n"
    << "    -t       number of processes\n"
    << std::endl;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  std::string xclbin_fn;
  int dev_id = 0;
  int p_id = 1;

  std::vector<std::string> args(argv + 1, argv + argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage(argv[0]);
      return EXIT_SUCCESS;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-k")
      xclbin_fn = arg;
    else if (cur == "-d")
      dev_id = std::stoi(arg);
    else if (cur == "-t")
      p_id = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  /* Sanity check */
  if (dev_id < 0)
    throw std::runtime_error("Negative device ID");

  if (p_id <= 0)
    throw std::runtime_error("Invalid process number");


  int ret = vcu_dec_test(xclbin_fn.c_str(), p_id, dev_id);
  if (ret != TRUE) {
    std::cout << "TEST FAILED: " << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "TEST PASSED: " << std::endl;

  return EXIT_SUCCESS;
}
