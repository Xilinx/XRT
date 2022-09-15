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

#include <array>

namespace xdp::IP::FIFO {

constexpr int alignment = 0x1000;

namespace AXI_LITE {
// On some devices (like edge) we cannot transfer values via
// the AXI-full connection.  Instead, we need to read the
// contents of the FIFO one-by-one over the AXI-Lite connection
constexpr unsigned int RDFD = 0x1000;
} // end namespace AXI_LITE

// IP and V++ specific
namespace properties {
    constexpr unsigned int SIZE_1K   = 1024;
    constexpr unsigned int SIZE_2K   = 2048;
    constexpr unsigned int SIZE_4K   = 4096;
    constexpr unsigned int SIZE_8K   = 8192;
    constexpr unsigned int SIZE_16K  = 16384;
    constexpr unsigned int SIZE_32K  = 32768;
    constexpr unsigned int SIZE_64K  = 65536;
    constexpr unsigned int SIZE_128K = 131072;
    // Property to size map
    // Property 0 represents 8k FIFO
    const std::array<unsigned int, 8> size = {
        SIZE_8K,
        SIZE_1K,
        SIZE_2K,
        SIZE_4K,
        SIZE_16K,
        SIZE_32K,
        SIZE_64K,
        SIZE_128K
    };
}

} // end namespace xdp::IP::FIFO

#endif
