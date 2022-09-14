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
// System-level PL Accelerator Deadlock Detection IP (ADD).

#ifndef ADD_DOT_H
#define ADD_DOT_H

namespace xdp::IP::ADD {

namespace AXI_LITE {
// These are the actual physical offsets of the 32-bit registers in
// the ADD IP accessible over the AXI-Lite connection.  If using
// xclRead or xclWrite, these offsets are used.
constexpr unsigned int STATUS = 0x0;

} // end namespace AXI_LITE

} // end namespace xdp::IP::ADD

#endif
