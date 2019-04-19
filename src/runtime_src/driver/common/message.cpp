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

#include "message.h"
#include "t_time.h"
#include "version.h"
#include "config_reader.h"

#include <unistd.h>
#include <syslog.h>
#include <map>
#include <fstream>
#include <iostream>
#include <thread>
#include <climits>
#include <sys/types.h>
#ifdef __GNUC__
# include <linux/limits.h>
# include <sys/stat.h>
#endif

namespace {

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
  virtual void send(severity_level l, const char* tag, const char* msg) {};
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
    { severity_level::EMERGENCY, "EMERGENCY: "},
    { severity_level::ALERT,     "ALERT: "},
    { severity_level::CRITICAL,  "CRITICAL: "},
    { severity_level::ERROR,     "ERROR: "},
    { severity_level::WARNING,   "WARNING: "},
    { severity_level::NOTICE,    "NOTICE: "},
    { severity_level::INFO,      "INFO: "},
    { severity_level::DEBUG,     "DEBUG: "}
  };
};

//--
class syslog_dispatch : public message_dispatch
{
public:
  syslog_dispatch();
  virtual ~syslog_dispatch();
  virtual void send(severity_level l, const char* tag, const char* msg) override;
private:
  std::map<severity_level, int> severityMap = {
    { severity_level::EMERGENCY, LOG_EMERG},
    { severity_level::ALERT,     LOG_ALERT},
    { severity_level::CRITICAL,  LOG_CRIT},
    { severity_level::ERROR,     LOG_ERR},
    { severity_level::WARNING,   LOG_WARNING},
    { severity_level::NOTICE,    LOG_NOTICE},
    { severity_level::INFO,      LOG_INFO},
    { severity_level::DEBUG,     LOG_DEBUG}
  };
};

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
    { severity_level::EMERGENCY, "EMERGENCY: "},
    { severity_level::ALERT,     "ALERT: "},
    { severity_level::CRITICAL,  "CRITICAL: "},
    { severity_level::ERROR,     "ERROR: "},
    { severity_level::WARNING,   "WARNING: "},
    { severity_level::NOTICE,    "NOTICE: "},
    { severity_level::INFO,      "INFO: "},
    { severity_level::DEBUG,     "DEBUG: "}
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
  else if(choice == "syslog")
    return new syslog_dispatch;
  else {
    if(choice.front() == '"'){
      std::string file = choice;
      file.erase(0, 1);
      file.erase(file.size()-1);
      return new file_dispatch(file);
    }else
      return new file_dispatch(choice);
  }
  return nullptr;
}

//syslog ops.
syslog_dispatch::
syslog_dispatch()
{
  openlog("sdaccel", LOG_PID|LOG_CONS, LOG_USER);
}

syslog_dispatch::
~syslog_dispatch()
{
  closelog();
}

void
syslog_dispatch::
send(severity_level l, const char* tag, const char* msg)
{
  syslog(severityMap[l], "%s", msg);
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
  handle << "PID: " << getpid() << std::endl;
  handle << "UID: " << getuid() << std::endl;
  //hostname
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  handle << "HOST: " <<  hostname << std::endl;
  handle << "EXE: " <<get_exe_path() << std::endl;
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
  std::cout << "PID: " << getpid() << std::endl;
  std::cout << "UID: " << getuid() << std::endl;
  std::cout << xrt_core::timestamp() << std::endl;
  //hostname
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  std::cout << "HOST: " <<  hostname << std::endl;
  std::cout << "EXE: " <<get_exe_path() << std::endl;
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
