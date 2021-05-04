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

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

// XRT includes
#include "experimental/xrt_xclbin.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

// This value is shared with worgroup size in kernel.cl
constexpr auto COUNT = 1024;

static void
usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  [-h]\n\n";
    std::cout << "* Bitstream is required\n";
}

std::ostream&
operator << (std::ostream& ostr, const xrt::xclbin::arg& arg)
{
  ostr << "argument:       " << arg.get_name() << "\n";
  ostr << "hosttype:       " << arg.get_host_type() << "\n";
  ostr << "port:           " << arg.get_port() << "\n";
  ostr << "size (bytes):   0x" << std::hex << arg.get_size() << std::dec << "\n";
  ostr << "offset:         0x" << std::hex << arg.get_offset() << std::dec << "\n";
  for (const auto& mem : arg.get_mems()) {
    ostr << "mem tag:        " << mem.get_tag() << "\n";
    ostr << "mem index:      " << mem.get_index() << "\n";
    ostr << "mem size (kb):  0x" << std::hex << mem.get_size_kb() << std::dec << "\n";
    ostr << "mem base addr:  0x" << std::hex << mem.get_base_address() << std::dec << "\n";
  }
  return ostr;
}

std::ostream&
operator << (std::ostream& ostr, const xrt::xclbin::ip& cu)
{
  ostr << "instance name:    " << cu.get_name() << "\n";
  ostr << "base address:     0x" << std::hex << cu.get_base_address() << std::dec << "\n";

  // ip arguments
  for (const auto& arg : cu.get_args())
    ostr << arg << '\n';

  return ostr;
}

std::ostream&
operator << (std::ostream& ostr, const xrt::xclbin::kernel& kernel)
{
  // kernel function
  ostr << kernel.get_name() << "(\n";
  size_t argidx = 0;
  for (const auto& arg : kernel.get_args()) {
    if (argidx++)
      ostr << ",\n";
    ostr << arg.get_host_type() << " " << arg.get_name();
  }
  ostr << "\n)\n\n";

  // kernel compute units
  for (const auto& cu : kernel.get_cus())
    ostr << cu << '\n';

  return ostr;
}

void
run(const std::string& xclbin_fnm)
{
  // Construct xclbin from fnm
  std::cout << "================================================================\n";
  auto xclbin = xrt::xclbin(xclbin_fnm);
  auto uuid = xclbin.get_uuid();
  std::cout << xclbin_fnm << "\n";
  std::cout << "xsa(" << xclbin.get_xsa_name() << ")\n";
  std::cout << "uuid(" << uuid.to_string() << ")\n\n";

  for (auto& kernel : xclbin.get_kernels())
    std::cout << kernel << '\n';
}

int 
run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  unsigned int device_index = 0;

  std::vector<std::string> args(argv+1,argv+argc);
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

    if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "-d")
      device_index = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }


  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  run(xclbin_fnm);

  return 0;
}

int main(int argc, char** argv)
{
  try {
    if (!run(argc, argv))
      std::cout << "PASSED TEST\n";
    return 0;
  }
  catch (const std::system_error& ex) {
    std::cout << "TEST FAILED: " << ex.what() << '\n';
    return ex.code().value();
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << '\n';
    return EXIT_FAILURE;
  }
  catch (...) {
    std::cout << "TEST FAILED for unknown reason\n";
    return EXIT_FAILURE; 
  }
}
