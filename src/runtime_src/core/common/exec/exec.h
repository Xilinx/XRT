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
schedule(command* cmd);

void
start();

void
stop();

void
init(xrt_core::device* device);

void
init(xrt_core::device* device, const std::vector<uint64_t>& cu_addr_map);

} // sws

/**
 * Embedded command scheduling
 */
namespace kds {

void
schedule(command* cmd);

void
start();

void
stop();

void
init(xrt_core::device* device);

} // kds

/**
 * Pass-through scheduler
 */
namespace pts {

void
schedule(command* cmd);

void
start();

void
stop();

void
init(xrt_core::device* device);

} // pts

namespace exec {
/**
 * Schedule a command for execution on either sws or mbs
 */
void
schedule(command* cmd);

void
start();

void
stop();

void
init(xrt_core::device* device);

} // scheduler


} // xrt_core

#endif
