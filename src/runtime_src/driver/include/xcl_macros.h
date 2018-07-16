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

// Copyright 2014 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.
//
#ifndef __XCL_MACROS__
#define __XCL_MACROS__

#ifdef __cplusplus
#include <cstdlib>
#include <cstdint>
#else
#include <stdlib.h>
#include <stdint.h>
#endif

#define xclOpen_n 0
#define xclClose_n 1
#define xclGetDeviceInfo_n 2
#define xclLoadBitstream_n 3
#define xclAllocDeviceBuffer_n 4
#define xclFreeDeviceBuffer_n 5
#define xclCopyBufferHost2Device_n 6
#define xclCopyBufferDevice2Host_n 7
#define xclWriteAddrSpaceDeviceRam_n 8
#define xclWriteAddrKernelCtrl_n 9
#define xclReadAddrSpaceDeviceRam_n 10
#define xclReadAddrKernelCtrl_n 11
#define xclUpgradeFirmware_n 12
#define xclBootFPGA_n 13
#define xclPerfMonReadCounters_n 14
#define xclPerfMonGetTraceCount_n 15
#define xclPerfMonReadTrace_n 16
#define xclGetDeviceTimestamp_n 17
#define xclReadBusStatus_n 18
#define xclGetDebugMessages_n 19
#define xclSetEnvironment_n 20
#define xclWriteHostEvent_n 21

#endif
 
