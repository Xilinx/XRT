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
// AXI Interface Monitor cores that may exist in the design.
// These values are shared between xbutil and the XDP library.

#ifndef AIM_DOT_H
#define AIM_DOT_H

namespace xdp::IP::AIM {

// The total number of 64-bit counters accessible on the IP
constexpr int NUM_COUNTERS = 13;
// The number of 64-bit counters displayed (as when accessed via xbutil)
constexpr int NUM_COUNTERS_REPORT = 9;

namespace AXI_LITE {
// These are the actual physical offsets of the 32-bit registers in
// the AIM IP accessible over the AXI-Lite connection.  If using 
// xclRead or xclWrite, these offsets are used.  The upper 32-bits
// of each register require a separate offset and access.
constexpr unsigned int CONTROL                  = 0x08;
constexpr unsigned int TRACE_CTRL               = 0x10;
constexpr unsigned int SAMPLE                   = 0x20; // Capture current values
constexpr unsigned int WRITE_BYTES              = 0x80;
constexpr unsigned int WRITE_TRANX              = 0x84;
constexpr unsigned int WRITE_LATENCY            = 0x88;
constexpr unsigned int READ_BYTES               = 0x8C;
constexpr unsigned int READ_TRANX               = 0x90;
constexpr unsigned int READ_LATENCY             = 0x94;
constexpr unsigned int MIN_MAX_WRITE_LATENCY    = 0x98; // Unused
constexpr unsigned int MIN_MAX_READ_LATENCY     = 0x9C; // Unused
constexpr unsigned int OUTSTANDING_COUNTS       = 0xA0;
constexpr unsigned int LAST_WRITE_ADDRESS       = 0xA4;
constexpr unsigned int LAST_WRITE_DATA          = 0xA8;
constexpr unsigned int LAST_READ_ADDRESS        = 0xAC;
constexpr unsigned int LAST_READ_DATA           = 0xB0;
constexpr unsigned int READ_BUSY_CYCLES         = 0xB4;
constexpr unsigned int WRITE_BUSY_CYCLES        = 0xB8;
constexpr unsigned int WRITE_BYTES_UPPER        = 0xC0;
constexpr unsigned int WRITE_TRANX_UPPER        = 0xC4;
constexpr unsigned int WRITE_LATENCY_UPPER      = 0xC8;
constexpr unsigned int READ_BYTES_UPPER         = 0xCC;
constexpr unsigned int READ_TRANX_UPPER         = 0xD0;
constexpr unsigned int READ_LATENCY_UPPER       = 0xD4;
// Reserved for high 32-bits of MIN_MAX_WRITE_LATENCY - 0xD8
// Reserved for high 32-bits of MIN_MAX_READ_LATENCY - 0xDC
constexpr unsigned int OUTSTANDING_COUNTS_UPPER = 0xE0;
constexpr unsigned int LAST_WRITE_ADDRESS_UPPER = 0xE4;
constexpr unsigned int LAST_WRITE_DATA_UPPER    = 0xE8;
constexpr unsigned int LAST_READ_ADDRESS_UPPER  = 0xEC;
constexpr unsigned int LAST_READ_DATA_UPPER     = 0xF0;
constexpr unsigned int READ_BUSY_CYCLES_UPPER   = 0xF4;
constexpr unsigned int WRITE_BUSY_CYCLES_UPPER  = 0xF8;
} // end namespace AXI_LITE

namespace sysfs {
// When accessing the IP via sysfs, an array of 64-bit words is
// put together and returned.  These numbers are the array offsets
// to access the specific registers returned.
// sysfs will return the 64-bit value so we don't need to
// separately access the upper bytes like we do if using
// xclRead/xclWrite
constexpr int WRITE_BYTES        = 0 ;
constexpr int WRITE_TRANX        = 1;
constexpr int WRITE_LATENCY      = 2;
constexpr int WRITE_BUSY_CYCLES  = 3;
constexpr int READ_BYTES         = 4;
constexpr int READ_TRANX         = 5;
constexpr int READ_LATENCY       = 6;
constexpr int READ_BUSY_CYCLES   = 7;
constexpr int OUTSTANDING_COUNT  = 8;
constexpr int WRITE_LAST_ADDRESS = 9;
constexpr int WRITE_LAST_DATA    = 10;
constexpr int READ_LAST_ADDRESS  = 11;
constexpr int READ_LAST_DATA     = 12;
} // end namespace sysfs

namespace report {
// When we are reporting the status of this IP, we
// strip away the write latency, write busy, read latency, and 
// read busy cycle information.  The numbers here represent the
// index into the array used by xbutil and XDP to access the register value
constexpr int WRITE_BYTES        = 0;
constexpr int WRITE_TRANX        = 1;
constexpr int READ_BYTES         = 2;
constexpr int READ_TRANX         = 3;
constexpr int OUTSTANDING_COUNT  = 4;
constexpr int WRITE_LAST_ADDRESS = 5;
constexpr int WRITE_LAST_DATA    = 6;
constexpr int READ_LAST_ADDRESS  = 7;
constexpr int READ_LAST_DATA     = 8;
} // end namespace report

namespace mask {
// These are masks on the property of the IP that give information
// on the specific instance.
constexpr unsigned int PROPERTY_HOST            = 0x4;
constexpr unsigned int PROPERTY_64BIT           = 0x8;
constexpr unsigned int PROPERTY_COARSE_MODE_OFF = 0x10;
constexpr unsigned int CR_COUNTER_RESET  = 0x00000002;
constexpr unsigned int CR_COUNTER_ENABLE = 0x00000001;
constexpr unsigned int TRACE_CTRL        = 0x00000003;
} // end namespace mask

} // end namespace xdp::IP::AIM

#endif
