/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef xrt_core_exec_h_
#define xrt_core_exec_h_

#include "core/common/config.h"
#include <vector>
#include <memory>

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
// exec_waq and by passes execution monitor used in managed execution
XRT_CORE_COMMON_EXPORT
void
unmanaged_wait(const command* cmd);

void
start();

void
stop();

XRT_CORE_COMMON_EXPORT
void
init(xrt_core::device* device);

} // scheduler


} // xrt_core

#endif
