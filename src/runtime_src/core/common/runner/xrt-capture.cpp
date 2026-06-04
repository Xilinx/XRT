// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#include "detail/process.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <filesystem>

// Wrapper utility to capture XRT application execution
//
// Usage: xrt-capture [options] -- <application> [app-args]
//
// Sets environment variables to configure XRT capture, launches the
// application, then reports results. Captured data is written to the
// specified output directory.
//
// Options:
//   --frames <num>       Number of frames to capture (required)
//   --output-dir <path>  Output directory for capture artifacts (default: ./xrt_capture)
//   --help, -h           Show help message

namespace {

struct capture_options
{
  uint32_t frames = 0;
  std::filesystem::path output_dir = "./xrt_capture";
  std::vector<std::string> app_args;
};

void
usage()
{
  std::cout << "Usage: xrt-capture [options] -- <application> [app-args]\n\n";
  std::cout << "Capture XRT application execution for replay.\n\n";
  std::cout << "Options:\n";
  std::cout << "  --frames <num>       Number of frames to capture (required)\n";
  std::cout << "  --output-dir <path>  Output directory for artifacts (default: ./xrt_capture)\n";
  std::cout << "  --help, -h           Show this help message\n\n";
  std::cout << "Examples:\n";
  std::cout << "  # Capture 10 frames from application\n";
  std::cout << "  xrt-capture --frames 10 -- ./my_app arg1 arg2\n\n";
  std::cout << "  # Capture to specific directory\n";
  std::cout << "  xrt-capture --frames 20 --output-dir /tmp/capture -- ./my_app\n";
}

capture_options
parse_args(int argc, char* argv[])
{
  capture_options opts;
  std::vector<std::string> args(argv + 1, argv + argc);

  bool found_separator = false;
  size_t i = 0;

  // Parse xrt-capture options
  while (i < args.size()) {
    const auto& arg = args[i];

    if (arg == "--") {
      found_separator = true;
      ++i;
      break;
    }

    if (arg == "--help" || arg == "-h") {
      usage();
      std::exit(0);
    }
    else if (arg == "--frames") {
      if (i + 1 >= args.size())
        throw std::runtime_error("--frames requires an argument");
      opts.frames = std::stoul(args[++i]);
    }
    else if (arg == "--output-dir") {
      if (i + 1 >= args.size())
        throw std::runtime_error("--output-dir requires an argument");
      opts.output_dir = args[++i];
    }
    else {
      throw std::runtime_error("Unknown option: " + arg);
    }

    ++i;
  }

  if (!found_separator)
    throw std::runtime_error("Missing '--' separator before application command");

  if (opts.frames == 0)
    throw std::runtime_error("--frames is required and must be > 0");

  // Remaining arguments are the application and its args
  opts.app_args.assign(args.begin() + i, args.end());

  if (opts.app_args.empty())
    throw std::runtime_error("No application specified after '--'");

  return opts;
}

// Set environment variables for XRT capture configuration
// XRT config reader checks environment variables before consulting xrt.ini
void
setup_capture_environment(const capture_options& opts)
{
  std::cout << "Configuring capture via environment...\n";
  std::cout << "  Runtime.capture_frames=" << opts.frames << "\n";
  std::cout << "  Runtime.capture_output_dir=" << opts.output_dir.string() << "\n";

  // Set environment variables using the same key format as xrt.ini
  // The config reader will check these before reading xrt.ini
#ifdef _WIN32
  _putenv_s("Runtime.capture_frames", std::to_string(opts.frames).c_str());
  _putenv_s("Runtime.capture_output_dir", opts.output_dir.string().c_str());
#else
  setenv("Runtime.capture_frames", std::to_string(opts.frames).c_str(), 1);
  setenv("Runtime.capture_output_dir", opts.output_dir.string().c_str(), 1);
#endif
}

void
run(int argc, char* argv[])
{
  auto opts = parse_args(argc, argv);

  std::cout << "XRT Capture Utility\n";
  std::cout << "===================\n";
  std::cout << "Application: " << opts.app_args[0] << "\n";
  std::cout << "Frames: " << opts.frames << "\n";
  std::cout << "Output: " << opts.output_dir.string() << "\n\n";

  // Configure capture via environment variables
  setup_capture_environment(opts);

  // Launch application
  std::cout << "Launching application...\n";
  int exit_code = xrt_core::detail::execute_process(opts.app_args);

  std::cout << "\nApplication exited with code " << exit_code << "\n";

  // Check if capture data was generated
  std::filesystem::path replay_json = opts.output_dir / "replay.json";
  if (std::filesystem::exists(replay_json)) {
    std::cout << "\nCapture successful!\n";
    std::cout << "  Replay script: " << replay_json.string() << "\n";
    std::cout << "  Artifacts: " << opts.output_dir.string() << "\n\n";
    std::cout << "To replay:\n";
    std::cout << "  xrt-replay --replay " << replay_json.string()
              << " --dir " << opts.output_dir.string() << "\n";
  }
  else {
    std::cout << "\nWarning: No replay.json found in " << opts.output_dir.string() << "\n";
    std::cout << "  Check that the application executed at least " << opts.frames << " frames\n";
  }
}

} // namespace

int
main(int argc, char* argv[])
{
  try {
    run(argc, argv);
    return 0;
  }
  catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << '\n';
    usage();
  }
  catch (...) {
    std::cerr << "Unknown error\n";
  }
  return 1;
}
