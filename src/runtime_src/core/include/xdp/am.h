/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc
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
// Accelerator Monitor cores that may exist in the design.  These
// values are shared between xbutil and the XDP library.

#ifndef AM_DOT_H
#define AM_DOT_H

namespace xdp {
  namespace IP {
    namespace AM {
      // The total number of 64-bit counters accessible on the IP
      constexpr int NUM_COUNTERS = 10;
      // The number of 64-bit counters displayed when accessd via xbutil
      constexpr int NUM_COUNTERS_XBUTIL = 8;

      namespace AXI_LITE {
        // These are the actual physical offsets of the 32-bit registers in
        // the AM IP accessible over the AXI-Lite connection.  If using
        // xclRead or xclWrite, these offsets are used.  The upper 32-bits
        // of each register require a separate offset and access.
        constexpr int CONTROL                    = 0x08;
        constexpr int TRACE_CTRL                 = 0x10;
        constexpr int SAMPLE                     = 0x20;
        constexpr int EXECUTION_COUNT            = 0x80;
        constexpr int EXECUTION_CYCLES           = 0x84;
        constexpr int STALL_INT                  = 0x88;
        constexpr int STALL_STR                  = 0x8C;
        constexpr int STALL_EXT                  = 0x90;
        constexpr int MIN_EXECUTION_CYCLES       = 0x94;
        constexpr int MAX_EXECUTION_CYCLES       = 0x98;
        constexpr int TOTAL_CU_START             = 0x9C;
        constexpr int EXECUTION_COUNT_UPPER      = 0xA0;
        constexpr int EXECUTION_CYCLES_UPPER     = 0xA4;
        constexpr int STALL_INT_UPPER            = 0xA8;
        constexpr int STALL_STR_UPPER            = 0xAC;
        constexpr int STALL_EXT_UPPER            = 0xB0;
        constexpr int MIN_EXECUTION_CYCLES_UPPER = 0xB4;
        constexpr int MAX_EXECUTION_CYCLES_UPPER = 0xB8;
        constexpr int TOTAL_CU_START_UPPER       = 0xBC;
        constexpr int BUSY_CYCLES                = 0xC0;
        constexpr int BUSY_CYCLES_UPPER          = 0xC4;
        constexpr int MAX_PARALLEL_ITER          = 0xC8;
        constexpr int MAX_PARALLEL_ITER_UPPER    = 0xCC;
      } // end namespace AXI_LITE

      namespace sysfs {
        // When accessing the IP via sysfs, an array of 64-bit words is
        // put together and returned.  These numbers are the array offsets
        // to access the specific registers returned.
        // sysfs will return the 64-bit value so we don't need to
        // separately access the upper bytes like we do if using
        // xclRead/xclWrite
        constexpr int EXECUTION_COUNT      = 0 ;
        constexpr int TOTAL_CU_START       = 1 ;
        constexpr int EXECUTION_CYCLES     = 2;
        constexpr int STALL_INT            = 3;
        constexpr int STALL_STR            = 4;
        constexpr int STALL_EXT            = 5;
        constexpr int BUSY_CYCLES          = 6;
        constexpr int MAX_PARALLEL_ITER    = 7;
        constexpr int MAX_EXECUTION_CYCLES = 8;
        constexpr int MIN_EXECUTION_CYCLES = 9;
      } // end namespace sysfs

      namespace xbutil {
        // When we are reporting the status of this IP using xbutil,
        // we strip away the busy cycles and max parallel iter (and also
        // reorder the registers) since dataflow may not be enabled.
        constexpr int EXECUTION_COUNT      = 0;
        constexpr int EXECUTION_CYCLES     = 1;
        constexpr int STALL_INT            = 2;
        constexpr int STALL_STR            = 3;
        constexpr int STALL_EXT            = 4;
        constexpr int MIN_EXECUTION_CYCLES = 5;
        constexpr int MAX_EXECUTION_CYCLES = 6; 
        constexpr int TOTAL_CU_START       = 7;
      } // end namespace xbutil

    } // end namespace AM
  } // end namespace IP
} // end namespace xdp

#endif
