// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// This test configures and runs a recipe one time
// g++ -g -std=c++17
//   -I/home/stsoe/git/stsoe/XRT/build/Debug/opt/xilinx/xrt/include
//   -I/home/stsoe/git/stsoe/XRT/src/runtime_src
//   -L/home/stsoe/git/stsoe/XRT/build/Debug/opt/xilinx/xrt/lib
//   -o runner.exe runner.cpp -lxrt_coreutil -pthread
//
// or
//
// mkdir build
// cd build
// cmake -DXILINX_XRT=/home/stsoe/git/stsoe/XRT/build/Debug/opt/xilinx/xrt
//       -DXRT_ROOT=/home/stsoe/git/stsoe/XRT ..
// cmake --build . --config Debug
//
// ./runner.exe -kp ... -kp ... -bd ... -bd ... -bg ... -recipe ...

#include "xrt/xrt_device.h"
#include "experimental/xrt_ext.h"
#include "core/common/runner/runner.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

static xrt_core::runner::artifacts_repository g_repo;
static std::map<std::string, std::string> g_buffer2data;
static std::map<std::string, xrt::bo> g_buffer2bo;
static std::map<std::string, std::string> g_buffer2golden;
static std::string g_recipe;

static void
usage()
{
  std::cout << "usage: %s [options]\n";
  std::cout << " --resource <key:path> artifact key data pair, the key is referenced by recipe\n";
  std::cout << " --buffer <key:path> external buffer data, the key is referenced by recipe\n";
  std::cout << " --golden <key:path> external buffer goldendata, the key matches a -bd pair\n";
  std::cout << " --recipe <recipe.json> recipe file to run\n";
  std::cout << "\n\n";
  std::cout << "host.exe -r elf:foo.elf \n"
            << "         -b ifm:ifm.bin -b ofm:ofm.bin -b wts:wts.bin\n"
            << "         -g ofm:gold.bin\n"
            << "         --recipe recipe.json\n";
}

static std::vector<char>
read_file(const std::string& fnm)
{
  std::ifstream ifs{fnm, std::ios::binary};
  if (!ifs)
    throw std::runtime_error("Failed to open file '" + fnm + "' for reading");

  ifs.seekg(0, std::ios::end);
  std::vector<char> data(ifs.tellg());
  ifs.seekg(0, std::ios::beg);
  ifs.read(data.data(), data.size());
  return data;
}

static void
add_repo_file(const std::string& key, const std::string& path)
{
  auto data = read_file(path);
  g_repo.emplace(key, std::move(data));
}

static void
run(const xrt::device& device, const std::string& recipe)
{
  // 1. Add artifacts to the repository (done during cmdline parsing)
  
  // 2. Create the runner from the recipe
  xrt_core::runner runner {device, recipe, g_repo};

  // 3. Create buffers for external input and output
  // 4. Bind to runner
  for (auto& [buffer, path] : g_buffer2data) {
    auto data = read_file(path);
    std::cout << buffer << " size = " << data.size() << "\n";
    xrt::bo bo = xrt::ext::bo{device, data.size()};
    auto bo_data = bo.map<char*>();
    std::copy(data.data(), data.data() + data.size(), bo_data);
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    runner.bind(buffer, bo);

    // Save if referenced for golden comparison
    g_buffer2bo.emplace(buffer, bo);
  }

  // 5. Execute the runner and wait for completion
  runner.execute();

  // 6. Wait for the runner to finish
  runner.wait();

  // 7. Compare the output with golden if any
  for (auto& [buffer, golden] : g_buffer2golden) {
    auto bo = g_buffer2bo.at(buffer);
    bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    
    auto bo_data = bo.map<char*>();
    auto golden_data = read_file(golden);
    if (bo.size() != golden_data.size())
      throw std::runtime_error("Golden and output size mismatch");

    std::cout << "Comparing golden and output data\n";
    if (!std::equal(golden_data.data(), golden_data.data() + golden_data.size(), bo_data)) {
      for (uint64_t i = 0; i < golden_data.size(); ++i) {
        if (golden_data[i] != bo_data[i])
          throw std::runtime_error("Golden and output mismatch at index " + std::to_string(i));
      }
    }
  }      
}

static void
run(const std::string& recipe)
{
  // Create device
  xrt::device device{0};
  run(device, recipe);
}

static void
run(int argc, char* argv[])
{
  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  std::string recipe;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "--resource" || cur == "-r") {
      auto pos = arg.find(":");
      if (pos == std::string::npos)
        throw std::runtime_error("resource option must take the form of '-resource key:path'");

      auto key = arg.substr(0,pos);
      auto path = arg.substr(pos+1);

      std::cout << "Adding repo (key, path): (" << key << ", " << path << ")\n";
      add_repo_file(key, path);
    }
    else if (cur == "--buffer" || cur =="-b") {
      auto pos = arg.find(":");
      if (pos == std::string::npos)
        throw std::runtime_error("buffer data option must take the form of '-buffer buffer:path'");

      auto buffer = arg.substr(0,pos);
      auto datapath = arg.substr(pos+1);

      std::cout << "Using (buffer, path): (" << buffer << ", " << datapath << ")\n";
      g_buffer2data.emplace(buffer, datapath);
    }
    else if (cur == "-golden" || cur == "-g") {
      auto pos = arg.find(":");
      if (pos == std::string::npos)
        throw std::runtime_error("golden data option must take the form of '-golden buffer:path'");

      auto buffer = arg.substr(0,pos);
      auto datapath = arg.substr(pos+1);

      std::cout << "Using golden (buffer, path): (" << buffer << ", " << datapath << ")\n";
      g_buffer2golden.emplace(buffer, datapath);
    }
    else if (cur == "--recipe") {
      std::cout << "Using recipe: " << arg << '\n';
      recipe = arg;
    }
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  run(recipe);
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
