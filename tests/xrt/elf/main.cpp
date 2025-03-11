// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "xrt/experimental/xrt_aie.h"
#include "xrt/experimental/xrt_elf.h"

#include <iostream>
#include <string>
#include <vector>

static void
usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  --elf <file>\n";
    std::cout << "  [-h]\n\n";
}

static void
true_or_error(bool cond, const std::string& msg)
{
  if (!cond)
    throw std::runtime_error("Error: condition failed - " + msg);
}

static void
test_elf(const std::string& elf_fnm)
{
  xrt::elf elf(elf_fnm);
}

static void
test_program(const std::string& elf_fnm)
{
  xrt::elf elf{elf_fnm};
  xrt::aie::program program1{elf_fnm};
  true_or_error(elf.get_handle() != program1.get_handle(), "expected different elf handles");
  
  xrt::aie::program program2{elf};
  true_or_error(elf.get_handle() == program2.get_handle(), "expected same elf handles");
  
  //auto col2 = program2.get_partition_size();

  xrt::aie::program program3{program2};
  true_or_error(elf.get_handle() == program3.get_handle(), "expected same elf handles");

  xrt::aie::program program4{std::move(program3)};
  true_or_error(elf.get_handle() == program4.get_handle(), "expected same elf handles");
}

static void
test_module(const std::string& elf_fnm)
{
}

static int
run(int argc, char** argv)
{
  if (argc < 2) {
    usage();
    return 1;
  }

  std::string elf_fnm;

  std::vector<std::string> args{argv + 1, argv + argc};
  std::string cur;
  for (const auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 0;
    }

    if (arg[0] == '-')
      cur = arg.substr(arg.find_first_not_of("-"));

    else if (cur == "elf")
      elf_fnm = arg;
  }

  if (elf_fnm.empty()) {
    std::cout << "Error: ELF file not specified\n";
    return 1;
  }

  test_elf(elf_fnm);
  test_program(elf_fnm);
  test_module(elf_fnm);

  return 0;
}

int main(int argc, char* argv[])
{
  try {
    return run(argc, argv);
  }
  catch (const std::exception& ex) {
    std::cout << "Caught exception: " << ex.what() << '\n';
  }
  catch (...) {
    std::cout << "Caught unknown exception\n";
  }
  return 1;
}
