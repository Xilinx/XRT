/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#define XRT_CORE_COMMON_SOURCE
#include "message.h"
#include "time.h"
#include "gen/version.h"
#include "config_reader.h"
#include "utils.h"

#include <map>
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstdarg>
#include <climits>
#ifdef __linux__
# include <syslog.h>
# include <linux/limits.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif
#ifdef _WIN32
# include <winsock.h>
#endif

namespace {

static unsigned int
get_userid()
{
#ifdef _WIN32
  return 0;
#else
  return getuid();
#endif
}

static std::string
get_exe_path()
{
#ifdef __linux__
  char buf[PATH_MAX] = {0};
  auto len = ::readlink("/proc/self/exe", buf, PATH_MAX);
  return std::string(buf, (len>0) ? len : 0);
#else
  return "";
#endif
}


using severity_level = xrt_core::message::severity_level;

//--
class message_dispatch
{
public:
  message_dispatch() {}
  virtual ~message_dispatch() {}
  static message_dispatch* make_dispatcher(const std::string& choice);
public:
  virtual void send(severity_level l, const char* tag, const char* msg) = 0;
};

//--
class null_dispatch : public message_dispatch
{
public:
  null_dispatch() {}
  virtual ~null_dispatch() {}
  virtual void send(severity_level, const char*, const char*) {};
};

//--
class console_dispatch : public message_dispatch
{
public:
  console_dispatch();
  virtual ~console_dispatch() {}
  virtual void send(severity_level l, const char* tag, const char* msg) override;
private:
  std::map<severity_level, const char*> severityMap = {
    { severity_level::emergency, "EMERGENCY: "},
    { severity_level::alert,     "ALERT: "},
    { severity_level::critical,  "CRITICAL: "},
    { severity_level::error,     "ERROR: "},
    { severity_level::warning,   "WARNING: "},
    { severity_level::notice,    "NOTICE: "},
    { severity_level::info,      "INFO: "},
    { severity_level::debug,     "DEBUG: "}
  };
};

//--
#ifndef _WIN32
class syslog_dispatch : public message_dispatch
{
public:
  syslog_dispatch()
  { openlog("sdaccel", LOG_PID|LOG_CONS, LOG_USER); }

  virtual ~syslog_dispatch()
  { closelog(); }

  virtual void send(severity_level l, const char* tag, const char* msg) override
  { syslog(severityMap[l], "%s", msg); }

private:
  std::map<severity_level, int> severityMap = {
    { severity_level::emergency, LOG_EMERG},
    { severity_level::alert,     LOG_ALERT},
    { severity_level::critical,  LOG_CRIT},
    { severity_level::error,     LOG_ERR},
    { severity_level::warning,   LOG_WARNING},
    { severity_level::notice,    LOG_NOTICE},
    { severity_level::info,      LOG_INFO},
    { severity_level::debug,     LOG_DEBUG}
  };
};
#endif

//--
class file_dispatch : public message_dispatch
{
public:
  explicit
  file_dispatch(const std::string& file);
  virtual ~file_dispatch();
  virtual void send(severity_level l, const char* tag, const char* msg) override;
private:
  std::ofstream handle;
  std::map<severity_level, const char*> severityMap = {
    { severity_level::emergency, "EMERGENCY: "},
    { severity_level::alert,     "ALERT: "},
    { severity_level::critical,  "CRITICAL: "},
    { severity_level::error,     "ERROR: "},
    { severity_level::warning,   "WARNING: "},
    { severity_level::notice,    "NOTICE: "},
    { severity_level::info,      "INFO: "},
    { severity_level::debug,     "DEBUG: "}
  };
};

//-------
message_dispatch*
message_dispatch::
make_dispatcher(const std::string& choice)
{
  if( (choice == "null") || (choice == ""))
    return new null_dispatch;
  else if(choice == "console")
    return new console_dispatch;
  else if(choice == "syslog") {
#ifndef _WIN32
    return new syslog_dispatch;
#else
    throw std::runtime_error("syslog not supported on windows");
#endif
  }
  else {
    if(choice.front() == '"') {
      std::string file = choice;
      file.erase(0, 1);
      file.erase(file.size()-1);
      return new file_dispatch(file);
    }
    else
      return new file_dispatch(choice);
  }
}

//file ops
file_dispatch::
file_dispatch(const std::string &file)
{
  handle.open(file.c_str());
  handle << "XRT build version: " << xrt_build_version << "\n";
  handle << "Build hash: " << xrt_build_version_hash << "\n";
  handle << "Build date: " << xrt_build_version_date << "\n";
  handle << "Git branch: " << xrt_build_version_branch<< "\n";
  handle << "[" << xrt_core::timestamp() << "]" << "\n";
  handle << "PID: " << xrt_core::utils::get_pid() << "\n";
  handle << "UID: " << get_userid() << "\n";
  handle << "HOST: " <<  xrt_core::utils::get_hostname() << "\n";
  handle << "EXE: " << get_exe_path() << std::endl;
}

file_dispatch::~file_dispatch() {
  handle.close();
}

void
file_dispatch::
send(severity_level l, const char* tag, const char* msg)
{
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  handle << "[" << xrt_core::timestamp() <<"] [" << tag << "] Tid: "
         << std::this_thread::get_id() << ", " << " " << severityMap[l]
         << msg << std::endl;
}

//console ops
console_dispatch::
console_dispatch()
{
  std::cerr << "XRT build version: " << xrt_build_version << "\n";
  std::cerr << "Build hash: " << xrt_build_version_hash << "\n";
  std::cerr << "Build date: " << xrt_build_version_date << "\n";
  std::cerr << "Git branch: " << xrt_build_version_branch<< "\n";
  std::cerr << "PID: " << xrt_core::utils::get_pid() << "\n";
  std::cerr << "UID: " << get_userid() << "\n";
  std::cerr << "[" << xrt_core::timestamp() << "]\n";
  std::cerr << "HOST: " << xrt_core::utils::get_hostname() << "\n";
  std::cerr << "EXE: " << get_exe_path() << std::endl;
}

void
console_dispatch::
send(severity_level l, const char* tag, const char* msg)
{
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  std::cerr << "[" << tag << "] " << severityMap[l]
            << msg << std::endl;
}

} //end unnamed namespace

namespace xrt_core { namespace message {

void
send(severity_level l, const char* tag, const char* msg)
{
  static const std::string logger =  xrt_core::config::get_logging();
  int ver = xrt_core::config::get_verbosity();
  int lev = static_cast<int>(l);

  if(ver >= lev) {
    static message_dispatch* dispatcher = message_dispatch::make_dispatcher(logger);
    dispatcher->send(l, tag, msg);
  }
}

void
sendv(severity_level l, const char* tag, const char* format, va_list args)
{
  static auto verbosity = xrt_core::config::get_verbosity();
  if (l > (xrt_core::message::severity_level)verbosity)
    return;

  va_list args_bak;
  // vsnprintf will mutate va_list so back it up
  va_copy(args_bak, args);
  int len = std::vsnprintf(nullptr, 0, format, args_bak);
  va_end(args_bak);
  if (len <= 0) {
    //illegal arguments
    std::string err_str = "ERROR: Illegal arguments or invalid format string. Format string is: ";
    err_str.append(format);
    send(l, tag, err_str);
    return;
  }
  ++len; //To include null terminator
  std::vector<char> buf(len, 0);
  std::ignore = std::vsnprintf(buf.data(), len, format, args);
  send(l, tag, buf.data());
}

}} // message,xrt
