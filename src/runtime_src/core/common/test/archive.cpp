// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// Unit test for archive extraction
//
// % cmake -B build -DXILINX_XRT=<path>
// % cmake --build build --config <Release|Debug>
//
// % ar q myarchive.a file1 file2 ...
// % <path>/archive -a myarchive.a -m file2 -g file2

#include "core/common/archive.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void
usage()
{}

void
run(int argc, char* argv[])
{
  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  std::string archive_filename;
  std::string archive_member;
  std::string golden_filename;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "--archive" || cur == "-a") {
      archive_filename = arg;
    }
    else if (cur == "--member" || cur == "-m") {
      archive_member = arg;
    }
    else if (cur == "--golden" || cur == "-g") {
      golden_filename = arg;
    }
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (archive_filename.empty())
    throw std::runtime_error("--archive must be specified");

  if (archive_member.empty())
    throw std::runtime_error("--member must be specified");

  // Open archive
  xrt_core::archive archive(archive_filename);

  // Extract archive member
  auto data = archive.data(archive_member);

  if (golden_filename.empty())
    return;

  // Compare data agaist golden
  std::ifstream ifs(golden_filename, std::ios::binary);
  if (!ifs)
    throw std::runtime_error("Failed to open file: " + golden_filename);

  std::string golden((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  if (data.size() != golden.size())
    throw std::runtime_error
      ("archive member data size (" + std::to_string(data.size())
       + ") mismatch (" + std::to_string(golden.size()) + ")");

  if (!std::equal(golden.data(), golden.data() + golden.size(), data.data()))
    throw std::runtime_error("archive member data mismatch");
}

int main(int argc, char* argv[])
{
  try {
    run(argc, argv);
    return 0;
  }
  catch (const std::exception& ex) {
    std::cout << "Exception caught: " << ex.what() << '\n';
  }
  catch (...) {
    std::cout << "Unknown exception\n";
  }
  return 1;
}
