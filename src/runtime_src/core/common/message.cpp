/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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
#include "t_time.h"
#include "gen/version.h"
#include "config_reader.h"

#include <map>
#include <fstream>
#include <iostream>
#include <thread>
#include <climits>
#ifdef __GNUC__
# include <unistd.h>
# include <syslog.h>
# include <linux/limits.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif
#ifdef _WIN32
# include <process.h>
# include <winsock.h>
#endif

namespace {

static int
get_processid()
{
#ifdef _WIN32
  return _getpid();
#else
  return getpid();
#endif
}

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
get_hostname()
{
  std::string hn;
#ifdef __GNUC__
  char hostname[256] = {0};
  gethostname(hostname, 256);
  hn = hostname;
#endif
  return hn;
}

static std::string
get_exe_path()
{
#ifdef __GNUC__
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
    { severity_level::XRT_EMERGENCY, "EMERGENCY: "},
    { severity_level::XRT_ALERT,     "ALERT: "},
    { severity_level::XRT_CRITICAL,  "CRITICAL: "},
    { severity_level::XRT_ERROR,     "ERROR: "},
    { severity_level::XRT_WARNING,   "WARNING: "},
    { severity_level::XRT_NOTICE,    "NOTICE: "},
    { severity_level::XRT_INFO,      "INFO: "},
    { severity_level::XRT_DEBUG,     "DEBUG: "}
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
    { severity_level::XRT_EMERGENCY, LOG_EMERG},
    { severity_level::XRT_ALERT,     LOG_ALERT},
    { severity_level::XRT_CRITICAL,  LOG_CRIT},
    { severity_level::XRT_ERROR,     LOG_ERR},
    { severity_level::XRT_WARNING,   LOG_WARNING},
    { severity_level::XRT_NOTICE,    LOG_NOTICE},
    { severity_level::XRT_INFO,      LOG_INFO},
    { severity_level::XRT_DEBUG,     LOG_DEBUG}
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
    { severity_level::XRT_EMERGENCY, "EMERGENCY: "},
    { severity_level::XRT_ALERT,     "ALERT: "},
    { severity_level::XRT_CRITICAL,  "CRITICAL: "},
    { severity_level::XRT_ERROR,     "ERROR: "},
    { severity_level::XRT_WARNING,   "WARNING: "},
    { severity_level::XRT_NOTICE,    "NOTICE: "},
    { severity_level::XRT_INFO,      "INFO: "},
    { severity_level::XRT_DEBUG,     "DEBUG: "}
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
  handle << "XRT build version: " << xrt_build_version << std::endl;
  handle << "Build hash: " << xrt_build_version_hash << std::endl;
  handle << "Build date: " << xrt_build_version_date << std::endl;
  handle << "Git branch: " << xrt_build_version_branch<< std::endl;
  handle << xrt_core::timestamp() << std::endl;
  handle << "PID: " << get_processid() << std::endl;
  handle << "UID: " << get_userid() << std::endl;
  handle << "HOST: " <<  get_hostname() << std::endl;
  handle << "EXE: " << get_exe_path() << std::endl;
}

file_dispatch::~file_dispatch() {
  handle.close();
}

void
file_dispatch::
send(severity_level l, const char* tag, const char* msg)
{
  handle << xrt_core::timestamp() <<" [" << tag << "] Tid: "
         << std::this_thread::get_id() << ", " << " " << severityMap[l]
         << msg << std::endl;
}

//console ops
console_dispatch::
console_dispatch()
{
  std::cout << "XRT build version: " << xrt_build_version << std::endl;
  std::cout << "Build hash: " << xrt_build_version_hash << std::endl;
  std::cout << "Build date: " << xrt_build_version_date << std::endl;
  std::cout << "Git branch: " << xrt_build_version_branch<< std::endl;
  std::cout << "PID: " << get_processid() << std::endl;
  std::cout << "UID: " << get_userid() << std::endl;
  std::cout << xrt_core::timestamp() << std::endl;
  std::cout << "HOST: " << get_hostname() << std::endl;
  std::cout << "EXE: " << get_exe_path() << std::endl;
}

void
console_dispatch::
send(severity_level l, const char* tag, const char* msg)
{
  std::cout << "[" << tag << "] " << severityMap[l]
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
  
}} // message,xrt
