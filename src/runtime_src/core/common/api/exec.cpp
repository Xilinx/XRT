// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common

#include "exec.h"
#include "core/common/config_reader.h"
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
# pragma warning( disable : 4996 )
#endif

namespace {

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swem;
}

inline bool
kds_enabled(bool forceoff=false)
{
  static bool iskdsemu = is_sw_emulation() ? (xrt_core::config::get_flag_kds_sw_emu() ? true : false) : true;
  static bool enabled = iskdsemu
    &&  xrt_core::config::get_kds()
    && !xrt_core::config::get_feature_toggle("Runtime.sws");

  if (forceoff)
    enabled = false;

  return enabled;
}

}

namespace xrt_core {  namespace exec {

void
start()
{
  if (kds_enabled())
    kds::start();
  else
    sws::start();
}

void
stop()
{
  if (kds_enabled())
    kds::stop();
  else
    sws::stop();
}


// Schedule a command for execution on either sws or kds. Use push
// execution, meaning host will be notified of command completion
void
managed_start(command* cmd)
{
  if (kds_enabled())
    kds::managed_start(cmd);
  else
    sws::managed_start(cmd);
}

// Schedule a command for execution on either sws or kds. Use poll
// execution, meaning host must explicitly call unmanaged_wait() to
// wait for command completion
void
unmanaged_start(command* cmd)
{
  if (kds_enabled())
    kds::unmanaged_start(cmd);
  else
    sws::unmanaged_start(cmd);
}

// Wait for a command to complete execution.  This function must be
// called in poll mode scheduling, and is safe to call in push mode.
void
unmanaged_wait(const command* cmd)
{
  if (kds_enabled())
    kds::unmanaged_wait(cmd);
  else
    sws::unmanaged_wait(cmd);
}

// Wait for a command to complete execution with timeout.
std::cv_status
unmanaged_wait(const command* cmd, const std::chrono::milliseconds& timeout_ms)
{
  if (kds_enabled())
    return kds::unmanaged_wait(cmd, timeout_ms);
  else
    return sws::unmanaged_wait(cmd, timeout_ms);
}

std::cv_status
exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms)
{
  if (kds_enabled())
    return kds::exec_wait(device, timeout_ms);
  else
    return sws::exec_wait(device, timeout_ms);
}

void
init(xrt_core::device* device)
{
  struct X {
    ~X() { try { stop(); } catch (...) { } } // coverity
  };
  static X x;

  static bool started = false;
  if (!started) {
    start();
    started = true;
  }

  if (kds_enabled())
    kds::init(device);
  else
    sws::init(device);
}

}} // exec, xrt_core
