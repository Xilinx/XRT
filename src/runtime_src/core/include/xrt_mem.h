/*
 * Copyright (C) 2019, Xilinx Inc - All rights reserved.
 * Xilinx Runtime (XRT) APIs
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

#ifndef _XRT_MEM_H_
#define _XRT_MEM_H_


#ifdef __cplusplus
extern "C" {
#endif

/**
 * XCL BO Flags bits layout
 *
 * bits  0 ~ 15: DDR BANK index
 * bits 24 ~ 31: BO flags
 */
#define XRT_BO_FLAGS_MEMIDX_MASK	(0xFFFFFFUL)
#define	XCL_BO_FLAGS_NONE		(0)
#define	XCL_BO_FLAGS_CACHEABLE		(1U << 24)
#define	XCL_BO_FLAGS_KERNBUF		(1U << 25)
#define	XCL_BO_FLAGS_SGL		(1U << 26)
#define	XCL_BO_FLAGS_SVM		(1U << 27)
#define	XCL_BO_FLAGS_DEV_ONLY		(1U << 28)
#define	XCL_BO_FLAGS_HOST_ONLY		(1U << 29)
#define	XCL_BO_FLAGS_P2P		(1U << 30)
#define	XCL_BO_FLAGS_EXECBUF		(1U << 31)

/**
 * XRT Native BO flags
 *
 * These flags are simple aliases for use with XRT native BO APIs.
 */
#define XRT_BO_FLAGS_NONE      XCL_BO_FLAGS_NONE
#define XRT_BO_FLAGS_CACHEABLE XCL_BO_FLAGS_CACHEABLE
#define XRT_BO_FLAGS_DEV_ONLY  XCL_BO_FLAGS_DEV_ONLY
#define XRT_BO_FLAGS_HOST_ONLY XCL_BO_FLAGS_HOST_ONLY
#define XRT_BO_FLAGS_P2P       XCL_BO_FLAGS_P2P

/**
 * This is the legacy usage of XCL DDR Flags.
 *
 * byte-0 lower 4 bits for DDR Flags are one-hot encoded
 */
enum xclDDRFlags {
    XCL_DEVICE_RAM_BANK0 = 0x00000000,
    XCL_DEVICE_RAM_BANK1 = 0x00000002,
    XCL_DEVICE_RAM_BANK2 = 0x00000004,
    XCL_DEVICE_RAM_BANK3 = 0x00000008,
};

#ifdef __cplusplus
}
#endif
#endif
