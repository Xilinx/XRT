// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// POSIX syslog implementation of syslog_dispatch.

#include "core/common/message.h"

#include <map>
#include <string>
#include <syslog.h>

namespace xrt_core::message {

class syslog_dispatch : public message_dispatch
{
public:
  syslog_dispatch()
  { openlog("XRT", LOG_PID|LOG_CONS, LOG_USER); }

  syslog_dispatch(const syslog_dispatch&) = delete;
  syslog_dispatch& operator=(const syslog_dispatch&) = delete;
  syslog_dispatch(syslog_dispatch&&) = delete;
  syslog_dispatch& operator=(syslog_dispatch&&) = delete;

  ~syslog_dispatch() override
  { closelog(); }

  void
  send(severity_level l, const char* tag, const char* msg) override
  {
    std::string full_msg = std::string("[") + tag + "] : " + msg;
    syslog(m_severity_map[l], "%s", full_msg.c_str());
  }

private:
  std::map<severity_level, int> m_severity_map = {
    { severity_level::emergency, LOG_EMERG   },
    { severity_level::alert,     LOG_ALERT   },
    { severity_level::critical,  LOG_CRIT    },
    { severity_level::error,     LOG_ERR     },
    { severity_level::warning,   LOG_WARNING },
    { severity_level::notice,    LOG_NOTICE  },
    { severity_level::info,      LOG_INFO    },
    { severity_level::debug,     LOG_DEBUG   }
  };
};

} // xrt_core::message
