/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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

/*
 * Performance Monitoring Internal Parameters
 * Date:   January 9, 2015
 * Author: Paul Schumacher
 *
 * NOTE: partially taken from file xaxipmon_hw.h in v5.0 of APM driver
 */

#ifndef _PERFMON_PARAMETERS_H_
#define _PERFMON_PARAMETERS_H_

/************************ AXI Stream FIFOs ************************************/

//#define AXI_FIFO_RDFD_AXI_FULL          0x1000

/************************ Accelerator Monitor (AM, earlier SAM) ************************/

//#define XAM_64BIT_PROPERTY_MASK        0x8

/************************** AXI Stream Monitor (ASM, earlier SSPM) *********************/
/* SSPM Control Mask */
//#define XASM_COUNTER_RESET_MASK       0x00000001

/******************* Accelerator Deadlock Detector ***********************/

/* Address offset */
//#define XACCEL_DEADLOCK_STATUS_OFFSET    0x0

#endif /* _PERFMON_PARAMETERS_H_ */

