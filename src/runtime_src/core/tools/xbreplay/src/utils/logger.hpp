// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <iostream>
#include <mutex>
#include <sstream>
#include <stdio.h>

namespace xrt_core::tools::xbreplay::utils {
enum class log_level
{
  debug = 0,
  info,
  warning,
  error
};

class logger
{
  public:
  static logger& get_instance()
  {
    static logger instance;
    return instance;
  }

  void set_loglevel(log_level level)
  {
    std::lock_guard lock(m_mutex);
    m_log_level = level;
  }

  void set_loglevel(const std::string& str)
  {
    std::lock_guard lock(m_mutex);
    auto level  = static_cast<log_level>(std::stoi(str));
    if ((level < log_level::debug) || (level > log_level::error))
    {
      std::cout <<"Invalid log level received: "<< str <<"\n";
    }
    else
    {
      m_log_level = (log_level) level;
    }
  }

  template<typename... Args>
  void debug(const std::string& file, const std::string& func,
                                  const uint32_t line, Args&&... args)
  {
    log(log_level::debug, create_log_prefix(file, func, line), std::forward<Args>(args)...);
  }

  template<typename... Args>
  void info(const std::string& file, const std::string& func,
                                  const uint32_t line, Args&&... args)
  {
    log(log_level::info, create_log_prefix(file, func, line), std::forward<Args>(args)...);
  }

  template<typename... Args>
  void warning(const std::string& file, const std::string& func,
                                 const uint32_t line, Args&&... args)
  {
    log(log_level::warning, create_log_prefix(file, func, line), std::forward<Args>(args)...);
  }

  template<typename... Args>
  void error(const std::string& file, const std::string& func,
                                const uint32_t line, Args&&... args)
  {
    log(log_level::error, create_log_prefix(file, func, line), std::forward<Args>(args)...);
  }

  std::string create_log_prefix(const std::string& file,
                                     const std::string& func, const uint32_t line)
  {
    return "[" + extract_filename(file) + "] [" + func + ":" + std::to_string(line) + "]";
  }

  log_level get_log_level()
  {
    return m_log_level;
  }

  private:
  logger()
  : m_log_level(log_level::info)
  {}

  logger(const logger&) = delete;
  logger& operator=(const logger&) = delete;

  std::mutex m_mutex;
  log_level m_log_level;

  std::string levelToString(log_level level) const
  {
    switch (level)
    {
      case log_level::debug: return "XBREPLAY_DEBUG";
      case log_level::info: return "XBREPLAY_INFO";
      case log_level::warning: return "XBREPLAY_WARNING";
      case log_level::error: return "XBREPLAY_ERROR";
      default: return "UNKNOWN";
    }
  }

  // Function to extract file name from __FILE__ macro
  std::string extract_filename(const std::string& filepath)
  {
    size_t lastSlashIndex = filepath.find_last_of("/\\");
    if (lastSlashIndex != std::string::npos)
      return filepath.substr(lastSlashIndex + 1);
     else
      return filepath;
  }

  template<typename... Args>
  void log(log_level level, const std::string& message, Args&&... args)
  {
    if (static_cast<int>(level) < static_cast<int>(m_log_level))
      return; // Skip logging if level is lower than current log level

    std::lock_guard lock(m_mutex);
    std::ostringstream oss;
    oss << "[" << levelToString(level) << "] " << message;

    auto to_string = [](const auto& arg) {
      std::ostringstream oss;
      oss << arg;
      return oss.str();
    };

    ((oss << " " << to_string(args)), ...);

    oss << "\n";
    std::cout << oss.str();
  }
};
} // end of namespace xrt_core::tools::xbreplay::utils

namespace {
namespace xbr = xrt_core::tools::xbreplay;
static xbr::utils::logger& l = xbr::utils::logger::get_instance();

/*
 * Clang warning:  implicitly decay an array into a pointer
 * Here issue is arising due to use of macro __FILE__ into a pointer and similarly
 * other macros __func__ & __Line__. The macros decays to Pointers which cannot be
 * avoided and impractically, suppresing the warning.
 */
#define XBREPLAY_DEBUG(...) /*NOLINT*/ l.debug( __FILE__,__func__, __LINE__, __VA_ARGS__)
#define XBREPLAY_INFO(...)  /*NOLINT*/ l.info( __FILE__,__func__,__LINE__, __VA_ARGS__)
#define XBREPLAY_WARN(...)  /*NOLINT*/ l.warning( __FILE__,__func__, __LINE__, __VA_ARGS__)
#define XBREPLAY_ERROR(...) /*NOLINT*/ l.error( __FILE__,__func__, __LINE__, __VA_ARGS__)

inline std::string hex_str(uint64_t number)
{
  std::stringstream ss;
  ss <<"0x"<< std::hex << number;
  return ss.str();
}
}

