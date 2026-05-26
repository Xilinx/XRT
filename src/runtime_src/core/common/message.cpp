// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "message.h"
#include "time.h"
#include "config_reader.h"
#include "utils.h"

#include "xrt/detail/version-git.h"

#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#ifdef __linux__
# include <syslog.h>
# include <linux/limits.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif
#ifdef _WIN32
# include <winsock.h>
// Windows Event Log API - required for syslog_dispatch
# include <windows.h>
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

// syslog_dispatch: routes to the OS-level centralized log on each platform.
// Linux   -> POSIX syslog
//
// Windows -> Windows Application Event Log under source "AMD_XRT"
// Filter in Event Viewer:
//   Windows Logs -> Application -> Source: AMD_XRT
// Filter via PowerShell:
//   Get-EventLog -LogName Application -Source "AMD_XRT"                        -- all
//   Get-EventLog -LogName Application -Source "AMD_XRT" -EntryType Error       -- errors only
//   Get-EventLog -LogName Application -Source "AMD_XRT" -EntryType Error,Warning
// Filter via cmd line (Level: 2=Error, 3=Warning, 4=Information):
//   wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT']]]" /f:text
//   wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT'] and Level=2]]" /f:text
//   wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT'] and (Level=2 or Level=3)]]" /f:text
#ifdef _WIN32
class syslog_dispatch : public message_dispatch
{
public:
  syslog_dispatch()
  {
    // RegisterEventSourceA opens a handle to the Application event log and
    // associates it with the source name "AMD_XRT". This handle is used in
    // every subsequent ReportEventA call to stamp events with that source name.
    // First arg (nullptr) means the local machine; a UNC server name can be
    // passed to write to a remote machine's event log.
    // This call does NOT require admin rights and does NOT touch the registry —
    // it only opens a write channel. Registry registration of "AMD_XRT" as a
    // known source is done separately at driver install time via the INF AddReg
    // directive
    m_handle = RegisterEventSourceA(nullptr, "AMD_XRT");
    // Do not throw on failure — syslog_dispatch is constructed lazily on the
    // first call to xrt_core::message::send(), so a throw here would crash the
    // application during normal logging if the Windows Event Log service is
    // unavailable.
    if (!m_handle)
      std::cerr << "XRT: Failed to open Windows Event Log source 'AMD_XRT' "
                << "(error " << GetLastError() << "). Logging disabled.\n";
  }

  virtual ~syslog_dispatch()
  {
    if (m_handle)
      DeregisterEventSource(m_handle);
  }

  void
  send(severity_level l, const char* tag, const char* msg) override
  {
    if (!m_handle)
      return; // Logging is unavailable if we failed to open the event source

    // Combine tag and message so Event Viewer shows "[xrt_elf] : some message"
    std::string full_msg = std::string("[") + tag + "] : " + msg;
    LPCSTR strings[] = {full_msg.c_str()};
    // Event ID 1 matches the pass-through entry in EventCreate.exe's message table
    // (%1 format string), so Event Viewer displays our message text directly
    // without the warning.
    // Severity filtering in Event Viewer uses wType (Level column), not event ID.
    static constexpr DWORD event_id = 1;
    ReportEventA(m_handle, to_event_type(l), 0, event_id,
                 nullptr, 1, 0, strings, nullptr);
  }

private:
  HANDLE m_handle = nullptr;

  // Maps XRT severity to Windows event type (controls icon in Event Viewer):
  //   EVENTLOG_ERROR_TYPE       -> red X
  //   EVENTLOG_WARNING_TYPE     -> yellow triangle
  //   EVENTLOG_INFORMATION_TYPE -> blue i
  static WORD
  to_event_type(severity_level l)
  {
    switch (l) {
    case severity_level::emergency:
    case severity_level::alert:
    case severity_level::critical:
    case severity_level::error:
      return EVENTLOG_ERROR_TYPE;
    case severity_level::warning:
      return EVENTLOG_WARNING_TYPE;
    default:
      return EVENTLOG_INFORMATION_TYPE;
    }
  }

};
#else
class syslog_dispatch : public message_dispatch
{
public:
  syslog_dispatch()
  { openlog("sdaccel", LOG_PID|LOG_CONS, LOG_USER); }

  virtual ~syslog_dispatch()
  { closelog(); }

  void
  send(severity_level l, const char* tag, const char* msg) override
  { syslog(severityMap[l], "%s", msg); }

private:
  // Maps XRT severity to POSIX syslog priority
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
  else if(choice == "syslog")
    return new syslog_dispatch;
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
  handle << "Build hash date: " << xrt_build_version_hash_date << "\n";
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
  std::cerr << "Build hash date: " << xrt_build_version_hash_date << "\n";
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
