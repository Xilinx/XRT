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

#ifndef xrt_scheduler_h_
#define xrt_scheduler_h_

#include "xrt/scheduler/command.h"
#include <vector>

namespace xrt {

using command_type = std::shared_ptr<command>;

/**
 * Software command scheduling
 */
namespace sws {

void
schedule(const command_type& cmd);

void
start();

void
stop();

void
init(xrt::device* device, const std::vector<uint32_t>& cu_addr_map);

} // sws

/**
 * Embedded command scheduling
 */
namespace kds {

void
schedule(const command_type& cmd);

void
start();

void
stop();

void
init(xrt::device* device, size_t slot_size, bool cu_isr, size_t num_cus, size_t cu_offset, size_t cu_base_addr, const std::vector<uint32_t>& cu_addr_map);

} // kds

namespace scheduler {
/**
 * Schedule a command for execution on either sws or mbs
 */
void
schedule(const command_type& cmd);

void
start();

void
stop();

void
init(xrt::device* device, size_t slot_size, bool cu_isr,size_t num_cus, size_t cu_offset, size_t cu_base_addr, const std::vector<uint32_t>& cu_addr_map);

} // scheduler


} // xrt

#endif
