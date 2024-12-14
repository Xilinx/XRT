// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include <iostream>
#include "../xrt_runner.h"
#include "../cpu.h"

static void
run(int argc, char **argv)
{
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <dll>\n";
    return;
  }
  
  auto dll = argv[1];
  xrt_core::cpu::function hello{"hello", dll};
  xrt_core::cpu::run run{hello};
  run.set_arg(0, 10);
  run.set_arg(1, std::string("world"));
  std::string out;
  run.set_arg(2, &out);
  run.execute();
  std::cout << out << "\n";
}

int
main(int argc, char **argv)
{
  try {
    run(argc, argv);
    return 0;
  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
  }
  catch (...) {
    std::cerr << "Unknown error" << "\n";
  }
  return 1;
  
}

