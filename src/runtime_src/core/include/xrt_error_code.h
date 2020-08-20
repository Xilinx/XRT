/*
 *  Copyright (C) 2020, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.  This program is
 *  distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 *  License for more details.  You should have received a copy of the
 *  GNU General Public License along with this program; if not, write
 *  to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA
 *
 */

#ifndef XRT_ERROR_CODE_H_
#define XRT_ERROR_CODE_H_

#if defined(__KERNEL__)
# include <linux/types.h>
#else
# include <stdint.h>
#endif

/**
 * xrtErrorCode layout
 *
 * This layout is internal to XRT (akin to a POSIX error code).
 *
 * The error code is populated by driver and consumed by XRT
 * implementation where it is translated into an actual error / info /
 * warning that is propagated to the end user.
 *
 * 63 - 48  47 - 40   39 - 32   31 - 24   23 - 16    15 - 0
 * --------------------------------------------------------
 * |    |    |    |    |    |    |    |    |    |    |----| xrtErrorNum
 * |    |    |    |    |    |    |    |    |----|---------- xrtErrorDriver
 * |    |    |    |    |    |    |----|-------------------- xrtErrorSeverity
 * |    |    |    |    |----|------------------------------ xrtErrorModule
 * |    |    |----|---------------------------------------- xrtErrorClass
 * |----|-------------------------------------------------- reserved
 *
 */
typedef uint64_t xrtErrorCode;

#define XRT_ERROR_NUM_MASK		0xFFFFUL
#define XRT_ERROR_NUM_SHIFT		0
#define XRT_ERROR_DRIVER_MASK		0xFUL
#define XRT_ERROR_DRIVER_SHIFT		16
#define XRT_ERROR_SEVERITY_MASK		0xFUL
#define XRT_ERROR_SEVERITY_SHIFT	24
#define XRT_ERROR_MODULE_MASK		0xFUL
#define XRT_ERROR_MODULE_SHIFT		32
#define XRT_ERROR_CLASS_MASK		0xFUL
#define XRT_ERROR_CLASS_SHIFT		40

#define	XRT_ERROR_CODE_BUILD(num, driver, severity, module, eclass) \
	((((num) & XRT_ERROR_NUM_MASK) << XRT_ERROR_NUM_SHIFT) | \
	(((driver) & XRT_ERROR_DRIVER_MASK) << XRT_ERROR_DRIVER_SHIFT) | \
	(((severity) & XRT_ERROR_SEVERITY_MASK) << XRT_ERROR_SEVERITY_SHIFT) | \
	(((module) & XRT_ERROR_MODULE_MASK) << XRT_ERROR_MODULE_SHIFT) | \
	(((eclass) & XRT_ERROR_CLASS_MASK) << XRT_ERROR_CLASS_SHIFT))

/**
 * xrt_error_num - XRT specific error numbers
 */
enum xrtErrorNum {
  XRT_ERROR_NUM_FIRWWALL_TRIP = 0,
  XRT_ERROR_NUM_TEMP_HIGH,
  XRT_ERROR_NUM_AXI_MM_SLAVE_TILE,
  XRT_ERROR_NUM_DM_ECC,
  XRT_ERROR_DMA_S2MM_0
};

enum xrtErrorDriver {
  XRT_ERROR_DRIVER_XOCL,
  XRT_ERROR_DRIVER_XCLMGMT,
  XRT_ERROR_DRIVER_ZOCL,
  XRT_ERROR_DRIVER_AIE
};

enum xrtErrorSeverity {
  XRT_ERROR_SEVERITY_EMERGENCY = 0,
  XRT_ERROR_SEVERITY_ALERT,
  XRT_ERROR_SEVERITY_CRITICAL,
  XRT_ERROR_SEVERITY_ERROR,
  XRT_ERROR_SEVERITY_WARNING,
  XRT_ERROR_SEVERITY_NOTICE,
  XRT_ERROR_SEVERITY_INFO,
  XRT_ERROR_SEVERITY_DEBUG
};

enum xrtErrorModule {
  XRT_ERROR_MODULE_FIREWALL = 0,
  XRT_ERROR_MODULE_CMC,
  XRT_ERROR_MODULE_AIE_CORE,
  XRT_ERROR_MODULE_AIE_MEMORY,
  XRT_ERROR_MODULE_AIE_SHIM
};

enum xrtErrorClass {
  XRT_ERROR_CLASS_FIRST_ENTRY = 0,
  XRT_ERROR_CLASS_SYSTEM = XRT_ERROR_CLASS_FIRST_ENTRY,
  XRT_ERROR_CLASS_AIE,
  XRT_ERROR_CLASS_HARDWARE,
  XRT_ERROR_CLASS_LAST_ENTRY,
};

#endif
