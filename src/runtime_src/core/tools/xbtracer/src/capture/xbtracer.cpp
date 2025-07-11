// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <capture/xbtracer.h>
#include <common/trace_utils.h>

using namespace xrt::tools::xbtracer;

static
void
usage(const char* cmd) {
  std::cout << "Usage: " << cmd << " [options] <App_full_path> [App Arguments]" << std::endl;
  std::cout << "This program is to capture XRT APIs calling sequence and arguments." << std::endl;
  std::cout << "Optinoal:" << std::endl;
  std::cout << "\t-h|--help Print usage" << std::endl;
  std::cout << "\t-v|--verbose turn on printing verbosely" << std::endl;
  std::cout << "\t-o|--out_dir output directory which holds trace output files" << std::endl;
}

// NOLINTBEGIN(*-avoid-c-arrays)
static
int
parse_args(struct tracer_arg &args, int argc, const char* argv[])
// NOLINTEND(*-avoid-c-arrays)
{
  if (argc < 2) {
    usage(argv[0]);
    std::cerr << "ERROR: xbtracer: not enough argument." << std::endl;
    return -EINVAL;
  }

  args.verbose = false;
  bool got_app = false;
  for (int i = 1; i < argc; i++) {
    std::string arg_str = argv[i];
    if (arg_str == "-h" || arg_str == "--help") {
      usage(argv[0]);
      return 1;
    }
    else if ((!got_app) && (arg_str == "-v" || arg_str == "--verbose")) {
      args.verbose = true;
    }
    else if ((!got_app) && (arg_str == "-o" || arg_str == "--out_dir")) {
      args.out_dir = argv[++i];
    }
    else if (!got_app && argv[i][0] == '-') {
      std::cerr << "ERROR: xbtracer: unsuppocrted argument: " + arg_str << std::endl;
      return -EINVAL;
    }
    else if (!got_app) {
      args.target_app.emplace_back(argv[i]);
      got_app = true;
    }
    else {
      args.target_app.push_back(arg_str);
    }
  }

  return 0;
}

static
int
init_logger(const struct tracer_arg &args)
{
  // setup logger environment variable, as we need to pass them to child process
  int ret = setenv_os("XBRACER_PRINT_NAME", "xbtracer");
  const char* plevel_str = "INFO";
  if (args.verbose)
    plevel_str = "DEBUG";
  ret |= setenv_os("XBRACER_PRINT_LEVEL", plevel_str);

  if (ret) {
    std::cerr << "ERROR: xbracer: failer to set logging env." << std::endl;
    return -EINVAL;
  }
  return 0;
}

static
int
init_tracer(const struct tracer_arg &args)
{
  std::filesystem::path opath;
  if (args.out_dir.empty())
    opath = std::filesystem::current_path();
  else
    opath = args.out_dir;
  std::string timestamp_str = xbtracer_get_timestamp_str();
  opath.append("trace_" + timestamp_str);
  std::string opath_str = opath.string();

  std::error_code ec;
  bool created = std::filesystem::create_directories(opath, ec);
  if (!created) {
    xbtracer_perror("failed to create tracer directory \"", opath_str, "\", ", ec.message(), "\".");
    return -EINVAL;
  }

  int ret = setenv_os("XBTRACER_OUT_DIR", opath_str.c_str());
  if (ret) {
    xbtracer_perror("failed to set tracer output file \"", opath.string(), "\".");
    return -EINVAL;
  }
  xbtracer_pinfo("tracer output to directory \"", opath.string(), "\".");
  return 0;
}

int
main(int argc, const char* argv[])
{
  try {
    struct tracer_arg args;

    int ret = parse_args(args, argc, argv);
    if (ret < 0) {
      std::cerr << "ERRPR: xbtracer: failed to parse user input arguments." << std::endl;
      return ret;
    }
    // --help
    if (ret > 0)
      return 0;

    ret = init_logger(args);
    if (ret)
      return -EINVAL;
    ret = init_tracer(args);
    if (ret)
      return -EINVAL;

    std::string app_str = std::accumulate(std::next(args.target_app.begin()), args.target_app.end(),
                                          args.target_app[0],
                                          [](const std::string& a, const std::string& b)
                                          {
                                            return a + " " + b;
                                          });
    xbtracer_pinfo("Starting to trace app \"", app_str, "\".");
    return launch_app(args);
  } catch (const std::exception& e) {
    std::cerr << "ERROR: [XBTRACER]: application launch has exception: " << e.what() << std::endl;
    return -EINVAL;
  }
  // Will not reach here
  return 0;
}
