// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "core/common/runner/runner.h"
#include "xrt/xrt_device.h"

#include <iostream>
#include <string>

#ifdef _WIN32
# pragma warning (disable: 4100)
#endif

static void
run(int argc, char* argv[])
{
  std::string recipe { argv[1] };
  xrt::device device{0};

  xrt_core::runner runner{device, recipe};
}

int
main(int argc, char* argv[])
{
  try {
    if (argc < 2) {
      std::cout << "Usage: " << argv[0] << " <recipe_file>" << '\n';
      return 1;
    }

    run(argc, argv);
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
