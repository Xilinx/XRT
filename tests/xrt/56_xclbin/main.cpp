// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

// XRT includes
#include "xrt/experimental/xrt_xclbin.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

// % g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o xclbin.exe main.cpp -lxrt_coreutil -luuid -pthread


// This value is shared with worgroup size in kernel.cl
static constexpr auto COUNT = 1024;

static void
usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  [-h]\n\n";
    std::cout << "* Bitstream is required\n";
}

std::ostream&
operator << (std::ostream& ostr, const xrt::xclbin::mem& mem)
{
  ostr << "mem tag:        " << mem.get_tag() << "\n";
  ostr << "mem used:       " << (mem.get_used() ? "true" : "false") << "\n";
  ostr << "mem index:      " << mem.get_index() << "\n";
  ostr << "mem size (kb):  0x" << std::hex << mem.get_size_kb() << std::dec << "\n";
  ostr << "mem base addr:  0x" << std::hex << mem.get_base_address() << std::dec << "\n";
  return ostr;
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
operator << (std::ostream& ostr, xrt::xclbin::ip::ip_type ip_type)
{
  switch (ip_type) {
  case xrt::xclbin::ip::ip_type::pl :
    ostr << "pl";
    return ostr;
  case xrt::xclbin::ip::ip_type::ps :
    ostr << "ps";
    return ostr;
  default:
    ostr << "not defined";
    return ostr;
  }

  return ostr;
}

std::ostream&
operator << (std::ostream& ostr, const xrt::xclbin::ip& cu)
{
  ostr << "instance name:  " << cu.get_name() << "\n";
  ostr << "base address:   0x" << std::hex << cu.get_base_address() << std::dec << "\n";
  ostr << "cu type:        " << cu.get_type() << "\n";

  // ip arguments
  for (const auto& arg : cu.get_args())
    ostr << arg << '\n';

  return ostr;
}

std::ostream&
operator << (std::ostream& ostr, xrt::xclbin::kernel::kernel_type kernel_type)
{
  switch (kernel_type) {
  case xrt::xclbin::kernel::kernel_type::none :
    ostr << "none";
    return ostr;
  case xrt::xclbin::kernel::kernel_type::pl :
    ostr << "pl";
    return ostr;
  case xrt::xclbin::kernel::kernel_type::ps :
    ostr << "ps";
    return ostr;
  case xrt::xclbin::kernel::kernel_type::dpu :
    ostr << "dpu";
    return ostr;
  default:
    ostr << "not defined";
    return ostr;
  }

  return ostr;
}

std::ostream&
operator << (std::ostream& ostr, const xrt::xclbin::kernel& kernel)
{
  // kernel function
  ostr << "kernel type: " << kernel.get_type() << "\n";
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

std::ostream&
operator << (std::ostream& ostr, const xrt::xclbin::aie_partition& aiep)
{
  ostr << "aie_partition\n";
  ostr << "operations_per_cycle: " << aiep.get_operations_per_cycle() << '\n';
  ostr << "inference_fingerprint: " << aiep.get_inference_fingerprint() << '\n';
  ostr << "pre_post_fingerprint: " << aiep.get_pre_post_fingerprint() << '\n';

  return ostr;
}

static void
list_xclbins_in_repo()
{
  // List all xclbins
  std::cout << "============================ XCLBINS ==========================\n";
  xrt::xclbin_repository repo{}; // repository, current directory, or ini
  auto end = repo.end();
  std::cout << "number of xclbins: " << std::distance(repo.begin(), end) << '\n';
  for (auto itr = repo.begin(); itr != end; ++itr) {
    std::cout << "xclbin: " << itr.path() << '\n';
    auto xclbin = (*itr);
    std::cout << "xsa(" << xclbin.get_xsa_name() << ")\n";
    std::cout << "uuid(" << xclbin.get_uuid().to_string() << ")\n";
  }
}

void
run_cpp(const std::string& xclbin_fnm)
{
  // Construct xclbin from fnm
  std::cout << "============================ CPP ==============================\n";
  auto xclbin = xrt::xclbin(xclbin_fnm);
  auto uuid = xclbin.get_uuid();
  std::cout << xclbin_fnm << "\n";
  std::cout << "xsa(" << xclbin.get_xsa_name() << ")\n";
  std::cout << "uuid(" << uuid.to_string() << ")\n";
  std::cout << "fpga(" << xclbin.get_fpga_device_name() << ")\n\n";

  for (auto& kernel : xclbin.get_kernels())
    std::cout << kernel << '\n';

  for (auto& mem : xclbin.get_mems())
    std::cout << mem << '\n';

  for (auto& aiep : xclbin.get_aie_partitions())
    std::cout << aiep << '\n';
}

void
run_c(const std::string& xclbin_fnm)
{
  std::cout << "============================= C ===============================\n";
  auto xhdl = xrtXclbinAllocFilename(xclbin_fnm.c_str());
  std::cout << xclbin_fnm << '\n';
  std::cout << "number of kernels " << xrtXclbinGetNumKernels(xhdl) << '\n';
  std::cout << "number of compute units " << xrtXclbinGetNumKernelComputeUnits(xhdl) << '\n';
  xrtXclbinFreeHandle(xhdl);
}

static int
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

  list_xclbins_in_repo();
  run_cpp(xclbin_fnm);
  run_c(xclbin_fnm);

  return 0;
}

int
main(int argc, char** argv)
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
