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

logger&
logger::get_instance()
{
  std::call_once(init_instance_flag, []() {
    char plevel[16] = {0};
    char name[16] = {0};
    char ofile[2048] = {0};
    getenv_os("XBRACER_PRINT_NAME", name, sizeof(name));
    getenv_os("XBRACER_PRINT_LEVEL", plevel, sizeof(plevel));
    getenv_os("XBTRACER_PRINT_FILE", ofile, sizeof(ofile));
    logger::level l = logger::level::INFO;

    if (strlen(plevel)) {
      // TODO: we only support DEFAULT tracing level for now.
      std::string plevel_str = plevel;
      if (plevel_str == "CRITICAL")
        l = logger::level::CRITICAL;
      else if(plevel_str == "ERROR")
        l = logger::level::ERR;
      else if(plevel_str == "WARNING")
        l = logger::level::WARNING;
      else if(plevel_str == "INFO")
        l = logger::level::INFO;
      else if(plevel_str == "DEBUG")
        l = logger::level::DEBUG;
      else
        throw std::runtime_error("xbtracer: unsupported print level: \"" + plevel_str + "\".");
    }

    const char* logger_name = name;
    if (!strlen(logger_name))
        logger_name = "unknown";
    const char* ofile_name = nullptr;
    if (strlen(ofile))
      ofile_name = ofile;
    instance = std::unique_ptr<logger>(new logger(name, l, ofile_name));
  });
  return *instance;
}

} // namespace xrt::tools::xbtracer

std::unique_ptr<xrt::tools::xbtracer::logger> xrt::tools::xbtracer::logger::instance = nullptr;
std::once_flag xrt::tools::xbtracer::logger::init_instance_flag;
