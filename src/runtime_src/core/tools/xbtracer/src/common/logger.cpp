// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include "common/trace_utils.h"

namespace xrt::tools::xbtracer
{
logger::logger(const char *logger_name, logger::level pl, const char* ofile_name):
  lname(logger_name),
  plevel(pl)
{
  if (ofile_name) {
    // Do not redirect stdout to the specified file.
    // we want to both write to the specified file and also output to stdout
    ofile.open(ofile_name);
    if (!ofile.is_open())
      throw std::runtime_error("failed to open logger file \"" + std::string(ofile_name) + " .");
  }
}

logger::~logger()
{
  if (ofile.is_open())
    ofile.close();
}

constexpr size_t logger_plevel_str_len_max = 16;
constexpr size_t logger_name_str_len_max = 16;
constexpr size_t logger_file_str_len_max = 2048;

logger&
logger::get_instance()
{
  std::call_once(init_instance_flag, []() {
    std::string plevel(logger_plevel_str_len_max, 0);
    std::string name(logger_name_str_len_max, 0);
    std::string ofile(logger_file_str_len_max, 0);
    getenv_os("XBRACER_PRINT_NAME", name.data(), name.capacity());
    getenv_os("XBRACER_PRINT_LEVEL", plevel.data(), plevel.capacity());
    getenv_os("XBTRACER_PRINT_FILE", ofile.data(), ofile.capacity());
    logger::level l = logger::level::INFO;

    plevel.resize(strlen(plevel.c_str()));
    if (strlen(plevel.c_str())) {
      if (plevel == "CRITICAL")
        l = logger::level::CRITICAL;
      else if(plevel == "ERROR")
        l = logger::level::ERR;
      else if(plevel == "WARNING")
        l = logger::level::WARNING;
      else if(plevel == "INFO")
        l = logger::level::INFO;
      else if(plevel == "DEBUG")
        l = logger::level::DEBUG;
      else
        throw std::runtime_error("xbtracer: unsupported print level: \"" + plevel + "\".");
    }

    if (!strlen(name.c_str()))
        name = "unknown";
    const char* ofile_name = nullptr;
    if (strlen(ofile.c_str()))
      ofile_name = ofile.c_str();
    instance = std::make_unique<logger>(name.c_str(), l, ofile_name);
  });
  return *instance;
}

} // namespace xrt::tools::xbtracer

std::unique_ptr<xrt::tools::xbtracer::logger> xrt::tools::xbtracer::logger::instance = nullptr;
std::once_flag xrt::tools::xbtracer::logger::init_instance_flag;
