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
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common

#include "exec.h"
#include "command.h"
#include "core/common/device.h"

// Pass through scheduling
// Host manages execution
namespace xrt_core { namespace pts {

void
schedule(xrt_core::command* cmd)
{
  auto device = cmd->get_device();
  device->exec_buf(cmd->get_exec_bo());
}

void
start()
{
}

void
stop()
{
}

void
init(xrt_core::device*)
{
}

}} // kds,xrt_core
