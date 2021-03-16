// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file defines shim level XRT AIE APIs.

#ifndef _XCL_COMMON_AIE_H_
#define _XCL_COMMON_AIE_H_

int
xclReadAieReg(xclDeviceHandle handle, int row, int col, const char* regName, uint32_t* value);

#endif
