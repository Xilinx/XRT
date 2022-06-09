/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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

// This file captures all the constants used to access the
// AXI-Full FIFO used for PL trace if it is instantiated in the design.

#ifndef FIFO_DOT_H
#define FIFO_DOT_H

namespace xdp::IP::FIFO {

constexpr int alignment = 0x1000;

namespace AXI_LITE {
// On some devices (like edge) we cannot transfer values via
// the AXI-full connection.  Instead, we need to read the
// contents of the FIFO one-by-one over the AXI-Lite connection
constexpr int RDFD = 0x1000;
} // end namespace AXI_LITE

} // end namespace xdp::IP::FIFO

#endif
