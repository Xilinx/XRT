// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// Windows Event Log implementation of syslog_dispatch.
// Routes XRT messages to the Application event log under source "AMD_XRT".
//
// Filter in Event Viewer:
//   Windows Logs -> Application -> Source: AMD_XRT
// Filter via PowerShell:
//   Get-EventLog -LogName Application -Source "AMD_XRT"
//   Get-EventLog -LogName Application -Source "AMD_XRT" -EntryType Error
//   Get-EventLog -LogName Application -Source "AMD_XRT" -EntryType Error,Warning
// Filter via cmd (Level: 2=Error, 3=Warning, 4=Information):
//   wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT']]]" /f:text
//   wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT'] and Level=2]]" /f:text
//   wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT'] and (Level=2 or Level=3)]]" /f:text

#include "core/common/message.h"

#include <iostream>
#include <string>
#include <windows.h>

namespace xrt_core { namespace message {

class syslog_dispatch : public message_dispatch
{
public:
  syslog_dispatch()
  {
    // RegisterEventSourceA opens a handle to the Application event log and
    // associates it with the source name "AMD_XRT".  Registry registration of
    // "AMD_XRT" as a known source is done at driver install time via the INF
    // AddReg directive.  This call does not require admin rights.
    m_handle = RegisterEventSourceA(nullptr, "AMD_XRT");
    // Do not throw on failure: this is constructed lazily on first
    // xrt_core::message::send(), so a throw here would crash the application
    // if the Windows Event Log service is unavailable.
    if (!m_handle)
      std::cerr << "XRT: Failed to open Windows Event Log source 'AMD_XRT' "
                << "(error " << GetLastError() << "). Logging disabled.\n";
  }

  ~syslog_dispatch()
  {
    if (m_handle)
      DeregisterEventSource(m_handle);
  }

  void
  send(severity_level l, const char* tag, const char* msg) override
  {
    if (!m_handle)
      return;

    std::string full_msg = std::string("[") + tag + "] : " + msg;
    LPCSTR strings[] = {full_msg.c_str()};
    // Event ID 1 matches the pass-through entry in EventCreate.exe's message
    // table (%1), so Event Viewer displays our text directly without a warning.
    // Severity filtering uses wType (Level column), not event ID.
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

}} // xrt_core::message
