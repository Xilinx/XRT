// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
#ifndef xrt_core_exec_h_
#define xrt_core_exec_h_

#include "core/common/config.h"
#include <condition_variable>
#include <memory>
#include <vector>

namespace xrt_core {

class device;
class command;

/**
 * Software command scheduling
 */
namespace sws {

void
managed_start(command* cmd);

inline void
unmanaged_start(command* cmd)
{
  managed_start(cmd);
}

void
unmanaged_wait(const command* cmd);

inline std::cv_status
unmanaged_wait(const command* cmd, const std::chrono::milliseconds&)
{
  unmanaged_wait(cmd);
  return std::cv_status::no_timeout;
}

std::cv_status
exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms);

void
start();

void
stop();

void
init(xrt_core::device* device);

} // sws

/**
 * Embedded command scheduling
 */
namespace kds {

void
managed_start(command* cmd);

void
unmanaged_start(command* cmd);

void
unmanaged_wait(const command* cmd);

std::cv_status
unmanaged_wait(const command* cmd, const std::chrono::milliseconds& timeout_ms);

std::cv_status
exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms);

void
start();

void
stop();

void
init(xrt_core::device* device);

} // kds

namespace exec {

// Schedule a command for execution on either sws or kds. Use push
// execution, meaning host will be notified of command completion This
// function start / schedules the argument command for execution and
// manages completion using execution monitor
void
managed_start(command* cmd);

// Schedule a command for execution on either sws or kds. Use poll
// execution, meaning host must explicitly call unmanaged_wait() to
// wait for command completion.  This function starts / schedules
// argument command for exectution but doesn't manage completion.  The
// command must be checked for completion manually.
XRT_CORE_COMMON_EXPORT
void
unmanaged_start(command* cmd);

// Wait for a command to complete execution.  This function must be
// called in poll mode (unmanaged) scheduling, and is safe to call in
// push mode.  The function provides a thread safe interface to
// exec_wait and by passes execution monitor used in managed execution
XRT_CORE_COMMON_EXPORT
void
unmanaged_wait(const command* cmd);

// Wait for a command to complete execution with a timeout.  This
// function must be called in poll mode (unmanaged) scheduling, and is
// safe to call in push mode.  The function provides a thread safe
// interface to exec_wait and by passes execution monitor used in
// managed execution
XRT_CORE_COMMON_EXPORT
std::cv_status
unmanaged_wait(const command* cmd, const std::chrono::milliseconds& timeout_ms);

// Wait for one call to exec_wait to return either from
// some command completing or from a timeout.
XRT_CORE_COMMON_EXPORT
std::cv_status
exec_wait(const xrt_core::device* device, const std::chrono::milliseconds& timeout_ms);

void
start();

XRT_CORE_COMMON_EXPORT
void
stop();

XRT_CORE_COMMON_EXPORT
void
init(xrt_core::device* device);

} // scheduler


} // xrt_core

#endif
