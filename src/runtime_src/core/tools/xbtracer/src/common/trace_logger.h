// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef xbtracer_common_logger_h
#define xbtracer_common_logger_h

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <utility>

namespace xrt::tools::xbtracer
{

// Helper to print a single argument
template<typename T>
void
print_one(std::ostream& os, const T& arg)
{
  std::cout << arg;
  if (&os != &std::cout)
    os << arg;
}

// Recursive variadic template to print all arguments
template<typename T, typename... Args>
void
print_all(std::ostream& os, T& first, const Args&... args)
{
  print_one(os, first);
  if constexpr (sizeof...(args) > 0)
    print_all(os, args...);
}

class logger
{
public:
  enum level
  {
    CRITICAL = 0,
    ERR = 1,
    WARNING = 2,
    INFO = 3,
    DEBUG = 4,
  };

  logger(const char *logger_name, level pl, const char* ofile_name);
  // only allows parameters constructor
  logger() = delete;
  // singleton, delete copy constructor and assignment operator to enforce singleton
  logger(const logger&) = delete;
  logger& operator=(const logger&) = delete;
  // singleton, elete move constructor and move assignment operator
  logger(logger&&) = delete;
  logger& operator=(logger&&) = delete;
  ~logger();

  static
  logger&
  get_instance();

  template<typename... Args>
  void
  print(level l, const Args&... args)
  {
    if (l > plevel)
      return;

    std::string prefix;
    if (l == level::CRITICAL)
      prefix = "CRITICAL";
    else if (l == level::ERR)
      prefix = "ERROR";
    else if (l == level::WARNING)
      prefix = "WARNING";
    else if (l == level::INFO)
      prefix = "INFO";
    else
      prefix = "DEBUG";

    prefix.append(": [" + lname + "]: ");
    if (ofile.is_open()) {
      print_one(ofile, prefix);
      print_all(ofile, args...);
      print_one(ofile, "\n");
    }
    else {
      print_one(std::cout, prefix);
      print_all(std::cout, args...);
      print_one(std::cout, "\n");
    }
  }

private:
  static std::unique_ptr<logger> instance;
  static std::once_flag init_instance_flag;
  std::string lname;
  std::ofstream ofile;
  level plevel;
}; // class xrt::tools::xbtracer::logger

} // namespace xrt::tools::xbtracer

template<typename... Args>
void
xbtracer_pcritical(const Args&... args)
{
  xrt::tools::xbtracer::logger::get_instance().print(xrt::tools::xbtracer::logger::level::CRITICAL,
                                                     args...);
  throw std::runtime_error("Critical error.");
}

template<typename... Args>
void
xbtracer_perror(const Args&... args)
{
  xrt::tools::xbtracer::logger::get_instance().print(xrt::tools::xbtracer::logger::level::ERR,
                                                     args...);
}

template<typename... Args>
void
xbtracer_pwarning(const Args&... args)
{
  xrt::tools::xbtracer::logger::get_instance().print(xrt::tools::xbtracer::logger::level::WARNING,
                                                     args...);
}

template<typename... Args>
void
xbtracer_pinfo(const Args&... args)
{
  xrt::tools::xbtracer::logger::get_instance().print(xrt::tools::xbtracer::logger::level::INFO,
                                                     args...);
}

template<typename... Args>
void
xbtracer_pdebug(const Args&... args)
{
  xrt::tools::xbtracer::logger::get_instance().print(xrt::tools::xbtracer::logger::level::DEBUG,
                                                     args...);
}

#endif // xbtracer_common_logger_h
