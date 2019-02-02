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

#include "config_reader.h"
#include <unistd.h>
#include <syslog.h>
#include <map>
#include <fstream>
#include <iostream>

namespace {

using severity_level = xrt_core::message::severity_level;

//--
class message_dispatch
{
public:
  message_dispatch() {}
  virtual ~message_dispatch() {}
  static message_dispatch* make_dispatcher(const std::string& choice);
public:
  virtual void send(severity_level l, const char* msg) = 0;
};

//--
class null_dispatch : public message_dispatch
{
public:
  null_dispatch() {}
  virtual ~null_dispatch() {}
  virtual void send(severity_level l, const char* msg) {};
};

//--
class console_dispatch : public message_dispatch
{
public:
  console_dispatch() {}
  virtual ~console_dispatch() {}
  virtual void send(severity_level l, const char* msg) override;
private:
  std::map<severity_level, const char*> severityMap = {
    { severity_level::ALERT,     "ALERT: "},
    { severity_level::CRITICAL,  "CRITICAL: "},
    { severity_level::DEBUG,     "DEBUG: "},
    { severity_level::EMERGENCY, "EMERGENCY: "},
    { severity_level::ERROR,     "ERROR: "},
    { severity_level::INFO,      "INFO: "},
    { severity_level::INTERNAL,  "INTERNAL: "},
    { severity_level::NOTICE,    "NOTICE: "},
    { severity_level::WARNING,   "WARNING: "}
  };
};

//--
class syslog_dispatch : public message_dispatch
{
public:
  syslog_dispatch();
  virtual ~syslog_dispatch();
  virtual void send(severity_level l, const char* msg) override;
private:
  std::map<severity_level, int> severityMap = {
    { severity_level::ALERT,     LOG_ALERT},
    { severity_level::CRITICAL,  LOG_CRIT},
    { severity_level::DEBUG,     LOG_DEBUG},
    { severity_level::EMERGENCY, LOG_EMERG},
    { severity_level::ERROR,     LOG_ERR},
    { severity_level::INFO,      LOG_INFO},
    //{ severity_level::INTERNAL,  INTERNAL_NOPRI},
    { severity_level::NOTICE,    LOG_NOTICE},
    { severity_level::WARNING,   LOG_WARNING}
  };
};

//--
class file_dispatch : public message_dispatch
{
public:
  explicit
  file_dispatch(const std::string& file);
  virtual ~file_dispatch();
  virtual void send(severity_level l, const char* msg) override;
private:
  std::ofstream handle;
  std::map<severity_level, const char*> severityMap = {
    { severity_level::ALERT,     "ALERT: "},
    { severity_level::CRITICAL,  "CRITICAL: "},
    { severity_level::DEBUG,     "DEBUG: "},
    { severity_level::EMERGENCY, "EMERGENCY: "},
    { severity_level::ERROR,     "ERROR: "},
    { severity_level::INFO,      "INFO: "},
    { severity_level::INTERNAL,  "INTERNAL: "},
    { severity_level::NOTICE,    "NOTICE: "},
    { severity_level::WARNING,   "WARNING: "}
  };
};

//-------
message_dispatch*
message_dispatch::make_dispatcher(const std::string& choice)
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
syslog_dispatch::syslog_dispatch() {
  openlog("sdaccel", LOG_PID|LOG_CONS, LOG_USER);
}

syslog_dispatch::~syslog_dispatch() {
  closelog();
}

void
syslog_dispatch::send(severity_level l, const char* msg) {
  syslog(severityMap[l], "%s", msg);
}

//file ops
file_dispatch::file_dispatch(const std::string &file) {
  handle.open(file.c_str());
}

file_dispatch::~file_dispatch() {
  handle.close();
}

void
file_dispatch::send(severity_level l, const char* msg) {
  handle << severityMap[l] << msg << std::endl;
}

//console ops
void
console_dispatch::send(severity_level l, const char* msg) {
  std::cout << severityMap[l]  << msg << std::endl;
}

} //end unnamed namespace

namespace xrt_core { namespace message {

void
send(severity_level l, const char* msg)
{
  static const std::string logger =  xrt_core::config::get_logging();
  static message_dispatch* dispatcher = message_dispatch::make_dispatcher(logger);
  dispatcher->send(l, msg);
}

}} // message,xrt
