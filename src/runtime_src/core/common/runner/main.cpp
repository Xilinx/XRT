// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// This test configures and runs a recipe one time
// g++ -g -std=c++17
//   -I/home/stsoe/git/stsoe/XRT/build/Debug/opt/xilinx/xrt/include
//   -I/home/stsoe/git/stsoe/XRT/src/runtime_src
//   -L/home/stsoe/git/stsoe/XRT/build/Debug/opt/xilinx/xrt/lib
//   -o xrt-runner.exe runner-profile.cpp -lxrt_coreutil -pthread
//
// or
//
// mkdir build
// cd build
// cmake -DXILINX_XRT=/home/stsoe/git/stsoe/XRT/build/Debug/opt/xilinx/xrt
//       -DXRT_ROOT=/home/stsoe/git/stsoe/XRT ..
// cmake --build . --config Debug
//
// ./xrt-runner.exe --recipe ... --profile ... [--dir ...]
#include "xrt/xrt_device.h"
#include "core/common/time.h"
#include "core/common/runner/runner.h"

#include "core/common/json/nlohmann/json.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

static void
usage()
{
  std::cout << "usage: xrt-runner.exe [options]\n";
  std::cout << " --recipe <recipe.json> recipe file to run\n";
  std::cout << " --profile <profile.json> execution profile\n";
  std::cout << " [--dir <path>] directory containing artifacts (default: current dir)\n";
  std::cout << "\n\n";
  std::cout << "xrt-runner.exe --recipe recipe.json --profile profile.json\n";
}

static void
run(const std::string& recipe,
    const std::string& profile,
    const std::string& dir,
    bool report)
{
  xrt_core::systime st;

  xrt::device device{0};
  xrt_core::runner runner {device, recipe, profile, dir};
  runner.execute();
  runner.wait();
  if (report) {
    auto [real, user, system] = st.get_rusage();
    auto jrpt = json::parse(runner.get_report());
    jrpt["system"] = { {"real", real.to_sec() }, {"user", user.to_sec() }, {"kernel", system.to_sec()} };
    std::cout << jrpt.dump(4) << "\n";
  }
  
  auto [real, user, system] = st.get_rusage();

  std::cout << "real: " <<  std::fixed << std::setprecision(4) << real.to_sec() << '\n';
  std::cout << "user: " <<  std::fixed << std::setprecision(4) << user.to_sec() << '\n';
  std::cout << "system: " <<  std::fixed << std::setprecision(4) << system.to_sec() << '\n';
}

static void
run(int argc, char* argv[])
{
  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  std::string recipe;
  std::string profile;
  std::string dir = ".";
  bool report = false;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return;
    }

    if (arg == "--report") {
      report = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "--recipe")
      recipe = arg;
    else if (cur == "--profile")
      profile = arg;
    else if (cur == "--dir")
      dir = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  run(recipe, profile, dir, report);
}

int
main(int argc, char **argv)
{
  try {
    run(argc, argv);
    return 0;
  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << '\n';
  }
  catch (...) {
    std::cerr << "Unknown error\n";
  }
  return 1;

}
